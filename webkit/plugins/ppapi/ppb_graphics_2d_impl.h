// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_2D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_2D_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCanvas.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace gfx {
class Point;
class Rect;
}

namespace content {
class PepperGraphics2DHost;
}

namespace webkit {
namespace ppapi {

class PPB_ImageData_Impl;
class PluginInstance;

class WEBKIT_PLUGINS_EXPORT PPB_Graphics2D_Impl :
    public ::ppapi::Resource,
    public ::ppapi::thunk::PPB_Graphics2D_API {
 public:
  virtual ~PPB_Graphics2D_Impl();

  static PP_Resource Create(PP_Instance instance,
                            const PP_Size& size,
                            PP_Bool is_always_opaque);

  bool is_always_opaque() const { return is_always_opaque_; }

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_Graphics2D_API* AsPPB_Graphics2D_API();
  virtual void LastPluginRefWasDeleted() OVERRIDE;

  // PPB_Graphics2D functions.
  virtual PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque) OVERRIDE;
  virtual void PaintImageData(PP_Resource image_data,
                              const PP_Point* top_left,
                              const PP_Rect* src_rect) OVERRIDE;
  virtual void Scroll(const PP_Rect* clip_rect,
                      const PP_Point* amount) OVERRIDE;
  virtual void ReplaceContents(PP_Resource image_data) OVERRIDE;
  virtual bool SetScale(float scale) OVERRIDE;
  virtual float GetScale() OVERRIDE;
  virtual int32_t Flush(
      scoped_refptr< ::ppapi::TrackedCallback> callback,
      PP_Resource* old_image_data) OVERRIDE;

  bool ReadImageData(PP_Resource image, const PP_Point* top_left);

  // Assciates this device with the given plugin instance. You can pass NULL to
  // clear the existing device. Returns true on success. In this case, a
  // repaint of the page will also be scheduled. Failure means that the device
  // is already bound to a different instance, and nothing will happen.
  bool BindToInstance(PluginInstance* new_instance);

  // Paints the current backing store to the web page.
  void Paint(WebKit::WebCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

  // Notifications about the view's progress painting.  See PluginInstance.
  // These messages are used to send Flush callbacks to the plugin.
  void ViewWillInitiatePaint();
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  PPB_ImageData_Impl* image_data() { return image_data_.get(); }
  PluginInstance* bound_instance() const { return bound_instance_; }

  // Scale |op_rect| to logical pixels, taking care to include partially-
  // covered logical pixels (aka DIPs). Also scale optional |delta| to logical
  // pixels as well for scrolling cases. Returns false for scrolling cases where
  // scaling either |op_rect| or |delta| would require scrolling to fall back to
  // invalidation due to rounding errors, true otherwise.
  static bool ConvertToLogicalPixels(float scale,
                                     gfx::Rect* op_rect,
                                     gfx::Point* delta);

 private:
  explicit PPB_Graphics2D_Impl(PP_Instance instance);

  bool Init(int width, int height, bool is_always_opaque);

  // Tracks a call to flush that requires a callback.
  class FlushCallbackData {
   public:
    FlushCallbackData() {
      Clear();
    }

    explicit FlushCallbackData(
        scoped_refptr< ::ppapi::TrackedCallback> callback) {
      Set(callback);
    }

    bool is_null() const {
      return !::ppapi::TrackedCallback::IsPending(callback_);
    }

    void Set(scoped_refptr< ::ppapi::TrackedCallback> callback) {
      callback_ = callback;
    }

    void Clear() {
      callback_ = NULL;
    }

    void Execute(int32_t result) {
      callback_->Run(result);
    }

    void PostAbort() {
      if (!is_null()) {
        callback_->PostAbort();
        Clear();
      }
    }

   private:
    scoped_refptr< ::ppapi::TrackedCallback> callback_;
  };

  // Called internally to execute the different queued commands. The
  // parameters to these functions will have already been validated. The last
  // rect argument will be filled by each function with the area affected by
  // the update that requires invalidation. If there were no pixels changed,
  // this rect can be untouched.
  void ExecutePaintImageData(PPB_ImageData_Impl* image,
                             int x, int y,
                             const gfx::Rect& src_rect,
                             gfx::Rect* invalidated_rect);
  void ExecuteScroll(const gfx::Rect& clip, int dx, int dy,
                     gfx::Rect* invalidated_rect);
  void ExecuteReplaceContents(PPB_ImageData_Impl* image,
                              gfx::Rect* invalidated_rect,
                              PP_Resource* old_image_data);

  // Schedules the offscreen callback to be fired at a future time. This
  // will add the given item to the offscreen_flush_callbacks_ vector.
  void ScheduleOffscreenCallback(const FlushCallbackData& callback);

  // Function scheduled to execute by ScheduleOffscreenCallback that actually
  // issues the offscreen callbacks.
  void ExecuteOffscreenCallback(FlushCallbackData data);

  // Returns true if there is any type of flush callback pending.
  bool HasPendingFlush() const;

  scoped_refptr<PPB_ImageData_Impl> image_data_;

  // Non-owning pointer to the plugin instance this context is currently bound
  // to, if any. If the context is currently unbound, this will be NULL.
  PluginInstance* bound_instance_;

  // Keeps track of all drawing commands queued before a Flush call.
  struct QueuedOperation;
  typedef std::vector<QueuedOperation> OperationQueue;
  OperationQueue queued_operations_;

  // The plugin can give us one "Flush" at a time. This flush will either be in
  // the "unpainted" state (in which case unpainted_flush_callback_ will be
  // non-NULL) or painted, in which case painted_flush_callback_ will be
  // non-NULL). There can also be an offscreen callback which is handled
  // separately (see offscreen_callback_pending_). Only one of these three
  // things may be set at a time to enforce the "only one pending flush at a
  // time" constraint.
  //
  // "Unpainted" ones are flush requests which have never been painted. These
  // could have been done while the RenderView was already waiting for an ACK
  // from a previous paint, so won't generate a new one yet.
  //
  // "Painted" ones are those flushes that have been painted by RenderView, but
  // for which the ACK from the browser has not yet been received.
  //
  // When we get updates from a plugin with a callback, it is first added to
  // the unpainted callbacks. When the renderer has initiated the paint, we move
  // it to the painted callback. When the renderer receives a flush, we execute
  // and clear the painted callback. This helps avoid the callback being called
  // prematurely in response to flush notifications for a previous update.
  FlushCallbackData unpainted_flush_callback_;
  FlushCallbackData painted_flush_callback_;

  // When doing offscreen flushes, we issue a task that issues the callback
  // later. This is set when one of those tasks is pending so that we can
  // enforce the "only one pending flush at a time" constraint in the API.
  bool offscreen_flush_pending_;

  // Set to true if the plugin declares that this device will always be opaque.
  // This allows us to do more optimized painting in some cases.
  bool is_always_opaque_;

  // Set to the scale between what the plugin considers to be one pixel and one
  // DIP
  float scale_;

  base::WeakPtrFactory<PPB_Graphics2D_Impl> weak_ptr_factory_;

  friend class content::PepperGraphics2DHost;
  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics2D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_2D_IMPL_H_
