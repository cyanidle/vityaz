#include "vityaz.h"

TAPKI_NORETURN
    static void lex_err(Lexer* lex, const char* msg) {
    Die("Error: %s! %s:%zu", msg, lex->source_name, lex->line);
}

static void eat_ws(Lexer* lex) {
    while(true) {
        switch(*lex->cursor) {
        case ' ':
            lex->col++;
        case '\r':
            lex->cursor++;
        default:
            return;
        case '\t':
            lex_err(lex, "Tabs not allowed");
        }
    }
}


// case '$':
//     ch = *lex->cursor++;
//     switch (ch) {
//     case '\n':
//         lex->line++;
//         lex->col = 0;
//     case ' ':
//     case ':':
//     case '$':
//         *VecPush(&lex->current) = ch;
//         goto again;
//     case '{':
//         goto lex_deref;
//     default:
//         *VecPush(&lex->current) = ch;
//     lex_deref:
//         lex_id(lex);
//         return lex->last = TOK_DEREF;
//     }

static void lex_id(Lexer* lex) {
    // check for valid id
}

static void lex_rhs(Lexer* lex) {
    // until newline
}

Token lex_next(Lexer* lex)
{
    eat_ws(lex);
    char ch;
again:
    ch = *lex->cursor++;
    switch(ch) {
    case '\0':
        return lex->last = TOK_EOF;
    case '\n':
        lex->line++;
        lex->col = 0;
        return lex->last = TOK_NEWLINE;
    case ':':
        return lex->last = TOK_EXPLICIT;
    case '=':
        if (lex->last != TOK_ID) {
            lex_err(lex, "'=' expected to follow identificator");
        }
        goto rhs;
    case '|':
        // todo
    // case '$':
    //     ch = *lex->cursor++;
    default:
        if (lex->last == TOK_NEWLINE) {
            lex_id(lex);
            if (strcmp(lex->current.d, "build")) {
                return lex->last = TOK_BUILD;
            } else if (strcmp(lex->current.d, "rule")) {
                return lex->last = TOK_RULE;
            } else if (strcmp(lex->current.d, "include")) {
                return lex->last = TOK_INCLUDE;
            } else if (strcmp(lex->current.d, "subninja")) {
                return lex->last = TOK_SUBNINJA;
            } else if (strcmp(lex->current.d, "default")) {
                return lex->last = TOK_DEFAULT;
            } else {
                return lex->last = TOK_ID;
            }

        } else {
        rhs:
            lex_rhs(lex);
            return lex->last = TOK_RHS;
        }
    }
}
