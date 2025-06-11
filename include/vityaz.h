#pragma once
#ifndef VITYAZ_H
#define VITYAZ_H
#include "tapki.h"
#include "lex.h"
#include "os.h"
#include "graph.h"

#endif //VITYAZ_H

/*
1. Special built-in variables ($in, $out).
2. Build-level variables from the build block.
3. Rule-level variables from the rule block (i.e. $command). (Note from the above discussion on expansion that these are expanded "late", and may make use of in-scope bindings like $in.)
4. File-level variables from the file that the build line was in.
5. Variables from the file that included that file using the subninja keyword.
*/
