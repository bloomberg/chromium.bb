/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_GRAPHICS_2D_H_
#define PPAPI_C_PPB_GRAPHICS_2D_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

struct PP_CompletionCallback;
struct PP_Point;
struct PP_Rect;
struct PP_Size;

#define PPB_GRAPHICS_2D_INTERFACE "PPB_Graphics2D;0.3"

/**
 * @file
 * Defines the PPB_Graphics2D struct.
 *
 * @addtogroup Interfaces
 * @{
 */

/** {PENDING: describe PPB_Graphics2D. */
struct PPB_Graphics2D {
  /**
   * The returned graphics context will not be bound to the plugin instance on
   * creation (call BindGraphics on the plugin instance to do that).
   *
   * Set the is_always_opaque flag if you know that you will be painting only
   * opaque data to this context. This will disable blending when compositing
   * the plugin with the web page, which will give slightly higher performance.
   *
   * If you set is_always_opaque, your alpha channel should always be set to
   * 0xFF or there may be painting artifacts. Being opaque will allow the
   * browser to do a memcpy rather than a blend to paint the plugin, and this
   * means your alpha values will get set on the page backing store. If these
   * values are incorrect, it could mess up future blending.
   *
   * If you aren't sure, it is always correct to specify that it it not opaque.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        const struct PP_Size* size,
                        PP_Bool is_always_opaque);

  /**
   * Returns PP_TRUE if the given resource is a valid Graphics2D, PP_FALSE if it
   * is an invalid resource or is a resource of another type.
   */
  PP_Bool (*IsGraphics2D)(PP_Resource resource);

  /**
   * Retrieves the configuration for the given graphics context, filling the
   * given values (which must not be NULL). On success, returns PP_TRUE. If the
   * resource is invalid, the output parameters will be set to 0 and it will
   * return PP_FALSE.
   */
  PP_Bool (*Describe)(PP_Resource graphics_2d,
                      struct PP_Size* size,
                      PP_Bool* is_always_opqaue);

  /**
   * Enqueues a paint of the given image into the context. THIS HAS NO EFFECT
   * UNTIL YOU CALL Flush(). As a result, what counts is the contents of the
   * bitmap when you call Flush, not when you call this function.
   *
   * The given image will be placed at |top_left| from the top left of the
   * context's internal backing store. Then the src_rect will be copied into the
   * backing store. This parameter may not be NULL. This means that the
   * rectangle being painted will be at src_rect offset by top_left.
   *
   * The src_rect is specified in the coordinate system of the image being
   * painted, not the context. For the common case of copying the entire image,
   * you may specify a NULL |src_rect| pointer. If you are frequently updating
   * the entire image, consider using ReplaceContents which will give slightly
   * higher performance.
   *
   * The painted area of the source bitmap must fall entirely within the
   * context. Attempting to paint outside of the context will result in an
   * error. However, the source bitmap may fall outside the context, as long
   * as the src_rect subset of it falls entirely within the context.
   *
   * There are two modes most plugins may use for painting. The first is
   * that you will generate a new ImageData (possibly representing a subset of
   * your plugin) and then paint it. In this case, you'll set the location of
   * your painting to top_left and set src_rect to NULL. The second is that
   * you're generating small invalid regions out of a larger bitmap
   * representing your entire plugin. In this case, you would set the location
   * of your image to (0,0) and then set src_rect to the pixels you changed.
   */
  void (*PaintImageData)(PP_Resource graphics_2d,
                         PP_Resource image_data,
                         const struct PP_Point* top_left,
                         const struct PP_Rect* src_rect);

  /**
   * Enqueues a scroll of the context's backing store. THIS HAS NO EFFECT UNTIL
   * YOU CALL Flush(). The data within the given clip rect (you may specify
   * NULL to scroll the entire region) will be shifted by (dx, dy) pixels.
   *
   * This will result in some exposed region which will have undefined
   * contents. The plugin should call PaintImageData on these exposed regions
   * to give the correct contents.
   *
   * The scroll can be larger than the area of the clip rect, which means the
   * current image will be scrolled out of the rect. This is not an error but
   * will be a no-op.
   */
  void (*Scroll)(PP_Resource graphics_2d,
                 const struct PP_Rect* clip_rect,
                 const struct PP_Point* amount);

