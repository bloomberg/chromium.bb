// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FULLSCREEN_CONTAINER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FULLSCREEN_CONTAINER_IMPL_H_

#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace WebKit {
struct WebCursorInfo;
struct WebRect;
}  // namespace WebKit

namespace webkit {
namespace ppapi {

// This class is like a lightweight WebPluginContainer for fullscreen PPAPI
// plugins, that only handles painting.
class FullscreenContainer {
 public:
  // Invalidates the full plugin region.
  virtual void Invalidate() = 0;

  // Invalidates a partial region of the plugin.
  virtual void InvalidateRect(const WebKit::WebRect&) = 0;

  // Scrolls a partial region of the plugin in the given direction.
  virtual void ScrollRect(int dx, int dy, const WebKit::WebRect&) = 0;

  // Destroys the fullscreen window. This also destroys the FullscreenContainer
  // instance.
  virtual void Destroy() = 0;

  // Notifies the container that the mouse cursor has changed.
  virtual void DidChangeCursor(const WebKit::WebCursorInfo& cursor) = 0;

  virtual PluginDelegate::PlatformContext3D* CreateContext3D() = 0;

  virtual void ReparentContext(PluginDelegate::PlatformContext3D*) = 0;

  virtual void SetLayer(WebKit::WebLayer* layer) = 0;

 protected:
  virtual ~FullscreenContainer() {}
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FULLSCREEN_CONTAINER_IMPL_H_
