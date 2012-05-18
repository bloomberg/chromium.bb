// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/accelerated_surface_win.h"

#include <windows.h>
#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_native_library.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "base/win/wrapped_window_proc.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gl/gl_switches.h"

namespace {

typedef HRESULT (WINAPI *Direct3DCreate9ExFunc)(UINT sdk_version,
                                                IDirect3D9Ex **d3d);

const wchar_t kD3D9ModuleName[] = L"d3d9.dll";
const char kCreate3D9DeviceExName[] = "Direct3DCreate9Ex";

UINT GetPresentationInterval() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
    return D3DPRESENT_INTERVAL_IMMEDIATE;
  else
    return D3DPRESENT_INTERVAL_ONE;
}

// Calculate the number necessary to transform |source_size| into |dest_size|
// by repeating downsampling of the image of |source_size| by a factor no more
// than 2.
int GetResampleCount(const gfx::Size& source_size, const gfx::Size& dest_size) {
  int width_count = 0;
  int width = source_size.width();
  while (width > dest_size.width()) {
    ++width_count;
    width >>= 1;
  }
  int height_count = 0;
  int height = source_size.height();
  while (height > dest_size.height()) {
    ++height_count;
    height >>= 1;
  }
  return std::max(width_count, height_count);
}

// Returns half the size of |size| no smaller than |min_size|.
gfx::Size GetHalfSizeNoLessThan(const gfx::Size& size,
                                const gfx::Size& min_size) {
  return gfx::Size(std::max(min_size.width(), size.width() / 2),
                   std::max(min_size.height(), size.height() / 2));
}

bool CreateTemporarySurface(IDirect3DDevice9* device,
                            const gfx::Size& size,
                            IDirect3DSurface9** surface) {
  HRESULT hr = device->CreateRenderTarget(
        size.width(),
        size.height(),
        D3DFMT_A8R8G8B8,
        D3DMULTISAMPLE_NONE,
        0,
        TRUE,
        surface,
        NULL);
  return SUCCEEDED(hr);
}

}  // namespace anonymous

// A PresentThread is a thread that is dedicated to presenting surfaces to a
// window. It owns a Direct3D device and a Direct3D query for this purpose.
class PresentThread : public base::Thread,
                      public base::RefCountedThreadSafe<PresentThread> {
 public:
  explicit PresentThread(const char* name);

  IDirect3DDevice9Ex* device() { return device_.get(); }
  IDirect3DQuery9* query() { return query_.get(); }

  void InitDevice();
  void ResetDevice();

 protected:
  virtual void CleanUp();

 private:
  friend class base::RefCountedThreadSafe<PresentThread>;

  ~PresentThread();

  base::ScopedNativeLibrary d3d_module_;
  base::win::ScopedComPtr<IDirect3DDevice9Ex> device_;

  // This query is used to wait until a certain amount of progress has been
  // made by the GPU and it is safe for the producer to modify its shared
  // texture again.
  base::win::ScopedComPtr<IDirect3DQuery9> query_;

  DISALLOW_COPY_AND_ASSIGN(PresentThread);
};

// There is a fixed sized pool of PresentThreads and therefore the maximum
// number of Direct3D devices owned by those threads is bounded.
class PresentThreadPool {
 public:
  static const int kNumPresentThreads = 4;

  PresentThreadPool();
  PresentThread* NextThread();

 private:
  int next_thread_;
  scoped_refptr<PresentThread> present_threads_[kNumPresentThreads];

  DISALLOW_COPY_AND_ASSIGN(PresentThreadPool);
};

// A thread safe map of presenters by surface ID that returns presenters via
// a scoped_refptr to keep them alive while they are referenced.
class AcceleratedPresenterMap {
 public:
  AcceleratedPresenterMap();
  scoped_refptr<AcceleratedPresenter> CreatePresenter(gfx::NativeWindow window);
  void RemovePresenter(const scoped_refptr<AcceleratedPresenter>& presenter);
  scoped_refptr<AcceleratedPresenter> GetPresenter(gfx::NativeWindow window);
 private:
  base::Lock lock_;
  typedef std::map<gfx::NativeWindow, AcceleratedPresenter*> PresenterMap;
  PresenterMap presenters_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedPresenterMap);
};

