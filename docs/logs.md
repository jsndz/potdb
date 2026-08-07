## 25/08/2024

The input to the front-end is a SQL query. The output is SQLite virtual machine bytecode (essentially a compiled program that can operate on the database).

### Front-End Consists of:

- Tokenizer
- Parser
- Code Generator

### Back-End Consists of:

- Virtual Machine (takes the bytecode to get instruction like a switch statement)
- B-tree (stores the table)
- Pager (reading and writing the correct page)
- OS Interface

### Phase 1: REPL Interface

```c
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>

typedef struct {
    char *buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

InputBuffer *new_input_buffer() {
    InputBuffer *input_buffer = (InputBuffer *)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;
    return input_buffer;
}
```

### Notes:

- `size_t` is an unsigned integer data type used to represent the size of an object in memory or the result of the `sizeof` operator.
- `ssize_t` is a signed integer version of `size_t`.
- `*` symbol is used to indicate a pointer. A pointer is a variable that stores the memory address of another variable.

---

## 27/08/2024

```c
void print_prompt() {
    printf("db>");
}

void read_input(InputBuffer *inputBuffer) {
    ssize_t bytes_read = getline(&(inputBuffer->buffer), &(inputBuffer->buffer_length), stdin);
    if (bytes_read <= 0) {
        printf("error in reading\n");
        exit(EXIT_FAILURE);
    }
    inputBuffer->buffer[bytes_read - 1] = 0;
    inputBuffer->input_length = bytes_read - 1;
}

void close_input_buffer(InputBuffer *inputBuffer) {
    free(inputBuffer->buffer);
    free(inputBuffer);
}

int main(int argc, int *argv) {
    InputBuffer *inputBuffer = new_input_buffer();
    while (1) {
        print_prompt();
        read_input(inputBuffer);
        if (strcmp(inputBuffer->buffer, ".exit") == 0) {
            close_input_buffer(inputBuffer);
            exit(EXIT_SUCCESS);
        } else {
            printf("Unrecognizable command '%s'", inputBuffer->buffer);
        }
    }
}
```

### Explanation:

To imitate the database interface, the `print_prompt()` function is made. Inside it, you'll find the first lines of what you want as the prompt. There are two functions here: `read_input` and `close_input_buffer`.

#### `read_input()`:

This reads the input buffer, like the commands you enter. The `getline()` function is used to get the size of the buffer to `bytes_read` and puts the command in `input_buffer`.

##### `getline()` Function:

`getline` reads a line of input and stores it in a buffer. If `*lineptr` is `NULL`, it allocates a buffer and updates `*n` with its size. If `*lineptr` is not `NULL`, it uses the provided buffer, resizing if needed. It returns the number of characters read, including the newline but not the null terminator. If an error occurs or end-of-file is reached, it returns `-1`.

The buffer, which stores the command's last character, is changed to `0` from "null character," and the `input_length` is decreased by 1.

#### `close_input_buffer()`:

Deallocates memory from the input buffer.

### Notes:

- `buffer`: A place (a memory buffer) where the input line is stored.
- `buffer_length`: The size of the buffer.
- `input_length`: The number of characters actually read from the input.
- The `getline` function in C is used to read an entire line of text from a stream, typically from standard input.
- `bytes_read` holds the number of characters that `getline` has read from the input, including the newline character.

---

## 31/08/2024

### Phase 2: SQL Compiler and Virtual Machine

We need to convert whatever SQL code we get to bytecode. The process is as follows:

- SQL Command --> Tokenizer --> Parser --> Bytecode --> Virtual Machine

---

## 03/09/2024

When getting an SQL command, you need to check if it is a meta command like `.exit`. So, I used `enum` to represent the meta command and a `do_meta_command` function to execute them.

```c
typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

MetaCommandResult do_meta_command(InputBuffer *input_buffer) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        exit(EXIT_SUCCESS);
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}
```

The other output we get is the actual SQL statement. We need to divide the SQL statement into what category it belongs to. The categories include DDL, DML, and so on.

We take the statement and give a type to it.

```c
typedef enum {
    SELECT_STATEMENT,
    INSERT_STATEMENT
} StatementType;

typedef struct {
    StatementType type;
} Statement;

PrepareResult prepare_statement(InputBuffer *inputBuffer, Statement *statement) {
    if (strncmp(inputBuffer->buffer, "insert", 6) == 0) {
        statement->type = INSERT_STATEMENT;
        return PREPARE_SUCCESS;
    } else if (strncmp(inputBuffer->buffer, "select", 6) == 0) {
        statement->type = SELECT_STATEMENT;
        return PREPARE_SUCCESS;
    } else {
        return PREPARE_UNRECOGNIZED_COMMAND;
    }
}
```

Then we execute the statement.

---

## 09/09/2024

### Phase 3: A Single Table Database

To be precise, an in-memory, append-only, single-table database.

### Updated `Statement` Struct:

We add a row to insert.

```c
typedef struct {
    StatementType type;
    Row row_to_insert;
} Statement;
```

### Row Structure:

```c
#define COL_USERNAME_SIZE 32
#define COL_EMAIL_SIZE 255

// Struct that represents the syntax of a row
typedef struct {
    uint32_t id;
    char userName[COL_USERNAME_SIZE];
    char email[COL_EMAIL_SIZE];
} Row;
```

With a new `Statement` and `Row` struct, we update `prepare_statement()`:

```c
PrepareResult prepare_statement(InputBuffer *inputBuffer, Statement *statement) {
    if (strncmp(inputBuffer->buffer, "insert", 6) == 0) {
        statement->type = INSERT_STATEMENT;
        int args_assigned = sscanf(inputBuffer->buffer, "insert %d %s %s", &(statement->row_to_insert.id), statement->row_to_insert.userName, statement->row_to_insert.email);
        if (args_assigned < 3) {
            return PREPARE_SYNTAX_ERROR;
        }
        return PREPARE_SUCCESS;
    } else if (strncmp(inputBuffer->buffer, "select", 6) == 0) {
        statement->type = SELECT_STATEMENT;
        return PREPARE_SUCCESS;
    } else {
        return PREPARE_UNRECOGNIZED_COMMAND;
    }
}
```

### Assigning Data to `row_to_insert` in `Statement`

After the statement type is assigned, the data in the row is assigned to `row_to_insert` in the `Statement`. We will convert the given data into a data structure to represent the table.

### Data Structure Representation

Typically, a B-tree is used for representing the table. However, initially, we'll use an array. The structure will be as follows:

```
Table -----> Page -----> Rows -----> Data
```

Each row is an array. To retrieve data like the username, you just need the offset that represents the position of the username in the row.

```c
// gives the size of attribute in the structure
#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

// size of inputs
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, userName);

// Offset size
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;
```

### Column Size and Offset

| Column    | Size (bytes) | Offset |
| --------- | ------------ | ------ |
| ID        | 4            | 0      |
| Username  | 32           | 4      |
| Email     | 255          | 36     |
| **Total** | **291**      |        |

### Data Serialization and Deserialization

We need to convert data structures or objects into a format that can be easily stored or transmitted and then reconstructed later.

**Serialization:** Store the data.

**Deserialization:** Retrieve the data.

```c
void serialize_row(Row *source, void *destination)
{
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->userName), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void *source, Row *destination)
{
    memcpy(&(destination->id), (source + ID_OFFSET), ID_SIZE);
    memcpy(&(destination->userName), (source + USERNAME_OFFSET), USERNAME_SIZE);
    memcpy(&(destination->email), (source + EMAIL_OFFSET), EMAIL_SIZE);
}
```

**Note:**

- `memcpy` is a standard library function in C used to copy a block of memory from one location to another.
- `memcpy` returns a pointer to the destination memory area.
- It is used to serialize complex data structures into a byte array for storage, converting the data structure into a linear sequence of bytes.

### Finding the Row Slot in Memory

To perform operations, we need to determine the exact memory location to perform the operations.

```c
void *row_slot(Table *table, uint32_t row_num)
{
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    // There is a fixed number of rows in a page. Dividing row_num by ROWS_PER_PAGE gives the page number.
    // Example: 121st row will be on the 2nd page (121/100 = 1.21, index will be 2nd page).

    void *page = table->pages[page_num];
    if (page == NULL)
    {
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    // There is a fixed number of rows in a page. Taking the remainder of row_num divided by ROWS_PER_PAGE gives the row's index within the page.
    // Example: If ROWS_PER_PAGE is 100, the 121st row will be the 21st row within its page (121 % 100 = 21).

    uint32_t byte_offset = ROW_SIZE * row_offset;
    // Each row occupies a fixed number of bytes (ROW_SIZE). Multiplying ROW_SIZE by row_offset gives the byte offset within the page where the row's data starts.
    // Example: If ROW_SIZE is 291 bytes and row_offset is 21, the data for the 121st row starts at the 6111th byte within the page (291 * 21 = 6111).

    return page + byte_offset;
    // This will give you the position of data (row) in a page.
}
```

---

## 23/09/2024

### functions to initialize and free the table

For starters lets add functions to initialize and free the table.

```c
Table *new_table()
{
    Table *table = (Table *)malloc(sizeof(Table));
    table->num_rows = 0;
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        table->pages[i] = NULL;
    }
    return table;
}
void free_table(Table *table)
{
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        free(table->pages[i]);
    }
    free(table);
}
```

### Print row

The following function will be used to print the row:

```c
void print_row(Row *row)
{
    printf("(%d,%s,%s)\n", row->id, row->userName, row->email);
}
```

Make a enum to define execution result

```c
typedef enum
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL

} ExecuteResult;
```

Now we need functions to execute the insert and select command.

### Insertion

For the insertion command

```c
ExecuteResult execute_insert(Statement *statement, Table *table)
{
    if (table->num_rows == TABLE_MAX_PAGES)
    {
        return EXECUTE_TABLE_FULL;
    }
    Row *row_to_insert = &(statement->row_to_insert);
    serialize_row(row_to_insert, row_slot(table, table->num_rows));
    table->num_rows += 1;
    return EXECUTE_SUCCESS;
}
```

We check if the number of rows ina table has been been maxed out.
Then we insert the data to the table through `serialize_row` function.

### Selection

```c
ExecuteResult execute_select(Statement *statement, Table *table)
{
    Row row;
    for (uint32_t i = 0; i < table->num_rows; i++)
    {
        deserialize_row(row_slot(table, i), &row);
    }
    print_row(&(row));
    return EXECUTE_SUCCESS;
}
```

We create a temporary Row (memory for row info) the add all the data in the table to the row.
We make changes in execute_statement and main funtions.

**Notes**:

- Having \* in front of a function return type indicates that the function returns a pointer to a particular data type.
- If a \* is there in the functions parameters it indicates that you need to pass address of the variable not the value of the variable.
- If you want to pass the address of the variable you can use &. Ex: &(your_variable);

