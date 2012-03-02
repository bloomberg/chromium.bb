/*
 * Copyright © 2012 Intel Corporation
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include "test-runner.h"

extern const struct test __start_test_section, __stop_test_section;

static int
run_one_test(const char *name)
{
	const struct test *t;

	for (t = &__start_test_section; t < &__stop_test_section; t++) {
		if (strcmp(t->name, name) == 0) {
			t->run();
			return EXIT_SUCCESS;
		}
	}

	fprintf(stderr, "uknown test: \"%s\"\n", name);

	return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
	const struct test *t;
	pid_t pid;
	int i, total, pass;
	siginfo_t info;

	if (argc == 2)
		return run_one_test(argv[1]);

	pass = 0;
	for (t = &__start_test_section; t < &__stop_test_section; t++) {
		fprintf(stderr, "running \"%s\"... ", t->name);
		pid = fork();
		assert(pid >= 0);
		if (pid == 0) { 
			t->run();
			exit(EXIT_SUCCESS);
		}
		if (waitid(P_ALL, 0, &info, WEXITED)) {
			fprintf(stderr, "waitid failed: %m\n");
			abort();
		}

		switch (info.si_code) {
		case CLD_EXITED:
			fprintf(stderr, "exit status %d\n", info.si_status);
			if (info.si_status == EXIT_SUCCESS)
				pass++;
			break;
		case CLD_KILLED:
		case CLD_DUMPED:
			fprintf(stderr, "signal %d\n", info.si_status);
			break;
		}
	}

	total = &__stop_test_section - &__start_test_section;
	fprintf(stderr, "%d tests, %d pass, %d fail\n",
		total, pass, total - pass);

	return pass == total ? EXIT_SUCCESS : EXIT_FAILURE;
}
