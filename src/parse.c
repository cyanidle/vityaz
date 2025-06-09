#include "vityaz.h"
#include <string.h>

static void consume(Lexer* lex, Token tok) {
    bool indent;
    Token got;
    if ((got = lex_next(lex, &indent)) != tok) {
        syntax_err(lex, "'%s' expected, got: '%s'", tok_print(tok), tok_print(got));
    }
}

static void parse_rule(Lexer* lex) {
    bool indent;
    consume(lex, TOK_ID);
    Str name = lex->ident;
    consume(lex, TOK_NEWLINE);
    while(true) {
        // todo
    }
}

void parse(Arena* arena, const char* file)
{
    Str data = ReadFile(file);
    Lexer lex = {file, data.d, data.d, arena};
    bool ident;
    Token last = TOK_EOF;
    Token tok;
    while((tok = lex_next(&lex, &ident)) != TOK_EOF) {
        if (tok != TOK_NEWLINE) {
            printf("%s%s %s\n", ident ? "  " : "", tok_print(tok), tok == TOK_ID ? lex.ident.d : "");
        }
        switch (tok) {
        case TOK_EOF:
            return;
        case TOK_RULE: {
            parse_rule(&lex);
            break;
        }
        case TOK_ID: {
            if (last != TOK_NEWLINE) {
                syntax_err(&lex, "variable name must follow a newline, last was: %s", tok_print(last));
            }
            break;
        }
        case TOK_EQ: {
            if (last != TOK_ID) {
                syntax_err(&lex, "'=' must follow a newline, last was: %s", tok_print(last));
            }
            Eval eval = {arena};
            lex_rhs(&lex, &eval);
            printf("\"%s\"\n", eval.result.d);
            break;
        }
        case TOK_INCLUDE: {
            Eval eval = {arena};
            lex_path(&lex, &eval);
            parse(arena, eval.result.d);
            break;
        }
        }
        last = tok;
    }
}
