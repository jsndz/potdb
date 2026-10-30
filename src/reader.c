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
