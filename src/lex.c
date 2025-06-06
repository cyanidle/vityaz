#include "vityaz.h"

TAPKI_NORETURN static void lex_err(Lexer* lex, const char* msg, const char* ctx) {
    size_t line = 0, col = 0;
    for(const char* it = lex->begin; it != lex->cursor; ++it) {
        if (*it == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    Die("%s:%zu:%zu => error: %s (%s)", lex->source_name, line, col, msg, ctx);
}

static void eat_comment(Lexer* lex) {
    while(true) {
        switch(*lex->cursor) {
        case '\0':
        case '\n':
            return;
        default:
            lex->cursor++;
        }
    }
}

static void eat_ws(Lexer* lex) {
    while(true) {
        switch(*lex->cursor) {
        case ' ':
        case '\r':
            lex->cursor++;
        default:
            return;
        case '\t':
            lex_err(lex, "Tabs not allowed", "\\t");
        }
    }
}

static void lex_id(char first, Lexer* lex) {
    Arena* arena = lex->arena;
    lex->ident = (Str){0};
    *VecPush(&lex->ident) = first;

    // until space
    // check for valid id
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
//         return TOK_DEREF;
//     }

void lex_evalstring(Lexer* lex, void* ctx, EvalCallback cb, bool is_path) {
    Str* tmp = &lex->_temp;
    while(true) {
        switch (lex->cursor[0]) {
        case '\n':
        case '\0':
            return;

            break;
        case ' ':
        default:
            break;
        }
    }
}

#define IDENT_BEGIN \
case '_': \
case 'a': case 'A': case 'b': case 'B': case 'c': case 'C': case 'd': case 'D': case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'h': case 'H': \
case 'i': case 'I': case 'j': case 'J': case 'k': case 'K': case 'l': case 'L': case 'm': case 'M': case 'n': case 'N': case 'o': case 'O': case 'p': case 'P': \
case 'q': case 'Q': case 'r': case 'R': case 's': case 'S': case 't': case 'T': case 'u': case 'U': case 'v': case 'V': case 'w': case 'W': case 'x': case 'X': \
case 'y': case 'Y': case 'z': case 'Z':

#define IDENT_BODY \
case '0': case '1': case '2': case '3': case '4': case '5': \
//todo

static Token do_lex_next(Lexer* lex, bool *indent, bool peek)
{
    *indent = *lex->cursor == ' ';
    eat_ws(lex);
    char ch;
again:
    if (TAPKI_UNLIKELY(!*lex->cursor)) {
        return TOK_EOF;
    }
    ch = *lex->cursor++;
    switch(ch) {
    case '#':
        eat_comment(lex);
        goto again;
    case '\n':
        return TOK_NEWLINE;
    case ':':
        return TOK_EXPLICIT;
    case '=':
        return TOK_EQ;
    case '|':
        switch (lex->cursor[0]) {
        case '@':
            lex->cursor++;
            return TOK_VALIDATOR;
        case '|':
            lex->cursor++;
            return TOK_ORDER_ONLY;
        default:
            return TOK_IMPLICIT;
        }
    IDENT_BEGIN
        if (peek) return TOK_ID;
        lex_id(ch, lex);
        if (strcmp(lex->ident.d, "build")) {
            return TOK_BUILD;
        } else if (strcmp(lex->ident.d, "rule")) {
            return TOK_RULE;
        } else if (strcmp(lex->ident.d, "include")) {
            return TOK_INCLUDE;
        } else if (strcmp(lex->ident.d, "subninja")) {
            return TOK_SUBNINJA;
        } else if (strcmp(lex->ident.d, "default")) {
            return TOK_DEFAULT;
        } else if (strcmp(lex->ident.d, "pool")) {
            return TOK_POOL;
        } else {
            return TOK_ID;
        }
    default: {
        char ctx[] = {'"', ch, '"', 0};
        lex_err(lex, "Unexpected token", ctx);
    }
    }
}

Token lex_peek(Lexer* lex, bool *indent)
{
    const char* was = lex->cursor;
    Token peek = do_lex_next(lex, indent, true);
    lex->cursor = was;
    return peek;
}

Token lex_next(Lexer* lex, bool *indent)
{
    return do_lex_next(lex, indent, false);
}
