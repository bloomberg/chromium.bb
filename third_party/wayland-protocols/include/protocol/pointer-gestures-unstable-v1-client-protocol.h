#ifndef POINTER_GESTURES_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define POINTER_GESTURES_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct zwp_pointer_gestures_v1;
struct zwp_pointer_gesture_swipe_v1;
struct zwp_pointer_gesture_pinch_v1;

extern const struct wl_interface zwp_pointer_gestures_v1_interface;
extern const struct wl_interface zwp_pointer_gesture_swipe_v1_interface;
extern const struct wl_interface zwp_pointer_gesture_pinch_v1_interface;

#define ZWP_POINTER_GESTURES_V1_GET_SWIPE_GESTURE	0
#define ZWP_POINTER_GESTURES_V1_GET_PINCH_GESTURE	1

static inline void
zwp_pointer_gestures_v1_set_user_data(struct zwp_pointer_gestures_v1 *zwp_pointer_gestures_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_pointer_gestures_v1, user_data);
}

static inline void *
zwp_pointer_gestures_v1_get_user_data(struct zwp_pointer_gestures_v1 *zwp_pointer_gestures_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_pointer_gestures_v1);
}

static inline void
zwp_pointer_gestures_v1_destroy(struct zwp_pointer_gestures_v1 *zwp_pointer_gestures_v1)
{
	wl_proxy_destroy((struct wl_proxy *) zwp_pointer_gestures_v1);
}

static inline struct zwp_pointer_gesture_swipe_v1 *
zwp_pointer_gestures_v1_get_swipe_gesture(struct zwp_pointer_gestures_v1 *zwp_pointer_gestures_v1, struct wl_pointer *pointer)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_pointer_gestures_v1,
			 ZWP_POINTER_GESTURES_V1_GET_SWIPE_GESTURE, &zwp_pointer_gesture_swipe_v1_interface, NULL, pointer);

	return (struct zwp_pointer_gesture_swipe_v1 *) id;
}

static inline struct zwp_pointer_gesture_pinch_v1 *
zwp_pointer_gestures_v1_get_pinch_gesture(struct zwp_pointer_gestures_v1 *zwp_pointer_gestures_v1, struct wl_pointer *pointer)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_pointer_gestures_v1,
			 ZWP_POINTER_GESTURES_V1_GET_PINCH_GESTURE, &zwp_pointer_gesture_pinch_v1_interface, NULL, pointer);

	return (struct zwp_pointer_gesture_pinch_v1 *) id;
}

/**
 * zwp_pointer_gesture_swipe_v1 - a swipe gesture object
 * @begin: multi-finger swipe begin
 * @update: multi-finger swipe motion
 * @end: multi-finger swipe end
 *
 * A swipe gesture object notifies a client about a multi-finger swipe
 * gesture detected on an indirect input device such as a touchpad. The
 * gesture is usually initiated by multiple fingers moving in the same
 * direction but once initiated the direction may change. The precise
 * conditions of when such a gesture is detected are
 * implementation-dependent.
 *
 * A gesture consists of three stages: begin, update (optional) and end.
 * There cannot be multiple simultaneous pinch or swipe gestures on a same
 * pointer/seat, how compositors prevent these situations is
 * implementation-dependent.
 *
 * A gesture may be cancelled by the compositor or the hardware. Clients
 * should not consider performing permanent or irreversible actions until
 * the end of a gesture has been received.
 */
struct zwp_pointer_gesture_swipe_v1_listener {
	/**
	 * begin - multi-finger swipe begin
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @surface: (none)
	 * @fingers: number of fingers
	 *
	 * This event is sent when a multi-finger swipe gesture is
	 * detected on the device.
	 */
	void (*begin)(void *data,
		      struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1,
		      uint32_t serial,
		      uint32_t time,
		      struct wl_surface *surface,
		      uint32_t fingers);
	/**
	 * update - multi-finger swipe motion
	 * @time: timestamp with millisecond granularity
	 * @dx: delta x coordinate in surface coordinate space
	 * @dy: delta y coordinate in surface coordinate space
	 *
	 * This event is sent when a multi-finger swipe gesture changes
	 * the position of the logical center.
	 *
	 * The dx and dy coordinates are relative coordinates of the
	 * logical center of the gesture compared to the previous event.
	 */
	void (*update)(void *data,
		       struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1,
		       uint32_t time,
		       wl_fixed_t dx,
		       wl_fixed_t dy);
	/**
	 * end - multi-finger swipe end
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @cancelled: 1 if the gesture was cancelled, 0 otherwise
	 *
	 * This event is sent when a multi-finger swipe gesture ceases to
	 * be valid. This may happen when one or more fingers are lifted or
	 * the gesture is cancelled.
	 *
	 * When a gesture is cancelled, the client should undo state
	 * changes caused by this gesture. What causes a gesture to be
	 * cancelled is implementation-dependent.
	 */
	void (*end)(void *data,
		    struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1,
		    uint32_t serial,
		    uint32_t time,
		    int32_t cancelled);
};

static inline int
zwp_pointer_gesture_swipe_v1_add_listener(struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1,
					  const struct zwp_pointer_gesture_swipe_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwp_pointer_gesture_swipe_v1,
				     (void (**)(void)) listener, data);
}

