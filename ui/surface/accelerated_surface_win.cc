// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/accelerated_surface_win.h"

#include <dwmapi.h>
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
#include "ui/gfx/rect.h"
#include "ui/gl/gl_switches.h"

namespace {

typedef HRESULT (WINAPI *Direct3DCreate9ExFunc)(UINT sdk_version,
                                                IDirect3D9Ex **d3d);

const wchar_t kD3D9ModuleName[] = L"d3d9.dll";
const char kCreate3D9DeviceExName[] = "Direct3DCreate9Ex";

const char kUseOcclusionQuery[] = "use-occlusion-query";

struct Vertex {
  float x, y, z, w;
  float u, v;
};

// See accelerated_surface_win.hlsl for source and compilation instructions.
const BYTE g_vertexMain[] = {
    0,   2, 254, 255, 254, 255,
   22,   0,  67,  84,  65,  66,
   28,   0,   0,   0,  35,   0,
    0,   0,   0,   2, 254, 255,
    0,   0,   0,   0,   0,   0,
    0,   0,   0,   1,   0,   0,
   28,   0,   0,   0, 118, 115,
   95,  50,  95,  48,   0,  77,
  105,  99, 114, 111, 115, 111,
  102, 116,  32,  40,  82,  41,
   32,  72,  76,  83,  76,  32,
   83, 104,  97, 100, 101, 114,
   32,  67, 111, 109, 112, 105,
  108, 101, 114,  32,  57,  46,
   50,  57,  46,  57,  53,  50,
   46,  51,  49,  49,  49,   0,
   31,   0,   0,   2,   0,   0,
    0, 128,   0,   0,  15, 144,
   31,   0,   0,   2,   5,   0,
    0, 128,   1,   0,  15, 144,
    1,   0,   0,   2,   0,   0,
   15, 192,   0,   0, 228, 144,
    1,   0,   0,   2,   0,   0,
    3, 224,   1,   0, 228, 144,
  255, 255,   0,   0
};

const BYTE g_pixelMain[] = {
    0,   2, 255, 255, 254, 255,
   32,   0,  67,  84,  65,  66,
   28,   0,   0,   0,  75,   0,
    0,   0,   0,   2, 255, 255,
    1,   0,   0,   0,  28,   0,
    0,   0,   0,   1,   0,   0,
   68,   0,   0,   0,  48,   0,
    0,   0,   3,   0,   0,   0,
    1,   0,   0,   0,  52,   0,
    0,   0,   0,   0,   0,   0,
  115,   0, 171, 171,   4,   0,
   12,   0,   1,   0,   1,   0,
    1,   0,   0,   0,   0,   0,
    0,   0, 112, 115,  95,  50,
   95,  48,   0,  77, 105,  99,
  114, 111, 115, 111, 102, 116,
   32,  40,  82,  41,  32,  72,
   76,  83,  76,  32,  83, 104,
   97, 100, 101, 114,  32,  67,
  111, 109, 112, 105, 108, 101,
  114,  32,  57,  46,  50,  57,
   46,  57,  53,  50,  46,  51,
   49,  49,  49,   0,  31,   0,
    0,   2,   0,   0,   0, 128,
    0,   0,   3, 176,  31,   0,
    0,   2,   0,   0,   0, 144,
    0,   8,  15, 160,  66,   0,
    0,   3,   0,   0,  15, 128,
    0,   0, 228, 176,   0,   8,
  228, 160,   1,   0,   0,   2,
    0,   8,  15, 128,   0,   0,
  228, 128, 255, 255,   0,   0
};

const static D3DVERTEXELEMENT9 g_vertexElements[] = {
  { 0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0 },
  { 0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 },
  D3DDECL_END()
};

UINT GetPresentationInterval() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
    return D3DPRESENT_INTERVAL_IMMEDIATE;
  else
    return D3DPRESENT_INTERVAL_ONE;
}

bool UsingOcclusionQuery() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kUseOcclusionQuery);
}

