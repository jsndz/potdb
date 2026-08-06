#ifndef ROW_H
#define ROW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define COL_USERNAME_SIZE 32
#define COL_EMAIL_SIZE 255

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

typedef struct
{
    uint32_t id;
    char username[COL_USERNAME_SIZE + 1];
    char email[COL_EMAIL_SIZE + 1];
} Row;

#define ID_SIZE ((uint32_t)size_of_attribute(Row, id))
#define USERNAME_SIZE ((uint32_t)size_of_attribute(Row, username))
#define EMAIL_SIZE ((uint32_t)size_of_attribute(Row, email))

#define ID_OFFSET ((uint32_t)0)
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

void serialize_row(Row *source, void *destination);
void deserialize_row(void *source, Row *destination);
void print_row(Row *row);

#endif // ROW_H
