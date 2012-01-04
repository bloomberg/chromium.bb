// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/surface/accelerated_surface_win.h"

#include <windows.h>

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/tracked_objects.h"
#include "base/win/wrapped_window_proc.h"
#include "ipc/ipc_message.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/gl/gl_switches.h"

namespace {

typedef HRESULT (WINAPI *Direct3DCreate9ExFunc)(UINT sdk_version,
                                                IDirect3D9Ex **d3d);

const wchar_t kD3D9ModuleName[] = L"d3d9.dll";
const char kCreate3D9DeviceExName[] = "Direct3DCreate9Ex";

class PresentThreadPool {
 public:
  static const int kNumPresentThreads = 4;

  PresentThreadPool();

  int NextThread();

  void PostTask(int thread,
                const tracked_objects::Location& from_here,
                const base::Closure& task);

 private:
  int next_thread_;
  scoped_ptr<base::Thread> present_threads_[kNumPresentThreads];

  DISALLOW_COPY_AND_ASSIGN(PresentThreadPool);
};

base::LazyInstance<PresentThreadPool>
    g_present_thread_pool = LAZY_INSTANCE_INITIALIZER;

PresentThreadPool::PresentThreadPool() : next_thread_(0) {
  for (int i = 0; i < kNumPresentThreads; ++i) {
    present_threads_[i].reset(new base::Thread(
        base::StringPrintf("PresentThread #%d", i).c_str()));
    present_threads_[i]->Start();
  }
}

int PresentThreadPool::NextThread() {
  next_thread_ = (next_thread_ + 1) % kNumPresentThreads;
  return next_thread_;
}

void PresentThreadPool::PostTask(int thread,
                                 const tracked_objects::Location& from_here,
                                 const base::Closure& task) {
  DCHECK_GE(thread, 0);
  DCHECK_LT(thread, kNumPresentThreads);

  present_threads_[thread]->message_loop()->PostTask(from_here, task);
}

UINT GetPresentationInterval() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
    return D3DPRESENT_INTERVAL_IMMEDIATE;
  else
    return D3DPRESENT_INTERVAL_ONE;
}

}  // namespace anonymous

AcceleratedSurface::AcceleratedSurface(HWND parent)
    : thread_affinity_(g_present_thread_pool.Pointer()->NextThread()),
      window_(parent),
      num_pending_resizes_(0) {
}

AcceleratedSurface::~AcceleratedSurface() {
  // Destroy should have been called prior to the last reference going away.
  DCHECK(!device_);
}

void AcceleratedSurface::Initialize() {
  g_present_thread_pool.Pointer()->PostTask(
      thread_affinity_,
      FROM_HERE,
      base::Bind(&AcceleratedSurface::DoInitialize, this));
}

void AcceleratedSurface::Destroy() {
  g_present_thread_pool.Pointer()->PostTask(
      thread_affinity_,
      FROM_HERE,
      base::Bind(&AcceleratedSurface::DoDestroy, this));
}

void AcceleratedSurface::AsyncPresentAndAcknowledge(
    const gfx::Size& size,
    int64 surface_id,
    const base::Closure& completion_task) {
  const int kRound = 64;
  gfx::Size quantized_size(
      std::max(1, (size.width() + kRound - 1) / kRound * kRound),
      std::max(1, (size.height() + kRound - 1) / kRound * kRound));

  if (pending_size_ != quantized_size) {
    pending_size_ = quantized_size;
    base::AtomicRefCountInc(&num_pending_resizes_);

    g_present_thread_pool.Pointer()->PostTask(
        thread_affinity_,
        FROM_HERE,
        base::Bind(&AcceleratedSurface::DoResize, this, quantized_size));
  }

  // This might unnecessarily post to the thread with which the swap chain has
  // affinity. This will only result in potentially delaying the present.
  g_present_thread_pool.Pointer()->PostTask(
      num_pending_resizes_ ?
          thread_affinity_ : g_present_thread_pool.Pointer()->NextThread(),
      FROM_HERE,
      base::Bind(&AcceleratedSurface::DoPresentAndAcknowledge,
                 this,
                 size,
                 surface_id,
                 completion_task));
}

