#include "ptdb.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static uint32_t crc32_table[256];
static int crc32_initialized = 0;

static void init_crc32_table(void)
{
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
        {
            if (c & 1)
            {
                c = 0xEDB88320L ^ (c >> 1);
            }
            else
            {
                c = c >> 1;
            }
        }
        crc32_table[i] = c;
    }
    crc32_initialized = 1;
}

uint32_t ptdb_crc32(const void *data, size_t length)
{
    if (!crc32_initialized)
    {
        init_crc32_table();
    }
    const uint8_t *buf = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

static inline void put_le16(uint8_t *dst, uint16_t val)
{
    dst[0] = (uint8_t)(val & 0xFF);
    dst[1] = (uint8_t)((val >> 8) & 0xFF);
}

static inline void put_le32(uint8_t *dst, uint32_t val)
{
    dst[0] = (uint8_t)(val & 0xFF);
    dst[1] = (uint8_t)((val >> 8) & 0xFF);
    dst[2] = (uint8_t)((val >> 16) & 0xFF);
    dst[3] = (uint8_t)((val >> 24) & 0xFF);
}

static inline void put_le64(uint8_t *dst, uint64_t val)
{
    dst[0] = (uint8_t)(val & 0xFF);
    dst[1] = (uint8_t)((val >> 8) & 0xFF);
    dst[2] = (uint8_t)((val >> 16) & 0xFF);
    dst[3] = (uint8_t)((val >> 24) & 0xFF);
    dst[4] = (uint8_t)((val >> 32) & 0xFF);
    dst[5] = (uint8_t)((val >> 40) & 0xFF);
    dst[6] = (uint8_t)((val >> 48) & 0xFF);
    dst[7] = (uint8_t)((val >> 56) & 0xFF);
}

static inline uint16_t get_le16(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0]) |
                      ((uint16_t)src[1] << 8));
}

static inline uint32_t get_le32(const uint8_t *src)
{
    return ((uint32_t)src[0])       |
           ((uint32_t)src[1] << 8)  |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static inline uint64_t get_le64(const uint8_t *src)
{
    return ((uint64_t)src[0])       |
           ((uint64_t)src[1] << 8)  |
           ((uint64_t)src[2] << 16) |
           ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) |
           ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) |
           ((uint64_t)src[7] << 56);
}

static inline void put_le_float(uint8_t *dst, float val)
{
    uint32_t raw;
    memcpy(&raw, &val, sizeof(float));
    put_le32(dst, raw);
}

static inline float get_le_float(const uint8_t *src)
{
    uint32_t raw = get_le32(src);
    float val;
    memcpy(&val, &raw, sizeof(float));
    return val;
}

static void serialize_file_header(uint8_t *dst, const FileHeader *h)
{
    put_le32(dst + 0, h->magic);
    put_le16(dst + 4, h->version);
    dst[6] = h->dim;
    dst[7] = h->time_unit;
    put_le64(dst + 8, h->reserved);
}

static void deserialize_file_header(FileHeader *h, const uint8_t *src)
{
    h->magic     = get_le32(src + 0);
    h->version   = get_le16(src + 4);
    h->dim       = src[6];
    h->time_unit = src[7];
    h->reserved  = get_le64(src + 8);
}

static void serialize_chunk_header_2d(uint8_t *dst, const ChunkHeader2D *h)
{
    put_le16(dst + 0,  h->magic);
    put_le16(dst + 2,  h->count);
    put_le64(dst + 4,  (uint64_t)h->start_time);
    put_le64(dst + 12, (uint64_t)h->end_time);
    put_le_float(dst + 20, h->min_x);
    put_le_float(dst + 24, h->max_x);
    put_le_float(dst + 28, h->min_y);
    put_le_float(dst + 32, h->max_y);
    put_le32(dst + 36, h->payload_size);
    put_le32(dst + 40, h->checksum);
}

static void deserialize_chunk_header_2d(ChunkHeader2D *h, const uint8_t *src)
{
    h->magic        = get_le16(src + 0);
    h->count        = get_le16(src + 2);
    h->start_time   = (int64_t)get_le64(src + 4);
    h->end_time     = (int64_t)get_le64(src + 12);
    h->min_x        = get_le_float(src + 20);
    h->max_x        = get_le_float(src + 24);
    h->min_y        = get_le_float(src + 28);
    h->max_y        = get_le_float(src + 32);
    h->payload_size = get_le32(src + 36);
    h->checksum     = get_le32(src + 40);
}

