#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generate.h"
#include "bison.tab.h"
#include "flex.l.h"

// Store the LLVM module and builder
LLVMBuilderRef builder;
LLVMModuleRef module;

// Store various aspects of state needed by the code generator
cond_stack_t *curr_cond = NULL;
loop_stack_t *curr_loop = NULL;
type_list_t *types = NULL;
agg_list_t *structs = NULL;
table_t *symbol_table = NULL;

// Check whether the current block of code has been terminated
#define FINISHED (LLVMGetInsertBlock(builder) && LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))

void create_struct(char* name, type_id_list_t *list){
    // Check if the type has already been defined
    LLVMTypeRef type = LLVMGetTypeByName(module, name);
    if(type ){
        if(!LLVMIsOpaqueStruct(type)){
            printf("Structure already defined\n");
            exit(0);
        }
    } else{
        type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);
        create_type(name, type);
    }
    if(list){ 
        // Create the struct using the specified information
        LLVMStructSetBody(type, list->type_list.types, list->type_list.length, true);
        agg_list_t *struct_type = malloc(sizeof(agg_list_t));
        struct_type->next = structs;
        struct_type->type = type;
        struct_type->components = *list;
        structs = struct_type;
    }
}

void create_type(char* name, LLVMTypeRef type){
    // Create a new type structure
    type_list_t *new_type = malloc(sizeof(type_list_t));
    new_type->name = name;
    new_type->type = type;
    new_type->next = types;
    type_list_t *curr = types;

    // Check if the type has been defined before
    while(curr){
        if(strcmp(curr->name, new_type->name) == 0){
            printf("Redefined Type: %s\n", curr->name);
            exit(0);
        }
        curr =  curr->next;
    }
    types = new_type;
}

LLVMTypeRef get_type(char* name, bool error){
    // Attempt to find the type name through traversal
    type_list_t *curr = types;
    while(curr){
        if(strcmp(curr->name, name) == 0){
            return curr->type;
        }
        curr =  curr->next;
    }
    if(error){
        printf("Couldn't find type %s\n", name);
        exit(0);
    }
    return NULL;
}


void create_function(char* name, LLVMTypeRef return_type, arg_def_t *args, bool is_definition){
    // Create a type for the new function
    LLVMTypeRef type = LLVMFunctionType(return_type, args->list.type_list.types, args->list.type_list.length, args->varg);
    value_t fn;
    fn.address = NULL;

    // Check if the function already exists
    fn.value = LLVMGetNamedFunction(module, name);
    LLVMBasicBlockRef entry;
    if(fn.value){
        // Check if the function definitions match up
        if(type != LLVMGetElementType(LLVMTypeOf(fn.value))){
            printf("function types don't equal!\n");
            exit(0);
        }
        // Check if the function is being redefined
        entry = LLVMGetFirstBasicBlock(fn.value);
        if(entry){
            printf("redefine function!\n");
            exit(0);
        }
    }

    // Otherwise, create the function for the first time
    else {
        fn.value = LLVMAddFunction(module, name, type);
        LLVMSetFunctionCallConv(fn.value, LLVMCCallConv);
        insert_value(symbol_table, name, fn);
    }

    // If it is a defintiion, extra instructions must be generated
    if(is_definition){
        // Setup the function's entry block and scope
        create_scope();
        entry = LLVMAppendBasicBlock(fn.value, "entry");
        LLVMPositionBuilderAtEnd(builder, entry);

        // Define each of the arguments in the function's scope
        for(int i = 0; i<args->list.type_list.length; i++){
            value_t arg;
            arg.value = NULL;
            arg.address = LLVMBuildAlloca(builder, args->list.type_list.types[i], "");
            LLVMBuildStore(builder, LLVMGetParam(fn.value, i), arg.address);
            insert_value(symbol_table, args->list.id_list.ids[i], arg);
        }
    }
}

void finish_function(){
    // End the functions scope
    finish_scope();

    // Insert a dummy return if the function return type is void
    LLVMBasicBlockRef block = LLVMGetInsertBlock(builder);
    LLVMValueRef fn = LLVMGetBasicBlockParent(block);
    LLVMTypeRef fn_type = LLVMGetElementType(LLVMTypeOf(fn));
    if(LLVMGetReturnType(fn_type) == LLVMVoidType() && !FINISHED){
        LLVMBuildRetVoid(builder);
    }

    // Reset the Instruction Builder
    LLVMClearInsertionPosition(builder);
}