## 29/09/2024

### add additional 1 byte for "\0" in the Row email and username

```c
typedef struct
{
    uint32_t id;
    char userName[COL_USERNAME_SIZE + 1];
    char email[COL_EMAIL_SIZE + 1];
    // additional 1 byte for "\0" in the string

} Row;
```

### Better Parsing for insert

In scanf if the string reading is larger than the buffer then it allocates the data in random places.
So We want to check the length of each string before we copy it into a Row structure.
So we use strtok() and strlen()
First we will tokenize the statement and store it in variables.
These variable include userName, id, email.
The variables will be checked for NEGATIVE values and their length.

```c
typedef enum
{
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_COMMAND,
    PREPARE_SYNTAX_ERROR,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_IS_TOO_LONG
} PrepareResult;
PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement)
{
    statement->type = INSERT_STATEMENT;
    const *keyword = strtok(input_buffer->buffer, " ");
    const *id_STRING = strtok(NULL, " ");
    const *userName = strtok(NULL, " ");
    const *email = strtok(NULL, " ");
    if (id_STRING == NULL || userName == NULL || email == NULL)
    {
        return PREPARE_SYNTAX_ERROR;
    }
    int id = atoi(id_STRING);
    if (id < 0)
    {
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(userName) < COL_USERNAME_SIZE)
    {
        return PREPARE_STRING_IS_TOO_LONG;
    }
    if (strlen(email) < COL_EMAIL_SIZE)
    {
        return PREPARE_STRING_IS_TOO_LONG;
    }
    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.userName, userName);
    strcpy(statement->row_to_insert.email, email);
    return PREPARE_SUCCESS;
}
```

**Notes**:

- `strtok()` is used in this case to split a string into smaller parts (tokens) based on a specified delimiter, which is a space (" ") in this project.
- It replaces the space character with a null terminator ('\0').
- Subsequent calls to `strtok()` with NULL as the first argument continue tokenizing the original string by finding the next delimiter and replacing it with a null terminator.
- The way `strtok()` knows where it left off is by using an internal static pointer to track its position in the original string.
- `strcpy()` does not copy the address of a string; it copies the contents

## 11/10/2024

### Phase 4: Persistent Database

### We need to make the data persist. For now we are using a file to store the data.

We will use pager abstraction to make it easier.
Pager will store data if there is a cache miss the file in the disk will be searched.
Pager acts as a page cache.

```c
typedef struct
{
    int file_descriptor;
    uint32_t file_length;
    void *pages[TABLE_MAX_PAGES];
} Pager;
```

Add pager to the Table struct.

### Function `pager_open()` will open the file with necessary permissions.

```c
Pager *pager_open(const char *filename)
{

    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        printf("Unable to open the file.\n");
        exit(EXIT_FAILURE);
    }
    off_t file_length = lseek(fd, 0, SEEK_END);
    Pager *pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_length = file_length;
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        pager->pages[i] = NULL;
    }
    return pager;
}
```

It opens the file and stores the size of the file.

**Notes:**

- `open()` is a system call returns a non-negative integer if opened, which is the file descriptor (fd). If it fails, it returns -1.
- A file descriptor is an integer that uniquely identifies an open file within a process
- The `lseek()` function in C is used to move the file offset (the current position in the file) to a different location.
- `lseek()` allows random access in files, meaning you can jump to any point in the file without reading through it sequentially.
- `off_t lseek(int fd, off_t offset, int whence)`.
- whence determines how the offset is applied.

### Change `new_table()` to `db_open()` with initializing table and page structures and opening database.

```c
Table *db_open(const char *fileName)
{

    Pager *pager = pager_open(fileName);
    uint32_t num_rows = pager->file_length / ROW_SIZE;
    Table *table = malloc(sizeof(table));
    table->pager = pager;
    table->num_rows = num_rows;
    return table;
}
```

### We have a function `get_page()` that is logic for handling a cache miss.

```c
void *get_page(Pager *pager, uint32_t page_num)
{
    if (page_num > TABLE_MAX_PAGES)
    {
        printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
               TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL)
    {
        // cache miss
        void *page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;
        // page only has partial data
        if (pager->file_length % PAGE_SIZE)
        {
            num_pages += 1;
        }
        if (page_num <= num_pages)

        {
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1)
            {
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }
        pager->pages[page_num] = page;
    }
    return pager->pages[page_num];
}
```

checks if the page number that is greate than max, then checks if the page is NULL if it is null then allocates the memory and stores the data from the memory.
Also checks for partial page save.

### When the user exits, we’ll call a new method called db_close()

- flushes the page cache to disk
- closes the database file
- frees the memory for the Pager and Table data structures

```c
void *db_close(Table *table)
{
    Pager *pager = table->pager;
    uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;
    for (uint32_t i = 0; i < num_full_pages; i++)
    {
        if (pager->pages[i] == NULL)
        {
            continue;
        }
        pager_flush(pager, i, PAGE_SIZE);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    // if there is additional data
    uint32_t num_additional_page = table->num_rows % ROWS_PER_PAGE;
    if (num_additional_page >= 1)
    {
        uint32_t page_num = num_full_pages;
        if (pager->pages[page_num] != NULL)
        {
            pager_flush(pager, page_num, num_additional_page * ROW_SIZE);
            free(pager->pages[page_num]);
            pager->pages[page_num] = NULL;
        }
    }
    int result = close(pager->file_descriptor);
    if (result == -1)
    {
        printf("error in clsoing.\n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        void *page = pager->pages[i];
        if (page)
        {
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);
    free(table);
}
```

### `pager_flush()`, is responsible for writing a page from memory (the cache) back to the file on disk.

The design currently keeps track of how many rows are in the database by the length of the file. Because of
that, when writing the final page (which may be only partially filled), we have to write a "partial page"
(i.e., not the full PAGE_SIZE) to the end of the file. This is why pager_flush() takes both a page_num and a
size—the size specifies how many bytes of the page need to be written.

```c
void *pager_flush(Pager *pager, uint32_t page_num, uint32_t size)
{
    if (pager->pages[page_num] == NULL)
    {
        printf("tried to flush empty page");
        exit(EXIT_FAILURE);
    }
    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
    if (offset == -1)
    {
        printf("Error seeking.\n");
        exit(EXIT_FAILURE);
    }
    ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], size);
    if (bytes_written == -1)
    {
        printf("error in writing.\n");
        exit(EXIT_FAILURE);
    }
}
```

The data from the pager is flushed to the memory by `write()` function.

Also change the main function:

```c
int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("Need to enter the File name.\n");
        exit(EXIT_FAILURE);
    }
    char *filename = argv[1];
    Table *table = db_open(filename);
    InputBuffer *inputBuffer = new_input_buffer();
    while (true)
    {
        print_prompt();
        read_input(inputBuffer);

        if ((inputBuffer->buffer[0] == '.'))
        {
            switch (do_meta_command(inputBuffer, table))
            {
            case (META_COMMAND_SUCCESS):
                continue;
            case (META_COMMAND_UNRECOGNIZED_COMMAND):
                printf("Unrecognized command '%s'\n", inputBuffer->buffer);
                continue;
            }
        }
        Statement statement;
        switch (prepare_statement(inputBuffer, &statement))
        {
        case PREPARE_SUCCESS:

            break;

        case PREPARE_NEGATIVE_ID:
            printf("ID can't be negative.\n");
            continue;
        case PREPARE_UNRECOGNIZED_COMMAND:
            printf("Unrecognized command at the start %s \n", inputBuffer->buffer);
            continue;
        case PREPARE_STRING_IS_TOO_LONG:
            printf("string is too long.\n");
            continue;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            continue;
        }
        switch (execute_statement(&statement, table))
        {
        case EXECUTE_SUCCESS:
            printf("Executed.\n");
            break;
        case EXECUTE_TABLE_FULL:
            printf("Table Full\n");
        }
    }
}
```

When running the file with the ./a.out command add the file name.
For example : `./a.out db.db`

## 18/10/2024

### Phase 5: Cursor Abstraction

### We add cursor for:

- Create a cursor at the beginning of the table
- Create a cursor at the end of the table
- Access the row the cursor is pointing to
- Advance the cursor to the next row

### Cursor Structure:

```c
typedef struct
{
    Table *table;
    uint32_t row_num;
    bool end_of_table;// represents if the the cursor is at the end
} Cursor;
```

### To move cursor to the starting of the table:

```c
Cursor *table_start(Table *table)
{
    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = 0;
    cursor->end_of_table = (table->num_rows == 0);

    return cursor;
}
```

### To move cursor to the end of the table:

```c
Cursor *table_end(Table *table)
{
    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = table->num_rows;
    cursor->end_of_table = true;
    return cursor;
}

```

### change the `row_slot()` function so that it can return the row which the cursor is pointing to:

```c
void *cursor_value(Cursor *cursor)
{
    uint32_t row_num = cursor->row_num;
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void *page = get_page(cursor->table->pager, page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = ROW_SIZE * row_offset;
    return page + byte_offset;
    // will give you the position of data(row) in a page
}
```

### To go to the next row with the cursor

```c
void cursor_advance(Cursor *cursor)
{
    cursor->row_num += 1;

    if (cursor->row_num >= cursor->table->num_rows)
    {
        cursor->end_of_table = true;
    }
}
```

### Change the `execut_select()` and `execute_insert()` functions to manage the cursor

```c
ExecuteResult execute_insert(Statement *statement, Table *table)
{

    if (table->num_rows >= TABLE_MAX_ROWS)
    {
        return EXECUTE_TABLE_FULL;
    }

    Row *row_to_insert = &(statement->row_to_insert);
    print_row(row_to_insert);
    Cursor *cursor = table_end(table);
    serialize_row(row_to_insert, cursor_value(cursor));
    table->num_rows += 1;
    free(cursor);
    return EXECUTE_SUCCESS;
}
ExecuteResult execute_select(Statement *statement, Table *table)
{
    Cursor *cursor = table_start(table);
    Row row;

    while (!(cursor->end_of_table))
    {
        deserialize_row(cursor_value(cursor), &(row));
        print_row(&(row));
        cursor_advance(cursor);
    }

    return EXECUTE_SUCCESS;
}
```

Internal nodes will point to their children by storing the page number that stores the child.
The btree asks the pager for a particular page number and gets back a pointer into the page cache

## 27/10/2024

### Phase 6: B-tree Implementation

### Why B-tree?

A B-tree is a self-balancing search tree used for storing sorted data.
Btree provides better insertion, deletion and retrieval.

