#include "reader.h"
#include "serialize.h"
#include "crc32.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct PTDBReader {
    int fd;
    uint8_t dim;
    uint8_t time_unit;
    size_t point_stride;
    size_t chunk_header_size;
    void *points_buffer;
    size_t points_buffer_capacity;
};

PTDBReader *ptdb_reader_open(const char *filepath)
{
    int fd = open(filepath, O_RDONLY);
    if (fd == -1)
    {
        perror("ptdb_reader_open: open");
        return NULL;
    }

    uint8_t raw_hdr[FILE_HEADER_SIZE];
    ssize_t bytes_read = read(fd, raw_hdr, FILE_HEADER_SIZE);
    if (bytes_read != (ssize_t)FILE_HEADER_SIZE)
    {
        perror("ptdb_reader_open: read header");
        close(fd);
        return NULL;
    }

    FileHeader header;
    deserialize_file_header(&header, raw_hdr);

    if (header.magic != PTDB_FILE_MAGIC)
    {
        fprintf(stderr, "Error: Invalid file magic in '%s'\n", filepath);
        close(fd);
        return NULL;
    }

    if (header.dim != 2 && header.dim != 3)
    {
        fprintf(stderr, "Error: Invalid dimension %u in '%s'\n", header.dim, filepath);
        close(fd);
        return NULL;
    }

    PTDBReader *reader = (PTDBReader *)calloc(1, sizeof(PTDBReader));
    if (!reader)
    {
        perror("ptdb_reader_open: malloc");
        close(fd);
        return NULL;
    }

    reader->fd = fd;
    reader->dim = header.dim;
    reader->time_unit = header.time_unit;
    reader->point_stride = (reader->dim == 2) ? sizeof(Point2D) : sizeof(Point3D);
    reader->chunk_header_size = (reader->dim == 2) ? sizeof(ChunkHeader2D) : sizeof(ChunkHeader3D);

    return reader;
}

uint8_t ptdb_reader_get_dim(const PTDBReader *reader)
{
    return reader ? reader->dim : 0;
}

int ptdb_reader_next_chunk(PTDBReader *reader, void *header_out, void **points_out, uint32_t *count_out)
{
    if (!reader)
    {
        return -1;
    }

    uint8_t header_buf[64];
    ssize_t h_read = read(reader->fd, header_buf, reader->chunk_header_size);
    if (h_read == 0)
    {
        return 0;
    }
    if (h_read != (ssize_t)reader->chunk_header_size)
    {
        fprintf(stderr, "Error: Truncated chunk header read (%zd / %zu bytes)\n",
                h_read, reader->chunk_header_size);
        return -1;
    }

    uint16_t magic = get_le16(header_buf + 0);
    if (magic != PTDB_CHUNK_MAGIC)
    {
        fprintf(stderr, "Error: Corrupted chunk magic: 0x%04X (expected 0x%04X)\n",
                magic, PTDB_CHUNK_MAGIC);
        return -1;
    }

    uint16_t count;
    uint32_t payload_size;
    uint32_t expected_checksum;

    if (reader->dim == 2)
    {
        ChunkHeader2D ch;
        deserialize_chunk_header_2d(&ch, header_buf);
        count = ch.count;
        payload_size = ch.payload_size;
        expected_checksum = ch.checksum;
        if (header_out)
        {
            memcpy(header_out, &ch, sizeof(ChunkHeader2D));
        }
    }
    else
    {
        ChunkHeader3D ch;
        deserialize_chunk_header_3d(&ch, header_buf);
        count = ch.count;
        payload_size = ch.payload_size;
        expected_checksum = ch.checksum;
        if (header_out)
        {
            memcpy(header_out, &ch, sizeof(ChunkHeader3D));
        }
    }

    if (payload_size != (uint32_t)count * (uint32_t)reader->point_stride)
    {
        fprintf(stderr, "Error: Invalid chunk payload size %u (expected %u)\n",
                payload_size, count * (uint32_t)reader->point_stride);
        return -1;
    }

    if (payload_size > reader->points_buffer_capacity)
    {
        void *new_buf = realloc(reader->points_buffer, payload_size);
        if (!new_buf)
        {
            perror("ptdb_reader_next_chunk: realloc");
            return -1;
        }
        reader->points_buffer = new_buf;
        reader->points_buffer_capacity = payload_size;
    }

    ssize_t p_read = read(reader->fd, reader->points_buffer, payload_size);
    if (p_read != (ssize_t)payload_size)
    {
        fprintf(stderr, "Error: Truncated chunk payload (%zd / %u bytes)\n", p_read, payload_size);
        return -1;
    }

    uint32_t computed_checksum = ptdb_crc32(reader->points_buffer, payload_size);
    if (computed_checksum != expected_checksum)
    {
        fprintf(stderr, "Error: Checksum mismatch in chunk (expected 0x%08X, got 0x%08X)\n",
                expected_checksum, computed_checksum);
        return -2;
    }

    if (points_out)
    {
        *points_out = reader->points_buffer;
    }
    if (count_out)
    {
        *count_out = count;
    }

    return 1;
}

