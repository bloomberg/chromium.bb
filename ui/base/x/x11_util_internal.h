// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_UTIL_INTERNAL_H_
#define UI_BASE_X_X11_UTIL_INTERNAL_H_

// This file declares utility functions for X11 (Linux only).
//
// These functions require the inclusion of the Xlib headers. Since the Xlib
// headers pollute so much of the namespace, this should only be included
// when needed.

extern "C" {
#include <X11/extensions/Xrender.h>
#include <X11/extensions/XShm.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
}

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/x/ui_base_x_export.h"
#include "ui/gfx/x/x11_types.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// --------------------------------------------------------------------------
// NOTE: these functions cache the results and must be called from the UI
// thread.
// Get the XRENDER format id for ARGB32 (Skia's format).
//
// NOTE:Currently this don't support multiple screens/displays.
UI_BASE_X_EXPORT XRenderPictFormat* GetRenderARGB32Format(Display* dpy);

// --------------------------------------------------------------------------
// X11 error handling.
// Sets the X Error Handlers. Passing NULL for either will enable the default
// error handler, which if called will log the error and abort the process.
UI_BASE_X_EXPORT void SetX11ErrorHandlers(XErrorHandler error_handler,
                                          XIOErrorHandler io_error_handler);

// NOTE: This function should not be called directly from the
// X11 Error handler because it queries the server to decode the
// error message, which may trigger other errors. A suitable workaround
// is to post a task in the error handler to call this function.
UI_BASE_X_EXPORT void LogErrorEventDescription(Display* dpy,
                                               const XErrorEvent& error_event);

// --------------------------------------------------------------------------
// Selects a visual with a preference for alpha support on compositing window
// managers.
class UI_BASE_X_EXPORT XVisualManager {
 public:
  static XVisualManager* GetInstance();

  // Picks the best argb or opaque visual given |want_argb_visual|.  If the
  // default visual is returned, |colormap| is set to CopyFromParent.
  void ChooseVisualForWindow(bool want_argb_visual,
                             Visual** visual,
                             int* depth,
                             Colormap* colormap,
                             bool* using_argb_visual);

  // Called by GpuDataManagerImplPrivate when GPUInfo becomes available.  It is
  // necessary for the GPU process to find out which visuals are best for GL
  // because we don't want to load GL in the browser process.  Returns false iff
  // |default_visual_id| or |transparent_visual_id| are invalid.
  bool OnGPUInfoChanged(bool software_rendering,
                        VisualID default_visual_id,
                        VisualID transparent_visual_id);

  // Are all of the system requirements met for using transparent visuals?
  bool ArgbVisualAvailable() const;

  ~XVisualManager();

 private:
  friend struct base::DefaultSingletonTraits<XVisualManager>;

  class XVisualData {
   public:
    explicit XVisualData(XVisualInfo visual_info);
    ~XVisualData();

    Colormap GetColormap();

    const XVisualInfo visual_info;

   private:
    Colormap colormap_;
  };

  XVisualManager();

  std::unordered_map<VisualID, std::unique_ptr<XVisualData>> visuals_;

  XDisplay* display_;

  VisualID default_visual_id_;

  // The system visual is usually the same as the default visual, but
  // may not be in general.
  VisualID system_visual_id_;
  VisualID transparent_visual_id_;

  bool using_compositing_wm_;
  bool using_software_rendering_;
  bool have_gpu_argb_visual_;

  DISALLOW_COPY_AND_ASSIGN(XVisualManager);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_UTIL_INTERNAL_H_
