#ifndef _TEST_RUNNER_H_
#define _TEST_RUNNER_H_

#ifdef NDEBUG
#error "Tests must not be built with NDEBUG defined, they rely on assert()."
#endif

#include <unistd.h>

struct test {
	const char *name;
	void (*run)(void);
	int must_fail;
} __attribute__ ((aligned (16)));

#define TEST(name)						\
	static void name(void);					\
								\
	const struct test test##name				\
		 __attribute__ ((section ("test_section"))) = {	\
		#name, name, 0					\
	};							\
								\
	static void name(void)

#define FAIL_TEST(name)						\
	static void name(void);					\
								\
	const struct test test##name				\
		 __attribute__ ((section ("test_section"))) = {	\
		#name, name, 1					\
	};							\
								\
	static void name(void)

int
count_open_fds(void);

void
exec_fd_leak_check(int nr_expected_fds); /* never returns */

int
get_current_alloc_num(void);

void
check_leaks(int supposed_allocs, int supposed_fds);

/*
 * set/reset the timeout in seconds. The timeout starts
 * at the point of invoking this function
 */
void
test_set_timeout(unsigned int);

/* test-runner uses alarm() and SIGALRM, so we can not
 * use usleep and sleep functions in tests (see 'man usleep'
 * or 'man sleep', respectively). Following functions are safe
 * to use in tests */
void
test_usleep(useconds_t);

void
test_sleep(unsigned int);

#define DISABLE_LEAK_CHECKS			\
	do {					\
		extern int leak_check_enabled;	\
		leak_check_enabled = 0;		\
	} while (0);

#endif
