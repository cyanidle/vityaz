#define TAPKI_IMPLEMENTATION
#include "vityaz.h"

int main(int argc, char** argv)
{
    Arena* arena = ArenaCreate(1024 * 20);
    Str change = {0};
    Str file = S("build.ninja");
    StrVec cli_targets = {0};
    bool dry_run = false;
    CLI cli[] = {
        {"-C,--cd", &change, .metavar="DIR", .help="change working directory before anything else"},
        {"-f,--file", &file, .metavar="FILE", .help="specify input build file [default=build.ninja]"},
        {"-n,--dry", &dry_run, .flag=true, .help="dry run (don't run commands but act like they succeeded)"},
        {"targets", &cli_targets, .many=true, .help="if targets are unspecified, builds the 'default' target (see ninja manual)."},
        {0},
    };
    int ret = 0;
    if ((ret = ParseCLI(cli, argc, argv)) != 0) {
        goto end;
    }
    if (change.size) {
        fprintf(stderr, "vityaz: Changing directory to: %s\n", change.d);
        fflush(stderr);
        OsChdir(change.d);
    }
    NinjaFile* nf = parse_file(arena, file.d);
    Builds targets = {0};
    if (cli_targets.size) {
        // todo: from cli
    } else if (nf->defaults.size) {
        targets = nf->defaults;
    } else {
        VecForEach(&nf->all, edge) {
            if (edge->outputs.size == 0 && edge_buildable(edge)) {
                *VecPush(&targets) = edge;
            }
        }
    }
end:
    ArenaFree(arena);
    return ret;
}
