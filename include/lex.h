﻿#pragma once
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

// static part of source location (shared by multiple parsed entities)
typedef struct SourceLocStatic {
    const char* data;
    const char* name;
} SourceLocStatic;

typedef struct SourceLoc {
    const SourceLocStatic* origin;
    size_t offset;
} SourceLoc;

struct Lexer;

SourceLoc loc_current(struct Lexer* lex);
size_t loc_line(SourceLoc loc, size_t* col);

typedef struct Lexer {
    const SourceLocStatic* source;
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

TAPKI_NORETURN TAPKI_FMT_ATTR(2, 3)
void syntax_err(SourceLoc loc, const char* fmt, ...);

#endif //VITYAZ_LEX_H
