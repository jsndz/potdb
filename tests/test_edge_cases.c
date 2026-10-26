#include "ptdb.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#define EDGE_DB_2D "edge_test_2d.db"
#define EDGE_DB_3D "edge_test_3d.db"
#define EDGE_DB_CORRUPT "edge_test_corrupt.db"

static void test_timestamp_edges(void)
{
    printf("[Edge Test 1] Timestamp Boundaries & Monotonicity...\n");
    unlink(EDGE_DB_2D);
    assert(ptdb_init(EDGE_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig config = { .max_points = 50, .max_time_window_ms = 1000 };
    PTDB *db = ptdb_open(EDGE_DB_2D, &config);
    assert(db != NULL);

    assert(ptdb_insert_2d(db, -5000, 1.0f, 2.0f) == 0);
    assert(ptdb_insert_2d(db, -5000, 1.5f, 2.5f) == 0);
    assert(ptdb_insert_2d(db, -3000, 2.0f, 3.0f) == 0);
    assert(ptdb_insert_2d(db, 0, 3.0f, 4.0f) == 0);

    assert(ptdb_insert_2d(db, -1, 0.0f, 0.0f) == -2);

    assert(ptdb_insert_2d(db, 100, 4.0f, 5.0f) == 0);
    assert(ptdb_insert_2d(db, 5000, 5.0f, 6.0f) == 0);

    assert(ptdb_insert_2d(db, 4999, 0.0f, 0.0f) == -2);

    assert(ptdb_close(db) == 0);

    PTDBReader *reader = ptdb_reader_open(EDGE_DB_2D);
    assert(reader != NULL);

    ChunkHeader2D ch;
    void *points = NULL;
    uint32_t count = 0;
    uint32_t total_points = 0;
    uint32_t total_chunks = 0;

    while (ptdb_reader_next_chunk(reader, &ch, &points, &count) == 1)
    {
        total_chunks++;
        total_points += count;
        assert(ch.start_time <= ch.end_time);
    }
    assert(total_points == 6);
    assert(total_chunks >= 2);

    ptdb_reader_close(reader);
    unlink(EDGE_DB_2D);
    printf("  -> PASSED\n");
}

static void test_coordinate_edges(void)
{
    printf("[Edge Test 2] Coordinate Boundaries & Zero-Span MBR...\n");
    unlink(EDGE_DB_2D);
    assert(ptdb_init(EDGE_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDB *db = ptdb_open(EDGE_DB_2D, NULL);
    assert(db != NULL);

    assert(ptdb_insert_2d(db, 100, 42.0f, -99.0f) == 0);
    assert(ptdb_insert_2d(db, 200, 42.0f, -99.0f) == 0);
    assert(ptdb_insert_2d(db, 300, 42.0f, -99.0f) == 0);
    assert(ptdb_flush(db) == 0);

    assert(ptdb_insert_2d(db, 1000, 0.0f, 0.0f) == 0);
    assert(ptdb_insert_2d(db, 1010, -1000.5f, 2000.5f) == 0);
    assert(ptdb_insert_2d(db, 1020, 1000.5f, -2000.5f) == 0);
    assert(ptdb_insert_2d(db, 1030, 1e20f, -1e20f) == 0);
    assert(ptdb_flush(db) == 0);

    assert(ptdb_insert_2d(db, 5000, 7.77f, 8.88f) == 0);
    assert(ptdb_close(db) == 0);

    PTDBReader *reader = ptdb_reader_open(EDGE_DB_2D);
    assert(reader != NULL);

    ChunkHeader2D ch1, ch2, ch3;
    void *pts = NULL;
    uint32_t cnt = 0;

    assert(ptdb_reader_next_chunk(reader, &ch1, &pts, &cnt) == 1);
    assert(cnt == 3);
    assert(ch1.min_x == 42.0f && ch1.max_x == 42.0f);
    assert(ch1.min_y == -99.0f && ch1.max_y == -99.0f);

    assert(ptdb_reader_next_chunk(reader, &ch2, &pts, &cnt) == 1);
    assert(cnt == 4);
    assert(ch2.min_x == -1000.5f);
    assert(ch2.max_x == 1e20f);
    assert(ch2.min_y == -1e20f);
    assert(ch2.max_y == 2000.5f);

    assert(ptdb_reader_next_chunk(reader, &ch3, &pts, &cnt) == 1);
    assert(cnt == 1);
    assert(ch3.start_time == 5000 && ch3.end_time == 5000);
    assert(ch3.min_x == 7.77f && ch3.max_x == 7.77f);
    assert(ch3.min_y == 8.88f && ch3.max_y == 8.88f);

    assert(ptdb_reader_next_chunk(reader, &ch3, &pts, &cnt) == 0);

    ptdb_reader_close(reader);
    unlink(EDGE_DB_2D);
    printf("  -> PASSED\n");
}

static void test_buffer_and_config_edges(void)
{
    printf("[Edge Test 3] Buffer Extremes (max_points=1, clamp >65535, empty flush)...\n");
    unlink(EDGE_DB_2D);
    assert(ptdb_init(EDGE_DB_2D, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBConfig cfg1 = { .max_points = 1, .max_time_window_ms = 100000 };
    PTDB *db = ptdb_open(EDGE_DB_2D, &cfg1);
    assert(db != NULL);

    for (int i = 0; i < 5; i++)
    {
        assert(ptdb_insert_2d(db, 1000 + i * 10, (float)i, (float)i * 2) == 0);
    }
    assert(ptdb_close(db) == 0);

    PTDBReader *reader = ptdb_reader_open(EDGE_DB_2D);
    assert(reader != NULL);

    ChunkHeader2D ch;
    void *pts = NULL;
    uint32_t cnt = 0;
    int chunk_count = 0;

    while (ptdb_reader_next_chunk(reader, &ch, &pts, &cnt) == 1)
    {
        assert(cnt == 1);
        chunk_count++;
    }
    assert(chunk_count == 5);
    ptdb_reader_close(reader);

    db = ptdb_open(EDGE_DB_2D, NULL);
    assert(db != NULL);
    assert(ptdb_flush(db) == 0);
    assert(ptdb_flush(db) == 0);
    assert(ptdb_close(db) == 0);

    reader = ptdb_reader_open(EDGE_DB_2D);
    assert(reader != NULL);
    chunk_count = 0;
    while (ptdb_reader_next_chunk(reader, &ch, &pts, &cnt) == 1)
    {
        chunk_count++;
    }
    assert(chunk_count == 5);
    ptdb_reader_close(reader);

    PTDBConfig cfg_large = { .max_points = 100000, .max_time_window_ms = 10000 };
    db = ptdb_open(EDGE_DB_2D, &cfg_large);
    assert(db != NULL);
    assert(ptdb_insert_2d(db, 2000, 1.0f, 2.0f) == 0);
    assert(ptdb_close(db) == 0);

    unlink(EDGE_DB_2D);
    printf("  -> PASSED\n");
}

static void test_corrupted_and_malformed_files(void)
{
    printf("[Edge Test 4] Corrupted & Malformed Files...\n");

    assert(ptdb_reader_open("non_existent_file.db") == NULL);
    assert(ptdb_open("non_existent_file.db", NULL) == NULL);

    int fd = open(EDGE_DB_CORRUPT, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd != -1);
    close(fd);

    assert(ptdb_open(EDGE_DB_CORRUPT, NULL) == NULL);
    assert(ptdb_reader_open(EDGE_DB_CORRUPT) == NULL);

    fd = open(EDGE_DB_CORRUPT, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd != -1);
    assert(write(fd, "PTDB_SHORT", 10) == 10);
    close(fd);

    assert(ptdb_open(EDGE_DB_CORRUPT, NULL) == NULL);
    assert(ptdb_reader_open(EDGE_DB_CORRUPT) == NULL);

    unlink(EDGE_DB_CORRUPT);
    assert(ptdb_init(EDGE_DB_CORRUPT, 2, PTDB_TIME_UNIT_MS) == 0);

    fd = open(EDGE_DB_CORRUPT, O_RDWR);
    assert(fd != -1);
    uint32_t bad_magic = 0xDEADBEEF;
    assert(lseek(fd, 0, SEEK_SET) == 0);
    assert(write(fd, &bad_magic, sizeof(uint32_t)) == sizeof(uint32_t));
    close(fd);

    assert(ptdb_open(EDGE_DB_CORRUPT, NULL) == NULL);
    assert(ptdb_reader_open(EDGE_DB_CORRUPT) == NULL);

    unlink(EDGE_DB_CORRUPT);
    assert(ptdb_init(EDGE_DB_CORRUPT, 2, PTDB_TIME_UNIT_MS) == 0);

    fd = open(EDGE_DB_CORRUPT, O_RDWR);
    assert(fd != -1);
    uint16_t bad_version = 99;
    assert(lseek(fd, 4, SEEK_SET) == 4);
    assert(write(fd, &bad_version, sizeof(uint16_t)) == sizeof(uint16_t));
    close(fd);

    assert(ptdb_open(EDGE_DB_CORRUPT, NULL) == NULL);

    unlink(EDGE_DB_CORRUPT);
    assert(ptdb_init(EDGE_DB_CORRUPT, 2, PTDB_TIME_UNIT_MS) == 0);

    PTDBReader *reader = ptdb_reader_open(EDGE_DB_CORRUPT);
    assert(reader != NULL);
    ChunkHeader2D ch;
    void *pts = NULL;
    uint32_t cnt = 0;
    assert(ptdb_reader_next_chunk(reader, &ch, &pts, &cnt) == 0);
    ptdb_reader_close(reader);

    PTDB *db = ptdb_open(EDGE_DB_CORRUPT, NULL);
    assert(db != NULL);
    for (int i = 0; i < 20; i++)
    {
        assert(ptdb_insert_2d(db, 1000 + i * 10, (float)i, (float)i) == 0);
    }
    assert(ptdb_close(db) == 0);

    off_t good_size = lseek(open(EDGE_DB_CORRUPT, O_RDONLY), 0, SEEK_END);
    assert(good_size > 0);

    assert(truncate(EDGE_DB_CORRUPT, FILE_HEADER_SIZE + 10) == 0);
    reader = ptdb_reader_open(EDGE_DB_CORRUPT);
    assert(reader != NULL);
    assert(ptdb_reader_next_chunk(reader, &ch, &pts, &cnt) == -1);
    ptdb_reader_close(reader);

    unlink(EDGE_DB_CORRUPT);
    assert(ptdb_init(EDGE_DB_CORRUPT, 2, PTDB_TIME_UNIT_MS) == 0);
    db = ptdb_open(EDGE_DB_CORRUPT, NULL);
    assert(db != NULL);
    for (int i = 0; i < 20; i++)
    {
        assert(ptdb_insert_2d(db, 1000 + i * 10, (float)i, (float)i) == 0);
    }
    assert(ptdb_close(db) == 0);

    assert(truncate(EDGE_DB_CORRUPT, FILE_HEADER_SIZE + CHUNK_HEADER_2D_SIZE + 20) == 0);
    reader = ptdb_reader_open(EDGE_DB_CORRUPT);
    assert(reader != NULL);
    assert(ptdb_reader_next_chunk(reader, &ch, &pts, &cnt) == -1);
    ptdb_reader_close(reader);

    unlink(EDGE_DB_CORRUPT);
    assert(ptdb_init(EDGE_DB_CORRUPT, 2, PTDB_TIME_UNIT_MS) == 0);
    db = ptdb_open(EDGE_DB_CORRUPT, NULL);
    assert(db != NULL);
    for (int i = 0; i < 20; i++)
    {
        assert(ptdb_insert_2d(db, 1000 + i * 10, (float)i, (float)i) == 0);
    }
    assert(ptdb_close(db) == 0);

    fd = open(EDGE_DB_CORRUPT, O_RDWR);
    assert(fd != -1);
    uint32_t bad_payload = 999999;
    assert(lseek(fd, FILE_HEADER_SIZE + 36, SEEK_SET) == FILE_HEADER_SIZE + 36);
    assert(write(fd, &bad_payload, 4) == 4);
    close(fd);

    reader = ptdb_reader_open(EDGE_DB_CORRUPT);
    assert(reader != NULL);
    assert(ptdb_reader_next_chunk(reader, &ch, &pts, &cnt) == -1);
    ptdb_reader_close(reader);

    unlink(EDGE_DB_CORRUPT);
    printf("  -> PASSED\n");
}

int main(void)
{
    printf("==================================================\n");
    printf("       PTDB EXTENSIVE EDGE-CASE TEST SUITE        \n");
    printf("==================================================\n");

    test_timestamp_edges();
    test_coordinate_edges();
    test_buffer_and_config_edges();
    test_corrupted_and_malformed_files();

    printf("==================================================\n");
    printf("       ALL EDGE-CASE TESTS PASSED SUCCESSFULLY!   \n");
    printf("==================================================\n");
    return 0;
}