void create_declaration(LLVMTypeRef type, value_id_list_t *list, bool is_local){
    // Global and local declarations are different
    if(is_local){
        // Check if the current block is finished
        if(FINISHED) return;

        // Get context of where the InstructionBuilder is located
        LLVMBasicBlockRef current = LLVMGetInsertBlock(builder);
        LLVMValueRef fn = LLVMGetBasicBlockParent(current);
        LLVMBasicBlockRef entry = LLVMGetFirstBasicBlock(fn);
        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(entry);

        // Store temporary variables
        value_t current_value;
        value_t casted_value;
        current_value.address = NULL;

        // Iterate through each value_id pair
        for(int i = 0; i<list->id_list.length; i++){
            // Move the instruction builder to the block
            if(!terminator)
                LLVMPositionBuilderAtEnd(builder, entry);
            else
                LLVMPositionBuilderBefore(builder, terminator);

            // Create an allocation for each local variable
            // Every local has space in the stack frame by default
            value_t var;
            var.value = NULL;
            var.address = LLVMBuildAlloca(builder, type, "");
            insert_value(symbol_table, list->id_list.ids[i], var);

            // If the declaration has an "=" initializer, move back and create a store
            if(list->value_list.values[i]){
                current_value.value = list->value_list.values[i];
                casted_value = cast(current_value, type, false);
                LLVMPositionBuilderAtEnd(builder, current);
                LLVMBuildStore(builder, LLVMBuildTruncOrBitCast(builder, casted_value.value, type, ""), var.address);
            }
        }
    
        // Reset the instruction builder to where it was
        LLVMPositionBuilderAtEnd(builder, current);
    }
    else{
        // Create a global for each value_id pair
        value_t current_value;
        current_value.address = NULL;
        for(int i = 0; i<list->id_list.length; i++){
            // Initialize the global value
            value_t var;
            var.value = NULL;
            var.address = LLVMAddGlobal(module, type, list->id_list.ids[i]);
            current_value.value = list->value_list.values[i];

            // Globals must be initialized instead of stored 
            if(current_value.value)
                LLVMSetInitializer(var.address, cast(current_value, type, false).value);
            insert_value(symbol_table, list->id_list.ids[i], var);
        }
    }
}
void create_scope(){
    // Create a new symbol table using the previous one
    symbol_table = create_table(symbol_table);
}

void finish_scope(){
    // Destroy the old symbol table and revert to the previous one
    table_t *prev = symbol_table->nexttable;
    destroy_table(symbol_table);
    symbol_table = prev;
}

void create_if(value_t condition){
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));

    // Create a new conditional struct
    cond_stack_t *new_cond = calloc(1, sizeof(cond_stack_t));
    new_cond->eliminate_done = false;
    new_cond->prev = curr_cond;
    curr_cond = new_cond;

    // Make sure that the block has not terminated
    if(!FINISHED){
        new_cond->if_branch = LLVMAppendBasicBlock(fn, "");
        new_cond->else_branch = LLVMAppendBasicBlock(fn, "");
        if(LLVMTypeOf(condition.value) != LLVMInt1Type())
            condition = truthy(condition);
        LLVMBuildCondBr(builder, condition.value, curr_cond->if_branch, curr_cond->else_branch);
        LLVMPositionBuilderAtEnd(builder, curr_cond->if_branch);
    }
}

void create_else(){
    // Check if the conditional is blank (terminated)
    if(!curr_cond->else_branch) return;

    // Create a done block for after the if/else branches
    LLVMBasicBlockRef prev_else =  curr_cond->else_branch;
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
    curr_cond->else_branch = LLVMAppendBasicBlock(fn, "");

    // Check if the current block is finished before building a jump
    if(!FINISHED)
        LLVMBuildBr(builder, curr_cond->else_branch);
    else 
        curr_cond->eliminate_done = true;

    // Move the instruction builder to the else branch
    LLVMPositionBuilderAtEnd(builder, prev_else);
}