|                   | Unsorted Array of Rows | Sorted Array of Rows | Tree of Nodes                    |
| ----------------- | ---------------------- | -------------------- | -------------------------------- |
| **Pages contain** | Only data              | Only data            | Metadata, primary keys, and data |
| **Rows per page** | More                   | More                 | Fewer                            |
| **Insertion**     | O(1)                   | O(n)                 | O(log(n))                        |
| **Deletion**      | O(n)                   | O(n)                 | O(log(n))                        |
| **Lookup by ID**  | O(n)                   | O(log(n))            | O(log(n))                        |

There are 2 types of node in a btree.

```c
typedef enum { NODE_INTERNAL, NODE_LEAF } NodeType;
```

Each Node will be a page.
Pages are stored in the database file one after the other in order of page number.
Every node has metadata.
The metadata in a B-tree node is stored at the beginning of the page as part of the node header.
Internal Node holds the pointer for children node.

### Components of an Internal Node:

1. **Node Type**:

   - Similar to leaf nodes, internal nodes also store a **node type** to indicate whether the node is internal or a leaf. This allows the system to distinguish between internal and leaf nodes during operations.

2. **Is Root**:

   - A flag that indicates whether this internal node is the root of the tree.

3. **Parent Pointer**:

   - A pointer to the parent node of the current internal node, useful for navigating up the tree.

4. **Number of Keys**:

   - A count of how many keys are stored in the internal node. These keys are used to direct searches and separate ranges of child nodes.

5. **Keys**:

   - An array of keys that help guide the search. Each key acts as a boundary to decide which child node to follow during a search.

6. **Child Pointers**:
   - An array of pointers (or references) to child nodes. The number of child pointers is usually one more than the number of keys, as each key separates a range of children.

### Node Layout

```c

// common NODE header Constants
const uint32_t NODE_TYPE_SIZE = sizeof(uint32_t);
const uint32_t NODE_TYPE_OFFSSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint32_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_SIZE + IS_ROOT_OFFSET;
const uint8_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

```

Here header of the Node consist of node_type, is_root and parent_pointer.
Offset indicated their position. For example offset of is_root is give as size of node_type.
Meaning that after the node_type size you will get the is_root.

```c

// Leaf Node header constants
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;

// Leaf node body constants
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_VALUE_OFFSSET = 0;
const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

```

1. **NODE Constants**: These constants define the structure of the common part of a node.

   - `NODE_TYPE_SIZE`: This is the size (in bytes) of a field that stores the node's type, which is 4 bytes (since it's a `uint32_t`).
   - `NODE_TYPE_OFFSET`: This is the starting position of the `NODE_TYPE` field in the node, which is at the beginning (offset 0).
   - `IS_ROOT_SIZE`: This defines the size of a field that stores whether the node is the root of the tree. Again, it's 4 bytes.
   - `IS_ROOT_OFFSET`: This is where the `IS_ROOT` field starts, right after the `NODE_TYPE` field.
   - `PARENT_POINTER_SIZE`: This is the size of a field (also 4 bytes) that stores a pointer to the parent node.
   - `PARENT_POINTER_OFFSET`: The offset of the `PARENT_POINTER` field, which is after the `IS_ROOT` field.
   - `COMMON_NODE_HEADER_SIZE`: The total size of the common part of the node, which includes the `NODE_TYPE`, `IS_ROOT`, and `PARENT_POINTER` fields.

2. **Leaf Node Constants**: These constants define the structure for a leaf node, which stores actual data.

   - `LEAF_NODE_NUM_CELLS_SIZE`: The size (4 bytes) of a field that stores how many cells (data entries) are in the leaf node.
   - `LEAF_NODE_NUM_CELLS_OFFSET`: The position of the `LEAF_NODE_NUM_CELLS` field, right after the common node header.
   - `LEAF_NODE_HEADER_SIZE`: The total size of the leaf node header, which is the size of the common node header plus the size of the `LEAF_NODE_NUM_CELLS` field.

