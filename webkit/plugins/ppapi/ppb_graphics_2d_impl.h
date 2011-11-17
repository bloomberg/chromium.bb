// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCanvas.h"

struct PPB_Graphics2D;

namespace gfx {
class Rect;
}

namespace webkit {
namespace ppapi {

class PPB_ImageData_Impl;
class PluginInstance;

class PPB_Graphics2D_Impl : public ::ppapi::Resource,
                            public ::ppapi::thunk::PPB_Graphics2D_API {
 public:
  virtual ~PPB_Graphics2D_Impl();

  static PP_Resource Create(PP_Instance instance,
                            const PP_Size& size,
                            PP_Bool is_always_opaque);

  bool is_always_opaque() const { return is_always_opaque_; }

  virtual ::ppapi::thunk::PPB_Graphics2D_API* AsPPB_Graphics2D_API();

  // Resource override.
  virtual PPB_Graphics2D_Impl* AsPPB_Graphics2D_Impl();

  // PPB_Graphics2D functions.
  virtual PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque);
  virtual void PaintImageData(PP_Resource image_data,
                              const PP_Point* top_left,
                              const PP_Rect* src_rect);
  virtual void Scroll(const PP_Rect* clip_rect, const PP_Point* amount);
  virtual void ReplaceContents(PP_Resource image_data);
  virtual int32_t Flush(PP_CompletionCallback callback);

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

  // Notifications that the view has rendered the page and that it has been
  // flushed to the screen. These messages are used to send Flush callbacks to
  // the plugin. See
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  PPB_ImageData_Impl* image_data() { return image_data_.get(); }

 private:
  explicit PPB_Graphics2D_Impl(PP_Instance instance);

  bool Init(int width, int height, bool is_always_opaque);

  // Tracks a call to flush that requires a callback.
  class FlushCallbackData {
   public:
    FlushCallbackData() {
      Clear();
    }

    explicit FlushCallbackData(const PP_CompletionCallback& callback) {
      Set(callback);
    }

    bool is_null() const { return !callback_.func; }

    void Set(const PP_CompletionCallback& callback) {
      callback_ = callback;
    }

    void Clear() {
      callback_ = PP_MakeCompletionCallback(NULL, 0);
    }

    void Execute(int32_t result) {
      PP_RunCompletionCallback(&callback_, result);
    }

   private:
    PP_CompletionCallback callback_;
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
                              gfx::Rect* invalidated_rect);

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
  // the unpainted callbacks. When the renderer has initiated a paint, we'll
  // move it to the painted callbacks list. When the renderer receives a flush,
  // we'll execute the callback and remove it from the list.
  FlushCallbackData unpainted_flush_callback_;
  FlushCallbackData painted_flush_callback_;

  // When doing offscreen flushes, we issue a task that issues the callback
  // later. This is set when one of those tasks is pending so that we can
  // enforce the "only one pending flush at a time" constraint in the API.
  bool offscreen_flush_pending_;

  // Set to true if the plugin declares that this device will always be opaque.
  // This allows us to do more optimized painting in some cases.
  bool is_always_opaque_;

  base::WeakPtrFactory<PPB_Graphics2D_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics2D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_2D_IMPL_H_