bool AcceleratedSurface::Present() {
  TRACE_EVENT0("surface", "Present");

  HRESULT hr;

  base::AutoLock locked(lock_);

  if (!device_)
    return true;

  RECT rect;
  if (!GetClientRect(window_, &rect))
    return true;

  {
    TRACE_EVENT0("surface", "PresentEx");
    hr = device_->PresentEx(&rect,
                            &rect,
                            NULL,
                            NULL,
                            D3DPRESENT_INTERVAL_IMMEDIATE);

    // If the device hung, force a resize to reset the device.
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEHUNG) {
      base::AtomicRefCountInc(&num_pending_resizes_);
      g_present_thread_pool.Pointer()->PostTask(
          thread_affinity_,
          FROM_HERE,
          base::Bind(&AcceleratedSurface::DoReset, this));

      // A device hang destroys the contents of the back buffer so no point in
      // scheduling a present.
    }

    if (FAILED(hr))
      return false;
  }

  hr = query_->Issue(D3DISSUE_END);
  if (FAILED(hr))
    return true;

  {
    TRACE_EVENT0("surface", "spin");
    do {
      hr = query_->GetData(NULL, 0, D3DGETDATA_FLUSH);

      if (hr == S_FALSE)
        Sleep(0);
    } while (hr == S_FALSE);
  }

  return true;
}

void AcceleratedSurface::DoInitialize() {
  TRACE_EVENT0("surface", "DoInitialize");

  HRESULT hr;

  d3d_module_.Reset(base::LoadNativeLibrary(FilePath(kD3D9ModuleName), NULL));

  Direct3DCreate9ExFunc create_func = reinterpret_cast<Direct3DCreate9ExFunc>(
      d3d_module_.GetFunctionPointer(kCreate3D9DeviceExName));
  if (!create_func)
    return;

  base::win::ScopedComPtr<IDirect3D9Ex> d3d;
  hr = create_func(D3D_SDK_VERSION, d3d.Receive());
  if (FAILED(hr))
    return;

  D3DPRESENT_PARAMETERS parameters = { 0 };
  parameters.BackBufferWidth = 1;
  parameters.BackBufferHeight = 1;
  parameters.BackBufferCount = 1;
  parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
  parameters.hDeviceWindow = window_;
  parameters.Windowed = TRUE;
  parameters.Flags = 0;
  parameters.PresentationInterval = GetPresentationInterval();
  parameters.SwapEffect = D3DSWAPEFFECT_COPY;

  hr = d3d->CreateDeviceEx(
      D3DADAPTER_DEFAULT,
      D3DDEVTYPE_HAL,
      window_,
      D3DCREATE_FPU_PRESERVE | D3DCREATE_SOFTWARE_VERTEXPROCESSING |
          D3DCREATE_MULTITHREADED,
      &parameters,
      NULL,
      device_.Receive());
  if (FAILED(hr))
    return;

  hr = device_->CreateQuery(D3DQUERYTYPE_EVENT, query_.Receive());
  if (FAILED(hr)) {
    device_ = NULL;
    return;
  }

  return;
}

void AcceleratedSurface::DoDestroy() {
  TRACE_EVENT0("surface", "DoDestroy");

  base::AutoLock locked(lock_);

  query_ = NULL;
  device_ = NULL;
}

void AcceleratedSurface::DoResize(const gfx::Size& size) {
  TRACE_EVENT0("surface", "DoResize");
  size_ = size;
  DoReset();
}