3. **Leaf Node Body Constants**:
   - `LEAF_NODE_KEY_SIZE`: This is the size (4 bytes, since it's a `uint32_t`) for storing the key in a leaf node.
   - `LEAF_NODE_KEY_OFFSET`: The starting position of the key field in each cell, which is set to 0 (i.e., the key comes first).
   - `LEAF_NODE_VALUE_SIZE`: This is the size (4 bytes) for storing the value associated with the key.
   - `LEAF_NODE_VALUE_OFFSET`: The starting position of the value field in each cell, which is right after the key. It’s calculated as `LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE`.
   - `LEAF_NODE_CELL_SIZE`: This is the total size of one cell (or entry) in the leaf node, which includes both the key and the value (`LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE`).
   - `LEAF_NODE_SPACE_FOR_CELLS`: This is the amount of space available for storing cells (key-value pairs) in the leaf node. It’s calculated as the total page size (`PAGE_SIZE`) minus the space taken by the leaf node header (`LEAF_NODE_HEADER_SIZE`).
   - `LEAF_NODE_MAX_CELLS`: This is the maximum number of cells (key-value pairs) that can fit in a leaf node. It’s calculated by dividing the available space for cells by the size of a single cell (`LEAF_NODE_CELL_SIZE`).

### To access the leaf node we use these pointer functions:

```c

uint32_t *leaf_node_num_cells(void *node)
{
    // returns a pointer to the number of cells(key - value pairs) stored in a leaf node of a B - tree.
    return node + LEAF_NODE_NUM_CELLS_OFFSET;
}
void *leaf_node_cell(void *node, uint32_t cell_num)
{
    // location of the leaf node
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}
uint32_t *leaf_node_key(void *node, uint32_t cell_num)
{
    // location of the leaf node key
    return leaf_node_cell(node, cell_num);
}
void *leaf_node_value(void *node, uint32_t cell_num)
{
    // location of the leaf node value
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}
void *initialize_leaf_node(void *node)
{
    // create a  leaf node
    *leaf_node_num_cells(node) = 0;
}
```

### In summary:

- **`leaf_node_num_cells`** accesses the number of cells in the node.
- **`leaf_node_cell`** finds the memory location of a specific cell (key-value pair).
- **`leaf_node_key`** retrieves the key from a cell.
- **`leaf_node_value`** retrieves the value from a cell.
- **`initialize_leaf_node`** initializes the leaf node by setting the number of cells to zero.

### change pager and table

Every node is going to take up exactly one page, even if it’s not full.
By having each node in the B-tree correspond to exactly one page, the database system can achieve a simplified, more efficient architecture that enhances performance, reduces complexity, and maintains consistency. This design choice streamlines various operations, making the overall system easier to implement and maintain.
So change `pager_flush()` and `db_close()` functions.

```c
void *pager_flush(Pager *pager, uint32_t page_num)
{
    if (pager->pages[page_num] == NULL)
    {
        printf("tried to flush empty page");
        exit(EXIT_FAILURE);
    }
    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
    // uses lseek to move the file pointer to the right spot in the database file.
    //The position is calculated as page_num * PAGE_SIZE, which finds where this page starts in the file.
    //lseek is not necessary
    if (offset == -1)
    {
        printf("Error seeking.\n");
        exit(EXIT_FAILURE);
    }
    ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
    if (bytes_written == -1)
    {
        printf("error in writing.\n");
        exit(EXIT_FAILURE);
    }
}
void *db_close(Table *table)
{
    Pager *pager = table->pager;
    for (uint32_t i = 0; i < pager->num_pages; i++)
    {
        if (pager->pages[i] == NULL)
        {
            continue;
        }
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    int result = close(pager->file_descriptor);
    if (result == -1)
    {
        printf("error in clsoing.\n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        void *page = pager->pages[i];
        if (page)
        {
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);
    free(table);
}
```

### Number of pages instead of rows

Since each page has fixed number of rows it is easier to search for row in a page rather than searching for row in specific.
And b-tree is self balancing tree so it will be in a sorted order.
Storing the number of pages aligns with how data is organized, accessed, and managed in a B-tree. It simplifies physical storage management, enables efficient paging, and aligns with the B-tree structure's needs without requiring a potentially complex row count mechanism.

```c
typedef struct
{
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    void *pages[TABLE_MAX_PAGES];
} Pager;
typedef struct
{
    uint32_t root_page_num;
    Pager *pager;
} Table;
```

Change `get_page()` and `pager_open()` accordingly.

### change in cursor

You need to know the page number and row(cell) number to acess the data.

```c
typedef struct
{
    Table *table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;
} Cursor;

Cursor *
table_start(Table *table)
{
    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->cell_num = 0;
    cursor->page_num = table->root_page_num;
    void *root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->end_of_table = (num_cells == 0);
    return cursor;
}
Cursor *table_end(Table *table)
{
    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;
    void *root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->cell_num = num_cells;
    cursor->end_of_table = true;
    return cursor;
}

void *cursor_value(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    const *page = get_page(cursor->table->pager, page_num);
    return leaf_node_value(page, cursor->cell_num);
    // will give you the position of data(row) in a page
}
void cursor_advance(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *node = get_page(cursor->table->pager, page_num);
    cursor->cell_num += 1;

    if (cursor->row_num >= (*leaf_node_num_cells(node)))
    {
        cursor->end_of_table = true;
    }
}
```

### Insertion Into a Leaf Node

When we open the db for the first time db will be empty so initialize page to leaf node.

```c
Table *db_open(const char *fileName)
{

    Pager *pager = pager_open(fileName);
    Table *table = malloc(sizeof(table));
    table->pager = pager;
    table->root_page_num = 0;
    if (pager->num_pages == 0)
    {
        // New DB file.
        void *root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
    }
    return table;
}
```

function for inserting value

```c
void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value) {
    // Step 1: Get a pointer to the page (node) where the insertion will happen
    void *node = get_page(cursor->table->pager, cursor->page_num);

    // Step 2: Find the current number of cells (rows) in the leaf node
    uint32_t num_cells = leaf_node_num_cells(node);

    // Step 3: Check if the node is full
    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        printf("Need to implement a split");
        exit(EXIT_FAILURE);  // Stop execution for now
    }

    // Step 4: Make space for the new cell, if necessary
    // If the insertion position is not at the end, shift existing cells to the right
    if (cursor->cell_num < num_cells) {
        for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }

    // Step 5: Write the key to the cell at the insertion position
    *(leaf_node_key(node, cursor->cell_num)) = key;

    // Step 6: Write the row data to the corresponding cell
    serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

ExecuteResult execute_insert(Statement *statement, Table *table)
{
    void *node = get_page(table->pager, table->root_page_num);
    if (*(leaf_node_num_cells(node)) >= LEAF_NODE_MAX_CELLS)
    {
        return EXECUTE_TABLE_FULL;
    }

    Row *row_to_insert = &(statement->row_to_insert);
    print_row(row_to_insert);
    Cursor *cursor = table_end(table);
    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
    free(cursor);
    return EXECUTE_SUCCESS;
}
```

**Notes :**

- data in the database file is stored in B-tree format directly on disk, meaning the structure of the B-tree is persisted.
- When the database needs to perform an operation (e.g., a search, insert, or delete), it uses the pager to load pages from the disk (database file) into RAM.
- In a B-tree structure, each page (which represents a node) is connected to other pages via page numbers stored within internal nodes. These page numbers act like pointers but are disk-based references instead of in-memory addresses.

## 04/11/2024

### Phase 7: Binary Search

For insertion we implementing it at the end of the table. But it is better to search the tree to find the correct place then insert the value.

Create a function `leaf_node_find()` to find the leaf node using binary search.

```c
rsor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *(leaf_node_num_cells(node));

    Cursor *cursor = (malloc(sizeof(Cursor)));
    cursor->table = table;
    cursor->page_num = page_num;

    // binary search
    uint32_t low = 0;
    uint32_t high = num_cells;
    while (high != low)
    {
        uint32_t mid = (high + low) / 2;
        uint32_t mid_key = *leaf_node_key(node, mid);
        if (mid_key == key)
        {
            cursor->cell_num = mid;
            return cursor;
        }
        else if (mid_key > key)
        {
            high = mid - 1;
        }
        else
        {
            low = mid + 1;
        }
    }
    cursor->cell_num = mid;
    return cursor;
}
```

Use the fuction for finding the key position and insert.

```c
ExecuteResult execute_insert(Statement *statement, Table *table)
{
    void *node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        return EXECUTE_TABLE_FULL;
    }

    Row *row_to_insert = &(statement->row_to_insert);
    uint32_t key_to_insert = row_to_insert->id;
    Cursor *cursor = table_find();
    if (cursor->cell_num < num_cells)
    {
        uint32_t key_at_index = leaf_node_key(node, cursor->cell_num);
        if (key_at_index == key_to_insert)
        {
            return EXECUTE_DUPLICATE_KEY;
        }
    }
    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
    free(cursor);
    return EXECUTE_SUCCESS;
}

Cursor *table_find(Table *table, uint32_t key)
{
    uint32_t root_page_num = table->root_page_num;
    void *root_node = get_page(table->pager, root_page_num);
    if (get_node_type(root_node) == NODE_LEAF)
    {
        return leaf_node_find(table, root_page_num, key);
    }
    else
    {
        printf("Need to implement searching an internal node\n");
        exit(EXIT_FAILURE);
    }
}

Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *(leaf_node_num_cells(node));

    Cursor *cursor = (malloc(sizeof(Cursor)));
    cursor->table = table;
    cursor->page_num = page_num;

    // binary search
    uint32_t low = 0;
    uint32_t high = num_cells;
    while (high != low)
    {
        uint32_t mid = (high + low) / 2;
        uint32_t mid_key = *leaf_node_key(node, mid);
        if (mid_key == key)
        {
            cursor->cell_num = mid;
            return cursor;
        }
        else if (mid_key > key)
        {
            high = mid - 1;
        }
        else
        {
            low = mid + 1;
        }
    }
    cursor->cell_num = mid;
    return cursor;
}

```

These functions returns node type or change node type.

```c
NodeType get_node_type(void *node)
{
    uint32_t value = *((uint8_t *)(node + NODE_TYPE_OFFSSET));
    return (NodeType)value;
}
void set_node_type(void *node, NodeType type)
{
    uint8_t value = key;
    *((uint8_t *)(node + NODE_TYPE_OFFSSET)) = value;
}
```

**Notes:**

- The * in *leaf_node_num_cells(node) is a dereference operator. The `leaf_node_num_cells()` originally returns the address of the variable but now it returns the value.
- In `Cursor* table_find(Table* table, uint32_t key)`
  the \* indicates that the function table_find returns a pointer to a Cursor type.

## 04/11/2024

### Phase 8: Node Splitting

### rules of btree

- the leaf nodes should be on same level
- every node has min and max number of keys
- max we choose min=max/2
- root node is exception

## what if there is a too many keys

consider max =4 , 7,18,24,35,45
we take min 2 and max 2 keys and make a seperate node for max keys
Left Leaf Node: [ (7, Row7), (18, Row18) ]
Right Leaf Node: [ (24, Row24), (35, Row35), (45, Row45) ]
and the middle key will be appended to the upper node
Internal Node: [ 24 ]
Left Pointer → Left Leaf Node: [ (7, Row7), (18, Row18) ]
Right Pointer → Right Leaf Node: [ (24, Row24), (35, Row35), (45, Row45) ]

## Splitting Algorithm

```c
void *leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
    // get old node
    void *old_node = get_page(cursor->table->pager, cursor->page_num);
    // at the end of the page create new node
    uint32_t new_page_num = get_unused_pages(cursor->table->pager);
    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);

    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *destination_node;

        // get the destination where to go
        // if the node is bigger than the middle one then should go to right node
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }
        // to get the index within node
        // if it is the biggest id node it should go to right most on new node
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        // LEAF_NODE_LEFT_SPLIT_COUNT = 8
        // LEAF_NODE_RIGHT_SPLIT_COUNT = 8
        // eg: if the max is 4 and 5 elements are there the biggest will be in 5 % 8 ==5 so in fifth positon
        void *destination = leaf_node_cell(destination_node, index_within_node);

        /*
        Assume the `LEAF_NODE_MAX_CELLS` is 6, with the left and right split counts (`LEAF_NODE_LEFT_SPLIT_COUNT` and `LEAF_NODE_RIGHT_SPLIT_COUNT`) both set to 4.
        A new key-value pair is to be inserted at position 2 (`cursor->cell_num = 2`). Before the split, the old node contains the keys `[1, 2, 3, 4, 5, 6]`.
        During the split, the process iterates from `i = 6` down to `i = 0`. For `i > 2`, the data is copied from the old node's `i - 1` position because the new key-value pair shifts the subsequent cells. For `i < 2`, data is directly copied from the old node's current position, as these cells remain untouched.
        Specifically, the steps are as follows: for `i = 6` to `i = 4`, values `6` and `5` are copied to the new node, filling positions `3` and `2`.
        For `i = 3`, the value `4` remains in the old node. At `i = 2`, the new key-value pair is inserted into the old node at position 2 using `serialize_row`.
        Finally, values `2` and `1` are copied from their respective positions in the old node. After the split, the old node contains `[1, 2, NEW, 4]`, and the new node contains `[5, 6]`.*/
        if (i == cursor->cell_num)
        {
            serialize_row(value, destination);
        }
        else if (i > cursor->cell_num)
        {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }



        /*
        Loop each cell
        Split the cells based on the its old node poisition
        find index within the old or new node
        find the destination node cell
        if the node is the same on as the cursor is pointing to just serialize row
        if not copy them to old node or new node
        */
        *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
        *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;
        if (is_root_node(old_node))
        {
            return create_new_root_node(cursor->table, new_page_num);
        }
        else
        {
            printf("Need to implement update root");
            exit(EXIT_FAILURE);
        }
    }
}
```

## Allocating New Pages

When we created a new leaf node, we put it in a page decided by get_unused_page_num():

```c
uint32_t get_unused_page_num(Pager *pager) { return pager->num_pages; }

```

## Internal Node Header Format

````c
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET =
    INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                           +INTERNAL_NODE_NUM_KEYS_SIZE +
                                           INTERNAL_NODE_RIGHT_CHILD_SIZE;
const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CELL_SIZE =
    INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;```
````

## Creating a New Root

```c

void *create_new_root_node(Table *table, uint32_t right_child_page_num)
{

    // get root node
    void *root = get_page(table->pager, table->root_page_num);
    //
    void *right_child = get_page(table->pager, right_child_page_num);
    // create a left child to keep the old root data
    uint32_t left_child_page_num = get_unused_pages(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);

    // Move the Old Root Data to the Left Child
    memcpy(left_child, root, PAGE_SIZE);
    set_root_node(left_child, false);
    // Initialize the New Root Node
    initialize_internal_node(root);
    set_root_node(root, true);
    *internal_node_num_key(root) = 1;
    // add left child
    *internal_node_child(root, 0) = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(left_child);
    // add right child
    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;
}
```

## Internal node

```c
// internal node
uint32_t *internal_node_num_key(void *node)
{
    return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}
void *initialize_internal_node(void *node)
{
    memset(node, 0, PAGE_SIZE);
    set_node_type(node, NODE_INTERNAL);
    set_root_node(node, false);
    *internal_node_num_key(node) = 0;
}

uint32_t *internal_node_right_child(void *node)
{
    return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}
uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
    return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}
uint32_t *internal_node_child(void *node, uint32_t child_num)
{
    uint32_t num_keys = *internal_node_num_key(node);
    if (child_num > num_keys)
    {
        printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    else if (child_num == num_keys)
    {
        return internal_node_right_child(node);
    }
    else
    {
        return internal_node_cell(node, child_num);
    }
}
uint32_t *internal_node_key(void *node, uint32_t key_num)
{
    return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}
```

## Get Max key of Node

```c
uint32_t get_node_max_key(void *node)
{
    switch (get_node_type(node))
    {
    case NODE_INTERNAL:
        return *internal_node_key(node, *internal_node_num_key(node) - 1);

    case NODE_LEAF:
        return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
    }
}
```

## To check is node is root and set root node

```c
bool is_root_node(void *node)
{
   uint8_t value = *((uint32_t *)(node + IS_ROOT_OFFSET));
   return value != 0;
}

void set_root_node(void *node, bool is_root)
{
    uint8_t value = is_root;
    *((uint32_t *)(node + IS_ROOT_OFFSET)) = value;
}
```

## Printing the Tree

Recursive function that any node, then prints it and its children

```c
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level)
{
    void *node = get_page(pager, page_num);
    uint32_t num_keys, child;
    switch (get_node_type(node))
    {
    case NODE_LEAF:
        num_keys = *leaf_node_num_cells(node);
        printf("%d", num_keys);
        indent(indentation_level);
        printf("-leaf (size %d)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentation_level + 1);
            printf("- %d", *leaf_node_key(node, i));
        }
        break;
    case NODE_INTERNAL:
        num_keys = *internal_node_num_key(node);
        indent(indentation_level);
        printf("-internal (size %d)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; i++)
        {
            // gets the child node
            child = *internal_node_child(node, i);
            // recursively calling for the child node
            print_tree(pager, child, indentation_level + 1);
            indent(indentation_level + 1);
            printf("-key %d\n", *internal_node_key(node, i));
        }
        break;
    }
}
```

## 28/11/2024

### Phase 9: Recursively Searching the B-Tree

```c
Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    // Fetch the current internal node
    void *node = get_page(table->pager, page_num);

    // Get the number of keys in the internal node
    uint32_t num_keys = *internal_node_num_key(node);

    // Initialize binary search bounds
    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    // Binary search to find the correct child
    while (max_index != min_index)
    {
        // Check the middle key
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *internal_node_key(node, index);

        //there are only two internal keys so binary search is optimised
        // Adjust search bounds based on comparison
        if (key < key_to_right)
        {
            max_index = index; // Key is in the left range
        }
        else
        {
            min_index = index + 1; // Key is in the right range
        }
    }

    // Find the child node corresponding to the key range
    uint32_t child_num = *internal_node_child(node, min_index);
    void *child = get_page(table->pager, child_num);

    // Recursively search based on the child node type
    switch (get_node_type(child))
    {
    case NODE_LEAF:
        // If the child is a leaf node, search within it
        return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
        // If the child is an internal node, continue recursively
        return internal_node_find(table, child_num, key);
    }
}


