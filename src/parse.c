#include "vityaz.h"
#include <string.h>

Rule phony_rule = {.command.single = {"!phony", strlen("!phony")}};
Pool console_pool = {1};
Pool default_pool = {0};

static Pool* lookup_pool(Arena* arena, Pools* pools, const char* name) {
    if (STRING_EQ(name, "console")) {
        return &console_pool;
    } else if (STRING_EQ(name, "")) {
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

Str eval_expand(Arena* arena, Eval* eval, const VarsScope* scope) {
    if (!eval->is_var.size) {
        return StrCopy(eval->single.d, eval->single.len);
    }
    Str result = {0};
    for (size_t i = 0; i < eval->parts.size; ++i) {
        bool is_var = eval->is_var.d[i];
        const char* part = eval->parts.d[i];
        // TODO: recursively expand variables
        //StrAppend(&result, is_var ? deref_var(scope, part) : part);
    }
    VecShrink(&result);
    return result;
}

MapImplement(EvalMap, STRING_LESS, STRING_EQ);
MapImplement(Rules, STRING_LESS, STRING_EQ);
MapImplement(Pools, STRING_LESS, STRING_EQ);

static void consume(Lexer* lex, Token tok) {
    if (TAPKI_UNLIKELY(lex_next(lex) != tok)) {
        syntax_err(loc_current(lex),
            "%s expected, got: %s",
            tok_print(tok), tok_print(lex->tok)
        );
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

static const char* parsing_print(ParsingMode mode) {
    switch (mode) {
    case PARSING_RULE: return "rule";
    case PARSING_BUILD: return "build";
    case PARSING_POOL: return "pool";
    }
}

static void parsing_start_new(Lexer* lex, ParsingState* state, ParsingMode mode, void* output) {
    if (TAPKI_UNLIKELY(state->output)) {
        syntax_err(loc_current(lex),
            "Cannot start parsing: '%s' => Already parsing: '%s'",
            parsing_print(mode), parsing_print(state->mode));
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
            *EvalMap_At(arena, &rule->vars.data, name) = *eval;
        }
        break;
    }
    case PARSING_BUILD: {
        Build* build = (Build*)state->output;
        if (!build->scope) {
            build->scope = (VarsScope*)ArenaAlloc(arena, sizeof(VarsScope));
            build->scope->fallback = scope;
        }
        *StrMap_At(&build->scope->data, name) = eval_expand(arena, eval, scope);
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
            syntax_err(loc_current(lex), "'command' not provided for rule");
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

static const char* print_item_type(BuildItemType type) {
    switch (type) {
    case OUTPUT_EXPLICIT: return "Explicit Outputs";
    case OUTPUT_IMPLICIT: return "Implicit Outputs (|)";
    case INPUT_EXPLICIT: return "Explicit Inputs (:)";
    case INPUT_IMPLICIT: return "Explicit Inputs (|)";
    case INPUT_ORDER_ONLY: return "Order-Only Inputs (||)";
    case INPUT_VALIDATOR: return "Validators (|@)";
    }
}

static BuildItemType advance_item_type(Lexer* lex, BuildItemType was, BuildItemType next) {
    if (TAPKI_UNLIKELY(next <= was)) {
        syntax_err(loc_current(lex),
            "'%s' cannot follow '%s' in 'build' statement",
            print_item_type(next), print_item_type(was));
    }
    return next;
}

// TODO: probably handles $<space>/$<nl>/$: incorrectly
static void parse_build(Lexer* lex, RulesScope* rules, VarsScope* vars, NinjaFile* result, ParsingState* state)
{
    Arena* arena = lex->arena;
    Build* build = VecPush(&result->all_builds);
    build->loc = loc_current(lex);
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
                type = advance_item_type(lex, type, OUTPUT_IMPLICIT);
                break;
            case TOK_EOF:
            case TOK_NEWLINE:
                syntax_err(loc_current(lex), "Unexpected EOF or newline inside 'build' statement");
            default:
                break;
            }
            Str path = parse_path(lex, vars);
            build_add_item(arena, result, build, &path, type);
        }
    }
inputs:
    consume(lex, TOK_ID);
    build->rule = lookup_rule(rules, lex->id.d);
    {
        BuildItemType type = INPUT_EXPLICIT;
        while(true) {
            Token peek = lex_peek(lex);
            switch (peek) {
            case TOK_IMPLICIT:
                lex_next(lex);
                type = advance_item_type(lex, type, INPUT_IMPLICIT);
                break;
            case TOK_ORDER_ONLY:
                lex_next(lex);
                type = advance_item_type(lex, type, INPUT_ORDER_ONLY);
                break;
            case TOK_VALIDATOR:
                lex_next(lex);
                type = advance_item_type(lex, type, INPUT_VALIDATOR);
                break;
            case TOK_EOF:
            case TOK_NEWLINE:
                goto done;
            default:
                break;
            }
            Str path = parse_path(lex, vars);
            build_add_item(arena, result, build, &path, type);
        }
    }
done:
    parsing_start_new(lex, state, PARSING_BUILD, build);
}

static void do_parse(
    Arena* arena, RulesScope* rules, VarsScope* vars,
    NinjaFile* nf, const char* source_name, const char* data)
{
    SourceLocStatic* source = ArenaAlloc(arena, sizeof(*source));
    source->data = data;
    source->name = source_name;
    Lexer lex = {source, data, arena};
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
            SourceLoc loc = loc_current(&lex);
            consume(&lex, TOK_ID);
            consume(&lex, TOK_NEWLINE);
            Str name = lex.id;
            Pool* pool = lookup_pool(arena, &nf->pools, name.d);
            pool->loc = loc;
            if (pool->depth) {
                syntax_err(loc_current(&lex), "Pool already defined: %s", name.d);
            }
            parsing_start_new(&lex, &state, PARSING_POOL, pool);
            break;
        }
        case TOK_RULE: {
            SourceLoc loc = loc_current(&lex);
            consume(&lex, TOK_ID);
            consume(&lex, TOK_NEWLINE);
            Str name = lex.id;
            if (TAPKI_UNLIKELY(STRING_EQ("phony", name.d))) {
                syntax_err(loc, "Cannot redefine 'phony' rule");
            }
            Rule* rule = Rules_At(arena, &rules->data, name.d);
            rule->vars.fallback = vars;
            rule->loc = loc;
            if (TAPKI_UNLIKELY(rule->command.parts.d)) {
                syntax_err(loc, "Rule already defined: %s", name.d);
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
                Str target = parse_path(&lex, vars);
                *VecPush(&nf->defaults) = file_get(arena, nf, &target);
            }
            break;
        }
        case TOK_BUILD: {
            parse_build(&lex, rules, vars, nf, &state);
            break;
        }
        case TOK_ID: {
            if (lex.last != TOK_NEWLINE) {
                syntax_err(loc_current(&lex),
                    "variable name must follow a newline, last was: %s",
                    tok_print(lex.last));
            }
            Str id = lex.id;
            consume(&lex, TOK_EQ);
            Eval eval = {0};
            // if parsing rule: its eval state should be allocated in persistent arena
            bool is_persistent_eval = state.mode == PARSING_RULE;
            lex_rhs(&lex, &eval, is_persistent_eval);
            if (indent) {
                if (state.output) {
                    parsing_add_var(arena, &state, id.d, &eval, vars);
                } else {
                    syntax_err(loc_current(&lex), "Unexpected indentation");
                }
            } else {
                Str value = eval_expand(arena, &eval, vars);
                // todo: add value to scope
            }
            ArenaClear(lex.eval_arena);
            break;
        }
        case TOK_SUBNINJA: {
            Str path = parse_path(&lex, vars);
            VarsScope* nested_vars = (VarsScope*)ArenaAlloc(arena, sizeof(VarsScope));
            nested_vars->fallback = vars;
            RulesScope* nested_rules = (RulesScope*)ArenaAlloc(arena, sizeof(RulesScope));
            nested_rules->prev = rules;
            FrameF("subninja %s", path.d) {
                do_parse(arena, nested_rules, nested_vars, nf, path.d, FileRead(path.d).d);
            }
            break;
        }
        case TOK_INCLUDE: {
            Str path = parse_path(&lex, vars);
            FrameF("include %s", path.d) {
                do_parse(arena, rules, vars, nf, path.d, FileRead(path.d).d);
            }
            break;
        }
        default: {
            syntax_err(loc_current(&lex), "Unexpected token: %s", tok_print(lex.tok));
        }
        }
    }
    parsing_try_commit(&lex, &state);
    ArenaFree(lex.eval_arena);
}


NinjaFile* parse_file(Arena* arena, const char* file)
{
    NinjaFile* result = ArenaAlloc(arena, sizeof(*result));
    Str data = FileRead(file);
    do_parse(arena, &result->root_rules, &result->root_vars, result, file, data.d);
    return result;
}
