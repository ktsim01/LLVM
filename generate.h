#ifndef GENERATE_H
#define GENERATE_H

#include <stdbool.h>
#include <llvm-c/Core.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Analysis.h>
#include "parse.h"
#include "table.h"  

// Store state of each conditional
typedef struct cond_stack {
    struct cond_stack *prev;
    LLVMBasicBlockRef if_branch;
    LLVMBasicBlockRef else_branch;
    bool eliminate_done;
} cond_stack_t;

// Store state of each loop
typedef struct loop_stack {
    struct loop_stack *prev;
    char* label;
    LLVMBasicBlockRef condition;
    LLVMBasicBlockRef body;
    LLVMBasicBlockRef end;
} loop_stack_t;

// Store each typedef
typedef struct type_list {
    struct type_list *next;
    char* name;
    LLVMTypeRef type;
} type_list_t;

// Store information for each structure
typedef struct agg_list {
    struct agg_list *next;
    LLVMTypeRef type;
    type_id_list_t components;
} agg_list_t;

// Different Types of Operations
typedef enum operation{
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_BIT_AND,
    OP_BIT_OR,
    OP_BIT_XOR,
    OP_BIT_NOT,
    OP_LSHIFT,
    OP_RSHIFT,
    OP_BOOL_AND,
    OP_BOOL_OR,
    OP_BOOL_NOT,
    OP_LESS,
    OP_LEQ,
    OP_GREATER,
    OP_GEQ,
    OP_EQ,
    OP_NEQ 
} operation_t;

// Create a struct type
void create_struct(char* name, type_id_list_t *content);

// Create/find type defintions
void create_type(char* name, LLVMTypeRef type);
LLVMTypeRef get_type(char* name, bool error);

// Create/finish function declarations
void create_function(char* name, LLVMTypeRef return_type, arg_def_t *args, bool is_definition);
void finish_function();

// Create local/global variable declarations
void create_declaration(LLVMTypeRef type, value_id_list_t *list, bool is_local);

// Create/end each variable scope
void create_scope();
void finish_scope();

// Create/end each conditional
void create_if(value_t condition);
void create_else();
void finish_if();

// Create/end each loop
void create_while(char* label);
void create_while_condition(value_t condition);
void finish_while();

// Create break/continue statement
void create_break_continue(char* label, bool is_break);
void create_return(value_t val);

// Check if a value is truthy
value_t truthy(value_t val);

// Cast a value to another type
value_t cast(value_t val, LLVMTypeRef type, bool is_explicit);
LLVMTypeRef implicit_cast(value_t lhs, value_t rhs, value_t *l_cast, value_t *r_cast);

// Create constants of different types
value_t create_int_constant(int64_t val);
value_t create_fp_constant(double val);
value_t create_string_constant(char* str);

// Create an assignment
value_t create_assignment(value_t left, value_t right);

// Create a function call
value_t create_call(value_t function, parse_list_t values);

// Arithmetic Operators
value_t create_math_binop(value_t left, value_t right, operation_t op);
value_t create_math_negate(value_t val);

// Bitwise Operators
value_t create_bitwise_binop(value_t left, value_t right, operation_t op);
value_t create_bitwise_not(value_t val);

// Boolean Operators
value_t create_boolean_binop(value_t left, value_t right, operation_t op);
value_t create_boolean_not(value_t val);

// Comparison Operators 
value_t create_comparison(value_t left, value_t right, operation_t op);

// Reference/Dereference Operators
value_t create_ref(value_t val);
value_t create_deref(value_t left);

// Indexing Operator
value_t create_index(value_t left, value_t right);

// Dot Operator
value_t create_dot(value_t left, char* name);

// sizeof() operator
value_t create_sizeof(LLVMTypeRef type);

// Look up identifier in the symbol table
value_t get_identifier(char* id);

// Generate LLVM Code
void generate(char* llvm_dir, char* input_file, char* output_file, int scheck, int rcheck);

#endif