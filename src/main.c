#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ptdb.h"

static void dump_database(const char *filepath)
{
    PTDBReader *reader = ptdb_reader_open(filepath);
    if (!reader)
    {
        return;
    }

    uint8_t dim = ptdb_reader_get_dim(reader);
    uint32_t chunk_idx = 0;
    uint32_t total_points = 0;

    if (dim == 2)
    {
        ChunkHeader2D ch;
        void *points = NULL;
        uint32_t count = 0;

        while (ptdb_reader_next_chunk(reader, &ch, &points, &count) == 1)
        {
            printf("--- Chunk %u [2D] ---\n", chunk_idx++);
            printf("  Points: %u | Time: [%ld .. %ld]\n",
                   count, (long)ch.start_time, (long)ch.end_time);
            printf("  MBR: X: [%.4f .. %.4f], Y: [%.4f .. %.4f]\n",
                   ch.min_x, ch.max_x, ch.min_y, ch.max_y);
            printf("  CRC32: 0x%08X (Verified)\n", ch.checksum);

            for (uint32_t i = 0; i < count; i++)
            {
                Point2D pt;
                ptdb_get_point_2d(points, i, &pt);
                printf("  (%ld, %.4f, %.4f)\n", (long)pt.timestamp, pt.x, pt.y);
            }
            total_points += count;
        }
    }
    else
    {
        ChunkHeader3D ch;
        void *points = NULL;
        uint32_t count = 0;

        while (ptdb_reader_next_chunk(reader, &ch, &points, &count) == 1)
        {
            printf("--- Chunk %u [3D] ---\n", chunk_idx++);
            printf("  Points: %u | Time: [%ld .. %ld]\n",
                   count, (long)ch.start_time, (long)ch.end_time);
            printf("  MBR: X: [%.4f .. %.4f], Y: [%.4f .. %.4f], Z: [%.4f .. %.4f]\n",
                   ch.min_x, ch.max_x, ch.min_y, ch.max_y, ch.min_z, ch.max_z);
            printf("  CRC32: 0x%08X (Verified)\n", ch.checksum);

            for (uint32_t i = 0; i < count; i++)
            {
                Point3D pt;
                ptdb_get_point_3d(points, i, &pt);
                printf("  (%ld, %.4f, %.4f, %.4f)\n",
                       (long)pt.timestamp, pt.x, pt.y, pt.z);
            }
            total_points += count;
        }
    }

    printf("Total Chunks: %u, Total Points: %u\n", chunk_idx, total_points);
    ptdb_reader_close(reader);
}

