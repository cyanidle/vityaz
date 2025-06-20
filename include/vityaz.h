#pragma once
#ifndef VITYAZ_H
#define VITYAZ_H
#include "tapki.h"
#include "lex.h"
#include "os.h"

typedef struct VarsScope {
    StrMap data;
    struct VarsScope* prev;
} VarsScope;

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

struct Build;

typedef struct File {
    const char* path;
    size_t path_len;
    uint64_t slash_bits;
    struct Build* producer;
} File;

typedef Vec(File) Files;

typedef struct Build {
    SourceLoc loc;
    const Rule* rule;
    VarsScope* scope;
    Files inputs; // explicit, implicit, order-only
    unsigned explicit_inputs;
    unsigned implicit_inputs;
    Files outputs; // explicit, implicit
    unsigned explicit_outputs;
    const Files* validators; // may be null
} Build;

typedef uintptr_t TaggedEdge;

// tagged Edge*
MapDeclare(CanonFiles, char*, File*);

typedef struct {
    Pools pools;
    RulesScope root_rules;
    VarsScope root_vars;
    Files defaults;
    Vec(Files) all_files;
    Vec(Build) all_builds;
    CanonFiles by_output;
} NinjaFile;

extern Rule depfile_rule; // marks edge as depfile-discovered
extern Rule phony_rule;
extern Pool console_pool;
extern Pool default_pool;

typedef enum {
    OUTPUT_EXPLICIT,
    OUTPUT_IMPLICIT,

    INPUT_EXPLICIT, //explicit
    INPUT_IMPLICIT, //implicit
    INPUT_ORDER_ONLY, //order-only
    INPUT_VALIDATOR, // propagate these build steps to top-level targets. no direct dep
} BuildItemType;

/// parse.c
const char* deref_var(const VarsScope* scope, const char* name);
Str eval_expand(Arena* arena, Eval* ctx, const VarsScope* scope);
NinjaFile* parse_file(Arena* arena, const char* file);
/// --------

/// graph.c
void edge_add_item(Arena* arena, NinjaFile *nf, Edge* edge, Str item, BuildItemType type);
static inline bool edge_buildable(Edge* edge) {
    return edge->rule && edge->rule != &phony_rule;
}
/// --------

#endif //VITYAZ_H

/* Deref rules:
1. Special built-in variables ($in, $out).
2. Build-level variables from the build block.
3. Rule-level variables from the rule block (i.e. $command). (Note from the above discussion on expansion that these are expanded "late", and may make use of in-scope bindings like $in.)
4. File-level variables from the file that the build line was in.
5. Variables from the file that included that file using the subninja keyword.
*/
