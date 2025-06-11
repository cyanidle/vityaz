#pragma once
#ifndef VITYAZ_OS_H
#define VITYAZ_OS_H
#include "tapki.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#endif

void OsChdir(const char* path);

typedef struct Job {
    Arena* arena;
    Str buf;
#ifdef _WIN32
    HANDLE child_;
    HANDLE pipe_;
    OVERLAPPED overlapped_;
    char overlapped_buf_[4 << 10];
    bool is_reading_;
#else
    enum {
        EXIT_OK,
        EXIT_FAIL = 1,
        EXIT_INTERRUPT = 130,
    };
    pid_t pid;
    int fd;
    char exit_status;
#endif
    bool use_console;
} Job;


#endif //VITYAZ_OS_H
