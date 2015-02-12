/*
 * Copyright © 2015 Collabora, Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file-util.h"

static int
current_time_str(char *str, size_t len, const char *fmt)
{
	time_t t;
	struct tm *t_local;
	int ret;

	t = time(NULL);
	t_local = localtime(&t);
	if (!t_local) {
		errno = ETIME;
		return -1;
	}

	ret = strftime(str, len, fmt, t_local);
	if (ret == 0) {
		errno = ETIME;
		return -1;
	}

	return ret;
}

static int
create_file_excl(const char *fname)
{
	return open(fname, O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, 00666);
}

/** Create a unique file with date and time in the name
 *
 * \param path_prefix Path and file name prefix.
 * \param suffix File name suffix.
 * \param name_out[out] Buffer for the resulting file name.
 * \param name_len Number of bytes usable in name_out.
 * \return stdio FILE pointer, or NULL on failure.
 *
 * Create and open a new file with the name concatenated from
 * path_prefix, date and time, and suffix. If a file with this name
 * already exists, an counter number is added to the end of the
 * date and time sub-string. The counter is increased until a free file
 * name is found.
 *
 * Once creating the file succeeds, the name of the file is in name_out.
 * On failure, the contents of name_out are undefined and errno is set.
 */
FILE *
file_create_dated(const char *path_prefix, const char *suffix,
		  char *name_out, size_t name_len)
{
	char timestr[128];
	int ret;
	int fd;
	int cnt = 0;

	if (current_time_str(timestr, sizeof(timestr), "%F_%H-%M-%S") < 0)
		return NULL;

	ret = snprintf(name_out, name_len, "%s%s%s",
		       path_prefix, timestr, suffix);
	if (ret < 0 || (size_t)ret >= name_len) {
		errno = ENOBUFS;
		return NULL;
	}

	fd = create_file_excl(name_out);

	while (fd == -1 && errno == EEXIST) {
		cnt++;

		ret = snprintf(name_out, name_len, "%s%s-%d%s",
			       path_prefix, timestr, cnt, suffix);
		if (ret < 0 || (size_t)ret >= name_len) {
			errno = ENOBUFS;
			return NULL;
		}

		fd = create_file_excl(name_out);
	}

	if (fd == -1)
		return NULL;

	return fdopen(fd, "w");
}
