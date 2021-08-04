#include "parse.h"
#include <stdlib.h>

// Initialize fields of parse_list
void initialize_parse_list(parse_list_t *list){
    list->capacity = 0;
    list->length = 0;
    list->ids = NULL;
}
void insert_parse_list(parse_list_t *list, void* data, parse_list_type_t type){
    // Check if list is full
    if(list->length == list->capacity){
        if(list->capacity == 0) { 
            // Create the list with malloc() if it doesn't exist
            list->capacity = 1;
            if(type == PL_ID)
                list->ids = malloc(sizeof(char*));
            else if(type == PL_TYPE)
                list->types = malloc(sizeof(LLVMTypeRef));  
            else if(type == PL_VALUE)
                list->values = malloc(sizeof(LLVMValueRef));  
        }
        else{
            // Resize the list using realloc()
            list->capacity *= 2;
            if(type == PL_ID)
                list->ids = realloc(list->ids, sizeof(char*) * list->capacity);
            else if(type == PL_TYPE)
                list->types = realloc(list->ids, sizeof(LLVMTypeRef) * list->capacity);
            else if(type == PL_VALUE)
                list->values = realloc(list->ids, sizeof(LLVMValueRef) * list->capacity);
        }
    }
    // Insert into list using the void* and union type
    if(type == PL_ID)
        list->ids[list->length] = data;
    else if(type == PL_TYPE)
        list->types[list->length] = data;  
    else if(type == PL_VALUE)
        list->values[list->length] = data;  
    list->length += 1;
}

// Initialize the internal parse_lists of type_id_list
void initialize_type_id_list(type_id_list_t *list){
    initialize_parse_list(&list->id_list);
    initialize_parse_list(&list->type_list);
}

// Insert into the internal parse_lists of type_id_list
void insert_type_id_list(type_id_list_t *list, LLVMTypeRef type, char* id) {
    insert_parse_list(&list->id_list, id, PL_ID);
    insert_parse_list(&list->type_list, type, PL_TYPE);
}

// Initialize the internal parse_lists of value_id_list
void initialize_value_id_list(value_id_list_t *list){
    initialize_parse_list(&list->id_list);
    initialize_parse_list(&list->value_list);
}

// Insert into the internal parse_lists of value_id_list
void insert_value_id_list(value_id_list_t *list, LLVMValueRef value, char* id){
    insert_parse_list(&list->id_list, id, PL_ID);
    insert_parse_list(&list->value_list, value, PL_VALUE);
}

// Create a value_id_list with an extra boolean flag for vargs
void create_arg_def(arg_def_t *def, type_id_list_t *list, bool varg){
    def->varg = varg;
    if(!list) initialize_type_id_list(&def->list);
    else def->list = *list;
}
