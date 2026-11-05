#include "ptdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

static void print_point_2d(const Point2D *p, void *user_data)
{
    (void)user_data;
    printf("  [Result] Timestamp: %ld, Coordinate: (%.2f, %.2f)\n",
           (long)p->timestamp, p->x, p->y);
}

int main(void)
{
    const char *db_path = "quickstart.db";
    unlink(db_path);

    printf("=== potDB Embedded Quickstart Example ===\n\n");

    if (ptdb_init(db_path, 2, PTDB_TIME_UNIT_MS) != 0)
    {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    printf("1. Database '%s' initialized (2D, milliseconds)\n", db_path);

    PTDBConfig config = {
        .max_points = 100,
        .max_time_window_ms = 5000,
        .sync_mode = PTDB_SYNC_ALWAYS,
        .sync_interval_flushes = 10
    };

    PTDB *db = ptdb_open(db_path, &config);
    if (!db)
    {
        fprintf(stderr, "Failed to open database for writing\n");
        return 1;
    }

    printf("2. Writing 5 spatial coordinates...\n");
    ptdb_insert_2d(db, 1700000000, 10.0f, 20.0f);
    ptdb_insert_2d(db, 1700000010, 12.5f, 22.5f);
    ptdb_insert_2d(db, 1700000020, 15.0f, 25.0f);
    ptdb_insert_2d(db, 1700000030, 18.2f, 28.1f);
    ptdb_insert_2d(db, 1700000040, 20.0f, 30.0f);

    ptdb_close(db);
    printf("   Points persisted and database closed.\n\n");

    printf("3. Querying points within bounding box X:[10..16], Y:[20..26]...\n");
    PTDBQueryRange2D query = {
        .min_time = 1700000000,
        .max_time = 1700000050,
        .min_x = 10.0f, .max_x = 16.0f,
        .min_y = 20.0f, .max_y = 26.0f
    };

    uint64_t matched = ptdb_query_range_2d(db_path, &query, print_point_2d, NULL);
    printf("   Total matching points: %lu\n\n", (unsigned long)matched);

    printf("4. Sequential chunk traversal with PTDBReader...\n");
    PTDBReader *reader = ptdb_reader_open(db_path);
    if (!reader)
    {
        fprintf(stderr, "Failed to open database for reading\n");
        return 1;
    }

    ChunkHeader2D ch;
    void *points = NULL;
    uint32_t count = 0;

    while (ptdb_reader_next_chunk(reader, &ch, &points, &count) == 1)
    {
        printf("   Chunk Header -> Points: %u, Time: [%ld..%ld], MBR: X:[%.2f..%.2f] Y:[%.2f..%.2f]\n",
               count, (long)ch.start_time, (long)ch.end_time,
               ch.min_x, ch.max_x, ch.min_y, ch.max_y);

        for (uint32_t i = 0; i < count; i++)
        {
            Point2D pt;
            ptdb_get_point_2d(points, i, &pt);
            printf("     Point #%u: (%ld, %.2f, %.2f)\n", i, (long)pt.timestamp, pt.x, pt.y);
        }
    }

    ptdb_reader_close(reader);
    unlink(db_path);

    printf("\nQuickstart executed successfully!\n");
    return 0;
}