base::LazyInstance<PresentThreadPool>
    g_present_thread_pool = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<AcceleratedPresenterMap>
    g_accelerated_presenter_map = LAZY_INSTANCE_INITIALIZER;

PresentThread::PresentThread(const char* name) : base::Thread(name) {
}

void PresentThread::InitDevice() {
  if (device_)
    return;

  TRACE_EVENT0("gpu", "PresentThread::Init");
  d3d_module_.Reset(base::LoadNativeLibrary(FilePath(kD3D9ModuleName), NULL));
  ResetDevice();
}

void PresentThread::ResetDevice() {
  TRACE_EVENT0("gpu", "PresentThread::ResetDevice");

  // This will crash some Intel drivers but we can't render anything without
  // reseting the device, which would be disappointing.
  query_ = NULL;
  device_ = NULL;

  Direct3DCreate9ExFunc create_func = reinterpret_cast<Direct3DCreate9ExFunc>(
      d3d_module_.GetFunctionPointer(kCreate3D9DeviceExName));
  if (!create_func)
    return;

  base::win::ScopedComPtr<IDirect3D9Ex> d3d;
  HRESULT hr = create_func(D3D_SDK_VERSION, d3d.Receive());
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
          D3DCREATE_DISABLE_PSGP_THREADING | D3DCREATE_MULTITHREADED,
      &parameters,
      NULL,
      device_.Receive());
  if (FAILED(hr))
    return;

  hr = device_->CreateQuery(D3DQUERYTYPE_EVENT, query_.Receive());
  if (FAILED(hr))
    device_ = NULL;
}

void PresentThread::CleanUp() {
  // The D3D device and query are leaked because destroying the associated D3D
  // query crashes some Intel drivers.
  device_.Detach();
  query_.Detach();
}

PresentThread::~PresentThread() {
  Stop();
}

PresentThreadPool::PresentThreadPool() : next_thread_(0) {
  // Do this in the constructor so present_threads_ is initialized before any
  // other thread sees it. See LazyInstance documentation.
  for (int i = 0; i < kNumPresentThreads; ++i) {
    present_threads_[i] = new PresentThread(
        base::StringPrintf("PresentThread #%d", i).c_str());
    present_threads_[i]->Start();
  }
}

PresentThread* PresentThreadPool::NextThread() {
  next_thread_ = (next_thread_ + 1) % kNumPresentThreads;
  return present_threads_[next_thread_].get();
}

AcceleratedPresenterMap::AcceleratedPresenterMap() {
}

scoped_refptr<AcceleratedPresenter> AcceleratedPresenterMap::CreatePresenter(
    gfx::NativeWindow window) {
  scoped_refptr<AcceleratedPresenter> presenter(
      new AcceleratedPresenter(window));

  base::AutoLock locked(lock_);
  DCHECK(presenters_.find(window) == presenters_.end());
  presenters_[window] = presenter.get();

  return presenter;
}

void AcceleratedPresenterMap::RemovePresenter(
    const scoped_refptr<AcceleratedPresenter>& presenter) {
  base::AutoLock locked(lock_);
  for (PresenterMap::iterator it = presenters_.begin();
      it != presenters_.end();
      ++it) {
    if (it->second == presenter.get()) {
      presenters_.erase(it);
      return;
    }
  }

  NOTREACHED();
}

scoped_refptr<AcceleratedPresenter> AcceleratedPresenterMap::GetPresenter(
    gfx::NativeWindow window) {
  base::AutoLock locked(lock_);
  PresenterMap::iterator it = presenters_.find(window);
  if (it == presenters_.end())
    return scoped_refptr<AcceleratedPresenter>();

  return it->second;
}

AcceleratedPresenter::AcceleratedPresenter(gfx::NativeWindow window)
    : present_thread_(g_present_thread_pool.Pointer()->NextThread()),
      window_(window),
      event_(false, false) {
}

