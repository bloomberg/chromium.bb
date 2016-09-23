/*
 * Copyright © 2008 Kristian Høgsberg
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

/** \file wayland-util.h
 *
 * \brief Utility classes, functions, and macros.
 */

#ifndef WAYLAND_UTIL_H
#define WAYLAND_UTIL_H

#include <math.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdarg.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_EXPORT __attribute__ ((visibility("default")))
#else
#define WL_EXPORT
#endif

/* Deprecated attribute */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_DEPRECATED __attribute__ ((deprecated))
#else
#define WL_DEPRECATED
#endif

/* Printf annotation */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_PRINTF(x, y) __attribute__((__format__(__printf__, x, y)))
#else
#define WL_PRINTF(x, y)
#endif

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

/** \class wl_list
 *
 * \brief Doubly-linked list
 *
 * On its own, an instance of `struct wl_list` represents the sentinel head of
 * a doubly-linked list, and must be initialized using wl_list_init().
 * When empty, the list head's `next` and `prev` members point to the list head
 * itself, otherwise `next` references the first element in the list, and `prev`
 * refers to the last element in the list.
 *
 * Use the `struct wl_list` type to represent both the list head and the links
 * between elements within the list. Use wl_list_empty() to determine if the
 * list is empty in O(1).
 *
 * All elements in the list must be of the same type. The element type must have
 * a `struct wl_list` member, often named `link` by convention. Prior to
 * insertion, there is no need to initialize an element's `link` - invoking
 * wl_list_init() on an individual list element's `struct wl_list` member is
 * unnecessary if the very next operation is wl_list_insert(). However, a
 * common idiom is to initialize an element's `link` prior to removal - ensure
 * safety by invoking wl_list_init() before wl_list_remove().
 *
 * Consider a list reference `struct wl_list foo_list`, an element type as
 * `struct element`, and an element's link member as `struct wl_list link`.
 *
 * The following code initializes a list and adds three elements to it.
 *
 * \code
 * struct wl_list foo_list;
 *
 * struct element {
 *         int foo;
 *         struct wl_list link;
 * };
 * struct element e1, e2, e3;
 *
 * wl_list_init(&foo_list);
 * wl_list_insert(&foo_list, &e1.link);   // e1 is the first element
 * wl_list_insert(&foo_list, &e2.link);   // e2 is now the first element
 * wl_list_insert(&e2.link, &e3.link); // insert e3 after e2
 * \endcode
 *
 * The list now looks like <em>[e2, e3, e1]</em>.
 *
 * The `wl_list` API provides some iterator macros. For example, to iterate
 * a list in ascending order:
 *
 * \code
 * struct element *e;
 * wl_list_for_each(e, foo_list, link) {
 *         do_something_with_element(e);
 * }
 * \endcode
 *
 * See the documentation of each iterator for details.
 * \sa http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/include/linux/list.h
 */
struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

/**
 * Initializes the list.
 *
 * \param list List to initialize
 *
 * \memberof wl_list
 */
void
wl_list_init(struct wl_list *list);

/**
 * Inserts an element into the list, after the element represented by \p list.
 * When \p list is a reference to the list itself (the head), set the containing
 * struct of \p elm as the first element in the list.
 *
 * \note If \p elm is already part of a list, inserting it again will lead to
 *       list corruption.
 *
 * \param list List element after which the new element is inserted
 * \param elm Link of the containing struct to insert into the list
 *
 * \memberof wl_list
 */
void
wl_list_insert(struct wl_list *list, struct wl_list *elm);

/**
 * Removes an element from the list.
 *
 * \note This operation leaves \p elm in an invalid state.
 *
 * \param elm Link of the containing struct to remove from the list
 *
 * \memberof wl_list
 */
void
wl_list_remove(struct wl_list *elm);

/**
 * Determines the length of the list.
 *
 * \note This is an O(n) operation.
 *
 * \param list List whose length is to be determined
 *
 * \return Number of elements in the list
 *
 * \memberof wl_list
 */