void finish_if(){
    // Eliminate the done block if it is not needed
    if(FINISHED && curr_cond->eliminate_done)
        LLVMDeleteBasicBlock(curr_cond->else_branch);
    else{
        // Either jump to done or just move the instruction builder there
        if(!FINISHED)
            LLVMBuildBr(builder, curr_cond->else_branch);
        if(curr_cond->else_branch){
            LLVMPositionBuilderAtEnd(builder, curr_cond->else_branch);
        }
    }  

    // Remove the conditional from the stack
    cond_stack_t *temp = curr_cond;
    curr_cond = curr_cond->prev;
    free(temp);
}

void create_while(char* label){
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));

    // Create a new conditional struct
    loop_stack_t *new_loop = calloc(1, sizeof(loop_stack_t));
    new_loop->prev = curr_loop;
    new_loop->label = label;
    curr_loop = new_loop;

    // Make sure that the block has not terminated
    if(!FINISHED){
        new_loop->condition = LLVMAppendBasicBlock(fn, "");
        new_loop->body = LLVMAppendBasicBlock(fn, "");
        new_loop->end = LLVMAppendBasicBlock(fn, "");
        LLVMBuildBr(builder, curr_loop->condition);
        LLVMPositionBuilderAtEnd(builder, curr_loop->condition);
        
    }
}

void create_while_condition(value_t condition){
    // Make sure that the block has not terminated
    if(FINISHED) return;

    // Get a truthy value for the condition
    if(LLVMTypeOf(condition.value) != LLVMInt1Type())
        condition = truthy(condition);

    // Build the conditional jmp and move to the loop's body
    LLVMBuildCondBr(builder, condition.value, curr_loop->body, curr_loop->end);
    LLVMPositionBuilderAtEnd(builder, curr_loop->body);
}

void finish_while(){
    // Make sure that the block has not terminated
    if(!FINISHED)
        LLVMBuildBr(builder, curr_loop->condition);
    
    // Move the instruction builder to the end of the loop
    if(curr_loop->end)
        LLVMPositionBuilderAtEnd(builder, curr_loop->end);

    // Remove the loop from the stack
    loop_stack_t *temp = curr_loop;
    curr_loop = curr_loop->prev;
    if(temp->label) free(temp->label);
    free(temp);
}

void create_break_continue(char* label, bool is_break){
    if(FINISHED) {
        if(label) free(label);
        return;
    }

    if(!label) LLVMBuildBr(builder, is_break ? curr_loop->end : curr_loop->condition);
    else {
        loop_stack_t *current = curr_loop;
        while(current){
            if(current->label && strcmp(current->label, label) == 0){
                LLVMBuildBr(builder, is_break ? current->end : current->condition);
                free(label);
                return;
            }
            current = current->prev;
        }
    }
    
}

void create_return(value_t val){
    // Check if the block has been termianted
    if(FINISHED) return;

    // Check if there is an actual return value
    if(val.value){
        // Try to cast the return value to the function return type
        LLVMBasicBlockRef block = LLVMGetInsertBlock(builder);
        LLVMValueRef fn = LLVMGetBasicBlockParent(block);
        LLVMTypeRef fn_type = LLVMGetElementType(LLVMTypeOf(fn));
        LLVMTypeRef return_type = LLVMGetReturnType(fn_type);
        LLVMBuildRet(builder, cast(val, return_type, false).value);
    } else{
        LLVMBuildRetVoid(builder);
    }
}