```

## 03-01-2024

### Phase 10:Scanning a Multi-Level B-Tree Sequentially

We can scan the whole tree by just using internal node but when scanning a table sequentially, moving from one leaf node to the next using a next_leaf pointer is faster than going back up to the parent (internal node) to find the next leaf.

## Change the code `table_start()` code to indicate the leaf nde intead of root node.

```c
Cursor *table_start(Table *table)
{
    Cursor *cursor = table_find(table, 0);
    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    cursor->end_of_table = (num_cells == 0);
    return cursor;
}


```

## New header and functions for NEXT leaf node sequential search

```c
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET =
    LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE;


uint32_t *leaf_node_next_leaf(void *node)
{
    return node + LEAF_NODE_NEXT_LEAF_OFFSET;
}

```

## Advacing Cursor using sequential Search

```c
void cursor_advance(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *node = get_page(cursor->table->pager, page_num);
    cursor->cell_num += 1;

    if (cursor->cell_num >= (*leaf_node_num_cells(node)))
    {
        uint32_t next_page_num = *leaf_node_next_leaf(node);
        if (next_page_num == 0)
        {
            cursor->end_of_table = true;
        }
        else
        {
            cursor->page_num = next_page_num;
            cursor->cell_num = 0;
        }
    }
}

```

Changes in `leaf_node_split_and_insert()`:

```c
void *leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
    // get old node
    void *old_node = get_page(cursor->table->pager, cursor->page_num);
    // at the end of the page create new node
    uint32_t new_page_num = get_unused_pages(cursor->table->pager);
    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);
    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;

    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *destination_node;

        // get the destination where to go
        // if the node is bigger than the middle one then should go to right node
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }
        // to get the index within node
        // if it is the biggest id node it should go to right most on new node
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        // eg: if the max is 4 and 5 elements are there the biggest will be in 5 % 2 ==1 so rightmost
        void *destination = leaf_node_cell(destination_node, index_within_node);
        if (i == cursor->cell_num)
        {
            serialize_row(value, leaf_node_value(destination_node, index_within_node));
            *leaf_node_key(destination_node, index_within_node) = key;
        }
        else if (i > cursor->cell_num)
        {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }

        /*
        Loop each cell
        Split the cells based on the its old node poisition
        find index within the old or new node
        find the destination node cell
        if the node is the same on as the cursor is pointing to just serialize row
        if not copy them to old node or new node
        */
        *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
        *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;
        if (is_root_node(old_node))
        {
            return create_new_root_node(cursor->table, new_page_num);
        }
        else
        {
            printf("Need to implement update root");
            exit(EXIT_FAILURE);
        }
    }
}

```

## 13-01-2024

### Phase 11: Updating Parent Node After a Split

After the split if the number of nodes increases there need to a pointer to the new node in the parent node.

```c
void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
    // get old node
    void *old_node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t old_max = get_node_max_key(cursor->table->pager, old_node);
    // at the end of the page create new node
    uint32_t new_page_num = get_unused_pages(cursor->table->pager);
    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);
    *node_parent(old_node) = *node_parent(old_node);
    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;

    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *destination_node;

        // get the destination where to go
        // if the node is bigger than the middle one then should go to right node
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }
        // to get the index within node
        // if it is the biggest id node it should go to right most on new node
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        // eg: if the max is 4 and 5 elements are there the biggest will be in 5 % 2 ==1 so rightmost
        void *destination = leaf_node_cell(destination_node, index_within_node);
        if (i == cursor->cell_num)
        {
            serialize_row(value, leaf_node_value(destination_node, index_within_node));
            *leaf_node_key(destination_node, index_within_node) = key;
        }
        else if (i > cursor->cell_num)
        {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }

        /*
        Loop each cell
        Split the cells based on the its old node poisition
        find index within the old or new node
        find the destination node cell
        if the node is the same on as the cursor is pointing to just serialize row
        if not copy them to old node or new node
        */
        *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
        *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;
        // if the old node(the initial node) was a root node
        // we need a new root node that points to the old node and new right node
        if (is_root_node(old_node))
        {
            return create_new_root_node(cursor->table, new_page_num);
        }
        else
        {
            uint32_t parent_page_num = *node_parent(old_node);
            //This line gets the page number of the parent node of the old_node.
            uint32_t new_max = get_node_max_key(cursor->table->pager, old_node);
            //This line finds the new largest key in the old_node.
            void *parent = get_page(cursor->table->pager, parent_page_num);
            //This retrieves the parent node from disk (or memory) using its page number.
            update_internal_node_key(parent, old_max, new_max);
            //The old maximum key (old_max) in the parent is updated to the new maximum key (new_max) of the old_node.
            internal_node_insert(cursor->table, parent_page_num, new_page_num);
            //This line inserts the new child node's key and pointer into the parent node.

            return;
        }
    }
}
```

## To get the parent node and update parent nodes

```c
uint32_t* node_parent(void* node)
{ return node + PARENT_POINTER_OFFSET; }

void create_new_root_node(Table *table, uint32_t right_child_page_num)
{

    // get root node
    void *root = get_page(table->pager, table->root_page_num);
    //
    void *right_child = get_page(table->pager, right_child_page_num);
    // create a left child to keep the old root data
    uint32_t left_child_page_num = get_unused_pages(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);
    if (get_node_type(root) == NODE_INTERNAL)
    {
        initialize_internal_node(right_child);
        initialize_internal_node(left_child);
    }
    // Move the Old Root Data to the Left Child
    memcpy(left_child, root, PAGE_SIZE);
    set_root_node(left_child, false);

    if (get_node_type(left_child) == NODE_INTERNAL)
    {
        void *child;
        for (int i = 0; i < *internal_node_num_key(left_child); i++)
        {
            child = get_page(table->pager, *internal_node_child(left_child, i));
            *node_parent(child) = left_child_page_num;
        }
        child = get_page(table->pager, *internal_node_right_child(left_child));
        *node_parent(child) = left_child_page_num;
    }

    // Initialize the New Root Node
    initialize_internal_node(root);
    set_root_node(root, true);
    *internal_node_num_key(root) = 1;
    // add left child
    *internal_node_child(root, 0) = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(table->pager, left_child);

    // add right child
    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;

    //updating parent pointers
    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;
}

```

## `internal_node_find()` code

`internal_node_find_child()` will give the index where you can insert the child pointer not the child pointer itself

```c

