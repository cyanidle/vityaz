#pragma once
#ifndef VITYAZ_LEX_H
#define VITYAZ_LEX_H
#include "tapki.h"

typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_EQ,

    TOK_ID,

    TOK_INCLUDE,
    TOK_DEFAULT,
    TOK_SUBNINJA,

    TOK_RULE, // rule
    TOK_BUILD, // build
    TOK_POOL, // pool
    TOK_INPUTS, // :
    TOK_IMPLICIT, // |
    TOK_ORDER_ONLY, // ||
    TOK_VALIDATOR, // |@

    TOK_INVALID,
} Token;

const char* tok_print(Token tok);

typedef struct Lexer {
    const char* source_name;
    const char* begin;
    const char* cursor;
    Arena* arena;
    Arena* eval_arena;

    // should be copied by lexer user!
    Str id;
    bool indented;

    Token last;
    Token tok;
} Lexer;

Token lex_next(Lexer* lexer);
Token lex_peek(Lexer* lexer);

typedef struct Eval {
    union {
        Vec(const char*) parts;
        struct {
            const char* d; //not null-terminated!
            size_t len;
        } single;
    };
    Vec(bool) is_var; //if size==0, then whole Eval is one token (inside single.d)
} Eval;

void lex_path(Lexer* lexer, struct Eval* ctx);
void lex_rhs(Lexer* lexer, struct Eval* ctx, bool persist);

TAPKI_NORETURN TAPKI_FMT_ATTR(2, 3) void syntax_err(Lexer* lex, const char* fmt, ...);

#endif //VITYAZ_LEX_H
