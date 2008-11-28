#ifndef WAYLAND_UTIL_H
#define WAYLAND_UTIL_H

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_EXPORT __attribute__ ((visibility("default")))
#else
#define WL_EXPORT
#endif

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct wl_hash {
	struct wl_object **objects;
	uint32_t count, alloc, id;
};

int wl_hash_insert(struct wl_hash *hash, struct wl_object *object);
struct wl_object *wl_hash_lookup(struct wl_hash *hash, uint32_t id);
void wl_hash_delete(struct wl_hash *hash, struct wl_object *object);

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

void wl_list_init(struct wl_list *list);
void wl_list_insert(struct wl_list *list, struct wl_list *elm);
void wl_list_remove(struct wl_list *elm);
int wl_list_length(struct wl_list *list);
int wl_list_empty(struct wl_list *list);


#endif
