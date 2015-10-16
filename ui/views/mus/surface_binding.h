// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SURFACE_BINDING_H_
#define UI_VIEWS_MUS_SURFACE_BINDING_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace cc {
class OutputSurface;
}

namespace mojo {
class Shell;
}

namespace mus {
class Window;
}

namespace views {

// SurfaceBinding is responsible for managing the connections necessary to
// bind a Window to the surfaces service.
// Internally SurfaceBinding manages one connection (and related structures) per
// ViewTreeConnection. That is, all Views from a particular ViewTreeConnection
// share the same connection.
class SurfaceBinding {
 public:
  SurfaceBinding(mojo::Shell* shell, mus::Window* window);
  ~SurfaceBinding();

  // Creates an OutputSurface that renders to the Window supplied to the
  // constructor.
  scoped_ptr<cc::OutputSurface> CreateOutputSurface();

 private:
  class PerConnectionState;

  mojo::Shell* shell_;
  mus::Window* window_;
  scoped_refptr<PerConnectionState> state_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceBinding);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SURFACE_BINDING_H_