scoped_refptr<AcceleratedPresenter> AcceleratedPresenter::GetForWindow(
    gfx::NativeWindow window) {
  return g_accelerated_presenter_map.Pointer()->GetPresenter(window);
}

void AcceleratedPresenter::AsyncPresentAndAcknowledge(
    const gfx::Size& size,
    int64 surface_handle,
    const base::Callback<void(bool)>& completion_task) {
  if (!surface_handle) {
    completion_task.Run(true);
    return;
  }

  present_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoPresentAndAcknowledge,
                 this,
                 size,
                 surface_handle,
                 completion_task));
}

bool AcceleratedPresenter::Present() {
  TRACE_EVENT0("gpu", "Present");

  bool result;

  present_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoPresent,
                 this,
                 &result));
  // http://crbug.com/125391
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  event_.Wait();
  return result;
}

void AcceleratedPresenter::DoPresent(bool* result)
{
  *result = DoRealPresent();
  event_.Signal();
}

bool AcceleratedPresenter::DoRealPresent()
{
  TRACE_EVENT0("gpu", "DoRealPresent");
  HRESULT hr;

  base::AutoLock locked(lock_);

  // Signal the caller to recomposite if the presenter has been suspended or no
  // surface has ever been presented.
  if (!swap_chain_)
    return false;

  // If invalidated, do nothing. The window is gone.
  if (!window_)
    return true;

  RECT rect = {
    0, 0,
    size_.width(), size_.height()
  };

  {
    TRACE_EVENT0("gpu", "PresentEx");
    hr = swap_chain_->Present(&rect,
                              &rect,
                              window_,
                              NULL,
                              D3DPRESENT_INTERVAL_IMMEDIATE);
    if (FAILED(hr))
      return false;
  }

  return true;
}

bool AcceleratedPresenter::CopyTo(const gfx::Size& size, void* buf) {
  base::AutoLock locked(lock_);

  if (!swap_chain_)
    return false;

  base::win::ScopedComPtr<IDirect3DSurface9> back_buffer;
  HRESULT hr = swap_chain_->GetBackBuffer(0,
                                          D3DBACKBUFFER_TYPE_MONO,
                                          back_buffer.Receive());
  if (FAILED(hr))
    return false;

  D3DSURFACE_DESC desc;
  hr = back_buffer->GetDesc(&desc);
  if (FAILED(hr))
    return false;

  const gfx::Size back_buffer_size(desc.Width, desc.Height);
  if (back_buffer_size.IsEmpty())
    return false;

  // Set up intermediate buffers needed for downsampling.
  const int resample_count =
      GetResampleCount(gfx::Size(desc.Width, desc.Height), size);
  base::win::ScopedComPtr<IDirect3DSurface9> final_surface;
  base::win::ScopedComPtr<IDirect3DSurface9> temp_buffer[2];
  if (resample_count == 0)
    final_surface = back_buffer;
  if (resample_count > 0) {
    if (!CreateTemporarySurface(present_thread_->device(),
                                size,
                                final_surface.Receive()))
      return false;
  }
  const gfx::Size half_size = GetHalfSizeNoLessThan(back_buffer_size, size);
  if (resample_count > 1) {
    if (!CreateTemporarySurface(present_thread_->device(),
                                half_size,
                                temp_buffer[0].Receive()))
      return false;
  }
  if (resample_count > 2) {
    const gfx::Size quarter_size = GetHalfSizeNoLessThan(half_size, size);
    if (!CreateTemporarySurface(present_thread_->device(),
                                quarter_size,
                                temp_buffer[1].Receive()))
      return false;
  }

  // Repeat downsampling the surface until its size becomes identical to
  // |size|. We keep the factor of each downsampling no more than two because
  // using a factor more than two can introduce aliasing.
  gfx::Size read_size = back_buffer_size;
  gfx::Size write_size = half_size;
  int read_buffer_index = 1;
  int write_buffer_index = 0;
  for (int i = 0; i < resample_count; ++i) {
    base::win::ScopedComPtr<IDirect3DSurface9> read_buffer =
        (i == 0) ? back_buffer : temp_buffer[read_buffer_index];
    base::win::ScopedComPtr<IDirect3DSurface9> write_buffer =
        (i == resample_count - 1) ? final_surface :
                                    temp_buffer[write_buffer_index];
    RECT read_rect = {0, 0, read_size.width(), read_size.height()};
    RECT write_rect = {0, 0, write_size.width(), write_size.height()};
    hr = present_thread_->device()->StretchRect(read_buffer,
                                                &read_rect,
                                                write_buffer,
                                                &write_rect,
                                                D3DTEXF_LINEAR);
    if (FAILED(hr))
      return false;
    read_size = write_size;
    write_size = GetHalfSizeNoLessThan(write_size, size);
    std::swap(read_buffer_index, write_buffer_index);
  }

  DCHECK(size == read_size);

  base::win::ScopedComPtr<IDirect3DSurface9> temp_surface;
  HANDLE handle = reinterpret_cast<HANDLE>(buf);
  hr =  present_thread_->device()->CreateOffscreenPlainSurface(
    size.width(),
    size.height(),
    D3DFMT_A8R8G8B8,
    D3DPOOL_SYSTEMMEM,
    temp_surface.Receive(),
    &handle);
  if (FAILED(hr))
    return false;

  // Copy the data in the temporary buffer to the surface backed by |buf|.
  hr = present_thread_->device()->GetRenderTargetData(final_surface,
                                                      temp_surface);
  if (FAILED(hr))
    return false;

  return true;
}

