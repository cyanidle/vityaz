#include "vityaz.h"
#include <string.h>

void syntax_err(Lexer* lex, const char* fmt, ...) {
    size_t line = 1, col = 0;
    for(const char* it = lex->begin; it != lex->cursor; ++it) {
        if (*it == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    va_list va;
    va_start(va, fmt);
    Str msg = TapkiVF(lex->arena, fmt, va);
    va_end(va);
    Die("%s:%zu (col %zu) => syntax error: %s", lex->source_name, line, col, msg.d);
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
            break;
        default:
            return;
        case '\t':
            syntax_err(lex, "Tabs not allowed");
        }
    }
}

#define IDENT_BEGIN '_': \
    case 'a': case 'A': case 'b': case 'B': case 'c': case 'C': case 'd': case 'D': \
    case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'h': case 'H': \
    case 'i': case 'I': case 'j': case 'J': case 'k': case 'K': case 'l': case 'L': \
    case 'm': case 'M': case 'n': case 'N': case 'o': case 'O': case 'p': case 'P': \
    case 'q': case 'Q': case 'r': case 'R': case 's': case 'S': case 't': case 'T': \
    case 'u': case 'U': case 'v': case 'V': case 'w': case 'W': case 'x': case 'X': \
    case 'y': case 'Y': case 'z': case 'Z'

#define IDENT_BODY \
    IDENT_BEGIN: case '0': case '1': case '2': case '3': case '4': case '5': \
    case '6': case '7': case '8': case '9'

static void lex_id(Lexer* lex, bool bracket) {
    Arena* arena = lex->arena;
    lex->id = (Str){""};
    while(true) {
        char curr = *lex->cursor;
        switch (curr) {
        case '#':
            eat_comment(lex);
            break;
        case ' ':
        case '\0':
        case '\r':
            lex->cursor++;
        case '\n':
            goto done;
        case IDENT_BODY:
            *VecPush(&lex->id) = curr;
            lex->cursor++;
            break;
        case '}':
            if (bracket) {
                lex->cursor++;
                goto done;
            }
        default: {
            syntax_err(lex, "Unexpected character in identifier: '%c'", curr);
        }
        }
    }
done:
    if (lex->id.size) {
        *VecPush(&lex->id) = 0;
    }
    VecShrink(&lex->id);
}

// return if deref
static bool lex_dollar(Lexer* lex)
{
    Arena* arena = lex->arena;
    switch (lex->cursor[0]) {
    case '\r':
        lex->cursor++;
    case ' ':
    case '$':
    case ':':
    case '\n':
        *VecPush(&lex->id) = lex->cursor[0];
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
        syntax_err(lex, "Unexpected $-escaped character: '%c'", lex->cursor[0]);
    }
    return false;
}

static void eval_add_part(Arena* arena, Eval* ctx, const char* str, size_t len, bool is_var) {
    *VecPush(&ctx->parts) = StrCopy(str, len).d;
    *VecPush(&ctx->is_var) = is_var;
}

static void lex_evalstring(Lexer* lex, Eval *ctx, bool is_path, bool persist) {
    lex->id = (Str){0};
    eat_ws(lex);
    Arena* eval_arena = persist ? lex->arena : lex->eval_arena;
    while(true) {
        switch (lex->cursor[0]) {
        case '$':
            if (lex->id.size) {
                eval_add_part(eval_arena, ctx, lex->id.d, lex->id.size, false);
                lex->id.size = 0;
            }
            lex->cursor++;
            if (lex_dollar(lex)) {
                eval_add_part(eval_arena, ctx, lex->id.d, lex->id.size, true);
            }
            break;
        case '\n':
        case '\0':
            goto done;
        case ':':
        case ' ': // terminates string only if path
            if (is_path)
                goto done;
        default:
            *TapkiVecPush(lex->arena, &lex->id) = *lex->cursor++;
        }
    }
done:
    if (lex->id.size) {
        eval_add_part(eval_arena, ctx, lex->id.d, lex->id.size, false);
        lex->id.size = 0;
    }
}

void lex_path(Lexer* lexer, Eval* ctx) {
    return lex_evalstring(lexer, ctx, true, false);
}

void lex_rhs(Lexer* lexer, Eval* ctx, bool persist) {
    return lex_evalstring(lexer, ctx, false, persist);
}

static Token do_lex_next(Lexer* lex, bool peek)
{
again:
    lex->indented = *lex->cursor == ' ';
    eat_ws(lex);
    if (TAPKI_UNLIKELY(!*lex->cursor)) {
        return TOK_EOF;
    }
    switch(lex->cursor[0]) {
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
        return TOK_INPUTS;
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
        if (lex->indented) {
            return TOK_ID;
        }
        if (STRING_EQ(lex->id.d, "build")) {
            return TOK_BUILD;
        } else if (STRING_EQ(lex->id.d, "rule")) {
            return TOK_RULE;
        } else if (STRING_EQ(lex->id.d, "include")) {
            return TOK_INCLUDE;
        } else if (STRING_EQ(lex->id.d, "subninja")) {
            return TOK_SUBNINJA;
        } else if (STRING_EQ(lex->id.d, "default")) {
            return TOK_DEFAULT;
        } else if (STRING_EQ(lex->id.d, "pool")) {
            return TOK_POOL;
        } else {
            return TOK_ID;
        }
    default: {
        if (peek) return TOK_INVALID;
        syntax_err(lex, "Unexpected character: '%c'", *lex->cursor);
    }
    }
}

Token lex_peek(Lexer* lex)
{
    const char* was = lex->cursor;
    Token peek = do_lex_next(lex, true);
    lex->cursor = was;
    return peek;
}

Token lex_next(Lexer* lex)
{
    Token next = do_lex_next(lex, false);
    lex->last = lex->tok;
    lex->tok = next;
    return next;
}

const char* tok_print(Token tok)
{
    switch (tok) {
    case TOK_EOF: return "<eof>";
    case TOK_NEWLINE: return "<newline>";
    case TOK_EQ: return "'='";
    case TOK_ID: return "<identificator>";
    case TOK_INCLUDE: return "'include'";
    case TOK_DEFAULT: return "'default'";
    case TOK_SUBNINJA: return "'subninja'";
    case TOK_RULE: return "'rule'";
    case TOK_BUILD: return "'build'";
    case TOK_POOL: return "'pool'";
    case TOK_INPUTS: return "':'";
    case TOK_IMPLICIT: return "'|'";
    case TOK_ORDER_ONLY: return "'||'";
    case TOK_VALIDATOR: return "'|@'";
    case TOK_INVALID: return "<invalid>";
    }
    return "<invalid>";
}
