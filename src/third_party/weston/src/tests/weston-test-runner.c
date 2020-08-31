/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>

#include "weston-test-runner.h"

#define SKIP 77

char __attribute__((weak)) *server_parameters="";

extern const struct weston_test __start_test_section, __stop_test_section;

static const char *test_name_;

const char *
get_test_name(void)
{
	return test_name_;
}

static const struct weston_test *
find_test(const char *name)
{
	const struct weston_test *t;

	for (t = &__start_test_section; t < &__stop_test_section; t++)
		if (strcmp(t->name, name) == 0)
			return t;

	return NULL;
}

static void
run_test(const struct weston_test *t, void *data, int iteration)
{
	char str[512];

	if (data) {
		snprintf(str, sizeof(str), "%s[%d]", t->name, iteration);
		test_name_ = str;
	} else {
		test_name_ = t->name;
	}

	t->run(data);
	exit(EXIT_SUCCESS);
}

static void
list_tests(void)
{
	const struct weston_test *t;

	fprintf(stderr, "Available test names:\n");
	for (t = &__start_test_section; t < &__stop_test_section; t++)
		fprintf(stderr, "	%s\n", t->name);
}

/* iteration is valid only if test_data is not NULL */
static int
exec_and_report_test(const struct weston_test *t, void *test_data, int iteration)
{
	int success = 0;
	int skip = 0;
	int hardfail = 0;
	siginfo_t info;

	pid_t pid = fork();
	assert(pid >= 0);

	if (pid == 0)
		run_test(t, test_data, iteration); /* never returns */

	if (waitid(P_ALL, 0, &info, WEXITED)) {
		fprintf(stderr, "waitid failed: %s\n", strerror(errno));
		abort();
	}

	if (test_data)
		fprintf(stderr, "test \"%s/%i\":\t", t->name, iteration);
	else
		fprintf(stderr, "test \"%s\":\t", t->name);

	switch (info.si_code) {
	case CLD_EXITED:
		fprintf(stderr, "exit status %d", info.si_status);
		if (info.si_status == EXIT_SUCCESS)
			success = 1;
		else if (info.si_status == SKIP)
			skip = 1;
		break;
	case CLD_KILLED:
	case CLD_DUMPED:
		fprintf(stderr, "signal %d", info.si_status);
		if (info.si_status != SIGABRT)
			hardfail = 1;
		break;
	}

	if (t->must_fail)
		success = !success;

	if (success && !hardfail) {
		fprintf(stderr, ", pass.\n");
		return 1;
	} else if (skip) {
		fprintf(stderr, ", skip.\n");
		return SKIP;
	} else {
		fprintf(stderr, ", fail.\n");
		return 0;
	}
}

/* Returns number of tests and number of pass / fail in param args.
 * Even non-iterated tests go through here, they simply have n_elements = 1 and
 * table_data = NULL.
 */
static int
iterate_test(const struct weston_test *t, int *passed, int *skipped)
{
	int ret, i;
	void *current_test_data = (void *) t->table_data;
	for (i = 0; i < t->n_elements; ++i, current_test_data += t->element_size)
	{
		ret = exec_and_report_test(t, current_test_data, i);
		if (ret == SKIP)
			++(*skipped);
		else if (ret)
			++(*passed);
	}

	return t->n_elements;
}

int main(int argc, char *argv[])
{
	const struct weston_test *t;
	int total = 0;
	int pass = 0;
	int skip = 0;

	if (argc == 2) {
		const char *testname = argv[1];
		if (strcmp(testname, "--help") == 0 ||
		    strcmp(testname, "-h") == 0) {
			fprintf(stderr, "Usage: %s [test-name]\n", program_invocation_short_name);
			list_tests();
			exit(EXIT_SUCCESS);
		}

		if (strcmp(testname, "--params") == 0 ||
		    strcmp(testname, "-p") == 0) {
			printf("%s", server_parameters);
			exit(EXIT_SUCCESS);
		}

		t = find_test(argv[1]);
		if (t == NULL) {
			fprintf(stderr, "unknown test: \"%s\"\n", argv[1]);
			list_tests();
			exit(EXIT_FAILURE);
		}

		int number_passed_in_test = 0, number_skipped_in_test = 0;
		total += iterate_test(t, &number_passed_in_test, &number_skipped_in_test);
		pass += number_passed_in_test;
		skip += number_skipped_in_test;
	} else {
		for (t = &__start_test_section; t < &__stop_test_section; t++) {
			int number_passed_in_test = 0, number_skipped_in_test = 0;
			total += iterate_test(t, &number_passed_in_test, &number_skipped_in_test);
			pass += number_passed_in_test;
			skip += number_skipped_in_test;
		}
	}

	fprintf(stderr, "%d tests, %d pass, %d skip, %d fail\n",
		total, pass, skip, total - pass - skip);

	if (skip == total)
		return SKIP;
	else if (pass + skip == total)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}