void ptdb_get_point_2d(const void *points, uint32_t index, Point2D *out)
{
    if (!points || !out)
    {
        return;
    }
    const uint8_t *ptr = (const uint8_t *)points + (index * sizeof(Point2D));
    out->timestamp = (int64_t)get_le64(ptr + 0);
    out->x = get_le_float(ptr + 8);
    out->y = get_le_float(ptr + 12);
}

void ptdb_get_point_3d(const void *points, uint32_t index, Point3D *out)
{
    if (!points || !out)
    {
        return;
    }
    const uint8_t *ptr = (const uint8_t *)points + (index * sizeof(Point3D));
    out->timestamp = (int64_t)get_le64(ptr + 0);
    out->x = get_le_float(ptr + 8);
    out->y = get_le_float(ptr + 12);
    out->z = get_le_float(ptr + 16);
}

void ptdb_reader_close(PTDBReader *reader)
{
    if (!reader)
    {
        return;
    }

    if (reader->points_buffer)
    {
        free(reader->points_buffer);
        reader->points_buffer = NULL;
    }

    if (reader->fd != -1)
    {
        close(reader->fd);
        reader->fd = -1;
    }

    free(reader);
}

uint64_t ptdb_query_range_2d(const char *filepath, const PTDBQueryRange2D *range, ptdb_point_2d_callback callback, void *user_data)
{
    if (!filepath || !range || !callback)
    {
        return 0;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1)
    {
        return 0;
    }

    uint8_t raw_hdr[FILE_HEADER_SIZE];
    if (read(fd, raw_hdr, FILE_HEADER_SIZE) != (ssize_t)FILE_HEADER_SIZE)
    {
        close(fd);
        return 0;
    }

    FileHeader fh;
    deserialize_file_header(&fh, raw_hdr);
    if (fh.magic != PTDB_FILE_MAGIC || fh.dim != 2)
    {
        close(fd);
        return 0;
    }

    uint8_t hdr_buf[CHUNK_HEADER_2D_SIZE];
    uint8_t *payload_buf = NULL;
    size_t payload_capacity = 0;
    uint64_t matched_points = 0;

    while (1)
    {
        ssize_t h_read = read(fd, hdr_buf, CHUNK_HEADER_2D_SIZE);
        if (h_read == 0)
        {
            break;
        }
        if (h_read != (ssize_t)CHUNK_HEADER_2D_SIZE)
        {
            break;
        }

        ChunkHeader2D ch;
        deserialize_chunk_header_2d(&ch, hdr_buf);
        if (ch.magic != PTDB_CHUNK_MAGIC)
        {
            break;
        }

        if (ch.end_time < range->min_time || ch.start_time > range->max_time)
        {
            lseek(fd, ch.payload_size, SEEK_CUR);
            continue;
        }

        if (ch.max_x < range->min_x || ch.min_x > range->max_x ||
            ch.max_y < range->min_y || ch.min_y > range->max_y)
        {
            lseek(fd, ch.payload_size, SEEK_CUR);
            continue;
        }

        if (ch.payload_size != (uint32_t)ch.count * (uint32_t)sizeof(Point2D))
        {
            break;
        }

        if (ch.payload_size > payload_capacity)
        {
            uint8_t *new_buf = (uint8_t *)realloc(payload_buf, ch.payload_size);
            if (!new_buf)
            {
                break;
            }
            payload_buf = new_buf;
            payload_capacity = ch.payload_size;
        }

        if (read(fd, payload_buf, ch.payload_size) != (ssize_t)ch.payload_size)
        {
            break;
        }

        if (ptdb_crc32(payload_buf, ch.payload_size) != ch.checksum)
        {
            break;
        }

        for (uint32_t i = 0; i < ch.count; i++)
        {
            Point2D pt;
            ptdb_get_point_2d(payload_buf, i, &pt);

            if (pt.timestamp >= range->min_time && pt.timestamp <= range->max_time &&
                pt.x >= range->min_x && pt.x <= range->max_x &&
                pt.y >= range->min_y && pt.y <= range->max_y)
            {
                callback(&pt, user_data);
                matched_points++;
            }
        }
    }

    if (payload_buf)
    {
        free(payload_buf);
    }
    close(fd);
    return matched_points;
}

