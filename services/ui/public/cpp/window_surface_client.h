// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_SURFACE_CLIENT_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_SURFACE_CLIENT_H_

#include "mojo/public/cpp/bindings/array.h"

namespace cc {
struct ReturnedResource;
}

namespace ui {

class WindowSurface;

class WindowSurfaceClient {
 public:
  virtual void OnResourcesReturned(
      WindowSurface* surface,
      mojo::Array<cc::ReturnedResource> resources) = 0;

 protected:
  virtual ~WindowSurfaceClient() {}
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_SURFACE_CLIENT_H_