int
wl_list_length(const struct wl_list *list);

/**
 * Determines if the list is empty.
 *
 * \param list List whose emptiness is to be determined
 *
 * \return 1 if empty, or 0 if not empty
 *
 * \memberof wl_list
 */
int
wl_list_empty(const struct wl_list *list);

/**
 * Inserts all of the elements of one list into another, after the element
 * represented by \p list.
 *
 * \note This leaves \p other in an invalid state.
 *
 * \param list List element after which the other list elements will be inserted
 * \param other List of elements to insert
 *
 * \memberof wl_list
 */
void
wl_list_insert_list(struct wl_list *list, struct wl_list *other);

/**
 * Retrieves a pointer to a containing struct, given a member name.
 *
 * This macro allows "conversion" from a pointer to a member to its containing
 * struct. This is useful if you have a contained item like a wl_list,
 * wl_listener, or wl_signal, provided via a callback or other means, and would
 * like to retrieve the struct that contains it.
 *
 * To demonstrate, the following example retrieves a pointer to
 * `example_container` given only its `destroy_listener` member:
 *
 * \code
 * struct example_container {
 *         struct wl_listener destroy_listener;
 *         // other members...
 * };
 *
 * void example_container_destroy(struct wl_listener *listener, void *data)
 * {
 *         struct example_container *ctr;
 *
 *         ctr = wl_container_of(listener, ctr, destroy_listener);
 *         // destroy ctr...
 * }
 * \endcode
 *
 * \note `sample` need not be a valid pointer. A null or uninitialised pointer
 *       is sufficient.
 *
 * \param ptr Valid pointer to the contained member
 * \param sample Pointer to a struct whose type contains \p ptr
 * \param member Named location of \p ptr within the \p sample type
 *
 * \return The container for the specified pointer
 */
#define wl_container_of(ptr, sample, member)				\
	(__typeof__(sample))((char *)(ptr) -				\
			     offsetof(__typeof__(*sample), member))

/**
 * Iterates over a list.
 *
 * This macro expresses a for-each iterator for wl_list. Given a list and
 * wl_list link member name (often named `link` by convention), this macro
 * assigns each element in the list to \p pos, which can then be referenced in
 * a trailing code block. For example, given a wl_list of `struct message`
 * elements:
 *
 * \code
 * struct message {
 *         char *contents;
 *         wl_list link;
 * };
 *
 * struct wl_list *message_list;
 * // Assume message_list now "contains" many messages
 *
 * struct message *m;
 * wl_list_for_each(m, message_list, link) {
 *         do_something_with_message(m);
 * }
 * \endcode
 *
 * \param pos Cursor that each list element will be assigned to
 * \param head Head of the list to iterate over
 * \param member Name of the link member within the element struct
 *
 * \relates wl_list
 */
#define wl_list_for_each(pos, head, member)				\
	for (pos = wl_container_of((head)->next, pos, member);	\
	     &pos->member != (head);					\
	     pos = wl_container_of(pos->member.next, pos, member))

/**
 * Iterates over a list, safe against removal of the list element.
 *
 * \note Only removal of the current element, \p pos, is safe. Removing
 *       any other element during traversal may lead to a loop malfunction.
 *
 * \sa wl_list_for_each()
 *
 * \param pos Cursor that each list element will be assigned to
 * \param tmp Temporary pointer of the same type as \p pos
 * \param head Head of the list to iterate over
 * \param member Name of the link member within the element struct
 *
 * \relates wl_list
 */
#define wl_list_for_each_safe(pos, tmp, head, member)			\
	for (pos = wl_container_of((head)->next, pos, member),		\
	     tmp = wl_container_of((pos)->member.next, tmp, member);	\
	     &pos->member != (head);					\
	     pos = tmp,							\
	     tmp = wl_container_of(pos->member.next, tmp, member))

