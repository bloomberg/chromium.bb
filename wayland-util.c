#include <stdlib.h>
#include "wayland.h"

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
