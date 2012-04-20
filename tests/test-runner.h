#ifndef _TEST_RUNNER_H_
#define _TEST_RUNNER_H_

#ifdef NDEBUG
#error "Tests must not be built with NDEBUG defined, they rely on assert()."
#endif

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

#endif