static void serialize_chunk_header_3d(uint8_t *dst, const ChunkHeader3D *h)
{
    put_le16(dst + 0,  h->magic);
    put_le16(dst + 2,  h->count);
    put_le64(dst + 4,  (uint64_t)h->start_time);
    put_le64(dst + 12, (uint64_t)h->end_time);
    put_le_float(dst + 20, h->min_x);
    put_le_float(dst + 24, h->max_x);
    put_le_float(dst + 28, h->min_y);
    put_le_float(dst + 32, h->max_y);
    put_le_float(dst + 36, h->min_z);
    put_le_float(dst + 40, h->max_z);
    put_le32(dst + 44, h->payload_size);
    put_le32(dst + 48, h->checksum);
}

static void deserialize_chunk_header_3d(ChunkHeader3D *h, const uint8_t *src)
{
    h->magic        = get_le16(src + 0);
    h->count        = get_le16(src + 2);
    h->start_time   = (int64_t)get_le64(src + 4);
    h->end_time     = (int64_t)get_le64(src + 12);
    h->min_x        = get_le_float(src + 20);
    h->max_x        = get_le_float(src + 24);
    h->min_y        = get_le_float(src + 28);
    h->max_y        = get_le_float(src + 32);
    h->min_z        = get_le_float(src + 36);
    h->max_z        = get_le_float(src + 40);
    h->payload_size = get_le32(src + 44);
    h->checksum     = get_le32(src + 48);
}

struct PTDB {
    int fd;                     
    uint8_t dim;                
    uint8_t time_unit;          
    size_t point_stride;        
    size_t chunk_header_size;   
    PTDBConfig config;          

    uint8_t *buffer;            
    uint32_t buffered_count;    
    int64_t first_point_timestamp; 
    int64_t last_timestamp;     
    bool has_first_point;       

    float min_x, max_x;         
    float min_y, max_y;         
    float min_z, max_z;         
};

int ptdb_init(const char *filepath, uint8_t dim, uint8_t time_unit)
{
    
    if (dim != 2 && dim != 3)
    {
        fprintf(stderr, "Error: Invalid dimension %u (must be 2 or 3)\n", dim);
        return -1;
    }

    if (time_unit == 0)
    {
        time_unit = PTDB_TIME_UNIT_MS;
    }

    int fd = open(filepath, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        perror("ptdb_init: open");
        return -1;
    }

    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1)
    {
        perror("ptdb_init: lseek");
        close(fd);
        return -1;
    }

    if (file_length > 0)
    {
        fprintf(stderr, "Error: Database '%s' is already initialized (size = %ld)\n",
                filepath, (long)file_length);
        close(fd);
        return -1;
    }

    FileHeader header;
    memset(&header, 0, sizeof(FileHeader));
    header.magic = PTDB_FILE_MAGIC;      
    header.version = PTDB_VERSION;        
    header.dim = dim;                    
    header.time_unit = time_unit;        
    header.reserved = 0;                 

    uint8_t raw_hdr[FILE_HEADER_SIZE];
    serialize_file_header(raw_hdr, &header);

    ssize_t bytes_written = write(fd, raw_hdr, FILE_HEADER_SIZE);
    if (bytes_written != (ssize_t)FILE_HEADER_SIZE)
    {
        perror("ptdb_init: write header");
        close(fd);
        return -1;
    }

    if (fsync(fd) == -1)
    {
        perror("ptdb_init: fsync");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

PTDB *ptdb_open(const char *filepath, const PTDBConfig *config)
{
    
    int fd = open(filepath, O_RDWR, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        perror("ptdb_open: open");
        return NULL;
    }

    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1)
    {
        perror("ptdb_open: lseek");
        close(fd);
        return NULL;
    }

    if (file_length == 0)
    {
        fprintf(stderr, "Error: Database '%s' is uninitialised.\n", filepath);
        close(fd);
        return NULL;
    }

    if (file_length < (off_t)FILE_HEADER_SIZE)
    {
        fprintf(stderr, "Error: Database '%s' is smaller than header size.\n", filepath);
        close(fd);
        return NULL;
    }

    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        perror("ptdb_open: lseek to start");
        close(fd);
        return NULL;
    }

    uint8_t raw_hdr[FILE_HEADER_SIZE];
    ssize_t bytes_read = read(fd, raw_hdr, FILE_HEADER_SIZE);
    if (bytes_read != (ssize_t)FILE_HEADER_SIZE)
    {
        perror("ptdb_open: read header");
        close(fd);
        return NULL;
    }

    FileHeader header;
    deserialize_file_header(&header, raw_hdr);

    if (header.magic != PTDB_FILE_MAGIC)
    {
        fprintf(stderr, "Error: Invalid file magic 0x%08X (expected 0x%08X)\n",
                header.magic, PTDB_FILE_MAGIC);
        close(fd);
        return NULL;
    }

    if (header.version != PTDB_VERSION)
    {
        fprintf(stderr, "Error: Unsupported version %u (expected %u)\n",
                header.version, PTDB_VERSION);
        close(fd);
        return NULL;
    }

    if (header.dim != 2 && header.dim != 3)
    {
        fprintf(stderr, "Error: Invalid dimension in header: %u\n", header.dim);
        close(fd);
        return NULL;
    }

    PTDB *db = (PTDB *)calloc(1, sizeof(PTDB));
    if (!db)
    {
        perror("ptdb_open: malloc PTDB");
        close(fd);
        return NULL;
    }

    db->fd = fd;
    db->dim = header.dim;
    db->time_unit = header.time_unit;
    db->point_stride = (db->dim == 2) ? sizeof(Point2D) : sizeof(Point3D);
    db->chunk_header_size = (db->dim == 2) ? sizeof(ChunkHeader2D) : sizeof(ChunkHeader3D);

    db->config.max_points = (config && config->max_points > 0) ? config->max_points : PTDB_DEFAULT_MAX_POINTS;
    if (db->config.max_points > 65535)
    {
        db->config.max_points = 65535;
    }
    db->config.max_time_window_ms = (config && config->max_time_window_ms > 0) ? config->max_time_window_ms : PTDB_DEFAULT_MAX_TIME_WINDOW_MS;

    db->buffer = (uint8_t *)malloc(db->config.max_points * db->point_stride);
    if (!db->buffer)
    {
        perror("ptdb_open: malloc RAM buffer");
        free(db);
        close(fd);
        return NULL;
    }

    db->buffered_count = 0;
    db->has_first_point = false;
    db->last_timestamp = INT64_MIN;

    if (lseek(fd, 0, SEEK_END) == -1)
    {
        perror("ptdb_open: lseek to end");
        free(db->buffer);
        free(db);
        close(fd);
        return NULL;
    }

    return db;
}

