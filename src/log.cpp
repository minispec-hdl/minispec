/** $lic$
 * Copyright (C) 2019-2022 by Daniel Sanchez
 *
 * This file is part of the Minispec compiler and toolset.
 *
 * Minispec is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * Minispec is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "log.h"
#include <stdlib.h>
#include <string.h>

const char* logHeader = "";

const char* logTypeNames[] = {"Harness", "Config", "Process", "Cache", "Mem", "Sched", "FSVirt", "TimeVirt"};

FILE* logFdOut = stdout;
FILE* logFdErr = stderr;

void InitLog(const char* header, const char* file) {
    logHeader = strdup(header);

    if (file) {
        FILE* fd = fopen(file, "a");
        if (fd == nullptr) {
            perror("fopen() failed");
            panic("Could not open logfile %s", file); //we can panic in InitLog (will dump to stderr)
        }
        logFdOut = fd;
        logFdErr = fd;
        //NOTE: We technically never close this fd, but always flush it
    }
}

// NOTE: These don't do anything b/c the compiler is single-threaded and I didn't want to bring in more zsim deps.
void __log_lock() {}
void __log_unlock() {}
