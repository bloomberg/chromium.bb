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
#include <stdint.h>
#include <string.h>
#include "wayland-util.h"

WL_EXPORT void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

WL_EXPORT void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

WL_EXPORT void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
}

WL_EXPORT int
wl_list_length(struct wl_list *list)
{
	struct wl_list *e;
	int count;

	count = 0;
	e = list->next;
	while (e != list) {
		e = e->next;
		count++;
	}

	return count;
}

WL_EXPORT int
wl_list_empty(struct wl_list *list)
{
	return list->next == list;
}

WL_EXPORT void
wl_list_insert_list(struct wl_list *list, struct wl_list *other)
{
	other->next->prev = list;
	other->prev->next = list->next;
	list->next->prev = other->prev;
	list->next = other->next;
}

WL_EXPORT void
wl_array_init(struct wl_array *array)
{
	memset(array, 0, sizeof *array);
}

WL_EXPORT void
wl_array_release(struct wl_array *array)
{
	free(array->data);
}

WL_EXPORT void *
wl_array_add(struct wl_array *array, int size)
{
	int alloc;
	void *data, *p;

	if (array->alloc > 0)
		alloc = array->alloc;
	else
		alloc = 16;

	while (alloc < array->size + size)
		alloc *= 2;

	if (array->alloc < alloc) {
		if (array->alloc > 0)
			data = realloc(array->data, alloc);
	        else
			data = malloc(alloc);

		if (data == NULL)
			return 0;
		array->data = data;
		array->alloc = alloc;
	}

	p = array->data + array->size;
	array->size += size;

	return p;
}

WL_EXPORT void
wl_array_copy(struct wl_array *array, struct wl_array *source)
{
	array->size = 0;
	wl_array_add(array, source->size);
	memcpy(array->data, source->data, source->size);
}

union map_entry {
	uintptr_t next;
	void *data;
};

WL_EXPORT void
wl_map_init(struct wl_map *map)
{
	memset(map, 0, sizeof *map);
}

WL_EXPORT void
wl_map_release(struct wl_map *map)
{
	wl_array_release(&map->entries);
}

WL_EXPORT uint32_t
wl_map_insert_new(struct wl_map *map, void *data)
{
	union map_entry *start, *entry;

	if (map->free_list) {
		start = map->entries.data;
		entry = &start[map->free_list >> 1];
		map->free_list = entry->next;
	} else {
		entry = wl_array_add(&map->entries, sizeof *entry);
		start = map->entries.data;
	}

	entry->data = data;

	return entry - start;
}

WL_EXPORT int
wl_map_insert_at(struct wl_map *map, uint32_t i, void *data)
{
	union map_entry *start;
	uint32_t count;

	/* assert(map->free_list == NULL */
	count = map->entries.size / sizeof *start;

	if (count < i)
		return -1;

	if (count == i)
		wl_array_add(&map->entries, sizeof *start);

	start = map->entries.data;
	start[i].data = data;

	return 0;
}

WL_EXPORT void
wl_map_remove(struct wl_map *map, uint32_t i)
{
	union map_entry *start;
	uint32_t count;

	start = map->entries.data;
	count = map->entries.size / sizeof *start;

	start[i].next = map->free_list;
	map->free_list = (i << 1) | 1;
}

WL_EXPORT void *
wl_map_lookup(struct wl_map *map, uint32_t i)
{
	union map_entry *start;
	uint32_t count;

	start = map->entries.data;
	count = map->entries.size / sizeof *start;

	if (i < count && !(start[i].next & 1))
		return start[i].data;

	return NULL;
}

WL_EXPORT void
wl_map_for_each(struct wl_map *map, wl_iterator_func_t func, void *data)
{
	union map_entry *start, *end, *p;

	start = map->entries.data;
	end = (union map_entry *) ((char *) map->entries.data + map->entries.size);

	for (p = start; p < end; p++)
		if (p->data && !(p->next & 1))
			func(p->data, data);
}
