#ifndef POINTER_GESTURES_UNSTABLE_V1_SERVER_PROTOCOL_H
#define POINTER_GESTURES_UNSTABLE_V1_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

struct wl_client;
struct wl_resource;

struct zwp_pointer_gestures_v1;
struct zwp_pointer_gesture_swipe_v1;
struct zwp_pointer_gesture_pinch_v1;

extern const struct wl_interface zwp_pointer_gestures_v1_interface;
extern const struct wl_interface zwp_pointer_gesture_swipe_v1_interface;
extern const struct wl_interface zwp_pointer_gesture_pinch_v1_interface;

/**
 * zwp_pointer_gestures_v1 - touchpad gestures
 * @get_swipe_gesture: get swipe gesture
 * @get_pinch_gesture: get pinch gesture
 *
 * A global interface to provide semantic touchpad gestures for a given
 * pointer.
 *
 * Two gestures are currently supported: swipe and zoom/rotate. All
 * gestures follow a three-stage cycle: begin, update, end and are
 * identified by a unique id.
 *
 * Warning! The protocol described in this file is experimental and
 * backward incompatible changes may be made. Backward compatible changes
 * may be added together with the corresponding interface version bump.
 * Backward incompatible changes are done by bumping the version number in
 * the protocol and interface names and resetting the interface version.
 * Once the protocol is to be declared stable, the 'z' prefix and the
 * version number in the protocol and interface names are removed and the
 * interface version number is reset.
 */
struct zwp_pointer_gestures_v1_interface {
	/**
	 * get_swipe_gesture - get swipe gesture
	 * @id: (none)
	 * @pointer: (none)
	 *
	 * Create a swipe gesture object. See the
	 * wl_pointer_gesture_swipe interface for details.
	 */
	void (*get_swipe_gesture)(struct wl_client *client,
				  struct wl_resource *resource,
				  uint32_t id,
				  struct wl_resource *pointer);
	/**
	 * get_pinch_gesture - get pinch gesture
	 * @id: (none)
	 * @pointer: (none)
	 *
	 * Create a pinch gesture object. See the
	 * wl_pointer_gesture_pinch interface for details.
	 */
	void (*get_pinch_gesture)(struct wl_client *client,
				  struct wl_resource *resource,
				  uint32_t id,
				  struct wl_resource *pointer);
};

/**
 * zwp_pointer_gesture_swipe_v1 - a swipe gesture object
 * @destroy: destroy the pointer swipe gesture object
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
struct zwp_pointer_gesture_swipe_v1_interface {
	/**
	 * destroy - destroy the pointer swipe gesture object
	 *
	 * 
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define ZWP_POINTER_GESTURE_SWIPE_V1_BEGIN	0
#define ZWP_POINTER_GESTURE_SWIPE_V1_UPDATE	1
#define ZWP_POINTER_GESTURE_SWIPE_V1_END	2

static inline void
zwp_pointer_gesture_swipe_v1_send_begin(struct wl_resource *resource_, uint32_t serial, uint32_t time, struct wl_resource *surface, uint32_t fingers)
{
	wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_SWIPE_V1_BEGIN, serial, time, surface, fingers);
}

static inline void
zwp_pointer_gesture_swipe_v1_send_update(struct wl_resource *resource_, uint32_t time, wl_fixed_t dx, wl_fixed_t dy)
{
	wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_SWIPE_V1_UPDATE, time, dx, dy);
}

static inline void
zwp_pointer_gesture_swipe_v1_send_end(struct wl_resource *resource_, uint32_t serial, uint32_t time, int32_t cancelled)
{
	wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_SWIPE_V1_END, serial, time, cancelled);
}

/**
 * zwp_pointer_gesture_pinch_v1 - a pinch gesture object
 * @destroy: destroy the pinch gesture object
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
struct zwp_pointer_gesture_pinch_v1_interface {
	/**
	 * destroy - destroy the pinch gesture object
	 *
	 * 
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define ZWP_POINTER_GESTURE_PINCH_V1_BEGIN	0
#define ZWP_POINTER_GESTURE_PINCH_V1_UPDATE	1
#define ZWP_POINTER_GESTURE_PINCH_V1_END	2

static inline void
zwp_pointer_gesture_pinch_v1_send_begin(struct wl_resource *resource_, uint32_t serial, uint32_t time, struct wl_resource *surface, uint32_t fingers)
{
	wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_PINCH_V1_BEGIN, serial, time, surface, fingers);
}

static inline void
zwp_pointer_gesture_pinch_v1_send_update(struct wl_resource *resource_, uint32_t time, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t scale, wl_fixed_t rotation)
{
	wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_PINCH_V1_UPDATE, time, dx, dy, scale, rotation);
}

static inline void
zwp_pointer_gesture_pinch_v1_send_end(struct wl_resource *resource_, uint32_t serial, uint32_t time, int32_t cancelled)
{
	wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_PINCH_V1_END, serial, time, cancelled);
}

#ifdef  __cplusplus
}
#endif

#endif
