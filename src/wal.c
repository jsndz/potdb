#include "statement.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include "wal.h"
#include "transaction.h"
#include "row.h"

int get_lsn(){
    return 0;
}

Logger* log_open(char* filename){
    Logger *logger =(Logger*) malloc(sizeof(Logger));
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if( fd == -1) {
        perror("open");
        free(logger);
        exit(EXIT_FAILURE);
    }
    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1) {
        perror("lseek");
        close(fd);
        free(logger);
        exit(EXIT_FAILURE);
    }

    if (file_length % LOG_SIZE != 0) {
        fprintf(stderr, "Corrupted WAL file\n");
        close(fd);
        free(logger);
        exit(EXIT_FAILURE);
    }

    logger->buffer_offset = 0;
    memset(logger->buffer,0,WAL_BUFFER_SIZE);
    logger->file_descriptor = fd;
    logger->next_lsn = 0;
    if(file_length != 0){
        char buf[LOG_SIZE];
        off_t  byte_offset = file_length - LOG_SIZE;
        if (lseek(fd, byte_offset, SEEK_SET) == -1) {
            perror("lseek");
            close(fd);
            free(logger);
            exit(EXIT_FAILURE);
        }
        ssize_t bytes_read= read(fd,buf,LOG_SIZE);
        if (bytes_read != LOG_SIZE) {
            perror("read");
            close(fd);
            free(logger);
            exit(EXIT_FAILURE);
        } 
        Log *lastLog =(Log*) malloc(sizeof(Log));
        deserialize_log(buf,lastLog);
        logger->next_lsn = lastLog->LSN;
    }

    return logger;
}

void log_append(Logger *logger,uint32_t page_num ,void *payload){
    (void)logger;
    (void)page_num;
    (void)payload;
}


Log* StatementToLog(Statement *s, uint32_t page_num) {
    Log* log = malloc(sizeof(Log));
    log->LSN = get_lsn();
    log->PageId = page_num;
    switch (s->type) {
        case TRANSACTION_BEGIN:
            log->type = LOG_RECORD_BEGIN;
            break;
        case TRANSACTION_COMMIT:
            log->type = LOG_RECORD_COMMIT;
            break;
        case TRANSACTION_ROLLBACK:
            log->type = LOG_RECORD_ROLLBACK;
            break;
        default:
            log->type = LOG_RECORD_INSERT;
            break;
    }
    log->row = &(s->row_to_insert);
    if (IN_TRANSACTION) {
        log->txn = transaction_id;
    } else {
        log->txn = 0;
    }
    return log;
}


void serialize_log(Log *source, void *destination)
{
    memcpy((char *)destination + LSN_OFFSET, &(source->LSN), LSN_SIZE);
    memcpy((char *)destination + TXN_OFFSET, &(source->txn), TXN_SIZE);
    memcpy((char *)destination + PAGE_ID_OFFSET, &(source->PageId), PAGE_ID_SIZE);
    uint32_t type_val = (uint32_t)source->type;
    memcpy((char *)destination + TYPE_OFFSET, &type_val, TYPE_SIZE);
    if (source->row != NULL) {
        serialize_row(source->row, (char *)destination + LOG_ROW_OFFSET);
    }
}

void deserialize_log(void *source, Log *destination)
{
    memcpy(&(destination->LSN), (char *)source + LSN_OFFSET, LSN_SIZE);
    memcpy(&(destination->txn), (char *)source + TXN_OFFSET, TXN_SIZE);
    memcpy(&(destination->PageId), (char *)source + PAGE_ID_OFFSET, PAGE_ID_SIZE);
    
    uint32_t type_val;
    memcpy(&type_val, (char *)source + TYPE_OFFSET, TYPE_SIZE);
    destination->type = (LogRecordType)type_val;

    if (destination->row == NULL) {
        destination->row = malloc(sizeof(Row));
    }
    deserialize_row((char *)source + LOG_ROW_OFFSET, destination->row);
}

