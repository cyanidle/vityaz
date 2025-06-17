#include "os.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void OsDie(const char* msg)
{
    Die("%s: [Errno: %d]: %s", msg, errno, strerror(errno));
}

void OsChdir(const char* path)
{
    if (!SetCurrentDirectoryA(path)) {
        OsDie("Change directory");
    }
}