uint8_t ptdb_get_dim(const PTDB *db)
{
    return db ? db->dim : 0;
}

int ptdb_flush(PTDB *db)
{
    
    if (!db || db->buffered_count == 0)
    {
        return 0;
    }

    uint32_t payload_size = db->buffered_count * (uint32_t)db->point_stride;
    uint32_t checksum = ptdb_crc32(db->buffer, payload_size);

    if (lseek(db->fd, 0, SEEK_END) == -1)
    {
        perror("ptdb_flush: lseek");
        return -1;
    }

    if (db->dim == 2)
    {
        ChunkHeader2D header;
        memset(&header, 0, sizeof(ChunkHeader2D));
        header.magic = PTDB_CHUNK_MAGIC;
        header.count = (uint16_t)db->buffered_count;
        header.start_time = db->first_point_timestamp;
        header.end_time = db->last_timestamp;
        header.min_x = db->min_x;
        header.max_x = db->max_x;
        header.min_y = db->min_y;
        header.max_y = db->max_y;
        header.payload_size = payload_size;
        header.checksum = checksum;

        uint8_t raw_hdr[CHUNK_HEADER_2D_SIZE];
        serialize_chunk_header_2d(raw_hdr, &header);

        ssize_t h_written = write(db->fd, raw_hdr, CHUNK_HEADER_2D_SIZE);
        if (h_written != (ssize_t)CHUNK_HEADER_2D_SIZE)
        {
            perror("ptdb_flush: write 2D header");
            return -1;
        }
    }
    else
    {
        ChunkHeader3D header;
        memset(&header, 0, sizeof(ChunkHeader3D));
        header.magic = PTDB_CHUNK_MAGIC;
        header.count = (uint16_t)db->buffered_count;
        header.start_time = db->first_point_timestamp;
        header.end_time = db->last_timestamp;
        header.min_x = db->min_x;
        header.max_x = db->max_x;
        header.min_y = db->min_y;
        header.max_y = db->max_y;
        header.min_z = db->min_z;
        header.max_z = db->max_z;
        header.payload_size = payload_size;
        header.checksum = checksum;

        uint8_t raw_hdr[CHUNK_HEADER_3D_SIZE];
        serialize_chunk_header_3d(raw_hdr, &header);

        ssize_t h_written = write(db->fd, raw_hdr, CHUNK_HEADER_3D_SIZE);
        if (h_written != (ssize_t)CHUNK_HEADER_3D_SIZE)
        {
            perror("ptdb_flush: write 3D header");
            return -1;
        }
    }

    ssize_t b_written = write(db->fd, db->buffer, payload_size);
    if (b_written != (ssize_t)payload_size)
    {
        perror("ptdb_flush: write payload");
        return -1;
    }

    if (fsync(db->fd) == -1)
    {
        perror("ptdb_flush: fsync");
        return -1;
    }

    db->buffered_count = 0;
    db->has_first_point = false;

    return 0;
}

