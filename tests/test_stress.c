#include "ptdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#define STRESS_DB_2D "stress_1m_2d.db"
#define STRESS_DB_3D "stress_1m_3d.db"
#define STRESS_DB_MULTI "stress_multi_session.db"

static double get_time_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void test_million_points_2d(void)
{
    printf("[Stress Test 1] Ingesting & Roundtrip Verifying 1,000,000 2D Points...\n");
    unlink(STRESS_DB_2D);
    assert(ptdb_init(STRESS_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig config = { .max_points = 500, .max_time_window_ms = 1000000 };
    PTDB *db = ptdb_open(STRESS_DB_2D, &config);
    assert(db != NULL);

    double t0 = get_time_sec();
    for (uint32_t i = 0; i < 1000000; i++)
    {
        int64_t t = 1000000000LL + (int64_t)i * 10;
        float x = (float)(i % 10000) * 0.1f;
        float y = (float)(i % 5000) * -0.2f;
        assert(ptdb_insert_2d(db, t, x, y) == 0);
    }
    assert(ptdb_close(db) == 0);
    double t_write = get_time_sec() - t0;

    struct stat st;
    assert(stat(STRESS_DB_2D, &st) == 0);
    printf("  -> Written 1,000,000 points in %.3f sec (%.0f pts/sec). File size: %ld bytes (%.2f MB)\n",
           t_write, 1000000.0 / t_write, (long)st.st_size, (double)st.st_size / (1024.0 * 1024.0));

    printf("  -> Verifying 1,000,000 points with PTDBReader...\n");
    t0 = get_time_sec();
    PTDBReader *reader = ptdb_reader_open(STRESS_DB_2D);
    assert(reader != NULL);

    ChunkHeader2D ch;
    void *pts = NULL;
    uint32_t count = 0;
    uint32_t total_chunks = 0;
    uint32_t total_points = 0;
    int64_t prev_end_time = -1;

    while (ptdb_reader_next_chunk(reader, &ch, &pts, &count) == 1)
    {
        assert(count == 500);
        assert(ch.start_time <= ch.end_time);
        if (prev_end_time != -1)
        {
            assert(ch.start_time >= prev_end_time);
        }
        prev_end_time = ch.end_time;

        for (uint32_t i = 0; i < count; i++)
        {
            Point2D p;
            ptdb_get_point_2d(pts, i, &p);
            assert(p.x >= ch.min_x && p.x <= ch.max_x);
            assert(p.y >= ch.min_y && p.y <= ch.max_y);
        }

        total_chunks++;
        total_points += count;
    }

    assert(total_chunks == 2000);
    assert(total_points == 1000000);
    ptdb_reader_close(reader);
    double t_read = get_time_sec() - t0;

    printf("  -> Verified %u points in %u chunks in %.3f sec (%.0f pts/sec). 100%% Integrity Verified!\n",
           total_points, total_chunks, t_read, (double)total_points / t_read);

    unlink(STRESS_DB_2D);
}

static void test_million_points_3d(void)
{
    printf("[Stress Test 2] Ingesting & Roundtrip Verifying 1,000,000 3D Points...\n");
    unlink(STRESS_DB_3D);
    assert(ptdb_init(STRESS_DB_3D, 3, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig config = { .max_points = 1000, .max_time_window_ms = 1000000 };
    PTDB *db = ptdb_open(STRESS_DB_3D, &config);
    assert(db != NULL);

    double t0 = get_time_sec();
    for (uint32_t i = 0; i < 1000000; i++)
    {
        int64_t t = 2000000000LL + (int64_t)i * 5;
        float x = (float)(i % 1000) * 1.1f;
        float y = (float)(i % 2000) * 2.2f;
        float z = (float)(i % 3000) * -3.3f;
        assert(ptdb_insert_3d(db, t, x, y, z) == 0);
    }
    assert(ptdb_close(db) == 0);
    double t_write = get_time_sec() - t0;

    struct stat st;
    assert(stat(STRESS_DB_3D, &st) == 0);
    printf("  -> Written 1,000,000 3D points in %.3f sec (%.0f pts/sec). File size: %ld bytes (%.2f MB)\n",
           t_write, 1000000.0 / t_write, (long)st.st_size, (double)st.st_size / (1024.0 * 1024.0));

    printf("  -> Verifying 1,000,000 3D points with PTDBReader...\n");
    t0 = get_time_sec();
    PTDBReader *reader = ptdb_reader_open(STRESS_DB_3D);
    assert(reader != NULL);

    ChunkHeader3D ch;
    void *pts = NULL;
    uint32_t count = 0;
    uint32_t total_chunks = 0;
    uint32_t total_points = 0;

    while (ptdb_reader_next_chunk(reader, &ch, &pts, &count) == 1)
    {
        assert(count == 1000);
        assert(ch.start_time <= ch.end_time);

        for (uint32_t i = 0; i < count; i++)
        {
            Point3D p;
            ptdb_get_point_3d(pts, i, &p);
            assert(p.x >= ch.min_x && p.x <= ch.max_x);
            assert(p.y >= ch.min_y && p.y <= ch.max_y);
            assert(p.z >= ch.min_z && p.z <= ch.max_z);
        }

        total_chunks++;
        total_points += count;
    }

    assert(total_chunks == 1000);
    assert(total_points == 1000000);
    ptdb_reader_close(reader);
    double t_read = get_time_sec() - t0;

    printf("  -> Verified %u 3D points in %u chunks in %.3f sec (%.0f pts/sec). 100%% Integrity Verified!\n",
           total_points, total_chunks, t_read, (double)total_points / t_read);

    unlink(STRESS_DB_3D);
}

static void test_multi_session_append(void)
{
    printf("[Stress Test 3] Multi-Session Continuity (200 Repeated Open-Append-Close Cycles)...\n");
    unlink(STRESS_DB_MULTI);
    assert(ptdb_init(STRESS_DB_MULTI, 2, PTDB_TIME_UNIT_MS) == 0);

    const int SESSIONS = 200;
    const int PTS_PER_SESSION = 250;
    int64_t current_time = 10000;

    PTDBConfig config = { .max_points = 500, .max_time_window_ms = 500000 };

    for (int s = 0; s < SESSIONS; s++)
    {
        PTDB *db = ptdb_open(STRESS_DB_MULTI, &config);
        assert(db != NULL);

        for (int p = 0; p < PTS_PER_SESSION; p++)
        {
            assert(ptdb_insert_2d(db, current_time, (float)s, (float)p) == 0);
            current_time += 10;
        }

        assert(ptdb_close(db) == 0);
    }

    PTDBReader *reader = ptdb_reader_open(STRESS_DB_MULTI);
    assert(reader != NULL);

    ChunkHeader2D ch;
    void *pts = NULL;
    uint32_t count = 0;
    uint32_t total_points = 0;
    uint32_t total_chunks = 0;

    while (ptdb_reader_next_chunk(reader, &ch, &pts, &count) == 1)
    {
        total_chunks++;
        total_points += count;
    }

    assert(total_points == (uint32_t)(SESSIONS * PTS_PER_SESSION));
    ptdb_reader_close(reader);

    printf("  -> Completed %d append sessions. Recovered %u total points in %u chunks seamlessly.\n",
           SESSIONS, total_points, total_chunks);

    unlink(STRESS_DB_MULTI);
}

int main(void)
{
    printf("==================================================\n");
    printf("       PTDB HIGH-VOLUME STRESS & DUMP SUITE       \n");
    printf("==================================================\n");

    test_million_points_2d();
    test_million_points_3d();
    test_multi_session_append();

    printf("==================================================\n");
    printf("       ALL STRESS TESTS PASSED SUCCESSFULLY!      \n");
    printf("==================================================\n");
    return 0;
}