void AcceleratedSurface::DoReset() {
  TRACE_EVENT0("surface", "DoReset");

  HRESULT hr;

  base::AtomicRefCountDec(&num_pending_resizes_);

  base::AutoLock locked(lock_);

  if (!device_)
    return;

  D3DPRESENT_PARAMETERS parameters = { 0 };
  parameters.BackBufferWidth = size_.width();
  parameters.BackBufferHeight = size_.height();
  parameters.BackBufferCount = 1;
  parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
  parameters.hDeviceWindow = window_;
  parameters.Windowed = TRUE;
  parameters.Flags = 0;
  parameters.PresentationInterval = GetPresentationInterval();
  parameters.SwapEffect = D3DSWAPEFFECT_COPY;

  hr = device_->ResetEx(&parameters, NULL);
  if (FAILED(hr))
    return;

  device_->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 0, 0);
}

void AcceleratedSurface::DoPresentAndAcknowledge(
    const gfx::Size& size,
    int64 surface_id,
    const base::Closure& completion_task) {
  TRACE_EVENT1("surface", "DoPresentAndAcknowledge", "surface_id", surface_id);

  HRESULT hr;

  base::AutoLock locked(lock_);

  // Ensure the task is always run and while the lock is taken.
  base::ScopedClosureRunner scoped_completion_runner(completion_task);

  if (!device_)
    return;

  if (!window_)
    return;

  HANDLE handle = reinterpret_cast<HANDLE>(surface_id);
  if (!handle)
    return;

  base::win::ScopedComPtr<IDirect3DTexture9> source_texture;
  {
    TRACE_EVENT0("surface", "CreateTexture");
    hr = device_->CreateTexture(size.width(),
                                size.height(),
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                source_texture.Receive(),
                                &handle);
    if (FAILED(hr))
      return;
  }

  base::win::ScopedComPtr<IDirect3DSurface9> source_surface;
  hr = source_texture->GetSurfaceLevel(0, source_surface.Receive());
  if (FAILED(hr))
    return;

  base::win::ScopedComPtr<IDirect3DSurface9> dest_surface;
  hr = device_->GetRenderTarget(0, dest_surface.Receive());
  if (FAILED(hr))
    return;

  RECT rect = {
    0, 0,
    size.width(), size.height()
  };

  {
    TRACE_EVENT0("surface", "StretchRect");
    hr = device_->StretchRect(source_surface,
                              &rect,
                              dest_surface,
                              &rect,
                              D3DTEXF_NONE);
    if (FAILED(hr))
      return;
  }

  hr = query_->Issue(D3DISSUE_END);
  if (FAILED(hr))
    return;

  // Flush so the StretchRect can be processed by the GPU while the window is
  // being resized.
  query_->GetData(NULL, 0, D3DGETDATA_FLUSH);

  ::SetWindowPos(
      window_,
      NULL,
      0, 0,
      size.width(), size.height(),
      SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE |SWP_NOOWNERZORDER |
          SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOSENDCHANGING |
          SWP_ASYNCWINDOWPOS | SWP_NOZORDER);

  // Wait for the StretchRect to complete before notifying the GPU process
  // that it is safe to write to its backing store again.
  {
    TRACE_EVENT0("surface", "spin");
    do {
      hr = query_->GetData(NULL, 0, D3DGETDATA_FLUSH);

      if (hr == S_FALSE)
        Sleep(0);
    } while (hr == S_FALSE);
  }

  scoped_completion_runner.Release();
  if (!completion_task.is_null())
    completion_task.Run();

  {
    TRACE_EVENT0("surface", "Present");
    hr = device_->Present(&rect, &rect, NULL, NULL);

    // If the device hung, force a resize to reset the device.
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEHUNG) {
      base::AtomicRefCountInc(&num_pending_resizes_);
      g_present_thread_pool.Pointer()->PostTask(
          thread_affinity_,
          FROM_HERE,
          base::Bind(&AcceleratedSurface::DoReset, this));

      // A device hang destroys the contents of the back buffer so no point in
      // scheduling a present.
    }
  }
}
