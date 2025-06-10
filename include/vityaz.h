#pragma once
#ifndef VITYAZ_H
#define VITYAZ_H
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
    int32_t depth;
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
    Rule* rule;
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
