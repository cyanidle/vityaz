#pragma once
#ifndef VITYAZ_H
#define VITYAZ_H
#include "tapki.h"

typedef enum {
    TOK_ERROR = 0,

    TOK_EOF,
    TOK_NEWLINE,
    TOK_EQ,

    TOK_ID,
    TOK_RHS,

    TOK_INCLUDE,
    TOK_DEFAULT,
    TOK_SUBNINJA,

    TOK_RULE, // rule
    TOK_BUILD, // build
    TOK_POOL, // pool
    TOK_EXPLICIT, // :
    TOK_IMPLICIT, // |
    TOK_ORDER_ONLY, // ||
    TOK_VALIDATOR, // |@
} Token;

typedef struct Lexer {
    const char* source_name;
    const char* begin;
    const char* cursor;
    Arena* arena;
    Str ident; //should be copied by lexer user!
} Lexer;

Token lex_next(Lexer* lexer, bool* indent);
Token lex_peek(Lexer* lexer, bool* indent);

typedef struct {
    Arena* arena;
    Str result;
} EvalContext;

// TODO: move these to another .c (not lex!)
static void eval_part(EvalContext* ctx, const char* part, size_t len) {
    Arena* arena = ctx->arena;
    StrAppendF(&ctx->result, "%.*s", (int)len, part);
}
static void eval_deref(EvalContext* ctx, const char* var, size_t len) {
    Arena* arena = ctx->arena;
    StrAppendF(&ctx->result, "${%.*s}", (int)len, var);
}

void lex_evalstring(Lexer* lexer, EvalContext* ctx, bool is_path);

#endif //VITYAZ_H

/*

# Scopes will be a linked list

Evaluation and scoping

Top-level variable declarations are scoped to the file they occur in.
Rule declarations are also scoped to the file they occur in. (Available since Ninja 1.6)

The subninja keyword, used to include another .ninja file, introduces a new scope. The included subninja file may use the variables and rules from the parent file, and shadow their values for the file’s scope, but it won’t affect values of the variables in the parent.
To include another .ninja file in the current scope, much like a C #include statement, use include instead of subninja.

Variable declarations indented in a build block are scoped to the build block. The full lookup order for a variable expanded in a build block (or the rule is uses) is:

    1. Special built-in variables ($in, $out).
    2. Build-level variables from the build block.
    3. Rule-level variables from the rule block (i.e. $command). (Note from the above discussion on expansion that these are expanded "late", and may make use of in-scope bindings like $in.)
    4. File-level variables from the file that the build line was in.
    5. Variables from the file that included that file using the subninja keyword.
*/
