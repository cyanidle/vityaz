#pragma once
#ifndef VITYAZ_GRAPH_H
#define VITYAZ_GRAPH_H
#include "tapki.h"
#include "lex.h"

struct Rule;
struct VarsScope;

struct Edge;
typedef Vec(struct Edge*) Edges;

typedef struct Edge {
    SourceLoc loc;
    const struct Rule* rule;
    struct VarsScope* scope;
    Edges inputs; // explicit, implicit, order-only
    Edges outputs; // explicit, implicit
    const Edges* validators; // may be null
} Edge;

MapDeclare(EdgesByOutputs, char*, Edge*);

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

void add_build_item(Edge* edge, BuildItem item);

#endif //VITYAZ_GRAPH_H