void AcceleratedPresenter::Suspend() {
  present_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoSuspend,
                 this));
}

void AcceleratedPresenter::ReleaseSurface() {
  present_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoReleaseSurface,
                 this));
}

void AcceleratedPresenter::Invalidate() {
  // Make any pending or future presentation tasks do nothing. Once the last
  // last pending task has been ignored, the reference count on the presenter
  // will go to zero and the presenter, and potentially also the present thread
  // it has a reference count on, will be destroyed.
  base::AutoLock locked(lock_);
  window_ = NULL;
}

AcceleratedPresenter::~AcceleratedPresenter() {
}

static base::TimeDelta GetSwapDelay() {
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  int delay = 0;
  if (cmd_line->HasSwitch(switches::kGpuSwapDelay)) {
    base::StringToInt(cmd_line->GetSwitchValueNative(
        switches::kGpuSwapDelay).c_str(), &delay);
  }
  return base::TimeDelta::FromMilliseconds(delay);
}

void AcceleratedPresenter::DoPresentAndAcknowledge(
    const gfx::Size& size,
    int64 surface_handle,
    const base::Callback<void(bool)>& completion_task) {
  TRACE_EVENT1(
      "gpu", "DoPresentAndAcknowledge", "surface_handle", surface_handle);

  HRESULT hr;

  base::AutoLock locked(lock_);

  // Initialize the device lazily since calling Direct3D can crash bots.
  present_thread_->InitDevice();

  if (!present_thread_->device()) {
    if (!completion_task.is_null())
      completion_task.Run(false);
    return;
  }

  // Ensure the task is always run and while the lock is taken.
  base::ScopedClosureRunner scoped_completion_runner(base::Bind(completion_task,
                                                                true));

  // If invalidated, do nothing, the window is gone.
  if (!window_)
    return;

  // Round up size so the swap chain is not continuously resized with the
  // surface, which could lead to memory fragmentation.
  const int kRound = 64;
  gfx::Size quantized_size(
      std::max(1, (size.width() + kRound - 1) / kRound * kRound),
      std::max(1, (size.height() + kRound - 1) / kRound * kRound));

  // Ensure the swap chain exists and is the same size (rounded up) as the
  // surface to be presented.
  if (!swap_chain_ || size_ != quantized_size) {
    TRACE_EVENT0("gpu", "CreateAdditionalSwapChain");
    size_ = quantized_size;

    D3DPRESENT_PARAMETERS parameters = { 0 };
    parameters.BackBufferWidth = quantized_size.width();
    parameters.BackBufferHeight = quantized_size.height();
    parameters.BackBufferCount = 1;
    parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    parameters.hDeviceWindow = GetShellWindow();
    parameters.Windowed = TRUE;
    parameters.Flags = 0;
    parameters.PresentationInterval = GetPresentationInterval();
    parameters.SwapEffect = D3DSWAPEFFECT_COPY;

    swap_chain_ = NULL;
    HRESULT hr = present_thread_->device()->CreateAdditionalSwapChain(
        &parameters,
        swap_chain_.Receive());
    if (FAILED(hr))
      return;
  }

  if (!source_texture_.get()) {
    TRACE_EVENT0("gpu", "CreateTexture");
    HANDLE handle = reinterpret_cast<HANDLE>(surface_handle);
    hr = present_thread_->device()->CreateTexture(size.width(),
                                                  size.height(),
                                                  1,
                                                  D3DUSAGE_RENDERTARGET,
                                                  D3DFMT_A8R8G8B8,
                                                  D3DPOOL_DEFAULT,
                                                  source_texture_.Receive(),
                                                  &handle);
    if (FAILED(hr))
      return;
  }

  base::win::ScopedComPtr<IDirect3DSurface9> source_surface;
  hr = source_texture_->GetSurfaceLevel(0, source_surface.Receive());
  if (FAILED(hr))
    return;

  base::win::ScopedComPtr<IDirect3DSurface9> dest_surface;
  hr = swap_chain_->GetBackBuffer(0,
                                  D3DBACKBUFFER_TYPE_MONO,
                                  dest_surface.Receive());
  if (FAILED(hr))
    return;

  RECT rect = {
    0, 0,
    size.width(), size.height()
  };

  {
    TRACE_EVENT0("gpu", "StretchRect");
    hr = present_thread_->device()->StretchRect(source_surface,
                                                &rect,
                                                dest_surface,
                                                &rect,
                                                D3DTEXF_NONE);
    if (FAILED(hr))
      return;
  }

  hr = present_thread_->query()->Issue(D3DISSUE_END);
  if (FAILED(hr))
    return;

  // Flush so the StretchRect can be processed by the GPU while the window is
  // being resized.
  present_thread_->query()->GetData(NULL, 0, D3DGETDATA_FLUSH);

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
    TRACE_EVENT0("gpu", "spin");
    do {
      hr = present_thread_->query()->GetData(NULL, 0, D3DGETDATA_FLUSH);

      if (hr == S_FALSE)
        Sleep(1);
    } while (hr == S_FALSE);
  }

  static const base::TimeDelta swap_delay = GetSwapDelay();
  if (swap_delay.ToInternalValue())
    base::PlatformThread::Sleep(swap_delay);

  {
    TRACE_EVENT0("gpu", "Present");
    hr = swap_chain_->Present(&rect, &rect, window_, NULL, 0);
    if (FAILED(hr) &&
        FAILED(present_thread_->device()->CheckDeviceState(window_))) {
      present_thread_->ResetDevice();
    }
  }
}

void AcceleratedPresenter::DoSuspend() {
  base::AutoLock locked(lock_);
  swap_chain_ = NULL;
}

void AcceleratedPresenter::DoReleaseSurface() {
  base::AutoLock locked(lock_);
  source_texture_.Release();
}

AcceleratedSurface::AcceleratedSurface(gfx::NativeWindow window)
    : presenter_(g_accelerated_presenter_map.Pointer()->CreatePresenter(
          window)) {
}

AcceleratedSurface::~AcceleratedSurface() {
  g_accelerated_presenter_map.Pointer()->RemovePresenter(presenter_);
  presenter_->Invalidate();
}

bool AcceleratedSurface::Present() {
  return presenter_->Present();
}

bool AcceleratedSurface::CopyTo(const gfx::Size& size, void* buf) {
  return presenter_->CopyTo(size, buf);
}

void AcceleratedSurface::Suspend() {
  presenter_->Suspend();
}