value_t cast(value_t val, LLVMTypeRef type, bool is_explicit){
    // Check if the block has been terminated or if the cast is unnecessary
    value_t return_val = val;
    LLVMTypeRef value_type = LLVMTypeOf(val.value);
    if(FINISHED || value_type == type) return return_val;

    // Get type information
    LLVMTypeKind type_kind = LLVMGetTypeKind(type);
    LLVMTypeKind value_type_kind = LLVMGetTypeKind(value_type);
    bool error = false;

    // Most of this is just casework...
    // General Rule: Downcasts are only allowed if is_explicit is true
    if(value_type_kind == LLVMStructTypeKind 
        || value_type_kind == LLVMArrayTypeKind
        || type_kind == LLVMStructTypeKind 
        || type_kind == LLVMArrayTypeKind) error = true;
    else if(type_kind == LLVMIntegerTypeKind){
        int width = LLVMGetIntTypeWidth(type);
        if(width == 1)
            return_val = truthy(val);
        else if(value_type_kind == LLVMIntegerTypeKind){
            int value_width = LLVMGetIntTypeWidth(value_type);
            if(value_width < width)
                if(value_width == 1)
                    return_val.value = LLVMBuildZExt(builder, val.value, type, "");
                else
                    return_val.value = LLVMBuildSExt(builder, val.value, type, "");
            else if(is_explicit)
                return_val.value = LLVMBuildTrunc(builder, val.value, type, "");
            else error = true;
        } else if(value_type_kind == LLVMFloatTypeKind){
            if(width > 32 || is_explicit)
                return_val.value = LLVMBuildFPToSI(builder, val.value, type, "");
            else error = true;
        
        } else if(value_type_kind == LLVMDoubleTypeKind){
            if(is_explicit)
                return_val.value = LLVMBuildFPToSI(builder, val.value, type, "");
            else error = true;
            
        } else if(value_type_kind == LLVMPointerTypeKind){
            if(is_explicit)
                return_val.value = LLVMBuildPtrToInt(builder, val.value, type, "");
            else error = true;
        } else error = true;
    } else if(type_kind == LLVMFloatTypeKind ){
        if(value_type_kind == LLVMIntegerTypeKind){
            int value_width = LLVMGetIntTypeWidth(value_type);
            if(value_width < 32 || is_explicit){
                if(value_width == 1)
                    return_val.value = LLVMBuildUIToFP(builder, val.value, type, "");
                else
                    return_val.value = LLVMBuildSIToFP(builder, val.value, type, "");
            }
            else error = true;
        } else if(value_type_kind == LLVMDoubleTypeKind){
            return_val.value = LLVMBuildFPTrunc(builder, val.value, type, "");
        } else error = true;
    } else if(type_kind == LLVMDoubleTypeKind){
        if(value_type_kind == LLVMIntegerTypeKind){
            int value_width = LLVMGetIntTypeWidth(value_type);
            if(value_width == 1)
                return_val.value = LLVMBuildUIToFP(builder, val.value, type, "");
            else
                return_val.value = LLVMBuildSIToFP(builder, val.value, type, "");
        }
        else if(value_type_kind == LLVMFloatTypeKind)
            return_val.value = LLVMBuildFPExt(builder, val.value, type, "");
        else error = true;
    } else if(type_kind == LLVMPointerTypeKind){
        if(value_type_kind == LLVMIntegerTypeKind)
            return_val.value = LLVMBuildIntToPtr(builder, val.value, type, "");
        else if(value_type_kind == LLVMPointerTypeKind){
            if(is_explicit)
                return_val.value = LLVMBuildPointerCast(builder, val.value, type, "");
            else error = true;
        } else error = true;
    }

    if(error){
        printf("invalid cast\n");
        exit(0);
    }
    if(val.address)
        return_val.address = LLVMBuildPointerCast(builder, val.address, LLVMPointerType(type, 0), "");
    return return_val;
}

value_t truthy(value_t val){
    // Check if the block has been terminated
    value_t return_val;
    return_val.value = NULL;
    return_val.address = NULL;
    if(FINISHED) return return_val;
    
    // Ensure the value is not a structure or array 
    LLVMTypeKind value_type_kind = LLVMGetTypeKind(LLVMTypeOf(val.value));
    if(value_type_kind == LLVMStructTypeKind || value_type_kind == LLVMArrayTypeKind){
        printf("Cannot check truthiness of an aggregate type\n");
        exit(0);
    } else if(value_type_kind == LLVMIntegerTypeKind){
        // Check if the integer is 0
        return_val.value = LLVMBuildICmp(builder, LLVMIntNE, val.value, LLVMConstInt(LLVMTypeOf(val.value), 0, false), "");
    } else if(value_type_kind == LLVMFloatTypeKind || value_type_kind == LLVMDoubleTypeKind){
        // Check if the floating-point number is NaN
        return_val.value = LLVMBuildFCmp(builder, LLVMRealORD, val.value, val.value, "");
    } else if(value_type_kind == LLVMPointerTypeKind){
        // Check if the pointer is null
        LLVMValueRef temp = LLVMBuildPtrToInt(builder, val.value, LLVMInt64Type(), "");
        return_val.value = LLVMBuildICmp(builder, LLVMIntNE, temp, LLVMConstInt(LLVMInt64Type(), 0, false), "");
    }
    return return_val;
}

