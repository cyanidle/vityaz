#pragma once
#ifndef VITYAZ_PARSE_H
#define VITYAZ_PARSE_H
#include "tapki.h"
#include "lex.h"
#include "graph.h"

typedef struct VarsScope {
    StrMap data;
    struct VarsScope* prev;
} VarsScope;

const char* deref_var(const VarsScope* scope, const char* name);
Str eval_expand(Arena* arena, Eval* ctx, const VarsScope* scope);

MapDeclare(LazyVars, char*, Eval);

typedef struct {
    Eval command;
    LazyVars vars;
    SourceLoc loc;
} Rule;

typedef struct {
    uint32_t depth;
    SourceLoc loc;
} Pool;

MapDeclare(Rules, char*, Rule);

typedef struct RulesScope {
    Rules data;
    struct RulesScope* prev;
} RulesScope;

MapDeclare(Pools, char*, Pool);

typedef struct {
    RulesScope* rules;
    VarsScope* vars;
} Scope;

typedef struct {
    Pools pools;
    RulesScope root_rules;
    VarsScope root_vars;
    Edges defaults;
    Vec(Edge) all;
    EdgesByOutputs by_output;
} NinjaFile;

extern Rule phony_rule;
extern Pool console_pool;
extern Pool default_pool;

NinjaFile* parse(Arena* arena, const char* file);


#endif //VITYAZ_PARSE_H