#define ZWP_POINTER_GESTURE_SWIPE_V1_DESTROY	0

static inline void
zwp_pointer_gesture_swipe_v1_set_user_data(struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_pointer_gesture_swipe_v1, user_data);
}

static inline void *
zwp_pointer_gesture_swipe_v1_get_user_data(struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_pointer_gesture_swipe_v1);
}

static inline void
zwp_pointer_gesture_swipe_v1_destroy(struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_pointer_gesture_swipe_v1,
			 ZWP_POINTER_GESTURE_SWIPE_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_pointer_gesture_swipe_v1);
}

/**
 * zwp_pointer_gesture_pinch_v1 - a pinch gesture object
 * @begin: multi-finger pinch begin
 * @update: multi-finger pinch motion
 * @end: multi-finger pinch end
 *
 * A pinch gesture object notifies a client about a multi-finger pinch
 * gesture detected on an indirect input device such as a touchpad. The
 * gesture is usually initiated by multiple fingers moving towards each
 * other or away from each other, or by two or more fingers rotating around
 * a logical center of gravity. The precise conditions of when such a
 * gesture is detected are implementation-dependent.
 *
 * A gesture consists of three stages: begin, update (optional) and end.
 * There cannot be multiple simultaneous pinch or swipe gestures on a same
 * pointer/seat, how compositors prevent these situations is
 * implementation-dependent.
 *
 * A gesture may be cancelled by the compositor or the hardware. Clients
 * should not consider performing permanent or irreversible actions until
 * the end of a gesture has been received.
 */
struct zwp_pointer_gesture_pinch_v1_listener {
	/**
	 * begin - multi-finger pinch begin
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @surface: (none)
	 * @fingers: number of fingers
	 *
	 * This event is sent when a multi-finger pinch gesture is
	 * detected on the device.
	 */
	void (*begin)(void *data,
		      struct zwp_pointer_gesture_pinch_v1 *zwp_pointer_gesture_pinch_v1,
		      uint32_t serial,
		      uint32_t time,
		      struct wl_surface *surface,
		      uint32_t fingers);
	/**
	 * update - multi-finger pinch motion
	 * @time: timestamp with millisecond granularity
	 * @dx: delta x coordinate in surface coordinate space
	 * @dy: delta y coordinate in surface coordinate space
	 * @scale: scale relative to the initial finger position
	 * @rotation: angle in degrees cw relative to the previous event
	 *
	 * This event is sent when a multi-finger pinch gesture changes
	 * the position of the logical center, the rotation or the relative
	 * scale.
	 *
	 * The dx and dy coordinates are relative coordinates in the
	 * surface coordinate space of the logical center of the gesture.
	 *
	 * The scale factor is an absolute scale compared to the
	 * pointer_gesture_pinch.begin event, e.g. a scale of 2 means the
	 * fingers are now twice as far apart as on
	 * pointer_gesture_pinch.begin.
	 *
	 * The rotation is the relative angle in degrees clockwise compared
	 * to the previous pointer_gesture_pinch.begin or
	 * pointer_gesture_pinch.update event.
	 */
	void (*update)(void *data,
		       struct zwp_pointer_gesture_pinch_v1 *zwp_pointer_gesture_pinch_v1,
		       uint32_t time,
		       wl_fixed_t dx,
		       wl_fixed_t dy,
		       wl_fixed_t scale,
		       wl_fixed_t rotation);
	/**
	 * end - multi-finger pinch end
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @cancelled: 1 if the gesture was cancelled, 0 otherwise
	 *
	 * This event is sent when a multi-finger pinch gesture ceases to
	 * be valid. This may happen when one or more fingers are lifted or
	 * the gesture is cancelled.
	 *
	 * When a gesture is cancelled, the client should undo state
	 * changes caused by this gesture. What causes a gesture to be
	 * cancelled is implementation-dependent.
	 */
	void (*end)(void *data,
		    struct zwp_pointer_gesture_pinch_v1 *zwp_pointer_gesture_pinch_v1,
		    uint32_t serial,
		    uint32_t time,
		    int32_t cancelled);
};

static inline int
zwp_pointer_gesture_pinch_v1_add_listener(struct zwp_pointer_gesture_pinch_v1 *zwp_pointer_gesture_pinch_v1,
					  const struct zwp_pointer_gesture_pinch_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwp_pointer_gesture_pinch_v1,
				     (void (**)(void)) listener, data);
}

#define ZWP_POINTER_GESTURE_PINCH_V1_DESTROY	0

static inline void
zwp_pointer_gesture_pinch_v1_set_user_data(struct zwp_pointer_gesture_pinch_v1 *zwp_pointer_gesture_pinch_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_pointer_gesture_pinch_v1, user_data);
}

static inline void *
zwp_pointer_gesture_pinch_v1_get_user_data(struct zwp_pointer_gesture_pinch_v1 *zwp_pointer_gesture_pinch_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_pointer_gesture_pinch_v1);
}

static inline void
zwp_pointer_gesture_pinch_v1_destroy(struct zwp_pointer_gesture_pinch_v1 *zwp_pointer_gesture_pinch_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_pointer_gesture_pinch_v1,
			 ZWP_POINTER_GESTURE_PINCH_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_pointer_gesture_pinch_v1);
}

#ifdef  __cplusplus
}
#endif

#endif