void print_cb(const Point2D *p, void *ud){
    (void)ud;
    printf("(%ld, %.4f, %.4f)\n", (long)p->timestamp, p->x, p->y);
}
void print_cb3(const Point3D *p, void *ud) {
    (void)ud;
    printf("(%ld, %.4f, %.4f, %.4f)\n", (long)p->timestamp, p->x, p->y, p->z);
}
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file> [init <dim> [time_unit] | insert <timestamp> <x> <y> [z] | select | dump]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *db_filename = argv[1];

    if (argc >= 3 && strcmp(argv[2], "init") == 0)
    {
        uint8_t dim = (argc >= 4) ? (uint8_t)atoi(argv[3]) : 2;
        uint8_t time_unit = (argc >= 5) ? (uint8_t)atoi(argv[4]) : PTDB_TIME_UNIT_MS;

        if (dim != 2 && dim != 3)
        {
            fprintf(stderr, "Error: Dimension must be 2 or 3 (got %u)\n", dim);
            return 1;
        }

        if (ptdb_init(db_filename, dim, time_unit) != 0)
        {
            return 1;
        }
        printf("Database '%s' initialized successfully (dim: %uD, time_unit: %u).\n",
               db_filename, dim, time_unit);
        return 0;
    }

    if (argc >= 3 && (strcmp(argv[2], "select") == 0 || strcmp(argv[2], "dump") == 0))
    {
        dump_database(db_filename);
        return 0;
    }

    if (argc >= 3 && strcmp(argv[2], "query") == 0)
    {
        PTDBReader *reader = ptdb_reader_open(db_filename);
        if (!reader)
        {
            return 1;
        }
        uint8_t dim = ptdb_reader_get_dim(reader);
        ptdb_reader_close(reader);

        if (dim == 2)
        {
            if (argc < 9)
            {
                fprintf(stderr, "Usage: %s %s query <min_time> <max_time> <min_x> <max_x> <min_y> <max_y>\n", argv[0], argv[1]);
                return 1;
            }
            PTDBQueryRange2D range = {
                .min_time = (int64_t)atoll(argv[3]),
                .max_time = (int64_t)atoll(argv[4]),
                .min_x = (float)atof(argv[5]),
                .max_x = (float)atof(argv[6]),
                .min_y = (float)atof(argv[7]),
                .max_y = (float)atof(argv[8])
            };
         
            uint64_t matched = ptdb_query_range_2d(db_filename, &range, print_cb, NULL);
            printf("Matched points: %lu\n", (unsigned long)matched);
            return 0;
        }
        else
        {
            if (argc < 11)
            {
                fprintf(stderr, "Usage: %s %s query <min_time> <max_time> <min_x> <max_x> <min_y> <max_y> <min_z> <max_z>\n", argv[0], argv[1]);
                return 1;
            }
            PTDBQueryRange3D range = {
                .min_time = (int64_t)atoll(argv[3]),
                .max_time = (int64_t)atoll(argv[4]),
                .min_x = (float)atof(argv[5]),
                .max_x = (float)atof(argv[6]),
                .min_y = (float)atof(argv[7]),
                .max_y = (float)atof(argv[8]),
                .min_z = (float)atof(argv[9]),
                .max_z = (float)atof(argv[10])
            };
         
            uint64_t matched = ptdb_query_range_3d(db_filename, &range, print_cb3, NULL);
            printf("Matched points: %lu\n", (unsigned long)matched);
            return 0;
        }
    }

    if (argc >= 3 && strcmp(argv[2], "insert") == 0)
    {
        PTDB *db = ptdb_open(db_filename, NULL);
        if (!db)
        {
            return 1;
        }

        uint8_t dim = ptdb_get_dim(db);
        if (dim == 2)
        {
            if (argc < 6)
            {
                fprintf(stderr, "Usage: %s %s insert <timestamp> <x> <y>\n", argv[0], argv[1]);
                ptdb_close(db);
                return 1;
            }

            int64_t timestamp = (int64_t)atoll(argv[3]);
            float x = (float)atof(argv[4]);
            float y = (float)atof(argv[5]);

            if (ptdb_insert_2d(db, timestamp, x, y) != 0)
            {
                ptdb_close(db);
                return 1;
            }
            printf("Inserted 2D point (%ld, %.4f, %.4f)\n", (long)timestamp, x, y);
        }
        else if (dim == 3)
        {
            if (argc < 7)
            {
                fprintf(stderr, "Usage: %s %s insert <timestamp> <x> <y> <z>\n", argv[0], argv[1]);
                ptdb_close(db);
                return 1;
            }

            int64_t timestamp = (int64_t)atoll(argv[3]);
            float x = (float)atof(argv[4]);
            float y = (float)atof(argv[5]);
            float z = (float)atof(argv[6]);

            if (ptdb_insert_3d(db, timestamp, x, y, z) != 0)
            {
                ptdb_close(db);
                return 1;
            }
            printf("Inserted 3D point (%ld, %.4f, %.4f, %.4f)\n", (long)timestamp, x, y, z);
        }

        ptdb_close(db);
        return 0;
    }

    PTDB *db = ptdb_open(db_filename, NULL);
    if (!db)
    {
        return 1;
    }

    uint8_t dim = ptdb_get_dim(db);
    char *line = NULL;
    size_t len = 0;
    ssize_t read_bytes;

    while ((read_bytes = getline(&line, &len, stdin)) != -1)
    {
        while (read_bytes > 0 && (line[read_bytes - 1] == '\n' || line[read_bytes - 1] == '\r'))
        {
            line[--read_bytes] = '\0';
        }
        if (read_bytes == 0)
        {
            continue;
        }

        char *ptr = line;
        if (strncmp(ptr, "insert ", 7) == 0)
        {
            ptr += 7;
        }

        if (dim == 2)
        {
            long long timestamp;
            float x, y;
            if (sscanf(ptr, "%lld %f %f", &timestamp, &x, &y) == 3)
            {
                ptdb_insert_2d(db, (int64_t)timestamp, x, y);
            }
            else
            {
                fprintf(stderr, "Invalid 2D input: %s\n", line);
            }
        }
        else if (dim == 3)
        {
            long long timestamp;
            float x, y, z;
            if (sscanf(ptr, "%lld %f %f %f", &timestamp, &x, &y, &z) == 4)
            {
                ptdb_insert_3d(db, (int64_t)timestamp, x, y, z);
            }
            else
            {
                fprintf(stderr, "Invalid 3D input: %s\n", line);
            }
        }
    }

    free(line);
    ptdb_close(db);
    return 0;
}
