#include "vityaz.h"

MapImplement(CanonEdges, STRING_LESS, STRING_EQ);

static void CanonicalizePath(char* path, size_t* len, uint64_t* slash_bits);

static TaggedEdge tag(Edge* edge, bool implicit) {
    Assert(edge);
    return (TaggedEdge)edge | (implicit ? 1 : 0);
}

static Edge* detag(TaggedEdge tagged) {
    return (Edge*)(tagged & ~1);
}

Edge* edge_by_output(Arena* arena, NinjaFile *nf, Str path)
{
    uint64_t slash_bits = 0;
    CanonicalizePath(path.d, &path.size, &slash_bits);
    TaggedEdge* tagged = CanonEdges_At(arena, &nf->by_output, path.d);
    if (!*tagged) {
        *tagged = tag(VecPush(&nf->all), false);
    }
    Edge* edge = detag(*tagged);
    edge->slash_bits = slash_bits;
    return edge;
}

static void edge_add_input(Arena *arena, NinjaFile* nf, Edge *edge, Edge* item, BuildItemType type)
{
    switch (type) {
    case INPUT_EXPLICIT: {

    }
    case INPUT_IMPLICIT: {

    }
    case INPUT_ORDER_ONLY: {

    }
    case INPUT_VALIDATOR: {

    }
    case OUTPUT_IMPLICIT:
    case OUTPUT_EXPLICIT:
        return; // handled
    }
}

static void edge_check_output(Edge* edge, Edge* was, Str item) {
    if (was && was != edge && edge_buildable(was)) {
        size_t col;
        size_t line = loc_line(was->loc, &col);
        syntax_err(edge->loc,
            "Output ('%s') already produced by 'build' statement @ %s:%zu (col %zu)",
            item.d, was->loc.origin->name, line, col);
    }
}

void edge_add_item(Arena *arena, NinjaFile* nf, Edge *edge, Str item, BuildItemType type)
{
    TaggedEdge* slot = CanonEdges_At(arena, &nf->by_output, item.d);
    if (type <= OUTPUT_IMPLICIT) {
        edge_check_output(edge, detag(*slot), item);
        VecForEach(&edge->outputs, output) {

        }
        // register this node by this output
        *slot = tag(edge, type == OUTPUT_IMPLICIT);
        return;
    } else {
        if (!*slot) {
            *slot = (TaggedEdge)VecPush(&nf->all);
        }
        edge_add_input(arena, nf, edge, detag(*slot), type);
    }
}





// taken from ninja-build:

static bool IsPathSeparator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

static void CanonicalizePath(char* path, size_t* len, uint64_t* slash_bits) {
    // WARNING: this function is performance-critical; please benchmark
    // any changes you make to it.
    if (*len == 0) {
        return;
    }

    char* start = path;
    char* dst = start;
    char* dst_start = dst;
    const char* src = start;
    const char* end = start + *len;
    const char* src_next;

           // For absolute paths, skip the leading directory separator
           // as this one should never be removed from the result.
    if (IsPathSeparator(*src)) {
#ifdef _WIN32
        // Windows network path starts with //
        if (src + 2 <= end && IsPathSeparator(src[1])) {
            src += 2;
            dst += 2;
        } else {
            ++src;
            ++dst;
        }
#else
        ++src;
        ++dst;
#endif
        dst_start = dst;
    } else {
        // For relative paths, skip any leading ../ as these are quite common
        // to reference source files in build plans, and doing this here makes
        // the loop work below faster in general.
        while (src + 3 <= end && src[0] == '.' && src[1] == '.' &&
            IsPathSeparator(src[2])) {
            src += 3;
            dst += 3;
        }
    }

           // Loop over all components of the paths _except_ the last one, in
           // order to simplify the loop's code and make it faster.
    int component_count = 0;
    char* dst0 = dst;
    for (; src < end; src = src_next) {
#ifndef _WIN32
        // Use memchr() for faster lookups thanks to optimized C library
        // implementation. `hyperfine canon_perftest` shows a significant
        // difference (e,g, 484ms vs 437ms).
        const char* next_sep = (const char*)(memchr(src, '/', end - src));
        if (!next_sep) {
            // This is the last component, will be handled out of the loop.
            break;
        }
#else
      // Need to check for both '/' and '\\' so do not use memchr().
      // Cannot use strpbrk() because end[0] can be \0 or something else!
        const char* next_sep = src;
        while (next_sep != end && !IsPathSeparator(*next_sep))
            ++next_sep;
        if (next_sep == end) {
            // This is the last component, will be handled out of the loop.
            break;
        }
#endif
       // Position for next loop iteration.
        src_next = next_sep + 1;
        // Length of the component, excluding trailing directory.
        size_t component_len = next_sep - src;

        if (component_len <= 2) {
            if (component_len == 0) {
                continue;  // Ignore empty component, e.g. 'foo//bar' -> 'foo/bar'.
            }
            if (src[0] == '.') {
                if (component_len == 1) {
                    continue;  // Ignore '.' component, e.g. './foo' -> 'foo'.
                } else if (src[1] == '.') {
                    // Process the '..' component if found. Back up if possible.
                    if (component_count > 0) {
                        // Move back to start of previous component.
                        --component_count;
                        while (--dst > dst0 && !IsPathSeparator(dst[-1])) {
                            // nothing to do here, decrement happens before condition check.
                        }
                    } else {
                        dst[0] = '.';
                        dst[1] = '.';
                        dst[2] = src[2];
                        dst += 3;
                    }
                    continue;
                }
            }
        }
        ++component_count;

               // Copy or skip component, including trailing directory separator.
        if (dst != src) {
            memmove(dst, src, src_next - src);
        }
        dst += src_next - src;
    }

           // Handling the last component that does not have a trailing separator.
           // The logic here is _slightly_ different since there is no trailing
           // directory separator.
    size_t component_len = end - src;
    do {
        if (component_len == 0)
            break;  // Ignore empty component (e.g. 'foo//' -> 'foo/')
        if (src[0] == '.') {
            if (component_len == 1)
                break;  // Ignore trailing '.' (e.g. 'foo/.' -> 'foo/')
            if (component_len == 2 && src[1] == '.') {
                // Handle '..'. Back up if possible.
                if (component_count > 0) {
                    while (--dst > dst0 && !IsPathSeparator(dst[-1])) {
                        // nothing to do here, decrement happens before condition check.
                    }
                } else {
                    dst[0] = '.';
                    dst[1] = '.';
                    dst += 2;
                    // No separator to add here.
                }
                break;
            }
        }
        // Skip or copy last component, no trailing separator.
        if (dst != src) {
            memmove(dst, src, component_len);
        }
        dst += component_len;
    } while (0);

           // Remove trailing path separator if any, but keep the initial
           // path separator(s) if there was one (or two on Windows).
    if (dst > dst_start && IsPathSeparator(dst[-1]))
        dst--;

    if (dst == start) {
        // Handle special cases like "aa/.." -> "."
        *dst++ = '.';
    }

    *len = dst - start;  // dst points after the trailing char here.
#ifdef _WIN32
    uint64_t bits = 0;
    uint64_t bits_mask = 1;

    for (char* c = start; c < start + *len; ++c) {
        switch (*c) {
        case '\\':
            bits |= bits_mask;
            *c = '/';
        case '/':
            bits_mask <<= 1;
        }
    }
    *slash_bits = bits;
#else
    *slash_bits = 0;
#endif
}

// --- end of taken
