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

#include <stdlib.h>
#include <string.h>
#include "wayland.h"

struct wl_hash {
	struct wl_object **objects;
	uint32_t count, alloc;
};

struct wl_hash *
wl_hash_create(void)
{
	struct wl_hash *hash;

	hash = malloc(sizeof *hash);
	if (hash == NULL)
		return hash;

	memset(hash, 0, sizeof *hash);

	return hash;
}

void
wl_hash_destroy(struct wl_hash *hash)
{
	free(hash);
}

int wl_hash_insert(struct wl_hash *hash, struct wl_object *object)
{
	struct wl_object **objects;
	uint32_t alloc;

	if (hash->count == hash->alloc) {
		if (hash->alloc == 0)
			alloc = 16;
		else
			alloc = hash->alloc * 2;
		objects = realloc(hash->objects, alloc * sizeof *objects);
		if (objects == NULL)
			return -1;

		hash->objects = objects;
		hash->alloc = alloc;
	}

	hash->objects[hash->count] = object;
	hash->count++;

	return 0;
}

struct wl_object *
wl_hash_lookup(struct wl_hash *hash, uint32_t id)
{
	int i;

	for (i = 0; i < hash->count; i++) {
		if (hash->objects[i]->id == id)
			return hash->objects[i];
	}

	return NULL;
}

void
wl_hash_delete(struct wl_hash *hash, struct wl_object *object)
{
	/* writeme */
}


void wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
}

int
wl_list_length(struct wl_list *list)
{
	struct wl_list *e;
	int count;

	e = list->next;
	while (e != list) {
		e = e->next;
		count++;
	}

	return count;
}

int
wl_list_empty(struct wl_list *list)
{
	return list->next == list;
}