uint32_t internal_node_find_child(void *node, uint32_t key)
{

    uint32_t num_keys = *internal_node_num_key(node);
    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    while (max_index != min_index)
    {
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *internal_node_key(node, index);

        if (key <= key_to_right)
        {
            max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }
    return min_index;
}


Cursor *internal_node_find(Table *table, uint32_t key, uint32_t page_num)
{

    void *node = get_page(table->pager, page_num);
    uint32_t child_index = internal_node_find_child(node, key);
    uint32_t child_num = *internal_node_child(node, child_index);
    void *child = get_page(table->pager, child_num);

    switch (get_node_type(child))
    {
    case NODE_LEAF:
        return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
        return internal_node_find(table, child_num, key);
    default:
        exit(EXIT_FAILURE);
    }
}
```

Consider :
Keys: [20, 40]
Children: [Child1, Child2, RightChild]
RightChild Key: 60

Now i have to insert node Child3 which is lesser than 60 but greater than 40
Take the max key of the child3.
i have to move RightChild to 1 position right
And insert the the Child3 here.
Then add 50 to the keys.

Keys: [20, 40, 50]
Children: [Child1, Child2, Child3, RightChild]
RightChild Key: 60

```c
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    //in the above example Keys: [20, 40] -->is parent
    //Child3 is the child
    void *parent = get_page(table->pager, parent_page_num);
    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max_key = get_node_max_key(table->pager, child);
    uint32_t index = internal_node_find_child(parent, child_max_key);
    //internal_node_find_child will give the index where you can insert the child pointer not the child pointer itself
    uint32_t original_num_keys = *internal_node_num_key(parent);
    if (original_num_keys >= INTERNAL_NODE_MAX_KEYS)
    {
        //if the size of the num_keys exceed the max limit
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = *internal_node_right_child(parent);
    //If the right child pointer of the parent is invalid this is the first child being added so just set the right child pointer to the new child and return.
    if (right_child_page_num == INVALID_PAGE_NUM)
    {
        *internal_node_right_child(parent) = child_page_num;
        return;
    }
    *internal_node_num_key(parent) = original_num_keys + 1;
    void *right_child = get_page(table->pager, right_child_page_num);
    //if the right_child_max_key is greater than child_max_key
    //then we need to move the max key of the right child to the internal node
    //Keys: [20, 40] Children: [Child1, Child2, RightChild] RightChild Max Key: 60
    // New Child Max Key: 70
    //Then the internal node key should be 60 not 70  Keys: [20, 40, 60]
    if (child_max_key > get_node_max_key(table->pager, right_child))

    {
        *internal_node_child(parent, original_num_keys) = right_child_page_num;
        //Updates the parent node to store the current right child's page number at the new key slot.
        *internal_node_key(parent, original_num_keys) = get_node_max_key(table->pager, right_child);
        //Stores the maximum key of the current right child in the parent node at the new key slot.
        *internal_node_right_child(parent) = child_page_num;
        //Updates the parent’s right child pointer to point to the new child node.
    }
    else
    {
        //Till the i reaches index shift all the cells to make space for index
        for (uint32_t i = original_num_keys; i > index; i--)
        {
            void *source = internal_node_cell(parent, i - 1);
            void *destination = internal_node_cell(parent, i);
            memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_child(parent, index) = child_page_num;
        *internal_node_key(parent, index) = child_max_key;
        // After the shifting, the new child pointer (child_page_num) is added at the calculated index.
        // The new key (child_max_key) is added to the parent at the same index.
    }
}
```

### Phase 11: Splitting Internal Nodes

## Recursively split and insert internal node

```c
#define INVALID_PAGE_NUM UINT32_MAX
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    void *parent = get_page(table->pager, parent_page_num);
    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max_key = get_node_max_key(table->pager, child);
    uint32_t index = internal_node_find_child(parent, child_max_key);
    uint32_t original_num_keys = *internal_node_num_key(parent);
    if (original_num_keys >= INTERNAL_NODE_MAX_KEYS)
    {
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = *internal_node_right_child(parent);
    if (right_child_page_num == INVALID_PAGE_NUM)
    {
        *internal_node_right_child(parent) = child_page_num;
        return;
    }
    *internal_node_num_key(parent) = original_num_keys + 1;
    void *right_child = get_page(table->pager, right_child_page_num);
    if (child_max_key > get_node_max_key(table->pager, right_child))
    {
        *internal_node_child(parent, original_num_keys) = right_child_page_num;
        *internal_node_key(parent, original_num_keys) = get_node_max_key(table->pager, right_child);
        *internal_node_right_child(parent) = child_page_num;
    }
    else
    {
        for (uint32_t i = original_num_keys; i > 0; i--)
        {
            void *source = internal_node_cell(parent, i - 1);
            void *destination = internal_node_cell(parent, i);
            memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_child(parent, index) = child_page_num;
        *internal_node_key(parent, index) = child_max_key;
    }
}

```

```c
void *initialize_internal_node(void *node)
{
    set_node_type(node, NODE_INTERNAL);
    set_root_node(node, false);
    *internal_node_num_key(node) = 0;
    *internal_node_right_child(node) = INVALID_PAGE_NUM;
}
```

```c

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
    uint32_t num_keys = *internal_node_num_key(node);
    if (child_num > num_keys)
    {
        printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    else if (child_num == num_keys)
    {
        uint32_t *right_child = internal_node_right_child(node);
        if (*right_child == INVALID_PAGE_NUM)
        {
            printf("Tried to access right child of node, but was invalid page\n");
            exit(EXIT_FAILURE);
        }
        return right_child;
    }
    else
    {
        uint32_t *child = internal_node_cell(node, child_num);
        if (*child == INVALID_PAGE_NUM)
        {
            printf("Tried to access right child of node, but was invalid page\n");
            exit(EXIT_FAILURE);
        }
        return child;
    }
}
```

```c
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level)
{
    void *node = get_page(pager, page_num);
    uint32_t num_keys, child;

    switch (get_node_type(node))
    {
    case NODE_LEAF:
        num_keys = *leaf_node_num_cells(node);
        indent(indentation_level);
        printf("- leaf (size %d)\n", num_keys);

        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentation_level + 1);
            printf("- %d\n", *leaf_node_key(node, i));
        }
        break;

    case NODE_INTERNAL:
        num_keys = *internal_node_num_key(node);
        indent(indentation_level);
        printf("- internal (size %d)\n", num_keys);

        if (num_keys > 0)
        {
            for (uint32_t i = 0; i < num_keys; i++)
            {
                child = *internal_node_child(node, i);
                print_tree(pager, child, indentation_level + 1);
            }
        }
        break;
    }
}
```

```c
void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    uint32_t old_page_num = parent_page_num;
    void *old_node = get_page(table->pager, parent_page_num);
    uint32_t old_max = get_node_max_key(table->pager, old_node);

    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table->pager, child);

    uint32_t new_page_num = get_unused_pages(table->pager);
    uint32_t splitting_root_node = is_root_node(old_node);

    void *parent;
    void *new_node;
    if (splitting_root_node)
    {
        create_new_root_node(table, new_page_num);
        parent = get_page(table->pager, table->root_page_num);
        old_page_num = *internal_node_child(parent, 0);
        old_node = get_page(table->pager, old_page_num);
    }
    else
    {
        parent = get_page(table->pager, *node_parent(old_node));
        new_node = get_page(table->pager, new_page_num);
        initialize_internal_node(new_node);
    }

    uint32_t *old_num_keys = internal_node_num_key(old_node);
    uint32_t cur_page_num = *internal_node_right_child(old_node);
    void *cur = get_page(table->pager, cur_page_num);
    internal_node_insert(table, new_page_num, cur_page_num);
    *node_parent(cur) = new_page_num;
    *internal_node_right_child(old_node) = INVALID_PAGE_NUM;
    for (int i = INTERNAL_NODE_MAX_KEYS - 1; i > INTERNAL_NODE_MAX_KEYS / 2; i--)
    {
        cur_page_num = *internal_node_child(old_node, i);
        cur = get_page(table->pager, cur_page_num);
        internal_node_insert(table, new_page_num, cur_page_num);
        *node_parent(cur) = new_page_num;
        (*old_num_keys)--;
    }

    *internal_node_right_child(old_node) = *internal_node_child(old_node, *old_num_keys - 1);
    (*old_num_keys)--;
    uint32_t max_after_split = get_node_max_key(table->pager, old_node);
    uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;
    internal_node_insert(table, destination_page_num, child_page_num);
    *node_parent(child) = destination_page_num;
    update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));
    if (!splitting_root_node)
    {
        internal_node_insert(table, *node_parent(old_node), new_page_num);
        *node_parent(new_node) = *node_parent(old_node);
    }
}

