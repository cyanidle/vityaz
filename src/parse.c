#include "vityaz.h"
#include <string.h>

MapImplement(LazyVars, STRING_LESS, STRING_EQ);
MapImplement(Rules, STRING_LESS, STRING_EQ);
MapImplement(Pools, STRING_LESS, STRING_EQ);

static void consume(Lexer* lex, Token tok) {
    bool indent;
    if (TAPKI_UNLIKELY(lex_next(lex, &indent) != tok)) {
        syntax_err(lex, "'%s' expected, got: '%s'", tok_print(tok), tok_print(lex->tok));
    }
}

typedef struct {
    enum {
        PARSING_RULE,
        PARSING_BUILD,
        PARSING_POOL,
    } parsing;
    void* output;
} PCurrent;

static void current_check_clean(Lexer* lex, PCurrent* curr) {
    if (TAPKI_UNLIKELY(curr->output)) {
        char* msg = 0;
        switch (curr->parsing) {
        case PARSING_RULE: msg = "rule"; break;
        case PARSING_BUILD: msg = "build"; break;
        case PARSING_POOL: msg = "pool"; break;
        }
        syntax_err(lex, "Already parsing: %s", msg);
    }
}

static void add_current(Arena* arena, PCurrent* curr, Str* name, Eval* eval) {

}

static void commit_current(Arena* arena, PCurrent* curr, NinjaFile* out) {

}

NinjaFile parse(Arena* arena, const char* file)
{
    Str data = ReadFile(file);
    Lexer lex = {file, data.d, data.d, arena};
    bool indent;
    NinjaFile result = {0};
    PCurrent current = {0};
    while(lex_next(&lex, &indent) != TOK_EOF) {
        switch (lex.tok) {
        case TOK_NEWLINE:
            break;
        case TOK_POOL: {
            consume(&lex, TOK_ID);
            consume(&lex, TOK_NEWLINE);
            Str name = lex.ident;

            break;
        }
        case TOK_RULE: {
            consume(&lex, TOK_ID);
            consume(&lex, TOK_NEWLINE);
            Str name = lex.ident;
            current_check_clean(&lex, &current);
            Rule* rule = Rules_At(arena, &result.rules, name.d);
            if (rule->command.result.d) {
                syntax_err(&lex, "Rule already defined: %s", name.d);
            }
            current.output = rule;
            current.parsing = PARSING_RULE;
            printf("rule '%s'\n", name.d);
            break;
        }
        case TOK_ID: {
            if (lex.last != TOK_NEWLINE) {
                syntax_err(&lex, "variable name must follow a newline, last was: %s", tok_print(lex.last));
            }
            Str id = lex.ident;
            consume(&lex, TOK_EQ);
            Eval eval = {0};
            lex_rhs(&lex, &eval);
            if (indent) {
                add_current(arena, &current, &id, &eval);
            } else {
                commit_current(arena, &current, &result);
                // todo: set global var
            }
            printf("%s%s = '%s'\n", indent ? "  " : "", id.d, eval.result.d);
            break;
        }
        case TOK_INCLUDE: {
            Eval eval = {0};
            lex_path(&lex, &eval);
            Str path = eval_expand(arena, &eval);
            FrameF("include(%s)", path.d) {
                parse(arena, path.d);
            }
            break;
        }
        default: {
            syntax_err(&lex, "Unexpected token: %s", tok_print(lex.tok));
        }
        }
    }
    return result;
}
