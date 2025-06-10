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

NinjaFile parse(Arena* arena, const char* file)
{
    Str data = ReadFile(file);
    Lexer lex = {file, data.d, data.d, arena};
    bool indent;
    NinjaFile result = {0};
    Rule* rule = 0;
    Pool* pool = 0;
    Build* build = 0;
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
            rule = Rules_At(arena, &result.rules, name.d);
            if (rule->command.result.d) {
                syntax_err(&lex, "Rule already defined: %s", name.d);
            }
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
            if (rule) {
                //Rules_At( id.d)
            } else if (build) {

            } else if (pool) {

            } else {

            }
            printf("%s%s = '%s'\n", indent ? "  " : "", id.d, eval.result.d);
            break;
        }
        case TOK_INCLUDE: {
            Eval eval = {0};
            lex_path(&lex, &eval);
            printf("include(%s)\n", eval.result.d);
            parse(arena, eval.result.d);
            break;
        }
        default: {
            syntax_err(&lex, "Unexpected token: %s", tok_print(lex.tok));
        }
        }
    }
    return result;
}
