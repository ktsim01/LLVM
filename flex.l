%{
#include <llvm-c/Core.h>
#include <stdbool.h>
#include "bison.tab.h"
#include <stdlib.h>
#include "table.h"
%}
/* Configure Flex to automatically end on EOF */
%option noyywrap 

/* Create comment states */
%x S_COMMENT
%x M_COMMENT        
%%

    /* Define comment states (ignore everything while these are active) */
"/*" BEGIN(M_COMMENT);
<M_COMMENT>"*/" BEGIN(INITIAL);
<M_COMMENT>[\r\n] ;
<M_COMMENT>. ;
"//" BEGIN(S_COMMENT);
<S_COMMENT>[\r\n] BEGIN(INITIAL);
<S_COMMENT>. ;

    /* The rest of tokenization is fairly trivial */
"bool" return BOOL;
"i8" return I8;
"i16" return I16;
"i32" return I32;
"i64" return I64;
"f32" return F32;
"f64" return F64;
"struct" return STRUCT;
"if" return IF;
"else" return ELSE;
"while" return WHILE;
"break" return BREAK;
"continue" return CONTINUE;
"return" return RETURN;
"typedef" return TYPEDEF;
"as" return AS;
":" return COLON;
"*" return ASTERISK;
"[" return L_SQUARE;
"]" return R_SQUARE;
"(" return L_PAREN;
")" return R_PAREN;
"{" return L_CURLY;
"}" return R_CURLY;
"," return COMMA;
";" return SEMICOLON;
"..." return ELLIPSES;
"->" return ARROW;
"fn" return FN;
"decl" return DECL;
"=" return ASSIGN;
"+" return ADD;
"-" return SUB;
"/" return DIV;
"%" return MOD;
"&&" return BOOL_AND;
"||" return BOOL_OR;
"!" return BOOL_NOT;
"&" return BIT_AND;
"|" return BIT_OR;
"^" return BIT_XOR;
"~" return BIT_NOT;
"<<" return LSHIFT;
">>" return RSHIFT;
"<" return LESS;
"<=" return LEQ;
">" return GREATER;
">=" return GEQ;
"==" return EQ;
"!=" return NEQ;
"." return DOT;
"sizeof" return SIZEOF;
\"([^"]*)\" {
    yylval.str = translate_special_chars(yytext+1, yyleng-2);
    return STR_LITERAL;
}
"null" {
    yylval.int_literal = 0;
    return INT_LITERAL;
}
"false"  {
    yylval.int_literal = 0;
    return INT_LITERAL;
} 
"true"  {
    yylval.int_literal = 1;
    return INT_LITERAL;
}
[0-9]+ {
    sscanf(yytext, "%ld", &yylval.int_literal);
    return INT_LITERAL;
}
[0-9]*\.[0-9]+ {
    sscanf(yytext, "%lf", &yylval.fp_literal);
    return FP_LITERAL;
}
[a-zA-Z0-9_]+ {
    yylval.str = strdup(yytext); 
    return ID;
}

    /* Ignore other whitespace */ 
[ \t\r\n]+ ;
%%