// Calculate the number necessary to transform |src_subrect| into |dst_size|
// by repeating downsampling of the image of |src_subrect| by a factor no more
// than 2.
int GetResampleCount(const gfx::Rect& src_subrect,
                     const gfx::Size& dst_size,
                     const gfx::Size& back_buffer_size) {
  if (src_subrect.size() == dst_size) {
    // Even when the size of |src_subrect| is equal to |dst_size|, it is
    // necessary to resample pixels at least once unless |src_subrect| exactly
    // covers the back buffer.
    return (src_subrect == gfx::Rect(back_buffer_size)) ? 0 : 1;
  }
  int width_count = 0;
  int width = src_subrect.width();
  while (width > dst_size.width()) {
    ++width_count;
    width >>= 1;
  }
  int height_count = 0;
  int height = src_subrect.height();
  while (height > dst_size.height()) {
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

}  // namespace

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
  scoped_refptr<AcceleratedPresenter> CreatePresenter(
      gfx::PluginWindowHandle window);
  void RemovePresenter(const scoped_refptr<AcceleratedPresenter>& presenter);
  scoped_refptr<AcceleratedPresenter> GetPresenter(
      gfx::PluginWindowHandle window);
 private:
  base::Lock lock_;
  typedef std::map<gfx::PluginWindowHandle, AcceleratedPresenter*> PresenterMap;
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

  if (UsingOcclusionQuery()) {
    hr = device_->CreateQuery(D3DQUERYTYPE_OCCLUSION, query_.Receive());
    if (FAILED(hr)) {
      device_ = NULL;
      return;
    }
  } else {
    hr = device_->CreateQuery(D3DQUERYTYPE_EVENT, query_.Receive());
    if (FAILED(hr)) {
      device_ = NULL;
      return;
    }
  }

  base::win::ScopedComPtr<IDirect3DVertexShader9> vertex_shader;
  hr = device_->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vertexMain),
                                   vertex_shader.Receive());
  if (FAILED(hr)) {
    device_ = NULL;
    query_ = NULL;
    return;
  }

  device_->SetVertexShader(vertex_shader);

  base::win::ScopedComPtr<IDirect3DPixelShader9> pixel_shader;
  hr = device_->CreatePixelShader(reinterpret_cast<const DWORD*>(g_pixelMain),
                                  pixel_shader.Receive());

  if (FAILED(hr)) {
    device_ = NULL;
    query_ = NULL;
    return;
  }

  device_->SetPixelShader(pixel_shader);

  base::win::ScopedComPtr<IDirect3DVertexDeclaration9> vertex_declaration;
  hr = device_->CreateVertexDeclaration(g_vertexElements,
                                        vertex_declaration.Receive());
  if (FAILED(hr)) {
    device_ = NULL;
    query_ = NULL;
    return;
  }

  device_->SetVertexDeclaration(vertex_declaration);
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
    gfx::PluginWindowHandle window) {
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
    gfx::PluginWindowHandle window) {
  base::AutoLock locked(lock_);
  PresenterMap::iterator it = presenters_.find(window);
  if (it == presenters_.end())
    return scoped_refptr<AcceleratedPresenter>();

  return it->second;
}

AcceleratedPresenter::AcceleratedPresenter(gfx::PluginWindowHandle window)
    : present_thread_(g_present_thread_pool.Pointer()->NextThread()),
      window_(window),
      event_(false, false),
      hidden_(true) {
}

scoped_refptr<AcceleratedPresenter> AcceleratedPresenter::GetForWindow(
    gfx::PluginWindowHandle window) {
  return g_accelerated_presenter_map.Pointer()->GetPresenter(window);
}

void AcceleratedPresenter::AsyncPresentAndAcknowledge(
    const gfx::Size& size,
    int64 surface_handle,
    const CompletionTask& completion_task) {
  if (!surface_handle) {
    TRACE_EVENT1("gpu", "EarlyOut_ZeroSurfaceHandle",
                 "surface_handle", surface_handle);
    completion_task.Run(true, base::TimeTicks(), base::TimeDelta());
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

bool AcceleratedPresenter::Present(HDC dc) {
  TRACE_EVENT0("gpu", "Present");

  bool result;

  present_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoPresent,
                 this,
                 dc,
                 &result));
  // http://crbug.com/125391
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  event_.Wait();
  return result;
}