/**
 * Iterates backwards over a list.
 *
 * \sa wl_list_for_each()
 *
 * \param pos Cursor that each list element will be assigned to
 * \param head Head of the list to iterate over
 * \param member Name of the link member within the element struct
 *
 * \relates wl_list
 */
#define wl_list_for_each_reverse(pos, head, member)			\
	for (pos = wl_container_of((head)->prev, pos, member);	\
	     &pos->member != (head);					\
	     pos = wl_container_of(pos->member.prev, pos, member))

/**
 * Iterates backwards over a list, safe against removal of the list element.
 *
 * \note Only removal of the current element, \p pos, is safe. Removing
 *       any other element during traversal may lead to a loop malfunction.
 *
 * \sa wl_list_for_each()
 *
 * \param pos Cursor that each list element will be assigned to
 * \param tmp Temporary pointer of the same type as \p pos
 * \param head Head of the list to iterate over
 * \param member Name of the link member within the element struct
 *
 * \relates wl_list
 */
#define wl_list_for_each_reverse_safe(pos, tmp, head, member)		\
	for (pos = wl_container_of((head)->prev, pos, member),	\
	     tmp = wl_container_of((pos)->member.prev, tmp, member);	\
	     &pos->member != (head);					\
	     pos = tmp,							\
	     tmp = wl_container_of(pos->member.prev, tmp, member))

struct wl_array {
	size_t size;
	size_t alloc;
	void *data;
};

#define wl_array_for_each(pos, array)					\
	for (pos = (array)->data;					\
	     (const char *) pos < ((const char *) (array)->data + (array)->size); \
	     (pos)++)

void
wl_array_init(struct wl_array *array);

void
wl_array_release(struct wl_array *array);

void *
wl_array_add(struct wl_array *array, size_t size);

int
wl_array_copy(struct wl_array *array, struct wl_array *source);

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

static inline int
wl_fixed_to_int(wl_fixed_t f)
{
	return f / 256;
}

static inline wl_fixed_t
wl_fixed_from_int(int i)
{
	return i * 256;
}

/**
 * \brief A union representing all of the basic data types that can be passed
 * along the wayland wire format.
 *
 * This union represents all of the basic data types that can be passed in the
 * wayland wire format.  It is used by dispatchers and runtime-friendly
 * versions of the event and request marshaling functions.
 */
union wl_argument {
	int32_t i; /**< signed integer */
	uint32_t u; /**< unsigned integer */
	wl_fixed_t f; /**< fixed point */
	const char *s; /**< string */
	struct wl_object *o; /**< object */
	uint32_t n; /**< new_id */
	struct wl_array *a; /**< array */
	int32_t h; /**< file descriptor */
};

/**
 * \brief A function pointer type for a dispatcher.
 *
 * A dispatcher is a function that handles the emitting of callbacks in client
 * code.  For programs directly using the C library, this is done by using
 * libffi to call function pointers.  When binding to languages other than C,
 * dispatchers provide a way to abstract the function calling process to be
 * friendlier to other function calling systems.
 *
 * A dispatcher takes five arguments:  The first is the dispatcher-specific
 * implementation data associated with the target object.  The second is the
 * object on which the callback is being invoked (either wl_proxy or
 * wl_resource).  The third and fourth arguments are the opcode the wl_message
 * structure corresponding to the callback being emitted.  The final argument
 * is an array of arguments received from the other process via the wire
 * protocol.
 */
typedef int (*wl_dispatcher_func_t)(const void *, void *, uint32_t,
				    const struct wl_message *,
				    union wl_argument *);

typedef void (*wl_log_func_t)(const char *, va_list) WL_PRINTF(1, 0);

/** \enum wl_iterator_result
 *
 * This enum represents the return value of an iterator function.
 */
enum wl_iterator_result {
	/** Stop the iteration */
	WL_ITERATOR_STOP,
	/** Continue the iteration */
	WL_ITERATOR_CONTINUE
};

#ifdef  __cplusplus
}
#endif

#endif
