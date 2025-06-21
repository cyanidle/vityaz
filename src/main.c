#define TAPKI_IMPLEMENTATION
#include "vityaz.h"

static void ParseNinjaCLI(Arena* arena, NinjaOpts* opts, int argc, char** argv) {
    opts->file = S("build.ninja");
    opts->error_limit = 1;
    CLI cli[] = {
        {"-C", &opts->change, .metavar="DIR",
            .help="change working directory before anything else"},
        {"-f", &opts->file, .metavar="FILE",
            .help="specify input build file [default=build.ninja]"},
        {"-t", &opts->tool, .metavar="TOOL",
            .help="run a subtool (use '-t list' to list subtools). Terminates toplevel options; further flags are passed to the tool"},
        {"-v,--verbose", &opts->verbose, .flag=true,
            .help="show all command lines while building"},
        {"-n", &opts->dry_run, .flag=true,
            .help="dry run (don't run commands but act like they succeeded)"},
        {"--quiet", &opts->quiet, .flag=true,
            .help="don't show progress status, just command output"},
        {"--version", &opts->version, .flag=true,
            .help="print ninja compatability version (\"" NINJA_COMPAT_VERSION "\")"},
        {"-w", &opts->warnings, .many=true, .metavar="FLAG",
            .help="adjust warnings (use '-w list' to list warnings)"},
        {"-j", &opts->jobs, .int64=true, .metavar="N",
            .help="run N jobs in parallel (0 means infinity)"},
        {"-k", &opts->error_limit, .int64=true, .metavar="N",
            .help="keep going until N jobs fail (0 means infinity) [default=1]"},
        {"targets", &opts->cli_targets, .many=true, .metavar="targets",
            .help="if targets are unspecified, builds the 'default' target (see ninja manual)."},
        {0},
        };
    if (ParseCLI(cli, argc, argv) != 0) {
        exit(1);
    }
    if (opts->version) {
        exit(0);
    }
}

typedef Vec(File*) Targets;

static Targets GetTargets(Arena* arena, NinjaOpts* opts, NinjaFile* nf) {
    Targets targets = {0};
    if (opts->cli_targets.size) {
        VecForEach(&opts->cli_targets, fname) {
            File* file = file_get(arena, nf, fname);
            if (!file->producer) {
                const char* closest = "<TODO>"; // todo: use Lihtenstein dist
                Die("unknown target: '%s', did you mean: '%s'?", file->path, closest);
            }
            *VecPush(&targets) = file;
        }
    } else if (nf->defaults.size) {
        VecForEach(&nf->defaults, _file) {
            File* file = *_file;
            if (!file->producer) {
                const char* closest = "<TODO>"; // todo: use Lihtenstein dist
                Die("'default' specified unknown target: '%s', did you mean: '%s'", file->path, closest);
            }
            *VecPush(&targets) = file;
        }
    } else {
        VecForEach(&nf->all_files, file) {
            if (!file->used_by_build && file->producer) {
                *VecPush(&targets) = file;
            }
        }
    }
    return targets;
}

int main(int argc, char** argv)
{
    SetDiePrefix("vityaz: error: ");
    Arena* arena = ArenaCreate(1024 * 4 * 10);
    NinjaOpts opts = {0};
    ParseNinjaCLI(arena, &opts, argc, argv);
    if (opts.change.size) {
        fprintf(stderr, "vityaz: Changing directory to: %s\n", opts.change.d);
        fflush(stderr);
        OsChdir(opts.change.d);
    }
    NinjaFile* nf = parse_file(arena, opts.file.d);
    Targets targets = GetTargets(arena, &opts, nf);
    // todo
    ArenaFree(arena);
}