void AcceleratedPresenter::DoPresent(HDC dc, bool* result)
{
  *result = DoRealPresent(dc);
  event_.Signal();
}

bool AcceleratedPresenter::DoRealPresent(HDC dc)
{
  TRACE_EVENT0("gpu", "DoRealPresent");
  HRESULT hr;

  base::AutoLock locked(lock_);

  // If invalidated, do nothing. The window is gone.
  if (!window_)
    return true;


  RECT window_rect;
  GetClientRect(window_, &window_rect);
  if (window_rect.right != present_size_.width() ||
      window_rect.bottom != present_size_.height()) {
    // If the window is a different size than the swap chain that was previously
    // presented and it is becoming visible then signal the caller to
    // recomposite at the new size.
    if (hidden_)
      return false;

    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    RECT fill_rect = window_rect;
    fill_rect.top = present_size_.height();
    FillRect(dc, &fill_rect, brush);
    fill_rect = window_rect;
    fill_rect.left = present_size_.width();
    fill_rect.bottom = present_size_.height();
    FillRect(dc, &fill_rect, brush);
  }

  // Signal the caller to recomposite if the presenter has been suspended or no
  // surface has ever been presented.
  if (!swap_chain_)
    return false;

  RECT present_rect = {
    0, 0,
    present_size_.width(), present_size_.height()
  };

  {
    TRACE_EVENT0("gpu", "PresentEx");
    hr = swap_chain_->Present(&present_rect,
                              &present_rect,
                              window_,
                              NULL,
                              D3DPRESENT_INTERVAL_IMMEDIATE);
    // For latency_tests.cc:
    UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete");
    if (FAILED(hr))
      return false;
  }

  return true;
}

