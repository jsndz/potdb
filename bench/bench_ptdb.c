#include "ptdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#define BENCH_DB "bench_target.db"

static double get_time_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void bench_write_buffer_sizes(void)
{
    const uint32_t buffer_sizes[] = { 100, 500, 1000, 5000, 10000, 50000 };
    const size_t num_sizes = sizeof(buffer_sizes) / sizeof(buffer_sizes[0]);
    const uint32_t TOTAL_POINTS = 200000;

    printf("\n--- 1. Ingestion Throughput vs Chunk Buffer Size (%u 2D Points) ---\n", TOTAL_POINTS);
    printf("%-12s | %-12s | %-14s | %-12s | %-12s\n",
           "max_points", "Elapsed (s)", "Throughput (p/s)", "Rate (MB/s)", "Flushes");
    printf("-------------+--------------+----------------+--------------+-------------\n");

    for (size_t s = 0; s < num_sizes; s++)
    {
        uint32_t bs = buffer_sizes[s];
        unlink(BENCH_DB);
        assert(ptdb_init(BENCH_DB, 2, PTDB_TIME_UNIT_MS) == 0);

        PTDBConfig config = { .max_points = bs, .max_time_window_ms = 10000000 };
        PTDB *db = ptdb_open(BENCH_DB, &config);
        assert(db != NULL);

        double t0 = get_time_sec();
        for (uint32_t i = 0; i < TOTAL_POINTS; i++)
        {
            int64_t t = 1000000LL + (int64_t)i * 10;
            float x = (float)i * 0.1f;
            float y = (float)i * -0.1f;
            ptdb_insert_2d(db, t, x, y);
        }
        ptdb_close(db);
        double elapsed = get_time_sec() - t0;

        struct stat st;
        stat(BENCH_DB, &st);
        double mb = (double)st.st_size / (1024.0 * 1024.0);
        double mb_per_sec = mb / elapsed;
        double pts_per_sec = (double)TOTAL_POINTS / elapsed;
        uint32_t flushes = (TOTAL_POINTS + bs - 1) / bs;

        printf("%-12u | %-12.4f | %-14.0f | %-12.2f | %-12u\n",
               bs, elapsed, pts_per_sec, mb_per_sec, flushes);
    }
    unlink(BENCH_DB);
}

static void bench_read_throughput(void)
{
    const uint32_t TOTAL_POINTS = 500000;
    const uint32_t CHUNK_SIZE = 1000;

    printf("\n--- 2. Sequential Read & Scan Throughput (%u Points) ---\n", TOTAL_POINTS);
    unlink(BENCH_DB);
    assert(ptdb_init(BENCH_DB, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig config = { .max_points = CHUNK_SIZE, .max_time_window_ms = 10000000 };
    PTDB *db = ptdb_open(BENCH_DB, &config);
    assert(db != NULL);

    for (uint32_t i = 0; i < TOTAL_POINTS; i++)
    {
        ptdb_insert_2d(db, 1000000LL + (int64_t)i * 10, (float)i, (float)i);
    }
    ptdb_close(db);

    struct stat st;
    stat(BENCH_DB, &st);
    double mb = (double)st.st_size / (1024.0 * 1024.0);

    PTDBReader *reader = ptdb_reader_open(BENCH_DB);
    assert(reader != NULL);

    ChunkHeader2D ch;
    void *pts = NULL;
    uint32_t count = 0;
    uint32_t read_points = 0;
    uint32_t read_chunks = 0;

    double t0 = get_time_sec();
    while (ptdb_reader_next_chunk(reader, &ch, &pts, &count) == 1)
    {
        read_chunks++;
        read_points += count;
    }
    double elapsed = get_time_sec() - t0;
    ptdb_reader_close(reader);

    printf("Read & CRC-verified %u points in %u chunks (%.2f MB)\n", read_points, read_chunks, mb);
    printf("Elapsed Time:      %.4f seconds\n", elapsed);
    printf("Read Throughput:   %.0f points/sec\n", (double)read_points / elapsed);
    printf("Data Bandwidth:    %.2f MB/sec\n", mb / elapsed);

    unlink(BENCH_DB);
}

static void bench_point_accessors(void)
{
    const uint32_t N = 1000000;
    printf("\n--- 3. Point Accessor Overhead (In-Memory Deserialization) ---\n");

    Point2D *buf2d = (Point2D *)malloc(N * sizeof(Point2D));
    assert(buf2d != NULL);
    for (uint32_t i = 0; i < N; i++)
    {
        buf2d[i].timestamp = (int64_t)i;
        buf2d[i].x = (float)i;
        buf2d[i].y = (float)i * 2.0f;
    }

    Point2D out2d;
    double t0 = get_time_sec();
    for (uint32_t i = 0; i < N; i++)
    {
        ptdb_get_point_2d(buf2d, i, &out2d);
    }
    double elapsed2d = get_time_sec() - t0;
    double ns_per_point_2d = (elapsed2d * 1e9) / (double)N;

    printf("2D Point Accessor: %.2f ns/point (%.0f points/sec)\n",
           ns_per_point_2d, (double)N / elapsed2d);

    free(buf2d);

    Point3D *buf3d = (Point3D *)malloc(N * sizeof(Point3D));
    assert(buf3d != NULL);
    for (uint32_t i = 0; i < N; i++)
    {
        buf3d[i].timestamp = (int64_t)i;
        buf3d[i].x = (float)i;
        buf3d[i].y = (float)i * 2.0f;
        buf3d[i].z = (float)i * 3.0f;
    }

    Point3D out3d;
    t0 = get_time_sec();
    for (uint32_t i = 0; i < N; i++)
    {
        ptdb_get_point_3d(buf3d, i, &out3d);
    }
    double elapsed3d = get_time_sec() - t0;
    double ns_per_point_3d = (elapsed3d * 1e9) / (double)N;

    printf("3D Point Accessor: %.2f ns/point (%.0f points/sec)\n",
           ns_per_point_3d, (double)N / elapsed3d);

    free(buf3d);
}

int main(void)
{
    printf("==================================================\n");
    printf("          PTDB ENGINE PERFORMANCE BENCHMARK       \n");
    printf("==================================================\n");

    bench_write_buffer_sizes();
    bench_read_throughput();
    bench_point_accessors();

    printf("\n==================================================\n");
    printf("          BENCHMARK COMPLETED SUCCESSFULLY!       \n");
    printf("==================================================\n");
    return 0;
}