uint64_t ptdb_query_range_3d(const char *filepath, const PTDBQueryRange3D *range, ptdb_point_3d_callback callback, void *user_data)
{
    if (!filepath || !range || !callback)
    {
        return 0;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1)
    {
        return 0;
    }

    uint8_t raw_hdr[FILE_HEADER_SIZE];
    if (read(fd, raw_hdr, FILE_HEADER_SIZE) != (ssize_t)FILE_HEADER_SIZE)
    {
        close(fd);
        return 0;
    }

    FileHeader fh;
    deserialize_file_header(&fh, raw_hdr);
    if (fh.magic != PTDB_FILE_MAGIC || fh.dim != 3)
    {
        close(fd);
        return 0;
    }

    uint8_t hdr_buf[CHUNK_HEADER_3D_SIZE];
    uint8_t *payload_buf = NULL;
    size_t payload_capacity = 0;
    uint64_t matched_points = 0;

    while (1)
    {
        ssize_t h_read = read(fd, hdr_buf, CHUNK_HEADER_3D_SIZE);
        if (h_read == 0)
        {
            break;
        }
        if (h_read != (ssize_t)CHUNK_HEADER_3D_SIZE)
        {
            break;
        }

        ChunkHeader3D ch;
        deserialize_chunk_header_3d(&ch, hdr_buf);
        if (ch.magic != PTDB_CHUNK_MAGIC)
        {
            break;
        }

        if (ch.end_time < range->min_time || ch.start_time > range->max_time)
        {
            lseek(fd, ch.payload_size, SEEK_CUR);
            continue;
        }

        if (ch.max_x < range->min_x || ch.min_x > range->max_x ||
            ch.max_y < range->min_y || ch.min_y > range->max_y ||
            ch.max_z < range->min_z || ch.min_z > range->max_z)
        {
            lseek(fd, ch.payload_size, SEEK_CUR);
            continue;
        }

        if (ch.payload_size != (uint32_t)ch.count * (uint32_t)sizeof(Point3D))
        {
            break;
        }

        if (ch.payload_size > payload_capacity)
        {
            uint8_t *new_buf = (uint8_t *)realloc(payload_buf, ch.payload_size);
            if (!new_buf)
            {
                break;
            }
            payload_buf = new_buf;
            payload_capacity = ch.payload_size;
        }

        if (read(fd, payload_buf, ch.payload_size) != (ssize_t)ch.payload_size)
        {
            break;
        }

        if (ptdb_crc32(payload_buf, ch.payload_size) != ch.checksum)
        {
            break;
        }

        for (uint32_t i = 0; i < ch.count; i++)
        {
            Point3D pt;
            ptdb_get_point_3d(payload_buf, i, &pt);

            if (pt.timestamp >= range->min_time && pt.timestamp <= range->max_time &&
                pt.x >= range->min_x && pt.x <= range->max_x &&
                pt.y >= range->min_y && pt.y <= range->max_y &&
                pt.z >= range->min_z && pt.z <= range->max_z)
            {
                callback(&pt, user_data);
                matched_points++;
            }
        }
    }

    if (payload_buf)
    {
        free(payload_buf);
    }
    close(fd);
    return matched_points;
}
