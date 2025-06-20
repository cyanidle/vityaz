#include "vityaz.h"

MapImplement(CanonFiles, STRING_LESS, STRING_EQ);

File* file_get(Arena* arena, NinjaFile* nf, Str file)
{
    uint64_t slashes;
    CanonicalizePath(file.d, &file.size, &slashes);
    File** slot = CanonFiles_At(arena, &nf->files, file.d);
    if (!*slot) {
        File* created = *slot = VecPush(&nf->all_files);
        *created = (File){
            file.d, file.size, slashes, NULL
        };
    }
    return *slot;
}

void build_add_item(Arena* arena, NinjaFile *nf, Build* build, Str item, BuildItemType type)
{
    File* file = file_get(arena, nf, item);

    switch (type) {
    case OUTPUT_EXPLICIT:
    case OUTPUT_IMPLICIT: {
        if (TAPKI_UNLIKELY(file->producer && file->producer != build)) {
            size_t was_line = loc_line(file->producer->loc, NULL);
            syntax_err(build->loc,
                "Output ('%s') already produced by another 'build' @ %s:%zu",
                file->path, file->producer->loc.origin->name, was_line);
        }
        file->producer = build;
        *VecPush(&build->outputs) = file;
        if (type == OUTPUT_EXPLICIT) {
            build->explicit_outputs++;
        }
        break;
    }
    case INPUT_EXPLICIT:
    case INPUT_IMPLICIT: {
        file->used_by_build = true;
    }
    case INPUT_ORDER_ONLY: {
        *VecPush(&build->inputs) = file;
        break;
    }
    case INPUT_VALIDATOR: {
        if (!build->validators) {
            build->validators = ArenaAlloc(arena, sizeof(Files));
        }
        *VecPush(build->validators) = file;
        break;
    }
    }
}