LLVMTypeRef implicit_cast(value_t lhs, value_t rhs, value_t *l_cast, value_t *r_cast){
    // Check if the block has been terminated of if the types already equal
    *l_cast = lhs;
    *r_cast = rhs;
    LLVMTypeRef left_type = LLVMTypeOf(lhs.value);
    LLVMTypeRef right_type = LLVMTypeOf(rhs.value);
    if(FINISHED ||  left_type == right_type) return left_type;

    // Iterate down the implicit type hierarchy
    LLVMTypeKind left_kind = LLVMGetTypeKind(LLVMTypeOf(lhs.value));
    LLVMTypeKind right_kind = LLVMGetTypeKind(LLVMTypeOf(rhs.value));
    if(left_kind == LLVMDoubleTypeKind)
        *r_cast = cast(rhs, left_type, false);
    else if(right_kind == LLVMDoubleTypeKind)
        *l_cast = cast(lhs, right_type, false);
    
    else if(left_type == LLVMInt64Type())
        *r_cast = cast(rhs, left_type, false);
    else if(right_type == LLVMInt64Type())
        *l_cast = cast(lhs, right_type, false);
    
    else if(left_kind == LLVMFloatTypeKind)
        *r_cast = cast(rhs, left_type, false);
    else if(right_kind == LLVMFloatTypeKind)
        *l_cast = cast(lhs, right_type, false);
    
    else if(left_type == LLVMInt32Type())
        *r_cast = cast(rhs, left_type, false);
    else if(right_type == LLVMInt32Type())
        *l_cast = cast(lhs, right_type, false);
    
    else if(left_type == LLVMInt16Type())
        *r_cast = cast(rhs, left_type, false);
    else if(right_type == LLVMInt16Type())
        *l_cast = cast(lhs, right_type, false);
    
    else if(left_type == LLVMInt8Type())
        *r_cast = cast(rhs, left_type, false);
    else if(right_type == LLVMInt8Type())
        *l_cast = cast(lhs, right_type, false);
    
    else if(left_type == LLVMInt1Type())
        *r_cast = cast(rhs, left_type, false);
    else if(right_type == LLVMInt1Type())
        *l_cast = cast(lhs, right_type, false);

    // Return the cast type (assuming success)
    return LLVMTypeOf(l_cast->value);
}

value_t create_int_constant(int64_t val){
    value_t return_val;
    return_val.address = NULL;
    // Create an integer constant of the appropriate type
    if(val == 0 || val == 1)
        return_val.value = LLVMConstInt(LLVMInt1Type(), val, false);
    else if(val >= INT8_MIN && val <= UINT8_MAX)
        return_val.value = LLVMConstInt(LLVMInt8Type(), val, false);
    else if(val >= INT16_MIN && val <= UINT16_MAX)
        return_val.value = LLVMConstInt(LLVMInt16Type(), val, false);
    else if(val >= INT32_MIN && val <= UINT64_MAX)
        return_val.value = LLVMConstInt(LLVMInt32Type(), val, false);
    else if(val >= INT32_MIN && val <= UINT64_MAX)
        return_val.value = LLVMConstInt(LLVMInt64Type(), val, false);
    return return_val;
}

value_t create_fp_constant(double val){
    value_t return_val;
    return_val.address = NULL;
    // Create an LLVMConstant floating-point number
    return_val.value = LLVMConstReal(LLVMDoubleType(), val);
    return return_val;
}

value_t create_string_constant(char* str){
    value_t return_val;
    return_val.address = NULL;
    // Create a global String
    return_val.value = LLVMBuildGlobalStringPtr(builder, str, "");
    free(str);
    return return_val;
}

value_t get_identifier(char* id){
    // Try to find the identifier in the table
    value_t val = get_value(symbol_table, id);
    if(val.address == NULL && val.value == NULL){
        printf("Couldn't find identifier %s\n", id);
        exit(0);
    }

    // If the value only has an address (variables), load in the actual value
    if(val.address && val.value == NULL && !FINISHED){
        val.value = LLVMBuildLoad2(builder, LLVMGetElementType(LLVMTypeOf(val.address)), val.address, "");
    }
    return val;
}

value_t create_assignment(value_t left, value_t right){
    if(!left.address){
        printf("Cannot assign to this value!\n");
        exit(0);
    }
    if(!FINISHED){
        LLVMBuildStore(builder, cast(right, LLVMGetElementType(LLVMTypeOf(left.address)), false).value, left.address);
    }
    return right;
}

