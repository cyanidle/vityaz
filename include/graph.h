#pragma once
#ifndef VITYAZ_GRAPH_H
#define VITYAZ_GRAPH_H
#include "tapki.h"
#include "parse.h"

struct Edge;

typedef Vec(struct Edge*) Edges;

typedef struct Edge {
    const Rule* rule;
    const VarsScope* scope;
    Edges inputs; // explicit, implicit, order-only
    Edges ouputs; // explicit, implicit
} Edge;

#endif //VITYAZ_GRAPH_H
