// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SURFACE_ACCELERATED_SURFACE_WIN_H_
#define UI_GFX_SURFACE_ACCELERATED_SURFACE_WIN_H_
#pragma once

#include <d3d9.h>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_native_library.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_comptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/surface_export.h"

class SURFACE_EXPORT AcceleratedSurface
    : public base::RefCountedThreadSafe<AcceleratedSurface> {
 public:
  explicit AcceleratedSurface(gfx::NativeWindow parent);
  ~AcceleratedSurface();

  void Initialize();
  void Destroy();

  // Schedule a frame to be presented. The completion callback will be invoked
  // when it is safe to write to the surface on another thread. The lock for
  // this surface will be held while the completion callback runs.
  void AsyncPresentAndAcknowledge(const gfx::Size& size,
                                  int64 surface_id,
                                  const base::Closure& completion_task);

  // Synchronously present a frame with no acknowledgement.
  bool Present();

 private:
  void DoInitialize();
  void DoDestroy();
  void DoResize(const gfx::Size& size);
  void DoReset();
  void DoPresentAndAcknowledge(const gfx::Size& size,
                               int64 surface_id,
                               const base::Closure& completion_task);

  // Immutable and accessible from any thread without the lock.
  const int thread_affinity_;
  const gfx::NativeWindow window_;
  base::ScopedNativeLibrary d3d_module_;

  // The size of the swap chain once any pending resizes have been processed.
  // Only accessed on the UI thread so the lock is unnecessary.
  gfx::Size pending_size_;

  // The current size of the swap chain. This is only accessed on the thread
  // with which the surface has affinity.
  gfx::Size size_;

  // The number of pending resizes. This is accessed with atomic operations so
  // the lock is not necessary.
  base::AtomicRefCount num_pending_resizes_;

  // Take the lock before accessing any other state.
  base::Lock lock_;

  // This device's swap chain is presented to the child window. Copy semantics
  // are used so it is possible to represent it to quickly validate the window.
  base::win::ScopedComPtr<IDirect3DDevice9Ex> device_;

  // This query is used to wait until a certain amount of progress has been
  // made by the GPU and it is safe for the producer to modify its shared
  // texture again.
  base::win::ScopedComPtr<IDirect3DQuery9> query_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurface);
};

#endif  // UI_GFX_SURFACE_ACCELERATED_SURFACE_WIN_H_
