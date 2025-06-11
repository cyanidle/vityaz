#include "os.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

static void OsDie(const char* msg) {
    Die("%s: [Errno: %d]: %s", msg, errno, strerror(errno));
}

void OsChdir(const char* path)
{
    if (chdir(path) == -1) {
        OsDie("Change directory");
    }
}
