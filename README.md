# potDB

potDB is a lightweight key-value database for storing an object's position over time.
**Time → Position**

It is designed to efficiently record and retrieve where an object was at a given point in time.

## Architecture

The architecture consists of a front-end that processes queries and a back-end that manages data storage using a B-tree structure.

The flow goes like this:
`Cursor -> Table -> Pager -> File`

- Each node is a page.
- Each cell (key-value pair) is a row, except for internal nodes which point to another node.

## Table of Contents

- [Phase 1: REPL Interface]
- [Phase 2: SQL Compiler and Virtual Machine]
- [Phase 3: Single Table Database]
- [Phase 4: Persistent Database]
- [Phase 5: Cursor Abstraction]
- [Phase 6: B-tree Implementation]
- [Phase 7: Binary Search]
- [Phase 8: Node Splitting]
- [Phase 9: Recursively Searching the B-Tree]
- [Phase 10: Scanning a Multi-Level B-Tree Sequentially]
- [Phase 11: Updating Parent Node After a Split]
- [Phase 12: Splitting Internal Nodes]
- [Notes]

## Installation

To build and run the project, follow these steps:

1. Clone the repository:

   ```bash
   git clone <repository-url>
   cd potdb
   ```

2. Compile the code:

   ```bash
   make
   # or manually compile with:
   gcc -Wall -Wextra -Iinclude src/*.c -o potdb
   ```

3. Run the database:
   ```bash
   ./potdb <database-file>
   ```

## Usage

You can interact with the database using commands. Here are some examples:

- To insert a new row:

  ```sql
  insert 1 "username" "email@example.com";
  insert 2 "xxx" "jfio" 
  insert 3 "xwxx" "jfwio" 
  ```

- To select rows:

  ```sql
  select;
  ```

- To exit the REPL:
  ```sql
  .exit
  ```

### A detailed explanation is provided in logs.md

---
