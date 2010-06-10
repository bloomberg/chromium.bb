// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_

#include <vector>

#include "base/basictypes.h"
#include "third_party/ppapi/c/ppb_device_context_2d.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"
#include "webkit/glue/plugins/pepper_resource.h"

typedef struct _ppb_DeviceContext2D PPB_DeviceContext2D;

namespace gfx {
class Rect;
}

namespace pepper {

class ImageData;
class PluginInstance;
class PluginModule;

class DeviceContext2D : public Resource {
 public:
  DeviceContext2D(PluginModule* module);
  virtual ~DeviceContext2D();

  // Returns a pointer to the interface implementing PPB_ImageData that is
  // exposed to the plugin.
  static const PPB_DeviceContext2D* GetInterface();

  bool Init(int width, int height, bool is_always_opaque);

  // Resource override.
  virtual DeviceContext2D* AsDeviceContext2D() { return this; }

  // PPB_DeviceContext2D functions.
  bool Describe(int32_t* width, int32_t* height, bool* is_always_opaque);
  bool PaintImageData(PP_Resource image,
                      int32_t x, int32_t y,
                      const PP_Rect* src_rect);
  bool Scroll(const PP_Rect* clip_rect, int32_t dx, int32_t dy);
  bool ReplaceContents(PP_Resource image);
  bool Flush(PPB_DeviceContext2D_FlushCallback callback,
             void* callback_data);
  bool ReadImageData(PP_Resource image, int32_t x, int32_t y);

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

 private:
  // Tracks a call to flush that requires a callback.
  // See unpainted_flush_callbacks_ below.
  class FlushCallbackData {
   public:
    FlushCallbackData(PPB_DeviceContext2D_FlushCallback c, void* d);

    void Execute(PP_Resource device_context);

   private:
    PPB_DeviceContext2D_FlushCallback callback_;
    void* callback_data_;
  };
  typedef std::vector<FlushCallbackData> FlushCallbackVector;

  // Called internally to execute the different queued commands. The
  // parameters to these functions will have already been validated. The last
  // rect argument will be filled by each function with the area affected by
  // the update that requires invalidation. If there were no pixels changed,
  // this rect can be untouched.
  void ExecutePaintImageData(ImageData* image,
                             int x, int y,
                             const gfx::Rect& src_rect,
                             gfx::Rect* invalidated_rect);
  void ExecuteScroll(const gfx::Rect& clip, int dx, int dy,
                     gfx::Rect* invalidated_rect);
  void ExecuteReplaceContents(ImageData* image,
                              gfx::Rect* invalidated_rect);

  // Schedules the offscreen callback to be fired at a future time. This
  // will add the given item to the offscreen_flush_callbacks_ vector.
  void ScheduleOffscreenCallback(const FlushCallbackData& callback);

  // Function scheduled to execute by ScheduleOffscreenCallback that actually
  // issues the offscreen callbacks.
  void ExecuteOffscreenCallback(FlushCallbackData data);

  scoped_refptr<ImageData> image_data_;

  // Non-owning pointer to the plugin instance this device context is currently
  // bound to, if any. If the device context is currently unbound, this will
  // be NULL.
  PluginInstance* bound_instance_;

  // Keeps track of all drawing commands queued before a Flush call.
  struct QueuedOperation;
  typedef std::vector<QueuedOperation> OperationQueue;
  OperationQueue queued_operations_;

  // Indicates whether any changes have been flushed to the backing store.
  // This is initially false and is set to true at the first Flush() call.
  bool flushed_any_data_;

  // The plugin may be constantly giving us paint messages. "Unpainted" ones
  // are paint requests which have never been painted. These could have been
  // done while the RenderView was already waiting for an ACK from a previous
  // paint, so won't generate a new one yet.
  //
  // "Painted" ones are those paints that have been painted by RenderView, but
  // for which the ACK from the browser has not yet been received.
  //
  // When we get updates from a plugin with a callback, it is first added to
  // the unpainted callbacks. When the renderer has initiated a paint, we'll
  // move it to the painted callbacks list. When the renderer receives a flush,
  // we'll execute the callback and remove it from the list.
  FlushCallbackVector unpainted_flush_callbacks_;
  FlushCallbackVector painted_flush_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(DeviceContext2D);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_
