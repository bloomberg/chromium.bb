/*
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef OS_COMPATIBILITY_H
#define OS_COMPATIBILITY_H

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#else
static inline int
backtrace(void **buffer, int size)
{
	return 0;
}
#endif

int
os_socketpair_cloexec(int domain, int type, int protocol, int *sv);

int
os_epoll_create_cloexec(void);

int
os_create_anonymous_file(off_t size);

#ifndef HAVE_STRCHRNUL
char *
strchrnul(const char *s, int c);
#endif

#endif /* OS_COMPATIBILITY_H */
