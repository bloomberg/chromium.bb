/*
 * Copyright (C) 2012 University of Szeged. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NumberOfCores.h"

#if OS(WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace WTF {

int numberOfProcessorCores()
{
    static int s_numberOfCores = -1;

    if (s_numberOfCores > 0)
        return s_numberOfCores;

#if OS(WIN)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    s_numberOfCores = sysInfo.dwNumberOfProcessors;
#else
    const int defaultIfUnavailable = 1;
    long sysconfResult = sysconf(_SC_NPROCESSORS_ONLN);

    s_numberOfCores = sysconfResult < 0 ? defaultIfUnavailable : static_cast<int>(sysconfResult);
#endif
    return s_numberOfCores;
}

}
