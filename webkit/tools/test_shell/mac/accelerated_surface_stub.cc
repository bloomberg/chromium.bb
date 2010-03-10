// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a complete and total hack intended to stub out some classes
// used by WebPluginDelegateImpl on Mac. Unfortunately, they live in
// chrome/common, so we can't compile them into TestShell. Instead, provide
// some stubs. It will need to be updated if new methods are added to
// AcceleratedSurface that get called from WebPluginDelegateImpl. It's not
// like plug-ins work in TestShell anyway.

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/OpenGL.h>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"

class TransportDIB {
 public:
  TransportDIB() { }
  ~TransportDIB() { }
};

class AcceleratedSurface {
 public:
  AcceleratedSurface();
  virtual ~AcceleratedSurface();

  bool Initialize();
  void Destroy();
  uint64 SetSurfaceSize(int32 width, int32 height);
  bool MakeCurrent();
  void Clear(const gfx::Rect& rect);
  void SwapBuffers();
  CGLContextObj context() { return NULL; }
 private:
  scoped_ptr<TransportDIB> ignore_;
};

AcceleratedSurface::AcceleratedSurface() { }
AcceleratedSurface::~AcceleratedSurface() { }
bool AcceleratedSurface::Initialize() { return false; }
void AcceleratedSurface::Destroy() { }
uint64 AcceleratedSurface::SetSurfaceSize(int32 width, int32 height)
    { return 0; }
bool AcceleratedSurface::MakeCurrent() { return false; }
void AcceleratedSurface::Clear(const gfx::Rect& rect) { }
void AcceleratedSurface::SwapBuffers() { }