```

```c
uint32_t get_node_max_key(Pager *pager, void *node)
{
    switch (get_node_type(node))
    {
    case NODE_INTERNAL:
        void *right_child = get_page(pager, *internal_node_right_child(node));
        return get_node_max_key(pager, right_child);
    case NODE_LEAF:
        return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
    }
}
```

# Entire code with comments:

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COL_USERNAME_SIZE 32
#define COL_EMAIL_SIZE 255

#define INVALID_PAGE_NUM UINT32_MAX
// gives the size of attribute in the structure
#define size_of_attribute(Struct, Attribute) sizeof(((Struct \*)0)->Attribute)
// compact representation of a row
// struct that represents the syntax of row

typedef struct
{
uint32_t id;
char username[COL_USERNAME_SIZE + 1];
char email[COL_EMAIL_SIZE + 1];
} Row;
// size of inputs
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);

// Offset size
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096;

#define TABLE_MAX_PAGES 100
typedef struct
{
int file_descriptor;
uint32_t file_length;
uint32_t num_pages;
void *pages[TABLE_MAX_PAGES];
} Pager;
typedef struct
{
uint32_t root_page_num;
Pager *pager;
} Table;
typedef struct
{
Table *table;
uint32_t page_num;
uint32_t cell_num;
bool end_of_table;
} Cursor;
// structure for buffer input
typedef struct
{
char *buffer;
size_t buffer_length;
ssize_t input_length;
} InputBuffer;
// structure for meta command result like ".exit"
typedef enum
{
META_COMMAND_SUCCESS,
META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum
{
PREPARE_SUCCESS,
PREPARE_UNRECOGNIZED_STATEMENT,
PREPARE_SYNTAX_ERROR,
PREPARE_NEGATIVE_ID,
PREPARE_STRING_TOO_LONG,
} PrepareResult;
typedef enum
{
EXECUTE_SUCCESS,
EXECUTE_TABLE_FULL,
EXECUTE_DUPLICATE_KEY

} ExecuteResult;
typedef enum
{
SELECT_STATEMENT,
INSERT_STATEMENT
} StatementType;

typedef struct
{
StatementType type;
Row row_to_insert;
} Statement;

typedef enum
{
NODE_INTERNAL,
NODE_LEAF
} NodeType;

/\*

- Common Node Header Layout
  \*/
  const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
  const uint32_t NODE_TYPE_OFFSET = 0;
  const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
  const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
  const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
  const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
  const uint8_t COMMON_NODE_HEADER_SIZE =
  NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

/\*

- Internal Node Header Layout
  \*/
  const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
  const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
  const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
  const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET =
  INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
  const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
  INTERNAL_NODE_NUM_KEYS_SIZE +
  INTERNAL_NODE_RIGHT_CHILD_SIZE;

/\*

- Internal Node Body Layout
  _/
  const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
  const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
  const uint32_t INTERNAL_NODE_CELL_SIZE =
  INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;
  /_ Keep this small for testing \*/
  const uint32_t INTERNAL_NODE_MAX_KEYS = 3;

/\*

- Leaf Node Header Layout
  \*/
  const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
  const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
  const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
  const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET =
  LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
  const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
  LEAF_NODE_NUM_CELLS_SIZE +
  LEAF_NODE_NEXT_LEAF_SIZE;

/\*

- Leaf Node Body Layout
  */
  const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
  const uint32_t LEAF_NODE_KEY_OFFSET = 0;
  const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
  const uint32_t LEAF_NODE_VALUE_OFFSET =
  LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
  const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
  const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
  const uint32_t LEAF_NODE_MAX_CELLS =
  LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;
  const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
  const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT =
  (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;
  uint32_t *leaf_node_num_cells(void *node)
  {
  // returns a pointer to the number of cells(key - value pairs) stored in a leaf node of a B - tree.
  return node + LEAF_NODE_NUM_CELLS_OFFSET;
  }
  void *leaf_node_cell(void _node, uint32_t cell_num)
  {
  // location of the leaf node
  return node + LEAF_NODE_HEADER_SIZE + cell_num _ LEAF_NODE_CELL_SIZE;
  }
  uint32_t *leaf_node_key(void *node, uint32_t cell_num)
  {
  // location of the leaf node key
  return leaf_node_cell(node, cell_num);
  }
  void *leaf_node_value(void *node, uint32_t cell_num)
  {
  // location of the leaf node value
  return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
  }
  void set_node_type(void _node, NodeType type)
  {
  uint8_t value = type;
  _((uint8_t *)(node + NODE_TYPE_OFFSET)) = value;
  }
  void set_root_node(void *node, bool is_root)
  {
  uint8_t value = is_root;
  _((uint32_t _)(node + IS_ROOT_OFFSET)) = value;
  }
  uint32_t *node_parent(void *node)
  {
  return node + PARENT_POINTER_OFFSET;
  }
  uint32_t *leaf_node_next_leaf(void *node)
  {
  return node + LEAF_NODE_NEXT_LEAF_OFFSET;
  }
  void *initialize_leaf_node(void *node)
  {
  // create a leaf node
  memset(node, 0, PAGE_SIZE);
  set_node_type(node, NODE_LEAF);
  set_root_node(node, false);
  *leaf_node_num_cells(node) = 0;
  *leaf_node_next_leaf(node) = 0;
  }

// internal node
uint32_t *internal_node_num_key(void *node)
{
return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}
uint32_t *internal_node_right_child(void *node)
{
return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}
void *initialize_internal_node(void *node)
{
set_node_type(node, NODE_INTERNAL);
set_root_node(node, false);
*internal_node_num_key(node) = 0;
*internal_node_right_child(node) = INVALID_PAGE_NUM;
}

uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
return node + INTERNAL_NODE_HEADER_SIZE + cell_num \* INTERNAL_NODE_CELL_SIZE;
}

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
uint32_t num_keys = *internal_node_num_key(node);
if (child_num > num_keys)
{
printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
exit(EXIT_FAILURE);
}
else if (child_num == num_keys)
{
uint32_t *right_child = internal_node_right_child(node);
if (*right_child == INVALID_PAGE_NUM)
{
printf("Tried to access right child of node, but was invalid page\n");
exit(EXIT_FAILURE);
}
return right_child;
}
else
{
uint32_t *child = internal_node_cell(node, child_num);
if (*child == INVALID_PAGE_NUM)
{
printf("Tried to access right child of node, but was invalid page\n");
exit(EXIT_FAILURE);
}
return child;
}
}
uint32_t *internal_node_key(void _node, uint32_t key_num)
{
return (void _)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}
NodeType get_node_type(void _node)
{
uint32_t value = _((uint8_t \*)(node + NODE_TYPE_OFFSET));

    return (NodeType)value;

}

bool is_root_node(void _node)
{
uint8_t value = _((uint32_t \*)(node + IS_ROOT_OFFSET));
return value != 0;
}

void *get_page(Pager *pager, uint32_t page_num)
{
if (page_num > TABLE_MAX_PAGES)
{
printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
TABLE_MAX_PAGES);
exit(EXIT_FAILURE);
}

    if (pager->pages[page_num] == NULL)
    {
        // cache miss
        void *page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;
        // page only has partial data
        if (pager->file_length % PAGE_SIZE)
        {
            num_pages += 1;
        }

        // Logic of loading data from the disk
        if (page_num <= num_pages)

        {
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1)
            {
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }
        pager->pages[page_num] = page;
        if (page_num >= pager->num_pages)
        {
            pager->num_pages = page_num + 1;
        }
    }
    return pager->pages[page_num];

}
uint32_t get_node_max_key(Pager *pager, void *node)
{
switch (get_node_type(node))
{
case NODE_INTERNAL:
void *right_child = get_page(pager, *internal_node_right_child(node));
return get_node_max_key(pager, right_child);
case NODE_LEAF:
return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
}
}
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
{
void *node = get_page(table->pager, page_num);
uint32_t num_cells = *leaf_node_num_cells(node);
Cursor *cursor = malloc(sizeof(Cursor));
cursor->table = table;
cursor->page_num = page_num;
// Binary search
uint32_t min_index = 0;
uint32_t one_past_max_index = num_cells;
while (one_past_max_index != min_index)
{
uint32_t index = (min_index + one_past_max_index) / 2;
uint32_t key_at_index = *leaf_node_key(node, index);
if (key == key_at_index)
{
cursor->cell_num = index;
return cursor;
}
if (key < key_at_index)
{
one_past_max_index = index;
}
else
{
min_index = index + 1;
}
}
cursor->cell_num = min_index;
return cursor;
}

void print_constants()
{
printf("Constants:\n");
printf("ROW_SIZE: %d\n", ROW_SIZE);
printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
}

uint32_t internal_node_find_child(void *node, uint32_t key)
{
// searchs for the key in the internal node and return its index
uint32_t num_keys = *internal_node_num_key(node);
uint32_t min_index = 0;
uint32_t max_index = num_keys;

    while (max_index != min_index)
    {
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *internal_node_key(node, index);

        if (key <= key_to_right)
        {
            max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }
    return min_index;

}
uint32_t *update_internal_node_key(void *node, uint32_t old_key, uint32_t new_key)
{
uint32_t old_child_index = internal_node_find_child(node, old_key);
*internal_node_key(node, old_child_index) = new_key;
}
Cursor *internal_node_find(Table *table, uint32_t key, uint32_t page_num)
{
void *node = get_page(table->pager, page_num);
uint32_t child_index = internal_node_find_child(node, key);
uint32_t child_num = *internal_node_child(node, child_index);
void *child = get_page(table->pager, child_num);

    switch (get_node_type(child))
    {
    case NODE_LEAF:
        return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
        return internal_node_find(table, child_num, key);
    default:
        exit(EXIT_FAILURE);
    }

}
Cursor *table_find(Table *table, uint32_t key)
{
uint32_t root_page_num = table->root_page_num;
void *root_node = get_page(table->pager, root_page_num);
if (get_node_type(root_node) == NODE_LEAF)
{
return leaf_node_find(table, root_page_num, key);
}
else
{
return internal_node_find(table, root_page_num, key);
}
}
Cursor *table_start(Table *table)
{
Cursor *cursor = table_find(table, 0);
void *node = get_page(table->pager, cursor->page_num);
uint32_t num_cells = *leaf_node_num_cells(node);
cursor->end_of_table = (num_cells == 0);
return cursor;
}

InputBuffer *
new_input_buffer()
{
InputBuffer *input_buffer = (InputBuffer *)malloc(sizeof(InputBuffer));
input_buffer->buffer = NULL;
input_buffer->buffer_length = 0;
input_buffer->input_length = 0;
return input_buffer;
}
void serialize_row(Row *source, void *destination)
{
memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
strncpy(destination + USERNAME_OFFSET, source->username, USERNAME_SIZE);
strncpy(destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);
}
void deserialize_row(void *source, Row \*destination)
{
memcpy(&(destination->id), (source + ID_OFFSET), ID_SIZE);
memcpy(&(destination->username), (source + USERNAME_OFFSET), USERNAME_SIZE);
memcpy(&(destination->email), (source + EMAIL_OFFSET), EMAIL_SIZE);
}

Pager *pager_open(const char *filename)
{

    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        printf("Unable to open the file.\n");
        exit(EXIT_FAILURE);
    }
    off_t file_length = lseek(fd, 0, SEEK_END);
    Pager *pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_length = file_length;
    pager->num_pages = (file_length / PAGE_SIZE);
    if (file_length % PAGE_SIZE != 0)
    {
        printf("Corrupt file. \n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        pager->pages[i] = NULL;
    }
    return pager;

}

Table *db_open(const char *fileName)
{

    Pager *pager = pager_open(fileName);
    Table *table = malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;
    if (pager->num_pages == 0)
    {
        // New DB file.
        void *root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
        set_root_node(root_node, true);
    }
    return table;

}

uint32_t get_unused_pages(Pager \*pager)
{
return pager->num_pages;
}

void create_new_root_node(Table \*table, uint32_t right_child_page_num)
{

    // get root node
    void *root = get_page(table->pager, table->root_page_num);
    //
    void *right_child = get_page(table->pager, right_child_page_num);
    // create a left child to keep the old root data
    uint32_t left_child_page_num = get_unused_pages(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);
    if (get_node_type(root) == NODE_INTERNAL)
    {
        initialize_internal_node(right_child);
        initialize_internal_node(left_child);
    }
    // Move the Old Root Data to the Left Child
    memcpy(left_child, root, PAGE_SIZE);
    set_root_node(left_child, false);

    if (get_node_type(left_child) == NODE_INTERNAL)
    {
        void *child;
        for (int i = 0; i < *internal_node_num_key(left_child); i++)
        {
            child = get_page(table->pager, *internal_node_child(left_child, i));
            *node_parent(child) = left_child_page_num;
        }
        child = get_page(table->pager, *internal_node_right_child(left_child));
        *node_parent(child) = left_child_page_num;
    }

    // Initialize the New Root Node
    initialize_internal_node(root);
    set_root_node(root, true);
    *internal_node_num_key(root) = 1;
    // add left child
    *internal_node_child(root, 0) = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(table->pager, left_child);

    // add right child
    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;
    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;

}
void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
void *parent = get_page(table->pager, parent_page_num);
void *child = get_page(table->pager, child_page_num);
uint32_t child_max_key = get_node_max_key(table->pager, child);
uint32_t index = internal_node_find_child(parent, child_max_key);
uint32_t original_num_keys = \*internal_node_num_key(parent);
if (original_num_keys >= INTERNAL_NODE_MAX_KEYS)
{
internal_node_split_and_insert(table, parent_page_num, child_page_num);
return;
}

    uint32_t right_child_page_num = *internal_node_right_child(parent);
    if (right_child_page_num == INVALID_PAGE_NUM)
    {
        *internal_node_right_child(parent) = child_page_num;
        return;
    }
    *internal_node_num_key(parent) = original_num_keys + 1;
    void *right_child = get_page(table->pager, right_child_page_num);
    if (child_max_key > get_node_max_key(table->pager, right_child))
    {
        *internal_node_child(parent, original_num_keys) = right_child_page_num;
        *internal_node_key(parent, original_num_keys) = get_node_max_key(table->pager, right_child);
        *internal_node_right_child(parent) = child_page_num;
    }
    else
    {
        for (uint32_t i = original_num_keys; i > index; i--)
        {
            void *source = internal_node_cell(parent, i - 1);
            void *destination = internal_node_cell(parent, i);
            memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_child(parent, index) = child_page_num;
        *internal_node_key(parent, index) = child_max_key;
    }

}

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
// get old node
void *old_node = get_page(cursor->table->pager, cursor->page_num);
uint32_t old_max = get_node_max_key(cursor->table->pager, old_node);
// at the end of the page create new node
uint32_t new_page_num = get_unused_pages(cursor->table->pager);
void *new_node = get_page(cursor->table->pager, new_page_num);
initialize_leaf_node(new_node);
*node_parent(old_node) = *node_parent(old_node);
*leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
\*leaf_node_next_leaf(old_node) = new_page_num;

    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *destination_node;

        // get the destination where to go
        // if the node is bigger than the middle one then should go to right node
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }
        // to get the index within node
        // if it is the biggest id node it should go to right most on new node
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        // eg: if the max is 4 and 5 elements are there the biggest will be in 5 % 2 ==1 so rightmost
        void *destination = leaf_node_cell(destination_node, index_within_node);
        if (i == cursor->cell_num)
        {
            serialize_row(value, leaf_node_value(destination_node, index_within_node));
            *leaf_node_key(destination_node, index_within_node) = key;
        }
        else if (i > cursor->cell_num)
        {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }

        /*
        Loop each cell
        Split the cells based on the its old node poisition
        find index within the old or new node
        find the destination node cell
        if the node is the same on as the cursor is pointing to just serialize row
        if not copy them to old node or new node
        */
        *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
        *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;
        // if the old node(the initial node) was a root node
        // we need a new root node that points to the old node and new right node
        if (is_root_node(old_node))
        {
            return create_new_root_node(cursor->table, new_page_num);
        }
        else
        {
            uint32_t parent_page_num = *node_parent(old_node);
            uint32_t new_max = get_node_max_key(cursor->table->pager, old_node);
            void *parent = get_page(cursor->table->pager, parent_page_num);
            update_internal_node_key(parent, old_max, new_max);
            internal_node_insert(cursor->table, parent_page_num, new_page_num);
            return;
        }
    }

}

void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value)
{
void *node = get_page(cursor->table->pager, cursor->page_num);
uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        // Node full
        printf("Leaf node full, splitting...\n");
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    if (cursor->cell_num < num_cells)
    {
        // Make room for new cell
        for (uint32_t i = num_cells; i > cursor->cell_num; i--)
        {
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }
    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));

    printf("Inserted key %d at cell %d\n", key, cursor->cell_num);

}
void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
uint32_t old_page_num = parent_page_num;
void *old_node = get_page(table->pager, parent_page_num);
uint32_t old_max = get_node_max_key(table->pager, old_node);

    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table->pager, child);

    uint32_t new_page_num = get_unused_pages(table->pager);
    uint32_t splitting_root_node = is_root_node(old_node);

    void *parent;
    void *new_node;
    if (splitting_root_node)
    {
        create_new_root_node(table, new_page_num);
        parent = get_page(table->pager, table->root_page_num);
        old_page_num = *internal_node_child(parent, 0);
        old_node = get_page(table->pager, old_page_num);
    }
    else
    {
        parent = get_page(table->pager, *node_parent(old_node));
        new_node = get_page(table->pager, new_page_num);
        initialize_internal_node(new_node);
    }

    uint32_t *old_num_keys = internal_node_num_key(old_node);
    uint32_t cur_page_num = *internal_node_right_child(old_node);
    void *cur = get_page(table->pager, cur_page_num);
    internal_node_insert(table, new_page_num, cur_page_num);
    *node_parent(cur) = new_page_num;
    *internal_node_right_child(old_node) = INVALID_PAGE_NUM;
    for (int i = INTERNAL_NODE_MAX_KEYS - 1; i > INTERNAL_NODE_MAX_KEYS / 2; i--)
    {
        cur_page_num = *internal_node_child(old_node, i);
        cur = get_page(table->pager, cur_page_num);
        internal_node_insert(table, new_page_num, cur_page_num);
        *node_parent(cur) = new_page_num;
        (*old_num_keys)--;
    }

    *internal_node_right_child(old_node) = *internal_node_child(old_node, *old_num_keys - 1);
    (*old_num_keys)--;
    uint32_t max_after_split = get_node_max_key(table->pager, old_node);
    uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;
    internal_node_insert(table, destination_page_num, child_page_num);
    *node_parent(child) = destination_page_num;
    update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));
    if (!splitting_root_node)
    {
        internal_node_insert(table, *node_parent(old_node), new_page_num);
        *node_parent(new_node) = *node_parent(old_node);
    }

}
void *pager_flush(Pager *pager, uint32_t page_num)
{
if (pager->pages[page_num] == NULL)
{
printf("tried to flush empty page");
exit(EXIT_FAILURE);
}
off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
if (offset == -1)
{
printf("Error seeking.\n");
exit(EXIT_FAILURE);
}
ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
if (bytes_written == -1)
{
printf("error in writing.\n");
exit(EXIT_FAILURE);
}
}
void *db_close(Table *table)
{
Pager *pager = table->pager;
for (uint32_t i = 0; i < pager->num_pages; i++)
{
if (pager->pages[i] == NULL)
{
continue;
}
pager_flush(pager, i);
free(pager->pages[i]);
pager->pages[i] = NULL;
}

    int result = close(pager->file_descriptor);
    if (result == -1)
    {
        printf("error in clsoing.\n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        void *page = pager->pages[i];
        if (page)
        {
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);
    free(table);

}
void free_table(Table *table)
{
for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
{
free(table->pager->pages[i]);
}
free(table);
}
void print_prompt()
{
printf("db>");
}
void print_row(Row *row)
{
printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}
void read_input(InputBuffer \*inputBuffer)
{
ssize_t bytes_read = getline(&(inputBuffer->buffer), &(inputBuffer->buffer_length), stdin);
if (bytes_read <= 0)
{
printf("error in reading\n");
exit(EXIT_FAILURE);
}
inputBuffer->buffer[bytes_read - 1] = 0;
inputBuffer->input_length = bytes_read - 1;
}

void close_input_buffer(InputBuffer *inputBuffer)
{
free(inputBuffer->buffer);
free(inputBuffer);
}
void indent(uint32_t level)
{
for (uint32_t i = 0; i < level; i++)
{
printf(" ");
}
}
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level)
{
void \*node = get_page(pager, page_num);
uint32_t num_keys, child;

    switch (get_node_type(node))
    {
    case NODE_LEAF:
        num_keys = *leaf_node_num_cells(node);
        indent(indentation_level);
        printf("- leaf (size %d)\n", num_keys);

        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentation_level + 1);
            printf("- %d\n", *leaf_node_key(node, i));
        }
        break;

    case NODE_INTERNAL:
        num_keys = *internal_node_num_key(node);
        indent(indentation_level);
        printf("- internal (size %d)\n", num_keys);

        if (num_keys > 0)
        {
            for (uint32_t i = 0; i < num_keys; i++)
            {
                child = *internal_node_child(node, i);
                print_tree(pager, child, indentation_level + 1);
            }
        }
        break;
    }

}

MetaCommandResult do_meta_command(InputBuffer *input_buffer, Table *table)
{
if (strcmp(input_buffer->buffer, ".exit") == 0)
{
db_close(table);
exit(EXIT_SUCCESS);
}
else if (strcmp(input_buffer->buffer, ".constants") == 0)
{
print_constants();
return META_COMMAND_SUCCESS;
}
else if (strcmp(input_buffer->buffer, ".btree") == 0)
{
printf("Tree:\n");
print_tree(table->pager, 0, 0);
return META_COMMAND_SUCCESS;
}
else
{
return META_COMMAND_UNRECOGNIZED_COMMAND;
}
}
void *cursor_value(Cursor *cursor)
{
uint32_t page_num = cursor->page_num;
void *page = get_page(cursor->table->pager, page_num);
return leaf_node_value(page, cursor->cell_num);
// will give you the position of data(row) in a page
}
void cursor_advance(Cursor *cursor)
{
uint32_t page_num = cursor->page_num;
void \*node = get_page(cursor->table->pager, page_num);
cursor->cell_num += 1;

    if (cursor->cell_num >= (*leaf_node_num_cells(node)))
    {
        uint32_t next_page_num = *leaf_node_next_leaf(node);
        if (next_page_num == 0)
        {
            cursor->end_of_table = true;
        }
        else
        {
            cursor->page_num = next_page_num;
            cursor->cell_num = 0;
        }
    }

}
PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement)
{
statement->type = INSERT_STATEMENT;
const char *keyword = strtok(input_buffer->buffer, " ");
const char *id_STRING = strtok(NULL, " ");
const char *username = strtok(NULL, " ");
const char *email = strtok(NULL, " ");
if (id_STRING == NULL || username == NULL || email == NULL)
{
return PREPARE_SYNTAX_ERROR;
}
int id = atoi(id_STRING);
if (id < 0)
{
return PREPARE_NEGATIVE_ID;
}
if (strlen(username) >= COL_USERNAME_SIZE)
{
return PREPARE_STRING_TOO_LONG;
}
if (strlen(email) >= COL_EMAIL_SIZE)
{
return PREPARE_STRING_TOO_LONG;
}
statement->row_to_insert.id = id;
strcpy(statement->row_to_insert.username, username);
strcpy(statement->row_to_insert.email, email);
return PREPARE_SUCCESS;
}
PrepareResult prepare_statement(InputBuffer *inputBuffer, Statement *statement)
{
if (strncmp(inputBuffer->buffer, "insert", 6) == 0)
{

        return prepare_insert(inputBuffer, statement);
    }
    else if (strncmp(inputBuffer->buffer, "select", 6) == 0)
    {

        statement->type = SELECT_STATEMENT;
        return PREPARE_SUCCESS;
    }
    else
    {
        return PREPARE_UNRECOGNIZED_STATEMENT;
    }

}
ExecuteResult execute_insert(Statement *statement, Table *table)
{
Row *row_to_insert = &(statement->row_to_insert);
uint32_t key_to_insert = row_to_insert->id;
Cursor *cursor = table_find(table, key_to_insert);

    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (cursor->cell_num < num_cells)
    {
        uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
        if (key_at_index == key_to_insert)
        {
            printf("Duplicate key error: %d\n", key_to_insert);

            return EXECUTE_DUPLICATE_KEY;
        }
    }

    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
    free(cursor);
    return EXECUTE_SUCCESS;

}
ExecuteResult execute_select(Statement *statement, Table *table)
{
Cursor \*cursor = table_start(table);
Row row;
while (!(cursor->end_of_table))
{

        deserialize_row(cursor_value(cursor), &(row));

        print_row(&(row));

        cursor_advance(cursor);
    }
    return EXECUTE_SUCCESS;

}
ExecuteResult execute_statement(Statement *statement, Table *table)
{
switch (statement->type)
{
case SELECT_STATEMENT:
execute_select(statement, table);
break;
case INSERT_STATEMENT:
execute_insert(statement, table);
break;
default:
break;
}
}

int main(int argc, char \*argv[])
{

    if (argc < 2)
    {
        printf("Need to enter the File name.\n");
        exit(EXIT_FAILURE);
    }
    char *filename = argv[1];
    Table *table = db_open(filename);
    InputBuffer *inputBuffer = new_input_buffer();
    while (true)
    {
        print_prompt();
        read_input(inputBuffer);

        if (inputBuffer->buffer[0] == '.')
        {
            switch (do_meta_command(inputBuffer, table))
            {
            case (META_COMMAND_SUCCESS):
                continue;
            case (META_COMMAND_UNRECOGNIZED_COMMAND):
                printf("Unrecognized command '%s'\n", inputBuffer->buffer);
                continue;
            }
        }
        Statement statement;
        switch (prepare_statement(inputBuffer, &statement))
        {
        case PREPARE_SUCCESS:
            break;
        case PREPARE_NEGATIVE_ID:
            printf("ID can't be negative.\n");
            continue;
        case PREPARE_UNRECOGNIZED_STATEMENT:
            printf("Unrecognized command at the start %s \n", inputBuffer->buffer);
            continue;
        case PREPARE_STRING_TOO_LONG:
            printf("string is too long.\n");
            continue;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            continue;
        }
        switch (execute_statement(&statement, table))
        {
        case EXECUTE_SUCCESS:
            printf("Executed.\n");
            break;

        case EXECUTE_DUPLICATE_KEY:
            printf("Key Already Exists.\n");
            break;
        }
    }

}
