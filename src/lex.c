#include "vityaz.h"

SourceLoc loc_current(Lexer* lex) {
    return (SourceLoc){lex->source, lex->cursor - lex->source->data};
}

size_t loc_line(SourceLoc loc, size_t* col)
{
    size_t _col;
    if (!col) col = &_col;
    size_t line = 1;
    *col = 0;
    const char* end = loc.origin->data + loc.offset;
    for(const char* it = loc.origin->data; it != end; ++it) {
        if (*it == '\n') {
            line++;
            *col = 0;
        } else {
            (*col)++;
        }
    }
    return line;
}

void syntax_err(SourceLoc loc, const char* fmt, ...) {
    size_t col;
    size_t line = loc_line(loc, &col);
    va_list va;
    va_start(va, fmt);
    Arena* temp = ArenaCreate(1024);
    Str msg = TapkiVF(temp, fmt, va);
    va_end(va);
    // TODO:
    // error msg
    // <snippet>
    //         ^ near here
    Die("%s:%zu => syntax error: %s", loc.origin->name, line, msg.d);
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
            syntax_err(loc_current(lex), "Tabs not allowed");
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

static void do_lex_id(Lexer* lex, bool variable, bool bracket) {
    Arena* arena = lex->arena;
    lex->id = (Str){"",0,0};
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
        case '"':
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
            if (variable) {
                lex->cursor++;
                goto done;
            }
            syntax_err(loc_current(lex), "Unexpected character in identifier: '%c'", curr);
        }
        }
    }
done:
    if (lex->id.size) {
        *VecPush(&lex->id) = 0;
    }
    VecShrink(&lex->id);
}

static void lex_id(Lexer* lex) {
    do_lex_id(lex, false, false);
}

static void lex_variable(Lexer* lex) {
    do_lex_id(lex, true, false);
}

static void lex_variable_in_braces(Lexer* lex) {
    lex->cursor++;
    do_lex_id(lex, true, true);
}

// return true if deref
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
        lex_variable_in_braces(lex);
        return true;
    case IDENT_BEGIN:
        lex_variable(lex);
        return true;
    default:
        syntax_err(loc_current(lex), "Unexpected $-escaped character: '%c'", lex->cursor[0]);
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
            lex->cursor++;
            Str was = lex->id;
            if (lex_dollar(lex)) {
                eval_add_part(eval_arena, ctx, was.d, was.size, false); // commit non-var
                eval_add_part(eval_arena, ctx, lex->id.d, lex->id.size, true);
                lex->id.size = 0;
            }
            break;
        case '\r': //TODO: assert next is NL
            lex->cursor++;
        case '\n':
        case '\0':
            goto done;
        case ':':
        case ' ': // terminates string only if path
            if (is_path)
                goto done;
        default:
            *TapkiVecPush(eval_arena, &lex->id) = *lex->cursor++;
        }
    }
done:
    if (lex->id.size) {
        if (ctx->is_var.size) {
            eval_add_part(eval_arena, ctx, lex->id.d, lex->id.size, false);
        } else {
            ctx->single.d = lex->id.d;
            ctx->single.len = lex->id.size;
        }
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
        lex_id(lex);
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
        syntax_err(loc_current(lex), "Unexpected character: '%c'", *lex->cursor);
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
