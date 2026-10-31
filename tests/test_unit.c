#include "ptdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#define TEST_DB_2D "test_positions_2d.log"
#define TEST_DB_3D "test_positions_3d.log"

static void test_init_and_schema_lock(void)
{
    printf("[Test 1] Schema Lock & Initialization Validation...\n");
    unlink(TEST_DB_2D);

    assert(ptdb_init(TEST_DB_2D, 1, PTDB_TIME_UNIT_MS) == -1);
    assert(ptdb_init(TEST_DB_2D, 4, PTDB_TIME_UNIT_MS) == -1);

    assert(ptdb_init(TEST_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    assert(ptdb_init(TEST_DB_2D, 2, PTDB_TIME_UNIT_MS) == -1);

    int fd = open(TEST_DB_2D, O_RDONLY);
    assert(fd != -1);
    FileHeader fh;
    assert(read(fd, &fh, sizeof(FileHeader)) == sizeof(FileHeader));
    close(fd);

    assert(fh.magic == PTDB_FILE_MAGIC);
    assert(fh.version == PTDB_VERSION);
    assert(fh.dim == 2);
    assert(fh.time_unit == PTDB_TIME_UNIT_MS);
    assert(fh.reserved == 0);

    unlink(TEST_DB_2D);
    printf("  -> PASSED\n");
}

static void test_dimension_mismatch_rejection(void)
{
    printf("[Test 2] Dimension Mismatch Rejection...\n");
    unlink(TEST_DB_2D);
    assert(ptdb_init(TEST_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDB *db = ptdb_open(TEST_DB_2D, NULL);
    assert(db != NULL);
    assert(ptdb_get_dim(db) == 2);

    assert(ptdb_insert_3d(db, 1000, 1.0f, 2.0f, 3.0f) == -1);

    assert(ptdb_insert_2d(db, 1000, 1.0f, 2.0f) == 0);

    ptdb_close(db);
    unlink(TEST_DB_2D);
    printf("  -> PASSED\n");
}

static void test_capacity_flush_2d(void)
{
    printf("[Test 3] Capacity-Driven Flush (max_points = 500)...\n");
    unlink(TEST_DB_2D);
    assert(ptdb_init(TEST_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig config = { .max_points = 500, .max_time_window_ms = 50000 };
    PTDB *db = ptdb_open(TEST_DB_2D, &config);
    assert(db != NULL);

    for (int i = 0; i < 500; i++)
    {
        int64_t t = 1000 + i * 10;
        float x = (float)i * 0.5f;
        float y = (float)i * 1.5f;
        assert(ptdb_insert_2d(db, t, x, y) == 0);
    }

    for (int i = 500; i < 550; i++)
    {
        int64_t t = 1000 + i * 10;
        float x = (float)i * 0.5f;
        float y = (float)i * 1.5f;
        assert(ptdb_insert_2d(db, t, x, y) == 0);
    }

    ptdb_close(db);

    PTDBReader *reader = ptdb_reader_open(TEST_DB_2D);
    assert(reader != NULL);
    assert(ptdb_reader_get_dim(reader) == 2);

    ChunkHeader2D ch1, ch2;
    void *points1 = NULL;
    void *points2 = NULL;
    uint32_t count1 = 0, count2 = 0;

    assert(ptdb_reader_next_chunk(reader, &ch1, &points1, &count1) == 1);
    assert(count1 == 500);
    assert(ch1.count == 500);
    assert(ch1.start_time == 1000);
    assert(ch1.end_time == 1000 + 499 * 10);
    assert(ch1.min_x == 0.0f);
    assert(ch1.max_x == 499 * 0.5f);
    assert(ch1.payload_size == 500 * sizeof(Point2D));

    assert(ptdb_reader_next_chunk(reader, &ch2, &points2, &count2) == 1);
    assert(count2 == 50);
    assert(ch2.count == 50);
    assert(ch2.start_time == 1000 + 500 * 10);
    assert(ch2.end_time == 1000 + 549 * 10);

    assert(ptdb_reader_next_chunk(reader, NULL, NULL, NULL) == 0);

    ptdb_reader_close(reader);
    unlink(TEST_DB_2D);
    printf("  -> PASSED\n");
}

static void test_time_window_flush(void)
{
    printf("[Test 4] Time-Window Flush (5000ms threshold)...\n");
    unlink(TEST_DB_2D);
    assert(ptdb_init(TEST_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig config = { .max_points = 500, .max_time_window_ms = 5000 };
    PTDB *db = ptdb_open(TEST_DB_2D, &config);
    assert(db != NULL);

    assert(ptdb_insert_2d(db, 1000, 10.0f, 20.0f) == 0);
    assert(ptdb_insert_2d(db, 3000, 15.0f, 25.0f) == 0);
    assert(ptdb_insert_2d(db, 6000, 30.0f, 40.0f) == 0);

    ptdb_close(db);

    PTDBReader *reader = ptdb_reader_open(TEST_DB_2D);
    assert(reader != NULL);

    ChunkHeader2D ch1, ch2;
    uint32_t count1 = 0, count2 = 0;

    assert(ptdb_reader_next_chunk(reader, &ch1, NULL, &count1) == 1);
    assert(count1 == 2);
    assert(ch1.start_time == 1000);
    assert(ch1.end_time == 3000);

    assert(ptdb_reader_next_chunk(reader, &ch2, NULL, &count2) == 1);
    assert(count2 == 1);
    assert(ch2.start_time == 6000);
    assert(ch2.end_time == 6000);

    assert(ptdb_reader_next_chunk(reader, NULL, NULL, NULL) == 0);

    ptdb_reader_close(reader);
    unlink(TEST_DB_2D);
    printf("  -> PASSED\n");
}

static void test_3d_coordinates_and_mbr(void)
{
    printf("[Test 5] 3D Coordinates & MBR Spatial Bounds...\n");
    unlink(TEST_DB_3D);
    assert(ptdb_init(TEST_DB_3D, 3, PTDB_TIME_UNIT_MS) == 0);

    PTDB *db = ptdb_open(TEST_DB_3D, NULL);
    assert(db != NULL);
    assert(ptdb_get_dim(db) == 3);

    assert(ptdb_insert_3d(db, 100, 10.5f, -5.2f, 100.0f) == 0);
    assert(ptdb_insert_3d(db, 200, 20.1f, 15.8f, 50.0f) == 0);
    assert(ptdb_insert_3d(db, 300, -2.0f, 0.0f, 250.0f) == 0);

    ptdb_close(db);

    PTDBReader *reader = ptdb_reader_open(TEST_DB_3D);
    assert(reader != NULL);

    ChunkHeader3D ch;
    void *points = NULL;
    uint32_t count = 0;

    assert(ptdb_reader_next_chunk(reader, &ch, &points, &count) == 1);
    assert(count == 3);
    assert(ch.start_time == 100);
    assert(ch.end_time == 300);

    assert(fabsf(ch.min_x - (-2.0f)) < 1e-5);
    assert(fabsf(ch.max_x - 20.1f) < 1e-5);
    assert(fabsf(ch.min_y - (-5.2f)) < 1e-5);
    assert(fabsf(ch.max_y - 15.8f) < 1e-5);
    assert(fabsf(ch.min_z - 50.0f) < 1e-5);
    assert(fabsf(ch.max_z - 250.0f) < 1e-5);

    Point3D p0, p1, p2;
    ptdb_get_point_3d(points, 0, &p0);
    ptdb_get_point_3d(points, 1, &p1);
    ptdb_get_point_3d(points, 2, &p2);
    assert(p0.timestamp == 100 && fabsf(p0.x - 10.5f) < 1e-5 && fabsf(p0.z - 100.0f) < 1e-5);
    assert(p1.timestamp == 200 && fabsf(p1.x - 20.1f) < 1e-5 && fabsf(p1.z - 50.0f) < 1e-5);
    assert(p2.timestamp == 300 && fabsf(p2.x - (-2.0f)) < 1e-5 && fabsf(p2.z - 250.0f) < 1e-5);

    ptdb_reader_close(reader);
    unlink(TEST_DB_3D);
    printf("  -> PASSED\n");
}

static void test_crc32_and_corruption_detection(void)
{
    printf("[Test 6] CRC32 Integrity & Corruption Detection...\n");
    unlink(TEST_DB_2D);
    assert(ptdb_init(TEST_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDB *db = ptdb_open(TEST_DB_2D, NULL);
    assert(db != NULL);
    assert(ptdb_insert_2d(db, 1000, 1.23f, 4.56f) == 0);
    ptdb_close(db);

    PTDBReader *reader = ptdb_reader_open(TEST_DB_2D);
    assert(reader != NULL);
    uint32_t count = 0;
    assert(ptdb_reader_next_chunk(reader, NULL, NULL, &count) == 1);
    assert(count == 1);
    ptdb_reader_close(reader);

    int fd = open(TEST_DB_2D, O_RDWR);
    assert(fd != -1);
    off_t payload_offset = FILE_HEADER_SIZE + sizeof(ChunkHeader2D) + 4;
    uint8_t corrupt_byte = 0xFF;
    assert(pwrite(fd, &corrupt_byte, 1, payload_offset) == 1);
    close(fd);

    reader = ptdb_reader_open(TEST_DB_2D);
    assert(reader != NULL);
    int res = ptdb_reader_next_chunk(reader, NULL, NULL, &count);
    assert(res == -2);
    ptdb_reader_close(reader);

    unlink(TEST_DB_2D);
    printf("  -> PASSED\n");
}

static void count_cb_2d(const Point2D *pt, void *ud)
{
    (void)pt;
    uint32_t *counter = (uint32_t *)ud;
    (*counter)++;
}

static void test_range_query(void)
{
    printf("[Test 7] Spatiotemporal Range Query (Chunk-skipping)...\n");
    unlink(TEST_DB_2D);
    assert(ptdb_init(TEST_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig config = { .max_points = 50, .max_time_window_ms = 100000 };
    PTDB *db = ptdb_open(TEST_DB_2D, &config);
    assert(db != NULL);

    for (int i = 0; i < 50; i++)
    {
        assert(ptdb_insert_2d(db, 1000 + i * 10, (float)i, (float)i) == 0);
    }
    assert(ptdb_flush(db) == 0);

    for (int i = 50; i < 100; i++)
    {
        assert(ptdb_insert_2d(db, 2000 + i * 10, (float)i, (float)i) == 0);
    }
    assert(ptdb_flush(db) == 0);

    for (int i = 100; i < 150; i++)
    {
        assert(ptdb_insert_2d(db, 5000 + i * 10, (float)i + 1000.0f, (float)i + 1000.0f) == 0);
    }
    assert(ptdb_close(db) == 0);

    PTDBQueryRange2D q1 = {
        .min_time = 1000, .max_time = 1200,
        .min_x = 0.0f, .max_x = 20.0f,
        .min_y = 0.0f, .max_y = 20.0f
    };
    uint32_t count = 0;
    uint64_t matched = ptdb_query_range_2d(TEST_DB_2D, &q1, count_cb_2d, &count);
    assert(matched == 21);
    assert(count == 21);

    PTDBQueryRange2D q_spatial_miss = {
        .min_time = 5000, .max_time = 8000,
        .min_x = 0.0f, .max_x = 100.0f,
        .min_y = 0.0f, .max_y = 100.0f
    };
    count = 0;
    matched = ptdb_query_range_2d(TEST_DB_2D, &q_spatial_miss, count_cb_2d, &count);
    assert(matched == 0);

    unlink(TEST_DB_2D);
    printf("  -> PASSED\n");
}

int main(void)
{
    printf("==================================================\n");
    printf("       PTDB STORAGE ENGINE VERIFICATION SUITE       \n");
    printf("==================================================\n");

    test_init_and_schema_lock();
    test_dimension_mismatch_rejection();
    test_capacity_flush_2d();
    test_time_window_flush();
    test_3d_coordinates_and_mbr();
    test_crc32_and_corruption_detection();
    test_range_query();

    printf("==================================================\n");
    printf("       ALL 7 TESTS PASSED SUCCESSFULLY!          \n");
    printf("==================================================\n");
    return 0;
}
