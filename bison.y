%{
#include <stdio.h>
#include "flex.l.h"
int yyerror(char *s) {
    printf("%s\n", s);
}
%}

// Include headers for union structures 
%code requires {
    #include "parse.h" 
    #include "generate.h"  
}

// Define all of the types that a rule can match to
%union {
    char *str;
    LLVMTypeRef type;
    int64_t int_literal;
    double fp_literal;
    type_id_list_t type_id_list;
    value_id_list_t value_id_list;
    parse_list_t parse_list;
    arg_def_t arg_def;
    value_t value;
}

// Define all of the tokens without union types 
%token FN STRUCT IF ELSE WHILE RETURN BREAK CONTINUE TYPEDEF DECL AS SIZEOF
%token L_PAREN R_PAREN L_SQUARE R_SQUARE L_CURLY R_CURLY 
%token COMMA SEMICOLON ASTERISK ELLIPSES ARROW COLON
%token ASSIGN ADD SUB DIV MOD 
%token BOOL_AND BOOL_OR BOOL_NOT
%token BIT_AND BIT_OR BIT_XOR BIT_NOT LSHIFT RSHIFT
%token BOOL I8 I16 I32 I64 F32 F64
%token LESS LEQ GREATER GEQ EQ NEQ DOT

// Define tokens that also have associated data in the union 
%token<str> ID
%token<str> STR_LITERAL
%token<int_literal> INT_LITERAL
%token<fp_literal> FP_LITERAL

// Define rules that have associated data in the union 
%type<str> label
%type<type_id_list> type_id_list;
%type<value_id_list> value_id_list;
%type<parse_list> type_list;
%type<parse_list> value_list;
%type<arg_def> arg_def;
%type<type> return_type;
%type<type> type;
%type<value> expression;
%type<value> constant;

// Precedence for if/then/else to avoid parsing conflict 
%precedence THEN
%precedence ELSE

// Define precedence/associativity for every operator 
// The lower a directive is, the more precedence it has 
%right ASSIGN
%left LESS LEQ GREATER GEQ 
%left EQ NEQ
%left BOOL_AND BOOL_OR
%left ADD SUB
%left ASTERISK DIV MOD
%left BIT_AND BIT_OR BIT_XOR LSHIFT RSHIFT
%precedence BIT_NOT BOOL_NOT NEG REF DEREF
%precedence DOT 
%precedence AS
%precedence L_SQUARE
%precedence L_PAREN

// Define the starting rule 
%start program
%%
program: 
    globals;

globals:
    %empty
    | globals global;

global: 
    function
    | global_declaration SEMICOLON
    | struct
    | typedef SEMICOLON;

global_declaration:
    DECL type value_id_list {create_declaration($2, &$3, false);};

function: 
    FN ID L_PAREN arg_def R_PAREN return_type SEMICOLON  {
        create_function($2, $6 , &$4, false);
    }
    | FN ID L_PAREN arg_def R_PAREN return_type {
        create_function($2, $6, &$4,  true);
    } statement { 
        finish_function();
    } 
    ;

return_type:
    %empty {$$ = LLVMVoidType();}
    | ARROW type {$$ = $2;};

struct:
    STRUCT ID SEMICOLON  {
        create_struct($2, NULL);
    }
    | STRUCT ID L_CURLY type_id_list R_CURLY {
        create_struct($2, &$4);
    };

typedef:
    TYPEDEF ID type {create_type($2, $3);};

statements:
    %empty
    | statements statement;

statement:
    L_CURLY {create_scope();} statements R_CURLY {finish_scope();}
    | local_declaration SEMICOLON;
    | conditional;
    | loop;
    | break SEMICOLON;
    | continue SEMICOLON;
    | return SEMICOLON;
    | expression SEMICOLON;

local_declaration: 
    DECL type value_id_list {create_declaration($2, &$3, true);};

conditional:
    if_statement  ELSE {create_else();} statement {finish_if();} 
    | if_statement %prec THEN {finish_if();} ;

if_statement:
    IF L_PAREN expression R_PAREN {create_if($3);} statement;

loop:
    label WHILE {create_while($1);} L_PAREN expression R_PAREN {create_while_condition($5); } statement {finish_while();};

label:
    %empty {$$ = NULL;}
    | ID COLON {$$ = $1;};

break:
    BREAK ID {create_break_continue($2, true);}
    | BREAK {create_break_continue(NULL, true);};

continue:
    CONTINUE ID {create_break_continue($2, false);}
    | CONTINUE {create_break_continue(NULL, false);};

return:
    RETURN expression {create_return($2);} |
    RETURN {
        value_t dummy;
        dummy.address = NULL;
        dummy.value = NULL;
        create_return(dummy);
    };

