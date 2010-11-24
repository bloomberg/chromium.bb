/*
 * Copyright © 2008 Kristian Høgsberg
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

#ifndef WAYLAND_UTIL_H
#define WAYLAND_UTIL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_EXPORT __attribute__ ((visibility("default")))
#else
#define WL_EXPORT
#endif

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#define ALIGN(n, a) ( ((n) + ((a) - 1)) & ~((a) - 1) )
#define DIV_ROUNDUP(n, a) ( ((n) + ((a) - 1)) / (a) )

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct wl_argument {
	uint32_t type;
	void *data;
};

struct wl_message {
	const char *name;
	const char *signature;
	const void **types;
};

struct wl_interface {
	const char *name;
	int version;
	int method_count;
	const struct wl_message *methods;
	int event_count;
	const struct wl_message *events;
};

struct wl_object {
	const struct wl_interface *interface;
	void (**implementation)(void);
	uint32_t id;
};

struct wl_hash_table;
struct wl_hash_table *wl_hash_table_create(void);
void wl_hash_table_destroy(struct wl_hash_table *ht);
void *wl_hash_table_lookup(struct wl_hash_table *ht, uint32_t hash);
int wl_hash_table_insert(struct wl_hash_table *ht, uint32_t hash, void *data);
void wl_hash_table_remove(struct wl_hash_table *ht, uint32_t hash);


struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

void wl_list_init(struct wl_list *list);
void wl_list_insert(struct wl_list *list, struct wl_list *elm);
void wl_list_remove(struct wl_list *elm);
int wl_list_length(struct wl_list *list);
int wl_list_empty(struct wl_list *list);

#define __container_of(ptr, sample, member)				\
	(void *)((char *)(ptr)	-					\
		 ((char *)&(sample)->member - (char *)(sample)))

#define wl_list_for_each(pos, head, member)				\
	for (pos = 0, pos = __container_of((head)->next, pos, member);	\
	     &pos->member != (head);					\
	     pos = __container_of(pos->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member)			\
	for (pos = 0, tmp = 0, 						\
	     pos = __container_of((head)->next, pos, member),		\
	     tmp = __container_of((pos)->member.next, tmp, member);	\
	     &pos->member != (head);					\
	     pos = tmp,							\
	     tmp = __container_of(pos->member.next, tmp, member))

#define wl_list_for_each_reverse(pos, head, member)			\
	for (pos = 0, pos = __container_of((head)->prev, pos, member);	\
	     &pos->member != (head);					\
	     pos = __container_of(pos->member.prev, pos, member))

struct wl_array {
	uint32_t size;
	uint32_t alloc;
	void *data;
};

void wl_array_init(struct wl_array *array);
void wl_array_release(struct wl_array *array);
void *wl_array_add(struct wl_array *array, int size);

#ifdef  __cplusplus
}
#endif

#endif
