#ifndef _TEST_RUNNER_H_
#define _TEST_RUNNER_H_

#ifdef NDEBUG
#error "Tests must not be built with NDEBUG defined, they rely on assert()."
#endif

#include "../src/compositor.h"

struct test {
	const char *name;
	void (*run)(struct weston_compositor *compositor);
} __attribute__ ((aligned (16)));

#define TEST(name)						\
	static void name(struct weston_compositor *compositor);	\
								\
	const struct test test##name				\
		 __attribute__ ((section ("test_section"))) = {	\
		#name, name					\
	};							\
								\
	static void name(struct weston_compositor *compositor)

struct test_client {
	struct weston_compositor *compositor;
	struct wl_client *client;
	struct weston_process proc;
	int fd;
	int done;
	int status;
	int terminate;
};

struct test_client *test_client_launch(struct weston_compositor *compositor);
void test_client_send(struct test_client *client, const char *fmt, ...);

#endif