value_t create_call(value_t function, parse_list_t value_list){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;
    LLVMTypeRef function_pointer_type = LLVMTypeOf(function.value);
    LLVMTypeRef function_type = LLVMGetElementType(function_pointer_type);
     if(LLVMGetTypeKind(function_pointer_type) != LLVMPointerTypeKind
        || LLVMGetTypeKind(function_type) != LLVMFunctionTypeKind){
        printf("Not a callable function!\n");
        exit(0);
    }
    int num_params = LLVMCountParamTypes(function_type);
    if((!LLVMIsFunctionVarArg(function_type) && value_list.length != num_params)
        || (LLVMIsFunctionVarArg(function_type) && value_list.length < num_params)){
        printf("Incorrect number of parameters!\n");
        exit(0);
    }
    LLVMValueRef* args = calloc(value_list.length, sizeof(LLVMValueRef));
    LLVMTypeRef* types = calloc(num_params, sizeof(LLVMTypeRef));
    LLVMGetParamTypes(function_type, types);
    value_t cur_arg;
    cur_arg.address = NULL;
    for(int i = 0; i<value_list.length; i++){
        cur_arg.value = value_list.values[i];
        if(i < num_params)
            args[i] = cast(cur_arg, types[i], false).value;
        else
            args[i] = cur_arg.value;
    }
    return_val.value = LLVMBuildCall(builder, function.value, args, value_list.length, "");
    free(args);
    free(types);
    return return_val;
}

value_t create_math_binop(value_t left, value_t right, operation_t op){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;
    value_t left_cast, right_cast;
    LLVMTypeRef cast = implicit_cast(left, right, &left_cast, &right_cast);
    LLVMTypeKind cast_kind = LLVMGetTypeKind(cast);
    if(cast_kind == LLVMIntegerTypeKind){
        if(op == OP_ADD)
            return_val.value = LLVMBuildAdd(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_SUB)
            return_val.value = LLVMBuildSub(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_MUL)
            return_val.value = LLVMBuildMul(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_DIV)
            return_val.value = LLVMBuildSDiv(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_MOD)
            return_val.value = LLVMBuildSRem(builder, left_cast.value, right_cast.value, "");
    }
    else if(cast_kind == LLVMFloatTypeKind || cast_kind == LLVMDoubleTypeKind ){
        if(op == OP_ADD)
            return_val.value = LLVMBuildFAdd(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_SUB)
            return_val.value = LLVMBuildFSub(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_MUL)
            return_val.value = LLVMBuildFMul(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_DIV)
            return_val.value = LLVMBuildFDiv(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_MOD)
            return_val.value = LLVMBuildFRem(builder, left_cast.value, right_cast.value, "");
    }
    return return_val;
}
value_t create_math_negate(value_t val){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Use integer intructions for integer types 
    LLVMTypeKind kind = LLVMGetTypeKind(LLVMTypeOf(val.value));
    if(kind == LLVMIntegerTypeKind)
        return_val.value = LLVMBuildNeg(builder, val.value, "");
    // Use floating-point intructions for floating-point types 
    else if(kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind)
        return_val.value = LLVMBuildFNeg(builder, val.value, "");
    return return_val;
}

value_t create_bitwise_binop(value_t left, value_t right, operation_t op)
{
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Try to cast values up
    value_t left_cast, right_cast;
    LLVMTypeRef cast = implicit_cast(left, right, &left_cast, &right_cast);
    LLVMTypeKind cast_kind = LLVMGetTypeKind(cast);
    
    // Use integer intructions for integer types 
    if(cast_kind == LLVMIntegerTypeKind)
        if(op == OP_BIT_AND)
            return_val.value = LLVMBuildAnd(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_BIT_OR)
            return_val.value = LLVMBuildOr(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_BIT_XOR)
            return_val.value = LLVMBuildXor(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_LSHIFT)
            return_val.value = LLVMBuildShl(builder, left_cast.value, right_cast.value, "");
        else if(op == OP_RSHIFT)
            return_val.value = LLVMBuildAShr(builder, left_cast.value, right_cast.value, "");
    else{
        // Bitwise operations are only valid on integers
        printf("Bitwise operations only support integers\n");
        exit(0);
    }
    return return_val;
}

