#pragma once
#ifndef VITYAZ_PARSE_H
#define VITYAZ_PARSE_H
#include "tapki.h"
#include "lex.h"


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
} Rule;

typedef struct {
    uint32_t depth;
} Pool;

typedef enum {
    OUTPUT_EXPLICIT,
    OUTPUT_IMPLICIT,

    INPUT_EXPLICIT, //explicit
    INPUT_IMPLICIT, //implicit
    INPUT_ORDER_ONLY, //order-only
    INPUT_VALIDATOR, // propagate these build steps to top-level targets. no direct dep
} BuildItemType;

typedef struct {
    const char* path;
    BuildItemType type;
} BuildItem;

typedef Vec(BuildItem) BuildItems;

typedef struct {
    const Rule* rule;
    VarsScope scope;
    BuildItems items;
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
    StrVec defaults;
    RulesScope root_rules;
    VarsScope root_vars;
} NinjaFile;

extern Rule phony_rule;
extern Pool console_pool;
extern Pool default_pool;

NinjaFile* parse(Arena* arena, const char* file);


#endif //VITYAZ_PARSE_H
