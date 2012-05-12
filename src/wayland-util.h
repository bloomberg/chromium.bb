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

#include <math.h>
#include <stddef.h>
#include <inttypes.h>

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_EXPORT __attribute__ ((visibility("default")))
#else
#define WL_EXPORT
#endif

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

#define container_of(ptr, type, member) ({				\
	const __typeof__( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct wl_message {
	const char *name;
	const char *signature;
	const struct wl_interface **types;
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
	void (* const * implementation)(void);
	uint32_t id;
};

/**
 * wl_list - linked list
 *
 * The list head is of "struct wl_list" type, and must be initialized
 * using wl_list_init().  All entries in the list must be of the same
 * type.  The item type must have a "struct wl_list" member. This
 * member will be initialized by wl_list_insert(). There is no need to
 * call wl_list_init() on the individual item. To query if the list is
 * empty in O(1), use wl_list_empty().
 *
 * Let's call the list reference "struct wl_list foo_list", the item type as
 * "item_t", and the item member as "struct wl_list link". The following code
 *
 * The following code will initialize a list:
 *
 * 	struct wl_list foo_list;
 *
 * 	struct item_t {
 * 		int foo;
 * 		struct wl_list link;
 * 	};
 * 	struct item_t item1, item2, item3;
 *
 *	wl_list_init(&foo_list);
 *	wl_list_insert(&foo_list, &item1.link);	Pushes item1 at the head
 *	wl_list_insert(&foo_list, &item2.link);	Pushes item2 at the head
 *	wl_list_insert(&item2.link, &item3.link);Pushes item3 after item2
 *
 * The list now looks like [item2, item3, item1]
 *
 * Will iterate the list in ascending order:
 *
 *	item_t *item;
 *	wl_list_for_each(item, foo_list, link) {
 *		Do_something_with_item(item);
 *	}
 */
struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

void wl_list_init(struct wl_list *list);
void wl_list_insert(struct wl_list *list, struct wl_list *elm);
void wl_list_remove(struct wl_list *elm);
int wl_list_length(struct wl_list *list);
int wl_list_empty(struct wl_list *list);
void wl_list_insert_list(struct wl_list *list, struct wl_list *other);

#ifdef __GNUC__
#define __wl_container_of(ptr, sample, member)				\
	(__typeof__(sample))((char *)(ptr)	-			\
		 ((char *)&(sample)->member - (char *)(sample)))
#else
#define __wl_container_of(ptr, sample, member)				\
	(void *)((char *)(ptr)	-				        \
		 ((char *)&(sample)->member - (char *)(sample)))
#endif

#define wl_list_for_each(pos, head, member)				\
	for (pos = 0, pos = __wl_container_of((head)->next, pos, member);	\
	     &pos->member != (head);					\
	     pos = __wl_container_of(pos->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member)			\
	for (pos = 0, tmp = 0, 						\
	     pos = __wl_container_of((head)->next, pos, member),		\
	     tmp = __wl_container_of((pos)->member.next, tmp, member);	\
	     &pos->member != (head);					\
	     pos = tmp,							\
	     tmp = __wl_container_of(pos->member.next, tmp, member))

#define wl_list_for_each_reverse(pos, head, member)			\
	for (pos = 0, pos = __wl_container_of((head)->prev, pos, member);	\
	     &pos->member != (head);					\
	     pos = __wl_container_of(pos->member.prev, pos, member))

#define wl_list_for_each_reverse_safe(pos, tmp, head, member)		\
	for (pos = 0, tmp = 0, 						\
	     pos = __wl_container_of((head)->prev, pos, member),	\
	     tmp = __wl_container_of((pos)->member.prev, tmp, member);	\
	     &pos->member != (head);					\
	     pos = tmp,							\
	     tmp = __wl_container_of(pos->member.prev, tmp, member))

struct wl_array {
	size_t size;
	size_t alloc;
	void *data;
};

#define wl_array_for_each(pos, array)					\
	for (pos = (array)->data;					\
	     (const char *) pos < ((const char *) (array)->data + (array)->size); \
	     (pos)++)

void wl_array_init(struct wl_array *array);
void wl_array_release(struct wl_array *array);
void *wl_array_add(struct wl_array *array, size_t size);
void wl_array_copy(struct wl_array *array, struct wl_array *source);

typedef int32_t wl_fixed_t;

static inline double
wl_fixed_to_double (wl_fixed_t f)
{
	union {
		double d;
		int64_t i;
	} u;

	u.i = ((1023LL + 44LL) << 52) + (1LL << 51) + f;

	return u.d - (3LL << 43);
}

static inline wl_fixed_t
wl_fixed_from_double(double d)
{
	union {
		double d;
		int64_t i;
	} u;

	u.d = d + (3LL << (51 - 8));

	return u.i;
}

static inline int wl_fixed_to_int(wl_fixed_t f)
{
	return f / 256;
}
static inline wl_fixed_t wl_fixed_from_int(int i)
{
	return i * 256;
}

#ifdef  __cplusplus
}
#endif

#endif
