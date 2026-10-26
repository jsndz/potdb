#include "table.h"
#include "file.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int db_init(const char *filepath, Config *config)
{
    int fd = open(filepath, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        perror("open");
        return -1;
    }

    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1)
    {
        perror("lseek");
        close(fd);
        return -1;
    }

    if (file_length > 0)
    {
        fprintf(stderr, "Database is already initialized.\n");
        close(fd);
        return -1;
    }

    File_header header;
    memset(&header, 0, sizeof(File_header));
    memcpy(&header.magic, "POTD", 4);
    header.version = 1;
    header.dim = (config && config->dim > 0) ? config->dim : 2;
    header.time_unit = (config && config->time_unit > 0) ? config->time_unit : 1;
    header.reserved = 0;

    ssize_t bytes_written = write(fd, &header, FILE_HEADER_SIZE);
    if (bytes_written != (ssize_t)FILE_HEADER_SIZE)
    {
        perror("write header");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

Table *db_open(const char *filepath)
{
    int fd = open(filepath, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1)
    {
        perror("lseek");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (file_length == 0)
    {
        fprintf(stderr, "Database is uninitialised.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (file_length < (off_t)FILE_HEADER_SIZE)
    {
        fprintf(stderr, "Corrupted database file: size %ld is smaller than header size %zu.\n",
                (long)file_length, FILE_HEADER_SIZE);
        close(fd);
        exit(EXIT_FAILURE);
    }

    File_header header;
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        perror("lseek");
        close(fd);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read = read(fd, &header, FILE_HEADER_SIZE);
    if (bytes_read != (ssize_t)FILE_HEADER_SIZE)
    {
        perror("read header");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (memcmp(&header.magic, "POTD", 4) != 0)
    {
        fprintf(stderr, "Corrupted database file: invalid magic header.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    off_t data_length = file_length - FILE_HEADER_SIZE;
    if (data_length % ROW_SIZE != 0)
    {
        fprintf(stderr, "Corrupted database file: data length %ld is not a multiple of %zu.\n",
                (long)data_length, ROW_SIZE);
        close(fd);
        exit(EXIT_FAILURE);
    }

    Table *table = malloc(sizeof(Table));
    if (!table)
    {
        perror("malloc");
        close(fd);
        exit(EXIT_FAILURE);
    }

    table->file_descriptor = fd;
    table->file_length = file_length;
    table->num_rows = (uint32_t)((file_length - FILE_HEADER_SIZE) / ROW_SIZE);

    return table;
}

void db_close(Table *table)
{
    if (table == NULL)
    {
        return;
    }

    if (table->file_descriptor != -1)
    {
        if (close(table->file_descriptor) == -1)
        {
            perror("close");
            exit(EXIT_FAILURE);
        }
        table->file_descriptor = -1;
    }

    free(table);
}

void table_append_row(Table *table, Row *row)
{
    char serialized[ROW_SIZE];
    serialize_row(row, serialized);

    if (lseek(table->file_descriptor, 0, SEEK_END) == -1)
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(table->file_descriptor, serialized, ROW_SIZE);
    if (bytes_written != ROW_SIZE)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }

    table->file_length += ROW_SIZE;
    table->num_rows++;
    printf("Inserted row (%ld, %d, %d)\n", (long)row->timestamp, row->pos.x, row->pos.y);
}

void table_select_all(Table *table)
{
    if (lseek(table->file_descriptor, FILE_HEADER_SIZE, SEEK_SET) == -1)
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }

    char buffer[ROW_SIZE];
    ssize_t bytes_read;
    Row row;

    while ((bytes_read = read(table->file_descriptor, buffer, ROW_SIZE)) == ROW_SIZE)
    {
        deserialize_row(buffer, &row);
        print_row(&row);
    }

    if (bytes_read == -1)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
}
