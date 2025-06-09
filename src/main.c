#define TAPKI_IMPLEMENTATION
#include "vityaz.h"

int main(int argc, char** argv)
{
    Arena* arena = ArenaCreate(1024 * 20);
    Str change = S(".");
    Str file = S("build.ninja");
    StrVec targets = {0};
    bool dry_run = false;
    CLI cli[] = {
        {"--change,-C", &change, .help="change working directory before anything else"},
        {"--file,-f", &file, .help="specify input build file [default=build.ninja]"},
        {"--dry-run,-n", &dry_run, .flag=true, .help="dry run (don't run commands but act like they succeeded)"},
        {"targets", &targets, .many=true, .help="if targets are unspecified, builds the 'default' target (see ninja manual)."},
        {0}
    };
    if (!targets.size) {
        VecAppend(&targets, S("default"));
    }
    int ret = 0;
    if ((ret = ParseCLI(cli, argc, argv)) != 0) {
        goto end;
    }
end:
    ArenaFree(arena);
    return ret;
}
