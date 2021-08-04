#ifndef PARSE_H
#define PARSE_H

#include <llvm-c/Core.h>
#include "table.h"

#include <stdbool.h>

// Store the type of a parse_list
typedef enum parse_list_type {
    PL_ID,
    PL_TYPE, 
    PL_VALUE
} parse_list_type_t;

// Resizable Array structure for parsing 
// LLVM expects pointers to contiguous arrays, so linked lists don't work 
typedef struct parse_list{
    union {
        char** ids;
        LLVMTypeRef *types;
        LLVMValueRef *values;
    };
    uint32_t length;
    uint32_t capacity;
} parse_list_t;

// Structure for lists of types and IDs
// Used in function declarations and structures
typedef struct type_id_list{
    parse_list_t id_list;
    parse_list_t type_list;
} type_id_list_t;

// Structure for lists of values and IDs
// Used in function calls and declarations
typedef struct value_id_list{
    parse_list_t id_list;
    parse_list_t value_list;
} value_id_list_t;

// Structure to declare parameters of functions
typedef struct arg_def {
    type_id_list_t list;
    bool varg;
} arg_def_t;

// Initialization/Insertion into parse_list
void initialize_parse_list(parse_list_t *list);
void insert_parse_list(parse_list_t *list, void* data, parse_list_type_t type);

// Initialization/Insertion into type_id_list
void initialize_type_id_list(type_id_list_t *list);
void insert_type_id_list(type_id_list_t *list, LLVMTypeRef type, char* id);

// Initialization/Insertion into value_id_list
void initialize_value_id_list(value_id_list_t *list);
void insert_value_id_list(value_id_list_t *list, LLVMValueRef value, char* id);

// Initialization of arg_def
void create_arg_def(arg_def_t *def, type_id_list_t *list, bool varg);

#endif 