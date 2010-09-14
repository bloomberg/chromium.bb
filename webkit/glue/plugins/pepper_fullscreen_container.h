// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FULLSCREEN_CONTAINER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FULLSCREEN_CONTAINER_H_

namespace WebKit {
struct WebRect;
}  // namespace WebKit

namespace pepper {

// This class is like a lightweight WebPluginContainer for fullscreen pepper
// plugins, that only handles painting.
class FullscreenContainer {
 public:
  virtual ~FullscreenContainer() {}

  // Invalidates the full plugin region.
  virtual void Invalidate() = 0;

  // Invalidates a partial region of the plugin.
  virtual void InvalidateRect(const WebKit::WebRect&) = 0;

  // Destroys the fullscreen window. This also destroys the FullscreenContainer
  // instance.
  virtual void Destroy() = 0;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FULLSCREEN_CONTAINER_H_