value_t create_bitwise_not(value_t val)
{
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Use integer intructions for integer types 
    LLVMTypeKind kind = LLVMGetTypeKind(LLVMTypeOf(val.value));
    if(kind == LLVMIntegerTypeKind)
        LLVMBuildNot(builder, val.value, "");
    else{
        // Bitwise operations are only valid on integers
        printf("Bitwise operations only support integers\n");
        exit(0);
    }
}

value_t create_boolean_binop(value_t left, value_t right, operation_t op){
    // Boolean Operation = Bitwise Operation with Truthy values
    if(op == OP_BOOL_AND)
        return create_bitwise_binop(truthy(left), truthy(right), OP_BIT_AND);
    else if(op == OP_BOOL_OR)
        return create_bitwise_binop(truthy(left), truthy(right), OP_BIT_OR);
}

value_t create_boolean_not(value_t val){
    // Boolean Operation = Bitwise Operation with Truthy values
    return create_boolean_not(truthy(val));
}

value_t create_comparison(value_t left, value_t right, operation_t op){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;
    value_t left_cast, right_cast;
    LLVMTypeRef cast = implicit_cast(left, right, &left_cast, &right_cast);
    LLVMTypeKind cast_kind = LLVMGetTypeKind(cast);
    
    // Use integer intructions for integer types 
    if(cast_kind == LLVMIntegerTypeKind){
        if(op == OP_LESS)
            return_val.value = LLVMBuildICmp(builder, LLVMIntSLT, left_cast.value, right_cast.value, "");
        else if(op == OP_LEQ)
            return_val.value = LLVMBuildICmp(builder,LLVMIntSLE, left_cast.value, right_cast.value, "");
        else if(op == OP_GREATER)
            return_val.value = LLVMBuildICmp(builder,LLVMIntSGT, left_cast.value, right_cast.value, "");
        else if(op == OP_GEQ)
            return_val.value = LLVMBuildICmp(builder, LLVMIntSGE, left_cast.value, right_cast.value, "");
        else if(op == OP_EQ)
            return_val.value = LLVMBuildICmp(builder, LLVMIntEQ, left_cast.value, right_cast.value, "");
        else if(op == OP_NEQ)
            return_val.value = LLVMBuildICmp(builder, LLVMIntNE, left_cast.value, right_cast.value, "");
    }
   
    // Use floating-point intructions for floating-point types 
    else if(cast_kind == LLVMFloatTypeKind || cast_kind == LLVMDoubleTypeKind )
        if(op == OP_LESS)
            return_val.value = LLVMBuildFCmp(builder, LLVMRealOLT, left_cast.value, right_cast.value, "");
        else if(op == OP_LEQ)
            return_val.value = LLVMBuildFCmp(builder,LLVMRealOLE, left_cast.value, right_cast.value, "");
        else if(op == OP_GREATER)
            return_val.value = LLVMBuildFCmp(builder,LLVMRealOGT, left_cast.value, right_cast.value, "");
        else if(op == OP_GEQ)
            return_val.value = LLVMBuildFCmp(builder, LLVMRealOGE, left_cast.value, right_cast.value, "");
        else if(op == OP_EQ)
            return_val.value = LLVMBuildFCmp(builder, LLVMRealOEQ, left_cast.value, right_cast.value, "");
        else if(op == OP_NEQ)
            return_val.value = LLVMBuildFCmp(builder, LLVMRealONE, left_cast.value, right_cast.value, "");
    return return_val;
}   

value_t create_deref(value_t val){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Only pointers can be dereferenced
    if(LLVMGetTypeKind(LLVMTypeOf(return_val.value)) != LLVMPointerTypeKind){
        printf("Cannot dereference\n");
        exit(0);
    }

    // address = previous value
    return_val.address = val.value;

    // Load the value stored at the pointer from memory
    return_val.value = LLVMBuildLoad(builder, val.value, "");
    return return_val;
}

value_t create_ref(value_t val){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Check if the address is valid
    if(!val.address){
        printf("Cannot reference\n");
        exit(0);
    }

    // value = previous address
    return_val.value = val.address;
    return return_val;
}

