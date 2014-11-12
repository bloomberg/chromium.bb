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

#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "test-runner.h"

int
count_open_fds(void)
{
	DIR *dir;
	struct dirent *ent;
	int count = 0;

	dir = opendir("/proc/self/fd");
	assert(dir && "opening /proc/self/fd failed.");

	errno = 0;
	while ((ent = readdir(dir))) {
		const char *s = ent->d_name;
		if (s[0] == '.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0)))
			continue;
		count++;
	}
	assert(errno == 0 && "reading /proc/self/fd failed.");

	closedir(dir);

	return count;
}

void
exec_fd_leak_check(int nr_expected_fds)
{
	const char *exe = "./exec-fd-leak-checker";
	char number[16] = { 0 };

	snprintf(number, sizeof number - 1, "%d", nr_expected_fds);
	execl(exe, exe, number, (char *)NULL);
	assert(0 && "execing fd leak checker failed");
}

#define USEC_TO_NSEC(n) (1000 * (n))

/* our implementation of usleep and sleep functions that are safe to use with
 * timeouts (timeouts are implemented using alarm(), so it is not safe use
 * usleep and sleep. See man pages of these functions)
 */
void
test_usleep(useconds_t usec)
{
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = USEC_TO_NSEC(usec)
	};

	assert(nanosleep(&ts, NULL) == 0);
}

/* we must write the whole function instead of
 * wrapping test_usleep, because useconds_t may not
 * be able to contain such a big number of microseconds */
void
test_sleep(unsigned int sec)
{
	struct timespec ts = {
		.tv_sec = sec,
		.tv_nsec = 0
	};

	assert(nanosleep(&ts, NULL) == 0);
}
