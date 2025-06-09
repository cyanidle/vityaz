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
    Die("%s:%zu:%zu => syntax error: %s (%s)", lex->source_name, line, col, msg, ctx);
}

TAPKI_NORETURN static void lex_err_char(Lexer* lex, const char* msg, char ch) {
    char ctx[] = {'"', ch, '"', 0};
    lex_err(lex, msg, ctx);
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

#define IDENT_BEGIN \
'_': \
    case 'a': case 'A': case 'b': case 'B': case 'c': case 'C': case 'd': case 'D': case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'h': case 'H': \
    case 'i': case 'I': case 'j': case 'J': case 'k': case 'K': case 'l': case 'L': case 'm': case 'M': case 'n': case 'N': case 'o': case 'O': case 'p': case 'P': \
    case 'q': case 'Q': case 'r': case 'R': case 's': case 'S': case 't': case 'T': case 'u': case 'U': case 'v': case 'V': case 'w': case 'W': case 'x': case 'X': \
    case 'y': case 'Y': case 'z': case 'Z'

#define IDENT_BODY \
    IDENT_BEGIN: case '0': case '1': case '2': case '3': case '4': case '5': \
    case '6': case '7': case '8': case '9'

static void lex_id(Lexer* lex, bool bracket) {
    Arena* arena = lex->arena;
    lex->ident = (Str){""};
    while(true) {
        char curr = *lex->cursor;
        switch (curr) {
        case '#':
            eat_comment(lex);
        case ' ':
        case '\0':
        case '\r':
            lex->cursor++;
        case '\n':
            return;
        case IDENT_BODY:
            *VecPush(&lex->ident) = curr;
            lex->cursor++;
            break;
        case '}':
            if (bracket) {
                lex->cursor++;
                return;
            }
        default: {
            lex_err_char(lex, "Unexpected character in identifier", curr);
        }
        }
    }
    if (lex->ident.size) {
        *VecPush(&lex->ident) = 0;
    }
}

// return if $ is a deref
static bool lex_dollar(Lexer* lex)
{
    Arena* arena = lex->arena;
    switch (lex->cursor[0]) {
    case '\r':
        lex->cursor++;
        if (TAPKI_UNLIKELY(lex->cursor[0] != '\n')) {
            lex_err_char(lex, "Expected '\\n', got: ", lex->cursor[0]);
        }
    case ' ':
    case '$':
    case ':':
    case '\n':
        *VecPush(&lex->ident) = lex->cursor[0];
        lex->cursor++;
        break;
    case '{':
        lex->cursor++;
        lex_id(lex, true);
        return true;
    case IDENT_BEGIN:
        lex_id(lex, false);
        return true;
    default:
        lex_err_char(lex, "Unexpected $-escaped character", lex->cursor[0]);
    }
    return false;
}

void lex_evalstring(Lexer* lex, EvalContext *ctx, bool is_path) {
    Arena* arena = lex->arena;
    while(true) {
        switch (lex->cursor[0]) {
        case '$':
            lex->cursor++;
            if (lex_dollar(lex)) {
                eval_deref(ctx, lex->ident.d, lex->ident.size);
            }
            break;
        case '\n':
        case '\0':
            return;
        case ' ': // terminates string only if path
            if (is_path)
                return;
        default:
            *VecPush(&lex->ident) = *lex->cursor++;
        }
    }
}

static Token do_lex_next(Lexer* lex, bool *indent, bool peek)
{
    *indent = *lex->cursor == ' ';
    eat_ws(lex);
again:
    if (TAPKI_UNLIKELY(!*lex->cursor)) {
        return TOK_EOF;
    }
    switch(*lex->cursor) {
    case '#':
        lex->cursor++;
        eat_comment(lex);
        goto again;
    case '\r':
        lex->cursor++;
    case '\n':
        lex->cursor++;
        return TOK_NEWLINE;
    case ':':
        lex->cursor++;
        return TOK_EXPLICIT;
    case '=':
        lex->cursor++;
        return TOK_EQ;
    case '|':
        lex->cursor++;
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
    case IDENT_BEGIN:
        if (peek)
            return TOK_ID;
        lex_id(lex, false);
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
        lex_err_char(lex, "Unexpected character", *lex->cursor);
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
