/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "StackBounds.h"

#if OS(DARWIN)

#include <mach/task.h>
#include <mach/thread_act.h>
#include <pthread.h>

#elif OS(WINDOWS)

#include <windows.h>

#elif OS(SOLARIS)

#include <thread.h>

#elif OS(UNIX)

#include <pthread.h>
#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#endif

namespace WTF {

// Bug 26276 - Need a mechanism to determine stack extent
//
// These platforms should now be working correctly:
//     DARWIN, UNIX
// These platforms are not:
//     WINDOWS, SOLARIS, OPENBSD
//
// FIXME: remove this! - this code unsafely guesses at stack sizes!
#if OS(WINDOWS) || OS(SOLARIS) || OS(OPENBSD)
// Based on the current limit used by the JSC parser, guess the stack size.
static const ptrdiff_t estimatedStackSize = 128 * sizeof(void*) * 1024;
// This method assumes the stack is growing downwards.
static void* estimateStackBound(void* origin)
{
    return static_cast<char*>(origin) - estimatedStackSize;
}
#endif

#if OS(DARWIN)

void StackBounds::initialize()
{
    pthread_t thread = pthread_self();
    m_origin = pthread_get_stackaddr_np(thread);
    m_bound = static_cast<char*>(m_origin) - pthread_get_stacksize_np(thread);
}

#elif OS(SOLARIS)

void StackBounds::initialize()
{
    stack_t s;
    thr_stksegment(&s);
    m_origin = s.ss_sp;
    m_bound = estimateStackBound(m_origin);
}

#elif OS(OPENBSD)

void StackBounds::initialize()
{
    pthread_t thread = pthread_self();
    stack_t stack;
    pthread_stackseg_np(thread, &stack);
    m_origin = stack.ss_sp;
    m_bound = estimateStackBound(m_origin);
}

#elif OS(UNIX)

void StackBounds::initialize()
{
    void* stackBase = 0;
    size_t stackSize = 0;

    pthread_t thread = pthread_self();
    pthread_attr_t sattr;
    pthread_attr_init(&sattr);
#if HAVE(PTHREAD_NP_H) || OS(NETBSD)
    // e.g. on FreeBSD 5.4, neundorf@kde.org
    pthread_attr_get_np(thread, &sattr);
#else
    // FIXME: this function is non-portable; other POSIX systems may have different np alternatives
    pthread_getattr_np(thread, &sattr);
#endif
    int rc = pthread_attr_getstack(&sattr, &stackBase, &stackSize);
    (void)rc; // FIXME: Deal with error code somehow? Seems fatal.
    ASSERT(stackBase);
    pthread_attr_destroy(&sattr);
    m_bound = stackBase;
    m_origin = static_cast<char*>(stackBase) + stackSize;
}

#elif OS(WINDOWS)

void StackBounds::initialize()
{
#if CPU(X86) && COMPILER(MSVC)
    // offset 0x18 from the FS segment register gives a pointer to
    // the thread information block for the current thread
    NT_TIB* pTib;
    __asm {
        MOV EAX, FS:[18h]
        MOV pTib, EAX
    }
    m_origin = static_cast<void*>(pTib->StackBase);
#elif CPU(X86) && COMPILER(GCC)
    // offset 0x18 from the FS segment register gives a pointer to
    // the thread information block for the current thread
    NT_TIB* pTib;
    asm ( "movl %%fs:0x18, %0\n"
          : "=r" (pTib)
        );
    m_origin = static_cast<void*>(pTib->StackBase);
#elif CPU(X86_64)
    PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb());
    m_origin = reinterpret_cast<void*>(pTib->StackBase);
#else
#error Need a way to get the stack bounds on this platform (Windows)
#endif
    // Looks like we should be able to get pTib->StackLimit
    m_bound = estimateStackBound(m_origin);
}

#else
#error Need a way to get the stack bounds on this platform
#endif

} // namespace WTF