int ptdb_insert_2d(PTDB *db, int64_t timestamp, float x, float y)
{
    if (!db)
    {
        return -1;
    }

    if (db->dim != 2)
    {
        fprintf(stderr, "Error: Schema mismatch. Database is %uD, cannot insert 2D point.\n", db->dim);
        return -1;
    }

    if (db->last_timestamp != INT64_MIN && timestamp < db->last_timestamp)
    {
        fprintf(stderr, "Error: Non-monotonic timestamp: %ld < %ld\n", (long)timestamp, (long)db->last_timestamp);
        return -2;
    }

    if (db->buffered_count > 0 && db->has_first_point)
    {
        if ((timestamp - db->first_point_timestamp) >= (int64_t)db->config.max_time_window_ms)
        {
            if (ptdb_flush(db) != 0)
            {
                return -1;
            }
        }
    }

    uint8_t *dest = db->buffer + (db->buffered_count * db->point_stride);
    put_le64(dest + 0, (uint64_t)timestamp);
    put_le_float(dest + 8, x);
    put_le_float(dest + 12, y);

    if (db->buffered_count == 0)
    {
        
        db->first_point_timestamp = timestamp;
        db->has_first_point = true;
        db->min_x = db->max_x = x;
        db->min_y = db->max_y = y;
    }
    else
    {
        
        if (x < db->min_x) db->min_x = x;
        if (x > db->max_x) db->max_x = x;
        if (y < db->min_y) db->min_y = y;
        if (y > db->max_y) db->max_y = y;
    }

    db->last_timestamp = timestamp;
    db->buffered_count++;

    if (db->buffered_count >= db->config.max_points)
    {
        if (ptdb_flush(db) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int ptdb_insert_3d(PTDB *db, int64_t timestamp, float x, float y, float z)
{
    if (!db)
    {
        return -1;
    }

    if (db->dim != 3)
    {
        fprintf(stderr, "Error: Schema mismatch. Database is %uD, cannot insert 3D point.\n", db->dim);
        return -1;
    }

    if (db->last_timestamp != INT64_MIN && timestamp < db->last_timestamp)
    {
        fprintf(stderr, "Error: Non-monotonic timestamp: %ld < %ld\n", (long)timestamp, (long)db->last_timestamp);
        return -2;
    }

    if (db->buffered_count > 0 && db->has_first_point)
    {
        if ((timestamp - db->first_point_timestamp) >= (int64_t)db->config.max_time_window_ms)
        {
            if (ptdb_flush(db) != 0)
            {
                return -1;
            }
        }
    }

    uint8_t *dest = db->buffer + (db->buffered_count * db->point_stride);
    put_le64(dest + 0, (uint64_t)timestamp);
    put_le_float(dest + 8, x);
    put_le_float(dest + 12, y);
    put_le_float(dest + 16, z);

    if (db->buffered_count == 0)
    {
        
        db->first_point_timestamp = timestamp;
        db->has_first_point = true;
        db->min_x = db->max_x = x;
        db->min_y = db->max_y = y;
        db->min_z = db->max_z = z;
    }
    else
    {
        
        if (x < db->min_x) db->min_x = x;
        if (x > db->max_x) db->max_x = x;
        if (y < db->min_y) db->min_y = y;
        if (y > db->max_y) db->max_y = y;
        if (z < db->min_z) db->min_z = z;
        if (z > db->max_z) db->max_z = z;
    }

    db->last_timestamp = timestamp;
    db->buffered_count++;

    if (db->buffered_count >= db->config.max_points)
    {
        if (ptdb_flush(db) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int ptdb_write_2d(PTDB *db, int64_t timestamp, float x, float y)
{
    return ptdb_insert_2d(db, timestamp, x, y);
}

int ptdb_write_3d(PTDB *db, int64_t timestamp, float x, float y, float z)
{
    return ptdb_insert_3d(db, timestamp, x, y, z);
}

int ptdb_insert_point(PTDB *db, const void *point)
{
    if (!db || !point)
    {
        return -1;
    }

    if (db->dim == 2)
    {
        const Point2D *p = (const Point2D *)point;
        return ptdb_insert_2d(db, p->timestamp, p->x, p->y);
    }
    else if (db->dim == 3)
    {
        const Point3D *p = (const Point3D *)point;
        return ptdb_insert_3d(db, p->timestamp, p->x, p->y, p->z);
    }

    return -1;
}

int ptdb_close(PTDB *db)
{
    if (!db)
    {
        return 0;
    }

    if (db->buffered_count > 0)
    {
        ptdb_flush(db);
    }

    if (db->buffer)
    {
        free(db->buffer);
        db->buffer = NULL;
    }

    if (db->fd != -1)
    {
        close(db->fd);
        db->fd = -1;
    }

    free(db);
    return 0;
}

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

