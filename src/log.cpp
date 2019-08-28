/** $lic$
 * Copyright (C) 2012-2013 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2012 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * This is an internal version, and is not under GPL. All rights reserved.
 * Only MIT and Stanford students and faculty are allowed to use this version.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2010) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
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
