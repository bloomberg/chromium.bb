// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_VISIBILITY_CLIENT_H_
#define UI_AURA_CLIENT_VISIBILITY_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {
class RootWindow;
class Window;
namespace client {

// An interface implemented by an object that manages the visibility of Windows'
// layers as Window visibility changes.
class AURA_EXPORT VisibilityClient {
 public:
  // Called when |window|'s visibility is changing to |visible|. The implementor
  // can decide whether or not to pass on the visibility to the underlying
  // layer.
  virtual void UpdateLayerVisibility(Window* window, bool visible) = 0;

 protected:
  virtual ~VisibilityClient() {}
};

// Sets/Gets the VisibilityClient on the RootWindow.
AURA_EXPORT void SetVisibilityClient(RootWindow* root_window,
                                     VisibilityClient* client);
AURA_EXPORT VisibilityClient* GetVisibilityClient(RootWindow* root_window);

}  // namespace clients
}  // namespace aura

#endif  // UI_AURA_CLIENT_VISIBILITY_CLIENT_H_
