/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_FastMalloc_h
#define WTF_FastMalloc_h

#include "wtf/WTFExport.h"
#include <cstddef>

namespace WTF {

    // These functions crash safely if an allocation fails.
    WTF_EXPORT void* fastMalloc(size_t);
    WTF_EXPORT void* fastZeroedMalloc(size_t);
    WTF_EXPORT void* fastCalloc(size_t numElements, size_t elementSize);
    WTF_EXPORT void* fastRealloc(void*, size_t);
    WTF_EXPORT char* fastStrDup(const char*);

    WTF_EXPORT void fastFree(void*);

#ifndef NDEBUG
    WTF_EXPORT void fastMallocForbid();
    WTF_EXPORT void fastMallocAllow();
#endif

    WTF_EXPORT void releaseFastMallocFreeMemory();

    struct FastMallocStatistics {
        size_t reservedVMBytes;
        size_t committedVMBytes;
        size_t freeListBytes;
    };
    WTF_EXPORT FastMallocStatistics fastMallocStatistics();

    // This defines a type which holds an unsigned integer and is the same
    // size as the minimally aligned memory allocation.
    typedef unsigned long long AllocAlignmentInteger;

} // namespace WTF

using WTF::fastCalloc;
using WTF::fastFree;
using WTF::fastMalloc;
using WTF::fastRealloc;
using WTF::fastStrDup;
using WTF::fastZeroedMalloc;

#ifndef NDEBUG
using WTF::fastMallocForbid;
using WTF::fastMallocAllow;
#endif

#endif /* WTF_FastMalloc_h */
