#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include<wal.h>


Logger* log_open(char* filename){
    Logger *logger = malloc(sizeof(Logger));
    int fd = open(filename, 0, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if( fd == -1) {
        printf("Unable to open the file.\n");
        exit(EXIT_FAILURE);
    }
    uint32_t file_length = lseek(fd, 0, SEEK_END);
    logger->file_length=file_length;
    logger->file_descriptor = fd;
    if (file_length % WAL_FRAME_SIZE != 0)
    {
        printf("Corrupt file. \n");
        exit(EXIT_FAILURE);
    }
    logger->num_pages = file_length / WAL_FRAME_SIZE;
    for (uint32_t i=0;i<WAL_MAX_PAGES;i++){
        logger->log_pages[i] =NULL;
    }
    lseek(fd,0,SEEK_SET);
    for (uint32_t i=0;i<logger->num_pages;i++){
        void *framebuf =  malloc(WAL_FRAME_SIZE);
        ssize_t bytes_read = read(fd, framebuf, WAL_FRAME_SIZE);
        if (bytes_read != WAL_FRAME_SIZE) {
            printf("Failed to read WAL frame %d into memory.\n", i);
            exit(EXIT_FAILURE);
        }
        logger->log_pages[i]= framebuf;
    }
    return logger;
}