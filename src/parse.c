#include "vityaz.h"
#include <string.h>

void eval_add_part(Arena* arena, Eval* ctx, const char* str, size_t len, bool is_var) {
    *VecPush(&ctx->parts) = StrCopy(str, len).d;
    *VecPush(&ctx->is_var) = is_var;
}

const char* deref_var(const VarsScope* scope, const char* name)
{
    while(scope) {
        Str* result = StrMap_Find(&scope->data, name);
        if (result) {
            return result->d;
        }
        scope = scope->prev;
    }
    Die("Could not deref variable: %s", name);
}

static Rule* deref_rule(const RulesScope* scope, const char* name)
{
    while(scope) {
        Rule* result = Rules_Find(&scope->data, name);
        if (result) {
            return result;
        }
        scope = scope->prev;
    }
    Die("Could not find rule: %s", name);
}

Str eval_expand(Arena* arena, Eval* ctx, const VarsScope* scope) {
    Str result = {0};
    for (size_t i = 0; i < ctx->parts.size; ++i) {
        bool is_var = ctx->is_var.d[i];
        const char* part = ctx->parts.d[i];
        StrAppend(&result, is_var ? deref_var(scope, part) : part);
    }
    VecShrink(&result);
    return result;
}

MapImplement(LazyVars, STRING_LESS, STRING_EQ);
MapImplement(Rules, STRING_LESS, STRING_EQ);
MapImplement(Pools, STRING_LESS, STRING_EQ);

static void consume(Lexer* lex, Token tok) {
    if (TAPKI_UNLIKELY(lex_next(lex) != tok)) {
        syntax_err(lex, "'%s' expected, got: '%s'", tok_print(tok), tok_print(lex->tok));
    }
}

typedef enum {
    PARSING_RULE,
    PARSING_BUILD,
    PARSING_POOL,
} ParsingMode;

typedef struct {
    ParsingMode mode;
    void* output;
} ParsingState;

static const char* parsing_print(ParsingState* state) {
    switch (state->mode) {
    case PARSING_RULE: return "rule";
    case PARSING_BUILD: return "build";
    case PARSING_POOL: return "pool";
    }
}

static void parsing_start_new(Lexer* lex, ParsingState* state, ParsingMode mode, void* output) {
    if (TAPKI_UNLIKELY(state->output)) {
        syntax_err(lex, "Already parsing: %s", parsing_print(state));
    }
    state->output = output;
    state->mode = mode;
}

static void parsing_add_var(Arena* arena, ParsingState* state, const char* name, Eval* eval, const VarsScope* scope) {
    switch (state->mode) {
    case PARSING_RULE: {
        Rule* rule = (Rule*)state->output;
        if (STRING_EQ(name, "command")) {
            rule->command = *eval;
        } else {
            *LazyVars_At(arena, &rule->vars, name) = *eval;
        }
        break;
    }
    case PARSING_BUILD: {
        Build* build = (Build*)state->output;
        *StrMap_At(&build->scope.data, name) = eval_expand(arena, eval, scope);
        break;
    }
    case PARSING_POOL: {
        Pool* pool = (Pool*)state->output;
        if (STRING_EQ(name, "depth")) {
            pool->depth = ToI32(eval_expand(arena, eval, scope).d);
        }
        break;
    }
    }
}

static void parsing_try_commit(Lexer* lex, ParsingState* state) {
    if (!state->output)
        return;
    switch (state->mode) {
    case PARSING_RULE: {
        Rule* rule = (Rule*)state->output;
        if (!rule->command.parts.d) {
            syntax_err(lex, "command not provided for rule");
        }
        break;
    }
    default: break;
    }
    state->output = NULL;
}

static void parse_build(Lexer* lex, Scope scope, NinjaFile* result, ParsingState* state)
{

}

static void do_parse(Arena* arena, Scope scope, NinjaFile* result, const char* source_name, const char* data)
{
    Lexer lex = {source_name, data, data, arena};
    ParsingState state = {0};
    while(lex_next(&lex) != TOK_EOF) {
        if (lex.tok == TOK_NEWLINE)
            continue;
        bool indent = lex.indented;
        if (!indent) {
            parsing_try_commit(&lex, &state);
        }
        switch (lex.tok) {
        case TOK_POOL: {
            consume(&lex, TOK_ID);
            consume(&lex, TOK_NEWLINE);
            Str name = lex.id;
            Pool* pool = Pools_At(arena, &result->pools, name.d);
            if (pool->depth) {
                syntax_err(&lex, "Pool already defined: %s", name.d);
            }
            parsing_start_new(&lex, &state, PARSING_POOL, pool);
            break;
        }
        case TOK_RULE: {
            consume(&lex, TOK_ID);
            consume(&lex, TOK_NEWLINE);
            Str name = lex.id;
            Rule* rule = Rules_At(arena, &scope.rules->data, name.d);
            if (rule->command.parts.d) {
                syntax_err(&lex, "Rule already defined: %s", name.d);
            }
            parsing_start_new(&lex, &state, PARSING_RULE, rule);
            printf("rule '%s'\n", name.d);
            break;
        }
        case TOK_DEFAULT: {
            // todo
            break;
        }
        case TOK_BUILD: {
            parse_build(&lex, scope, result, &state);
            break;
        }
        case TOK_ID: {
            if (lex.last != TOK_NEWLINE) {
                syntax_err(&lex, "variable name must follow a newline, last was: %s", tok_print(lex.last));
            }
            Str id = lex.id;
            consume(&lex, TOK_EQ);
            Eval eval = {0};
            lex_rhs(&lex, &eval);
            if (indent) {
                if (state.output) {
                    parsing_add_var(arena, &state, id.d, &eval, scope.vars);
                } else {
                    syntax_err(&lex, "Unexpected indentation");
                }
            } else {
                Str value = eval_expand(arena, &eval, scope.vars);
                *StrMap_At(&scope.vars->data, id.d) = value;
            }
            break;
        }
        case TOK_SUBNINJA:
        case TOK_INCLUDE: {
            Eval eval = {0};
            lex_path(&lex, &eval);
            Str path = eval_expand(arena, &eval, scope.vars);
            const char* action = lex.tok == TOK_INCLUDE ? "include" : "subninja";
            Scope nested_scope = scope;
            if (lex.tok == TOK_SUBNINJA) {
                nested_scope.vars = (VarsScope*)ArenaAlloc(arena, sizeof(VarsScope));
                nested_scope.vars->prev = scope.vars;
                nested_scope.rules = (RulesScope*)ArenaAlloc(arena, sizeof(RulesScope));
                nested_scope.rules->prev = scope.rules;
            }
            FrameF("%s %s", action, path.d) {
                printf("%s %s\n", action, path.d);
                Str next_data = ReadFile(path.d);
                do_parse(arena, nested_scope, result, path.d, next_data.d);
            }
            break;
        }
        default: {
            syntax_err(&lex, "Unexpected token: %s", tok_print(lex.tok));
        }
        }
    }
    parsing_try_commit(&lex, &state);
}

NinjaFile* parse(Arena* arena, const char* file)
{
    NinjaFile* result = ArenaAlloc(arena, sizeof(*result));
    *Rules_At(arena, &result->root_rules.data, "phony") = (Rule){0};
    Pools_At(arena, &result->pools, "console")->depth = 1;
    Str data = ReadFile(file);
    Scope scope = {&result->root_rules, &result->root_vars};
    do_parse(arena, scope, result, file, data.d);
    return result;
}