expression:
    expression ASSIGN expression {$$ = create_assignment($1, $3);}
    | expression ADD expression {$$ = create_math_binop($1, $3, OP_ADD);}
    | expression SUB expression { $$ = create_math_binop($1, $3, OP_SUB);}
    | expression ASTERISK expression {$$ = create_math_binop($1, $3, OP_MUL);}
    | expression DIV expression {$$ = create_math_binop($1, $3, OP_DIV);}
    | expression MOD expression {$$ = create_math_binop($1, $3, OP_MOD);}
    | SUB  expression %prec NEG { $$ = create_math_negate($2);}
    | expression BIT_AND expression {$$ = create_bitwise_binop($1, $3, OP_BIT_AND);}
    | expression BIT_OR expression {$$ = create_bitwise_binop($1, $3, OP_BIT_OR);}
    | expression BIT_XOR expression {$$ = create_bitwise_binop($1, $3, OP_BIT_XOR);}
    | BIT_NOT expression  {$$ = create_bitwise_not($2);}
    | expression LSHIFT expression {$$ = create_bitwise_binop($1, $3, OP_LSHIFT);}
    | expression RSHIFT expression {$$ = create_bitwise_binop($1, $3, OP_RSHIFT);}
    | expression BOOL_AND expression {$$ = create_boolean_binop($1, $3, OP_BOOL_AND);}
    | expression BOOL_OR expression {$$ = create_boolean_binop($1, $3, OP_BOOL_OR);}
    | BOOL_NOT expression { $$ = create_boolean_not($2);}
    | expression LESS expression {$$ = create_comparison($1, $3, OP_LESS);}
    | expression LEQ expression {$$ = create_comparison($1, $3, OP_LEQ);}
    | expression GREATER expression {$$ = create_comparison($1, $3, OP_GREATER);}
    | expression GEQ expression {$$ = create_comparison($1, $3, OP_GEQ);}
    | expression EQ expression {$$ = create_comparison($1, $3, OP_EQ);}
    | expression NEQ expression {$$ = create_comparison($1, $3, OP_NEQ);}
    | SIZEOF L_PAREN type R_PAREN  {$$ = create_sizeof($3);}
    | expression L_PAREN value_list R_PAREN{ $$ = create_call($1, $3); } 
    | expression L_PAREN  R_PAREN{ 
        parse_list_t temp; 
        initialize_parse_list(&temp);
        $$ = create_call($1, temp); 
    } 
    | expression AS type { $$ = cast($1,$3, true);}
    | expression DOT ID { $$ = create_dot($1,$3);}
    | expression L_SQUARE expression R_SQUARE { $$ = create_index($1, $3); } 
    | ASTERISK expression %prec DEREF {$$ = create_deref($2);}
    | BIT_AND expression %prec REF {$$ = create_ref($2);}
    | L_PAREN expression R_PAREN {$$ = $2;}
    | ID  {$$ = get_identifier($1);}
    | constant;

constant:
    INT_LITERAL {$$ = create_int_constant($1);}
    | FP_LITERAL {$$ = create_fp_constant($1);}
    | STR_LITERAL {$$ = create_string_constant($1);}

arg_def:
    %empty {create_arg_def(&$$, NULL, false);}
    | ELLIPSES {create_arg_def(&$$, NULL, true);}
    | type_id_list {create_arg_def(&$$, &$1, false);}
    | type_id_list COMMA ELLIPSES {create_arg_def(&$$, &$1, true);}
    ;

type_id_list:
    type ID { 
        initialize_type_id_list(&$$);
        insert_type_id_list(&$$, $1, $2); 
    }
    | type_id_list COMMA type ID { 
        $$ = $1; 
        insert_type_id_list(&$$, $3, $4); 
    };

type_list:
    type {
        initialize_parse_list(&$$);
        insert_parse_list(&$$, $1, PL_TYPE); 
    }
    | type_list COMMA type {
        $$ = $1;
        insert_parse_list(&$$, $3, PL_TYPE); 
    };

value_list:
    expression {
        initialize_parse_list(&$$);
        insert_parse_list(&$$, $1.value, PL_VALUE); 
    }
    | value_list COMMA expression {
        $$ = $1;
        insert_parse_list(&$$, $3.value, PL_VALUE); 
    };

value_id_list:
    ID {
        initialize_value_id_list(&$$);
        insert_value_id_list(&$$, NULL, $1);
    }
    | ID ASSIGN expression {
        initialize_value_id_list(&$$);
        insert_value_id_list(&$$, $3.value, $1);
    }
    | value_id_list COMMA ID {
        $$ = $1;
        insert_value_id_list(&$$, NULL, $3);
    } 
    | value_id_list COMMA ID ASSIGN expression {
        $$ = $1;
        insert_value_id_list(&$$, $5.value, $3);
    };

type:
    ID {$$ = get_type($1, true);}
    | BOOL      {$$ = LLVMInt1Type();}
    | I8      {$$ = LLVMInt8Type();}
    | I16   {$$ = LLVMInt16Type();}
    | I32   {$$ = LLVMInt32Type();}
    | I64   {$$ = LLVMInt64Type();}
    | F32   {$$ = LLVMFloatType();}
    | F64   {$$ = LLVMDoubleType();}
    | FN L_PAREN R_PAREN return_type {
        $$ = LLVMPointerType(LLVMFunctionType($4, NULL, 0, false), 0);
    } | FN L_PAREN type_list R_PAREN return_type {
        $$ = LLVMPointerType(LLVMFunctionType($5, $3.types, $3.length, false), 0);
    } | FN L_PAREN ELLIPSES R_PAREN return_type {
        $$ = LLVMPointerType(LLVMFunctionType($5, NULL, 0, true), 0);
    } | FN L_PAREN type_list COMMA ELLIPSES R_PAREN return_type {
        $$ = LLVMPointerType(LLVMFunctionType($7, $3.types, $3.length, true), 0);
    }
    | L_PAREN type R_PAREN  {$$ = $2;}
    | ASTERISK type {$$ = LLVMPointerType($2, 0);}
    | L_SQUARE INT_LITERAL R_SQUARE type { $$ = LLVMArrayType($4, $2); };
%%