#ifndef TABLE_H
#define TABLE_H

#include <llvm-c/Core.h>
#include <stdbool.h>

// A "value" has both the actual value and the optional address of the value
typedef struct value{
    LLVMValueRef address;
    LLVMValueRef value;
} value_t;

// An entry associates values with names
typedef struct entry{
    struct entry *nextentry;
    char* name;
    value_t val;
} entry_t;

// A table stores a linked list of entries
typedef struct table{
    struct table *nexttable;
    entry_t* entrylist;
} table_t;

// Initialization/Destructor functions for tables
table_t *create_table(table_t *prev);
void destroy_table(table_t *current);

// Try to insert a value into the current symbol table
void insert_value(table_t* table, char* name, value_t value);

// Check if the current symbol table contains a name
bool contains_name(table_t* table, char* name);

// Try to get a value from the symbol table with the same name
// This operation is recursively called on the previous symbol table
value_t get_value(table_t* table, char* name);

// String function to correct escape sequences
char* translate_special_chars(char* str, int length);

#endif