  /**
   * This function provides a slightly more efficient way to paint the entire
   * plugin's image. Normally, calling PaintImageData requires that the browser
   * copy the pixels out of the image and into the graphics context's backing
   * store. This function replaces the graphics context's backing store with the
   * given image, avoiding the copy.
   *
   * The new image must be the exact same size as this graphics context. If the
   * new image uses a different image format than the browser's native bitmap
   * format (use PPB_ImageData.GetNativeImageDataFormat to retrieve this), then
   * a conversion will be done inside the browser which may slow the performance
   * a little bit.
   *
   * THE NEW IMAGE WILL NOT BE PAINTED UNTIL YOU CALL FLUSH.
   *
   * After this call, you should take care to release your references to the
   * image. If you paint to the image after ReplaceContents, there is the
   * possibility of significant painting artifacts because the page might use
   * partially-rendered data when copying out of the backing store.
   *
   * In the case of an animation, you will want to allocate a new image for the
   * next frame. It is best if you wait until the flush callback has executed
   * before allocating this bitmap. This gives the browser the option of
   * caching the previous backing store and handing it back to you (assuming
   * the sizes match). In the optimal case, this means no bitmaps are allocated
   * during the animation, and the backing store and "front buffer" (which the
   * plugin is painting into) are just being swapped back and forth.
   */
  void (*ReplaceContents)(PP_Resource graphics_2d, PP_Resource image_data);

  /**
   * Flushes any enqueued paint, scroll, and replace commands for the backing
   * store. This actually executes the updates, and causes a repaint of the
   * webpage, assuming this graphics context is bound to a plugin instance. This
   * can run in two modes:
   *
   * - In synchronous mode, you specify NULL for the callback and the callback
   *   data. This function will block the calling thread until the image has
   *   been painted to the screen. It is not legal to block the main thread of
   *   the plugin, you can use synchronous mode only from background threads.
   *
   * - In asynchronous mode, you specify a callback function and the argument
   *   for that callback function. The callback function will be executed on
   *   the calling thread when the image has been painted to the screen. While
   *   you are waiting for a Flush callback, additional calls to Flush will
   *   fail.
   *
   * Because the callback is executed (or thread unblocked) only when the
   * plugin's current state is actually on the screen, this function provides a
   * way to rate limit animations. By waiting until the image is on the screen
   * before painting the next frame, you can ensure you're not generating
   * updates faster than the screen can be updated.
   *
   * <dl>
   * <dt>Unbound contexts</dt>
   * <dd>
   *   If the context is not bound to a plugin instance, you will
   *   still get a callback. It will execute after the Flush function returns
   *   to avoid reentrancy. Of course, it will not wait until anything is
   *   painted to the screen because there will be nothing on the screen. The
   *   timing of this callback is not guaranteed and may be deprioritized by
   *   the browser because it is not affecting the user experience.
   * </dd>
   *
   * <dt>Off-screen instances</dt>
   * <dd>
   *   If the context is bound to an instance that is
   *   currently not visible (for example, scrolled out of view) it will behave
   *   like the "unbound context" case.
   * </dd>
   *
   * <dt>Detaching a context</dt>
   * <dd>
   *   If you detach a context from a plugin instance, any
   *   pending flush callbacks will be converted into the "unbound context"
   *   case.
   * </dd>
   *
   * <dt>Released contexts</dt>
   * <dd>
   *   A callback may or may not still get called even if you have released all
   *   of your references to the context. This can occur if there are internal
   *   references to the context that means it has not been internally
   *   destroyed (for example, if it is still bound to an instance) or due to
   *   other implementation details. As a result, you should be careful to
   *   check that flush callbacks are for the context you expect and that
   *   you're capable of handling callbacks for context that you may have
   *   released your reference to.
   * </dd>
   *
   * <dt>Shutdown</dt>
   * <dd>
   *   If a plugin instance is removed when a Flush is pending, the
   *   callback will not be executed.
   * </dd>
   * </dl>
   *
   * Returns PP_OK on success, PP_Error_BadResource if the graphics context is
   * invalid, PP_Error_BadArgument if the callback is null and Flush is being
   * called from the main thread of the plugin, or PP_Error_InProgress if a
   * Flush is already pending that has not issued its callback yet.  In the
   * failure case, nothing will be updated and no callback will be scheduled.
   */
  /* TODO(darin): We should ensure that the completion callback always runs, so
   * that it is easier for consumers to manage memory referenced by a callback.
   */
  int32_t (*Flush)(PP_Resource graphics_2d,
                   struct PP_CompletionCallback callback);

};

/**
 * @}
 */
#endif  /* PPAPI_C_PPB_GRAPHICS_2D_H_ */

