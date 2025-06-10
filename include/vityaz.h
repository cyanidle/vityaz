#pragma once
#ifndef VITYAZ_H
#define VITYAZ_H
#include "tapki.h"

/// --- Lexer

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
    TOK_EXPLICIT, // :
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

    // should be copied by lexer user!
    Str id;
    bool indented;

    Token last;
    Token tok;
} Lexer;

Token lex_next(Lexer* lexer);
Token lex_peek(Lexer* lexer);

struct Eval;

void lex_path(Lexer* lexer, struct Eval* ctx);
void lex_rhs(Lexer* lexer, struct Eval* ctx);

TAPKI_NORETURN TAPKI_FMT_ATTR(2, 3) void syntax_err(Lexer* lex, const char* fmt, ...);

/// -- Parser

typedef struct Eval {
    Vec(const char*) parts;
    Vec(bool) is_var;
} Eval;

typedef struct VarsScope {
    StrMap data;
    struct VarsScope* prev;
} VarsScope;

const char* deref_var(const VarsScope* scope, const char* name);
void eval_add_part(Arena* arena, Eval* ctx, const char* str, size_t len, bool is_var);
Str eval_expand(Arena* arena, Eval* ctx, const VarsScope* scope);

MapDeclare(LazyVars, char*, Eval);

typedef struct {
    Eval command;
    LazyVars vars;
} Rule;

typedef struct {
    int32_t depth;
} Pool;

typedef struct {
    Rule* rule;
    VarsScope scope;
} Build;

MapDeclare(Rules, char*, Rule);

typedef struct RulesScope {
    Rules data;
    struct RulesScope* prev;
} RulesScope;

MapDeclare(Pools, char*, Pool);
typedef Vec(Build) Builds;

typedef struct {
    RulesScope* rules;
    VarsScope* vars;
} Scope;

typedef struct {
    Pools pools;
    Builds builds;
    RulesScope root_rules;
    VarsScope root_vars;
} NinjaFile;

NinjaFile* parse(Arena* arena, const char* file);

#endif //VITYAZ_H

/*
# Scopes will be a linked list
Evaluation and scoping

Top-level variable declarations are scoped to the file they occur in.
Rule declarations are also scoped to the file they occur in. (Available since Ninja 1.6)

The subninja keyword, used to include another .ninja file, introduces a new scope.
The included subninja file may use the variables and rules from the parent file, and shadow their values for the file’s scope, but it won’t affect values of the variables in the parent.
To include another .ninja file in the current scope, much like a C #include statement, use include instead of subninja.

Variable declarations indented in a build block are scoped to the build block. The full lookup order for a variable expanded in a build block (or the rule is uses) is:

    1. Special built-in variables ($in, $out).
    2. Build-level variables from the build block.
    3. Rule-level variables from the rule block (i.e. $command). (Note from the above discussion on expansion that these are expanded "late", and may make use of in-scope bindings like $in.)
    4. File-level variables from the file that the build line was in.
    5. Variables from the file that included that file using the subninja keyword.
*/

/*
By default, if no targets are specified on the command line,
Ninja will build every output that is not named as an input elsewhere.
You can override this behavior using a default target statement.
A default target statement causes Ninja to build only a given subset of output files if none are specified on the command line.

Default target statements begin with the default keyword,
and have the format default targets.
A default target statement must appear after the build statement
that declares the target as an output file.
They are cumulative, so multiple statements
may be used to extend the list of default targets.

*/
