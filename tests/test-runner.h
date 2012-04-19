#ifndef _TEST_RUNNER_H_
#define _TEST_RUNNER_H_

#ifdef NDEBUG
#error "Tests must not be built with NDEBUG defined, they rely on assert()."
#endif

struct test {
	const char *name;
	void (*run)(void);
};

#define TEST(name)						\
	static void name(void);					\
								\
	const struct test test##name			\
		 __attribute__ ((section ("test_section"))) = {	\
		#name, name					\
	};							\
								\
	static void name(void)

#endif
