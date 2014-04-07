/*
 * Copyright © 2013 Intel Corporation
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

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include "config-parser.h"

static struct weston_config *
run_test(const char *text)
{
	struct weston_config *config;
	char file[] = "/tmp/weston-config-parser-test-XXXXXX";
	int fd, len;

	fd = mkstemp(file);
	len = write(fd, text, strlen(text));
	assert(len == (int) strlen(text));

	config = weston_config_parse(file);
	close(fd);
	unlink(file);

	return config;
}

static const char t0[] =
	"# nothing in this file...\n";

static const char t1[] =
	"# comment line here...\n"
	"\n"
	"[foo]\n"
	"a=b\n"
	"name=  Roy Batty    \n"
	"\n"
	"\n"
	"[bar]\n"
	"# more comments\n"
	"number=5252\n"
	"flag=false\n"
	"\n"
	"[stuff]\n"
	"flag=     true \n"
	"\n"
	"[bucket]\n"
	"color=blue \n"
	"contents=live crabs\n"
	"pinchy=true\n"
	"\n"
	"[bucket]\n"
	"material=plastic \n"
	"color=red\n"
	"contents=sand\n";

static const char *section_names[] = {
	"foo", "bar", "stuff", "bucket", "bucket"
};

static const char t2[] =
	"# invalid section...\n"
	"[this bracket isn't closed\n";

static const char t3[] =
	"# line without = ...\n"
	"[bambam]\n"
	"this line isn't any kind of valid\n";

static const char t4[] =
	"# starting with = ...\n"
	"[bambam]\n"
	"=not valid at all\n";

int main(int argc, char *argv[])
{
	struct weston_config *config;
	struct weston_config_section *section;
	const char *name;
	char *s;
	int r, b, i;
	int32_t n;
	uint32_t u;

	config = run_test(t0);
	assert(config);
	weston_config_destroy(config);

	config = run_test(t1);
	assert(config);
	section = weston_config_get_section(config, "mollusc", NULL, NULL);
	assert(section == NULL);

	section = weston_config_get_section(config, "foo", NULL, NULL);
	r = weston_config_section_get_string(section, "a", &s, NULL);
	assert(r == 0 && strcmp(s, "b") == 0);
	free(s);

	section = weston_config_get_section(config, "foo", NULL, NULL);
	r = weston_config_section_get_string(section, "b", &s, NULL);
	assert(r == -1 && errno == ENOENT && s == NULL);

	section = weston_config_get_section(config, "foo", NULL, NULL);
	r = weston_config_section_get_string(section, "name", &s, NULL);
	assert(r == 0 && strcmp(s, "Roy Batty") == 0);
	free(s);

	section = weston_config_get_section(config, "bar", NULL, NULL);
	r = weston_config_section_get_string(section, "a", &s, "boo");
	assert(r == -1 && errno == ENOENT && strcmp(s, "boo") == 0);
	free(s);

	section = weston_config_get_section(config, "bar", NULL, NULL);
	r = weston_config_section_get_int(section, "number", &n, 600);
	assert(r == 0 && n == 5252);

	section = weston_config_get_section(config, "bar", NULL, NULL);
	r = weston_config_section_get_int(section, "+++", &n, 700);
	assert(r == -1 && errno == ENOENT && n == 700);

	section = weston_config_get_section(config, "bar", NULL, NULL);
	r = weston_config_section_get_uint(section, "number", &u, 600);
	assert(r == 0 && u == 5252);

	section = weston_config_get_section(config, "bar", NULL, NULL);
	r = weston_config_section_get_uint(section, "+++", &u, 600);
	assert(r == -1 && errno == ENOENT && u == 600);

	section = weston_config_get_section(config, "bar", NULL, NULL);
	r = weston_config_section_get_bool(section, "flag", &b, 600);
	assert(r == 0 && b == 0);

	section = weston_config_get_section(config, "stuff", NULL, NULL);
	r = weston_config_section_get_bool(section, "flag", &b, -1);
	assert(r == 0 && b == 1);

	section = weston_config_get_section(config, "stuff", NULL, NULL);
	r = weston_config_section_get_bool(section, "bonk", &b, -1);
	assert(r == -1 && errno == ENOENT && b == -1);

	section = weston_config_get_section(config, "bucket", "color", "blue");
	r = weston_config_section_get_string(section, "contents", &s, NULL);
	assert(r == 0 && strcmp(s, "live crabs") == 0);
	free(s);

	section = weston_config_get_section(config, "bucket", "color", "red");
	r = weston_config_section_get_string(section, "contents", &s, NULL);
	assert(r == 0 && strcmp(s, "sand") == 0);
	free(s);

	section = weston_config_get_section(config, "bucket", "color", "pink");
	assert(section == NULL);
	r = weston_config_section_get_string(section, "contents", &s, "eels");
	assert(r == -1 && errno == ENOENT && strcmp(s, "eels") == 0);
	free(s);

	section = NULL;
	i = 0;
	while (weston_config_next_section(config, &section, &name))
		assert(strcmp(section_names[i++], name) == 0);
	assert(i == 5);

	weston_config_destroy(config);

	config = run_test(t2);
	assert(config == NULL);

	config = run_test(t3);
	assert(config == NULL);

	config = run_test(t4);
	assert(config == NULL);

	weston_config_destroy(NULL);
	assert(weston_config_next_section(NULL, NULL, NULL) == 0);

	section = weston_config_get_section(NULL, "bucket", NULL, NULL);
	assert(section == NULL);

	return 0;
}
