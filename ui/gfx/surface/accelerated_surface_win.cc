// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/surface/accelerated_surface_win.h"

#include <d3d9.h>
#include <windows.h>

#include <list>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_native_library.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/tracked_objects.h"
#include "base/win/scoped_comptr.h"
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

base::LazyInstance<std::vector<linked_ptr<AcceleratedPresenter> > >
    g_unused_accelerated_presenters;

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

class AcceleratedPresenter {
 public:
  AcceleratedPresenter();
  ~AcceleratedPresenter();

  void AsyncPresentAndAcknowledge(gfx::NativeWindow window,
                                  const gfx::Size& size,
                                  int64 surface_id,
                                  const base::Closure& completion_task);

  bool Present(gfx::NativeWindow window);

  void Suspend();

 private:
  void DoInitialize();
  void DoResize(const gfx::Size& size);
  void DoReset();
  void DoPresentAndAcknowledge(gfx::NativeWindow window,
                               const gfx::Size& size,
                               int64 surface_id,
                               const base::Closure& completion_task);

  // Immutable and accessible from any thread without the lock.
  const int thread_affinity_;
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

  DISALLOW_COPY_AND_ASSIGN(AcceleratedPresenter);
};

AcceleratedPresenter::AcceleratedPresenter()
    : thread_affinity_(g_present_thread_pool.Pointer()->NextThread()),
      num_pending_resizes_(0) {
  g_present_thread_pool.Pointer()->PostTask(
      thread_affinity_,
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoInitialize, base::Unretained(this)));
}

AcceleratedPresenter::~AcceleratedPresenter() {
  // The D3D device and query are leaked because destroying the associated D3D
  // query crashes some Intel drivers.
  device_.Detach();
  query_.Detach();
}

void AcceleratedPresenter::AsyncPresentAndAcknowledge(
    gfx::NativeWindow window,
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
        base::Bind(&AcceleratedPresenter::DoResize,
                   base::Unretained(this),
                   quantized_size));
  }

  // This might unnecessarily post to the thread with which the swap chain has
  // affinity. This will only result in potentially delaying the present.
  g_present_thread_pool.Pointer()->PostTask(
      num_pending_resizes_ ?
          thread_affinity_ : g_present_thread_pool.Pointer()->NextThread(),
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoPresentAndAcknowledge,
                 base::Unretained(this),
                 window,
                 size,
                 surface_id,
                 completion_task));
}

bool AcceleratedPresenter::Present(gfx::NativeWindow window) {
  TRACE_EVENT0("surface", "Present");

  HRESULT hr;

  base::AutoLock locked(lock_);

  if (!device_)
    return true;

  RECT rect;
  if (!GetClientRect(window, &rect))
    return true;

  {
    TRACE_EVENT0("surface", "PresentEx");
    hr = device_->PresentEx(&rect,
                            &rect,
                            window,
                            NULL,
                            D3DPRESENT_INTERVAL_IMMEDIATE);

    // If the device hung, force a resize to reset the device.
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEHUNG) {
      base::AtomicRefCountInc(&num_pending_resizes_);
      g_present_thread_pool.Pointer()->PostTask(
          thread_affinity_,
          FROM_HERE,
          base::Bind(&AcceleratedPresenter::DoReset, base::Unretained(this)));

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

void AcceleratedPresenter::Suspend() {
  // Resize the swap chain to 1 x 1 to save memory while the presenter is not
  // in use.
  pending_size_ = gfx::Size(1, 1);
  base::AtomicRefCountInc(&num_pending_resizes_);

  g_present_thread_pool.Pointer()->PostTask(
      thread_affinity_,
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoResize,
                 base::Unretained(this),
                 gfx::Size(1, 1)));
}

void AcceleratedPresenter::DoInitialize() {
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

  // Any old window will do to create the device. In practice the window to
  // present to is an argument to IDirect3DDevice9::Present.
  HWND window = GetShellWindow();

  D3DPRESENT_PARAMETERS parameters = { 0 };
  parameters.BackBufferWidth = 1;
  parameters.BackBufferHeight = 1;
  parameters.BackBufferCount = 1;
  parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
  parameters.hDeviceWindow = window;
  parameters.Windowed = TRUE;
  parameters.Flags = 0;
  parameters.PresentationInterval = GetPresentationInterval();
  parameters.SwapEffect = D3DSWAPEFFECT_COPY;

  hr = d3d->CreateDeviceEx(
      D3DADAPTER_DEFAULT,
      D3DDEVTYPE_HAL,
      window,
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

void AcceleratedPresenter::DoResize(const gfx::Size& size) {
  TRACE_EVENT0("surface", "DoResize");
  size_ = size;
  DoReset();
}

void AcceleratedPresenter::DoReset() {
  TRACE_EVENT0("surface", "DoReset");

  HRESULT hr;

  base::AtomicRefCountDec(&num_pending_resizes_);

  base::AutoLock locked(lock_);

  if (!device_)
    return;

  gfx::NativeWindow window = GetShellWindow();

  D3DPRESENT_PARAMETERS parameters = { 0 };
  parameters.BackBufferWidth = size_.width();
  parameters.BackBufferHeight = size_.height();
  parameters.BackBufferCount = 1;
  parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
  parameters.hDeviceWindow = window;
  parameters.Windowed = TRUE;
  parameters.Flags = 0;
  parameters.PresentationInterval = GetPresentationInterval();
  parameters.SwapEffect = D3DSWAPEFFECT_COPY;

  hr = device_->ResetEx(&parameters, NULL);
  if (FAILED(hr))
    return;

  device_->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 0, 0);
}

void AcceleratedPresenter::DoPresentAndAcknowledge(
    gfx::NativeWindow window,
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
      window,
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
    hr = device_->Present(&rect, &rect, window, NULL);

    // If the device hung, force a resize to reset the device.
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEHUNG) {
      base::AtomicRefCountInc(&num_pending_resizes_);
      g_present_thread_pool.Pointer()->PostTask(
          thread_affinity_,
          FROM_HERE,
          base::Bind(&AcceleratedPresenter::DoReset, base::Unretained(this)));

      // A device hang destroys the contents of the back buffer so no point in
      // scheduling a present.
    }
  }
}

AcceleratedSurface::AcceleratedSurface() {
  if (g_unused_accelerated_presenters.Pointer()->empty()) {
    presenter_.reset(new AcceleratedPresenter);
  } else {
    presenter_ = g_unused_accelerated_presenters.Pointer()->back();
    g_unused_accelerated_presenters.Pointer()->pop_back();
  }
}

AcceleratedSurface::~AcceleratedSurface() {
  presenter_->Suspend();
  g_unused_accelerated_presenters.Pointer()->push_back(presenter_);
}

void AcceleratedSurface::AsyncPresentAndAcknowledge(
    HWND window,
    const gfx::Size& size,
    int64 surface_id,
    const base::Closure& completion_task) {
  presenter_->AsyncPresentAndAcknowledge(window,
                                         size,
                                         surface_id,
                                         completion_task);
}

bool AcceleratedSurface::Present(HWND window) {
  return presenter_->Present(window);
}