bool AcceleratedPresenter::CopyTo(const gfx::Rect& src_subrect,
                                  const gfx::Size& dst_size,
                                  void* buf) {
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
      GetResampleCount(src_subrect, dst_size, back_buffer_size);
  base::win::ScopedComPtr<IDirect3DSurface9> final_surface;
  base::win::ScopedComPtr<IDirect3DSurface9> temp_buffer[2];
  if (resample_count == 0)
    final_surface = back_buffer;
  if (resample_count > 0) {
    if (!CreateTemporarySurface(present_thread_->device(),
                                dst_size,
                                final_surface.Receive()))
      return false;
  }
  const gfx::Size half_size =
      GetHalfSizeNoLessThan(src_subrect.size(), dst_size);
  if (resample_count > 1) {
    if (!CreateTemporarySurface(present_thread_->device(),
                                half_size,
                                temp_buffer[0].Receive()))
      return false;
  }
  if (resample_count > 2) {
    const gfx::Size quarter_size = GetHalfSizeNoLessThan(half_size, dst_size);
    if (!CreateTemporarySurface(present_thread_->device(),
                                quarter_size,
                                temp_buffer[1].Receive()))
      return false;
  }


  // Repeat downsampling the surface until its size becomes identical to
  // |dst_size|. We keep the factor of each downsampling no more than two
  // because using a factor more than two can introduce aliasing.
  RECT read_rect = src_subrect.ToRECT();
  gfx::Size write_size = half_size;
  int read_buffer_index = 1;
  int write_buffer_index = 0;
  for (int i = 0; i < resample_count; ++i) {
    base::win::ScopedComPtr<IDirect3DSurface9> read_buffer =
        (i == 0) ? back_buffer : temp_buffer[read_buffer_index];
    base::win::ScopedComPtr<IDirect3DSurface9> write_buffer =
        (i == resample_count - 1) ? final_surface :
                                    temp_buffer[write_buffer_index];
    RECT write_rect = gfx::Rect(write_size).ToRECT();
    hr = present_thread_->device()->StretchRect(read_buffer,
                                                &read_rect,
                                                write_buffer,
                                                &write_rect,
                                                D3DTEXF_LINEAR);
    if (FAILED(hr))
      return false;
    read_rect = write_rect;
    write_size = GetHalfSizeNoLessThan(write_size, dst_size);
    std::swap(read_buffer_index, write_buffer_index);
  }

  base::win::ScopedComPtr<IDirect3DSurface9> temp_surface;
  HANDLE handle = reinterpret_cast<HANDLE>(buf);
  hr =  present_thread_->device()->CreateOffscreenPlainSurface(
    dst_size.width(),
    dst_size.height(),
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

void AcceleratedPresenter::WasHidden() {
  base::AutoLock locked(lock_);
  hidden_ = true;
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
    const CompletionTask& completion_task) {
  TRACE_EVENT2(
      "gpu", "DoPresentAndAcknowledge",
      "width", size.width(),
      "height", size.height());

  HRESULT hr;

  base::AutoLock locked(lock_);

  // Initialize the device lazily since calling Direct3D can crash bots.
  present_thread_->InitDevice();

  if (!present_thread_->device()) {
    if (!completion_task.is_null())
      completion_task.Run(false, base::TimeTicks(), base::TimeDelta());
    TRACE_EVENT0("gpu", "EarlyOut_NoDevice");
    return;
  }

  // Ensure the task is always run and while the lock is taken.
  base::ScopedClosureRunner scoped_completion_runner(
      base::Bind(completion_task, true, base::TimeTicks(), base::TimeDelta()));

  // If invalidated, do nothing, the window is gone.
  if (!window_) {
    TRACE_EVENT0("gpu", "EarlyOut_NoWindow");
    return;
  }

#if !defined(USE_AURA)
  // If the window is a different size than the swap chain that is being
  // presented then drop the frame.
  RECT window_rect;
  GetClientRect(window_, &window_rect);
  if (hidden_ && (window_rect.right != size.width() ||
      window_rect.bottom != size.height())) {
    TRACE_EVENT2("gpu", "EarlyOut_WrongWindowSize",
                 "backwidth", size.width(), "backheight", size.height());
    TRACE_EVENT2("gpu", "EarlyOut_WrongWindowSize2",
                 "windowwidth", window_rect.right,
                 "windowheight", window_rect.bottom);
    return;
  }
#endif

  // Round up size so the swap chain is not continuously resized with the
  // surface, which could lead to memory fragmentation.
  const int kRound = 64;
  gfx::Size quantized_size(
      std::max(1, (size.width() + kRound - 1) / kRound * kRound),
      std::max(1, (size.height() + kRound - 1) / kRound * kRound));

  // Ensure the swap chain exists and is the same size (rounded up) as the
  // surface to be presented.
  if (!swap_chain_ || quantized_size_ != quantized_size) {
    TRACE_EVENT0("gpu", "CreateAdditionalSwapChain");
    quantized_size_ = quantized_size;

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
  if (FAILED(hr)) {
    TRACE_EVENT0("gpu", "EarlyOut_NoSurfaceLevel");
    return;
  }

  base::win::ScopedComPtr<IDirect3DSurface9> dest_surface;
  hr = swap_chain_->GetBackBuffer(0,
                                  D3DBACKBUFFER_TYPE_MONO,
                                  dest_surface.Receive());
  if (FAILED(hr)) {
    TRACE_EVENT0("gpu", "EarlyOut_NoBackbuffer");
    return;
  }

  RECT rect = {
    0, 0,
    size.width(), size.height()
  };

  {
    TRACE_EVENT0("gpu", "Copy");

    // Use a simple pixel / vertex shader pair to render a quad that flips the
    // source texture on the vertical axis.
    IDirect3DSurface9 *default_render_target = NULL;
    present_thread_->device()->GetRenderTarget(0, &default_render_target);

    present_thread_->device()->SetRenderTarget(0, dest_surface);
    present_thread_->device()->SetTexture(0, source_texture_);

    D3DVIEWPORT9 viewport = {
      0, 0,
      size.width(), size.height(),
      0, 1
    };
    present_thread_->device()->SetViewport(&viewport);

    float halfPixelX = -1.0f / size.width();
    float halfPixelY = 1.0f / size.height();
    Vertex vertices[] = {
      { halfPixelX - 1, halfPixelY + 1, 0.5f, 1, 0, 1 },
      { halfPixelX + 1, halfPixelY + 1, 0.5f, 1, 1, 1 },
      { halfPixelX + 1, halfPixelY - 1, 0.5f, 1, 1, 0 },
      { halfPixelX - 1, halfPixelY - 1, 0.5f, 1, 0, 0 }
    };

    if (UsingOcclusionQuery()) {
      present_thread_->query()->Issue(D3DISSUE_BEGIN);
    }

    present_thread_->device()->BeginScene();
    present_thread_->device()->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,
                                               2,
                                               vertices,
                                               sizeof(vertices[0]));
    present_thread_->device()->EndScene();

    present_thread_->device()->SetTexture(0, NULL);
    present_thread_->device()->SetRenderTarget(0, default_render_target);
    default_render_target->Release();
  }

  hr = present_thread_->query()->Issue(D3DISSUE_END);
  if (FAILED(hr))
    return;

  present_size_ = size;

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
    // For latency_tests.cc:
    UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete");
    if (FAILED(hr) &&
        FAILED(present_thread_->device()->CheckDeviceState(window_))) {
      present_thread_->ResetDevice();
    }
  }

  // Disable call to DwmGetCompositionTimingInfo until we figure out why it
  // causes a flicker of the last software window during tab switch and
  // navigate. crbug.com/143854
#if 0
  {
    TRACE_EVENT0("gpu", "GetPresentationStats");
    base::TimeTicks timebase;
    base::TimeDelta interval;
    uint32 numerator = 0, denominator = 0;
    if (GetPresentationStats(&timebase, &numerator, &denominator) &&
        numerator > 0 && denominator > 0) {
      int64 interval_micros =
          1000000 * static_cast<int64>(numerator) / denominator;
      interval = base::TimeDelta::FromMicroseconds(interval_micros);
    }
    scoped_completion_runner.Release();
    completion_task.Run(true, timebase, interval);
    TRACE_EVENT2("gpu", "GetPresentationStats",
                 "timebase", timebase.ToInternalValue(),
                 "interval", interval.ToInternalValue());
  }
#endif

  hidden_ = false;
}

void AcceleratedPresenter::DoSuspend() {
  base::AutoLock locked(lock_);
  swap_chain_ = NULL;
}

void AcceleratedPresenter::DoReleaseSurface() {
  base::AutoLock locked(lock_);
  source_texture_.Release();
}

bool AcceleratedPresenter::GetPresentationStats(base::TimeTicks* timebase,
                                                uint32* interval_numerator,
                                                uint32* interval_denominator) {
  lock_.AssertAcquired();

  DWM_TIMING_INFO timing_info;
  timing_info.cbSize = sizeof(timing_info);
  HRESULT result = DwmGetCompositionTimingInfo(window_, &timing_info);
  if (result != S_OK)
    return false;

  *timebase = base::TimeTicks::FromQPCValue(
      static_cast<LONGLONG>(timing_info.qpcVBlank));
  // Offset from QPC-space to TimeTicks::Now-space.
  *timebase += (base::TimeTicks::Now() - base::TimeTicks::HighResNow());

  // Swap the numerator/denominator to convert frequency to period.
  *interval_numerator = timing_info.rateRefresh.uiDenominator;
  *interval_denominator = timing_info.rateRefresh.uiNumerator;

  return true;
}

AcceleratedSurface::AcceleratedSurface(gfx::PluginWindowHandle window)
    : presenter_(g_accelerated_presenter_map.Pointer()->CreatePresenter(
          window)) {
}

AcceleratedSurface::~AcceleratedSurface() {
  g_accelerated_presenter_map.Pointer()->RemovePresenter(presenter_);
  presenter_->Invalidate();
}

bool AcceleratedSurface::Present(HDC dc) {
  return presenter_->Present(dc);
}

bool AcceleratedSurface::CopyTo(const gfx::Rect& src_subrect,
                                const gfx::Size& dst_size,
                                void* buf) {
  return presenter_->CopyTo(src_subrect, dst_size, buf);
}

void AcceleratedSurface::Suspend() {
  presenter_->Suspend();
}

void AcceleratedSurface::WasHidden() {
  presenter_->WasHidden();
}
