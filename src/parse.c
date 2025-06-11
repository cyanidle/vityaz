#include "vityaz.h"
#include <string.h>

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

static bool _phony_is_var = false;
static const char* _phony_command = "phony";
Rule phony_rule = {.command = {.parts = {&_phony_command, 1, 1}, .is_var={&_phony_is_var, 1, 1}}, .vars = {0}};
Pool console_pool = {1};
Pool default_pool = {0};

static Pool* lookup_pool(Arena* arena, Pools* pools, const char* name) {
    if (STRING_EQ(name, "console")) {
        return &console_pool;
    } else if (*name == 0) {
        return &default_pool;
    } else {
        return Pools_At(arena, pools, name);
    }
}

static const Rule* lookup_rule(const RulesScope* scope, const char* name)
{
    if (STRING_EQ(name, "phony")) {
        return &phony_rule;
    }
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
    if (!ctx->is_var.size) {
        return StrCopy(ctx->single.d, ctx->single.len);
    }
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
        syntax_err(lex, "%s expected, got: %s", tok_print(tok), tok_print(lex->tok));
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
            FrameF("Parsing pool depth") {
                pool->depth = ToU32(eval_expand(arena, eval, scope).d);
            }
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

static Str parse_path(Lexer* lex, VarsScope* scope) {
    Eval eval = {0};
    lex_path(lex, &eval);
    Str res = eval_expand(lex->arena, &eval, scope);
    ArenaClear(lex->eval_arena);
    return res;
}

static Build* parse_build(Lexer* lex, Scope scope, NinjaFile* result, ParsingState* state)
{
    Arena* arena = lex->arena;
    Build* build = VecPush(&result->builds);
    build->scope.prev = scope.vars;
    {
        BuildItemType type = OUTPUT_EXPLICIT;
        while(true) {
            Token peek = lex_peek(lex);
            switch (peek) {
            case TOK_INPUTS:
                lex_next(lex);
                goto inputs;
            case TOK_IMPLICIT:
                lex_next(lex);
                type = OUTPUT_IMPLICIT;
                break;
            case TOK_EOF:
            case TOK_NEWLINE:
                syntax_err(lex, "Unexpected EOF or newline inside 'build' statement");
            default:
                break;
            }
            Str path = parse_path(lex, scope.vars);
            *VecPush(&build->items) = (BuildItem){path.d, type};
        }
    }
inputs:
    consume(lex, TOK_ID);
    build->rule = lookup_rule(scope.rules, lex->id.d);
    {
        BuildItemType type = INPUT_EXPLICIT;
        while(true) {
            Token peek = lex_peek(lex);
            switch (peek) {
            case TOK_IMPLICIT:
                lex_next(lex);
                type = INPUT_IMPLICIT;
                break;
            case TOK_ORDER_ONLY:
                lex_next(lex);
                type = INPUT_ORDER_ONLY;
                break;
            case TOK_VALIDATOR:
                lex_next(lex);
                type = INPUT_VALIDATOR;
                break;
            case TOK_EOF:
            case TOK_NEWLINE:
                goto done;
            default:
                break;
            }
            Str path = parse_path(lex, scope.vars);
            *VecPush(&build->items) = (BuildItem){path.d, type};
        }
    }
done:
    parsing_start_new(lex, state, PARSING_BUILD, build);
    return build;
}

static void do_parse(Arena* arena, Scope scope, NinjaFile* result, const char* source_name, const char* data)
{
    Lexer lex = {source_name, data, data, arena};
    lex.eval_arena = ArenaCreate(2048);
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
            Pool* pool = lookup_pool(arena, &result->pools, name.d);
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
            if (TAPKI_UNLIKELY(STRING_EQ("phony", name.d))) {
                syntax_err(&lex, "Cannot redefine 'phony' rule");
            }
            Rule* rule = Rules_At(arena, &scope.rules->data, name.d);
            if (TAPKI_UNLIKELY(rule->command.parts.d)) {
                syntax_err(&lex, "Rule already defined: %s", name.d);
            }
            parsing_start_new(&lex, &state, PARSING_RULE, rule);
            break;
        }
        case TOK_DEFAULT: {
            while(true) {
                Token peek = lex_peek(&lex);
                if (peek == TOK_EOF || peek == TOK_NEWLINE) {
                    break;
                }
                *VecPush(&result->defaults) = parse_path(&lex, scope.vars);
            }
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
            bool persistent_eval = state.mode == PARSING_RULE;
            lex_rhs(&lex, &eval, persistent_eval);
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
            ArenaClear(lex.eval_arena);
            break;
        }
        case TOK_SUBNINJA: {
            Str path = parse_path(&lex, scope.vars);
            Scope nested_scope = scope;
            nested_scope.vars = (VarsScope*)ArenaAlloc(arena, sizeof(VarsScope));
            nested_scope.vars->prev = scope.vars;
            nested_scope.rules = (RulesScope*)ArenaAlloc(arena, sizeof(RulesScope));
            nested_scope.rules->prev = scope.rules;
            FrameF("subninja %s", path.d) {
                do_parse(arena, nested_scope, result, path.d, ReadFile(path.d).d);
            }
            break;
        }
        case TOK_INCLUDE: {
            Str path = parse_path(&lex, scope.vars);
            FrameF("include %s", path.d) {
                do_parse(arena, scope, result, path.d, ReadFile(path.d).d);
            }
            break;
        }
        default: {
            syntax_err(&lex, "Unexpected token: %s", tok_print(lex.tok));
        }
        }
    }
    parsing_try_commit(&lex, &state);
    ArenaFree(lex.eval_arena);
}

NinjaFile* parse(Arena* arena, const char* file)
{
    NinjaFile* result = ArenaAlloc(arena, sizeof(*result));
    Str data = ReadFile(file);
    Scope scope = {&result->root_rules, &result->root_vars};
    do_parse(arena, scope, result, file, data.d);
    return result;
}