value_t create_index(value_t left, value_t right)
{
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Make sure the RHS is an integer
    // Make sure the LHS is a pointer or array
    LLVMTypeKind index_kind = LLVMGetTypeKind(LLVMTypeOf(right.value));
    LLVMTypeKind left_kind = LLVMGetTypeKind(LLVMTypeOf(left.value));
    if(index_kind != LLVMIntegerTypeKind 
        || (left_kind != LLVMArrayTypeKind && left_kind != LLVMPointerTypeKind)){
        printf("Invalid index\n");
        exit(0);
    }
    if(left_kind == LLVMArrayTypeKind){
        // Get the address of the element referenced by the index
        LLVMValueRef indices[2];
        indices[0] = LLVMConstInt(LLVMInt64Type(), 0, false);
        indices[1] = right.value;        
        return_val.address = LLVMBuildGEP(builder, left.address, indices, 2, "");
    } else{
        // Implement Pointer Arithmetic 
        // pointer + sizeof(*pointer * index)
        LLVMValueRef int_ptr = LLVMBuildPtrToInt(builder, left.value, LLVMInt64Type(), "");
        LLVMValueRef size = LLVMSizeOf(LLVMGetElementType(LLVMTypeOf(left.value)));
        size = LLVMBuildSExtOrBitCast(builder, size, LLVMTypeOf(right.value), "");
        LLVMValueRef product = LLVMBuildMul(builder, right.value, size, "");
        product = LLVMBuildSExt(builder, product, LLVMInt64Type(), "");
        LLVMValueRef change = LLVMBuildAdd(builder, int_ptr, product, "");
        return_val.address = LLVMBuildIntToPtr(builder, change, LLVMTypeOf(left.value), "");
    }

    // Load the value from the address
    return_val.value = LLVMBuildLoad(builder, return_val.address, "");
    return return_val;
}

value_t create_dot(value_t left, char* name){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Make sure the LHS of the dot operator is a struct
    LLVMTypeRef struct_type = LLVMTypeOf(left.value);
    if(LLVMGetTypeKind(struct_type) != LLVMStructTypeKind){
        printf("Can only dot structs\n");
        exit(0);
    }

    // Try to find the structure type in the aggregate list
    agg_list_t *current = structs;
    while(current){
        if(current->type == struct_type){
            // Try to find the field that matches the dot operator RHS
            for(int i = 0; i<current->components.id_list.length; i++){
                if(strcmp(name, current->components.id_list.ids[i]) == 0){
                    // If possible, store the address of the structure as well
                    if(left.address){
                        return_val.address = LLVMBuildStructGEP2(builder, struct_type, left.address, i, "");
                    }
                    return_val.value = LLVMBuildExtractValue(builder, left.value, i, "");
                }
            }
        }
        current = current->next;
    }
    free(name);
    return return_val;
}

value_t create_sizeof(LLVMTypeRef type){
    // Check if the block has been terminated
    value_t return_val;
    return_val.address = NULL;
    return_val.value = NULL;
    if(FINISHED) return return_val;

    // Return the size of the type
    return_val.value = LLVMSizeOf(type);
    return return_val;
}

void generate(char* llvm_dir, char* input_file, char* output_file, int scheck, int rcheck){
    // Keep track of LLVM errors
    char* LLVMError;

    // Create the LLVM Module and Instruction Builder
    module = LLVMModuleCreateWithName("");
    builder = LLVMCreateBuilder();

    // Create global scope
    create_scope();

    // Start tokenizing and parsing
    yyin = fopen(input_file, "r");
    if(!yyin){
        printf("Invalid source file!\n");
        exit(0);
    }
    yyparse();

    // End global scope
    finish_scope();

    // Verify that LLVM IR is correct
    LLVMVerifyModule(module, LLVMPrintMessageAction, &LLVMError);
    if(LLVMError){ 
        LLVMDisposeMessage(LLVMError);
        LLVMError = NULL;
    }
    
    // Write LLVM IR/Bitcode to the correct files
    if(rcheck){
        LLVMPrintModuleToFile(module, output_file, &LLVMError);
        if(LLVMError){ 
            LLVMDisposeMessage(LLVMError);
            LLVMError = NULL;
        }
    } else{
        // Create temporary bitcode
        LLVMWriteBitcodeToFile(module, "temp.bc");

        // Call llc command 
        strcat(llvm_dir, "/llc temp.bc ");
        if(scheck) {
            strcat(llvm_dir, " --filetype=asm -o ");
        } else {
            strcat(llvm_dir, " --filetype=obj -o ");
        }
        strcat(llvm_dir, output_file);
        printf("%s\n", llvm_dir);
        system(llvm_dir);
        
        // Delete temporary bitcode file
        remove("temp.bc");
    }
    
    // Cleanup
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
}