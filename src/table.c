#include "table.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

Table *db_open(const char *filename)
{
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
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

    if (file_length % ROW_SIZE != 0)
    {
        fprintf(stderr, "Corrupted database file: length %ld is not a multiple of %zu.\n",
                (long)file_length, ROW_SIZE);
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
    table->num_rows = (uint32_t)(file_length / ROW_SIZE);

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
    if (lseek(table->file_descriptor, 0, SEEK_SET) == -1)
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
