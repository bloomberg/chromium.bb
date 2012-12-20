// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/accelerated_surface_win.h"

#include <dwmapi.h>
#include <windows.h>
#include <algorithm>

#include "accelerated_surface_win_hlsl_compiled.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
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


using ui_surface::AcceleratedSurfaceWinHLSL::kVsOneTexture;
using ui_surface::AcceleratedSurfaceWinHLSL::kPsOneTexture;


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
  // At least one copy is required, since the back buffer itself is not
  // lockable.
  int min_resample_count = 1;
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
  return std::max(std::max(width_count, height_count),
                  min_resample_count);
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
  virtual void Init();
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
  hr = device_->CreateVertexShader(
      reinterpret_cast<const DWORD*>(kVsOneTexture),
      vertex_shader.Receive());
  if (FAILED(hr)) {
    device_ = NULL;
    query_ = NULL;
    return;
  }

  device_->SetVertexShader(vertex_shader);

  base::win::ScopedComPtr<IDirect3DPixelShader9> pixel_shader;
  hr = device_->CreatePixelShader(
      reinterpret_cast<const DWORD*>(kPsOneTexture),
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

void PresentThread::Init() {
  TRACE_EVENT0("gpu", "Initialize thread");
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
}

PresentThread* PresentThreadPool::NextThread() {
  next_thread_ = (next_thread_ + 1) % kNumPresentThreads;
  PresentThread* thread = present_threads_[next_thread_].get();
  if (!thread) {
    thread = new PresentThread(
        base::StringPrintf("PresentThread #%d", next_thread_).c_str());
    thread->Start();
    present_threads_[next_thread_] = thread;
  }

  return thread;
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

#if defined(USE_AURA)
  if (!window)
    return presenters_.begin()->second;
#endif

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
    const CopyCompletionTask& copy_completion_task,
    const PresentCompletionTask& present_completion_task) {
  if (!surface_handle) {
    TRACE_EVENT1("gpu", "EarlyOut_ZeroSurfaceHandle",
                 "surface_handle", surface_handle);
    copy_completion_task.Run(true);
    present_completion_task.Run(base::TimeTicks(), base::TimeDelta());
    return;
  }

  present_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoPresentAndAcknowledge,
                 this,
                 size,
                 surface_handle,
                 copy_completion_task,
                 present_completion_task));
}

void AcceleratedPresenter::Present(HDC dc) {
  TRACE_EVENT0("gpu", "Present");

  base::AutoLock locked(lock_);

  // If invalidated, do nothing. The window is gone.
  if (!window_)
    return;

  // Suspended or nothing has ever been presented.
  if (!swap_chain_)
    return;

  PresentWithGDI(dc);
}

void AcceleratedPresenter::AsyncCopyTo(
    const gfx::Rect& requested_src_subrect,
    const gfx::Size& dst_size,
    void* buf,
    const base::Callback<void(bool)>& callback) {
  present_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AcceleratedPresenter::DoCopyToAndAcknowledge,
                 this,
                 requested_src_subrect,
                 dst_size,
                 buf,
                 base::MessageLoopProxy::current(),
                 callback));
}

void AcceleratedPresenter::DoCopyToAndAcknowledge(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    void* buf,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
    const base::Callback<void(bool)>& callback) {

  bool result = DoCopyTo(src_subrect, dst_size, buf);
  callback_runner->PostTask(
      FROM_HERE,
      base::Bind(callback, result));
}

bool AcceleratedPresenter::DoCopyTo(const gfx::Rect& requested_src_subrect,
                                    const gfx::Size& dst_size,
                                    void* buf) {
  TRACE_EVENT2(
      "gpu", "CopyTo",
      "width", dst_size.width(),
      "height", dst_size.height());

  base::AutoLock locked(lock_);

  TRACE_EVENT0("gpu", "CopyTo_locked");

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

  // With window resizing, it's possible that the back buffer is smaller than
  // the requested src subset. Clip to the actual back buffer.
  gfx::Rect src_subrect = requested_src_subrect;
  src_subrect.Intersect(gfx::Rect(back_buffer_size));

  // Set up intermediate buffers needed for downsampling.
  const int resample_count =
      GetResampleCount(src_subrect, dst_size, back_buffer_size);
  base::win::ScopedComPtr<IDirect3DSurface9> final_surface;
  base::win::ScopedComPtr<IDirect3DSurface9> temp_buffer[2];
  if (resample_count == 0)
    final_surface = back_buffer;
  if (resample_count > 0) {
    TRACE_EVENT0("gpu", "CreateTemporarySurface");
    if (!CreateTemporarySurface(present_thread_->device(),
                                dst_size,
                                final_surface.Receive()))
      return false;
  }
  const gfx::Size half_size =
      GetHalfSizeNoLessThan(src_subrect.size(), dst_size);
  if (resample_count > 1) {
    TRACE_EVENT0("gpu", "CreateTemporarySurface");
    if (!CreateTemporarySurface(present_thread_->device(),
                                half_size,
                                temp_buffer[0].Receive()))
      return false;
  }
  if (resample_count > 2) {
    TRACE_EVENT0("gpu", "CreateTemporarySurface");
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
    TRACE_EVENT0("gpu", "StretchRect");
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
  D3DLOCKED_RECT locked_rect;

  // Empirical evidence seems to suggest that LockRect and memcpy are faster
  // than would be GetRenderTargetData to an offscreen surface wrapping *buf.
  {
    TRACE_EVENT0("gpu", "LockRect");
    hr = final_surface->LockRect(&locked_rect, NULL,
                                 D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK);
    if (FAILED(hr))
      return false;
  }

  {
    TRACE_EVENT0("gpu", "memcpy");
    size_t bytesPerDstRow = 4 * dst_size.width();
    size_t bytesPerSrcRow = locked_rect.Pitch;
    if (bytesPerDstRow == bytesPerSrcRow) {
      memcpy(reinterpret_cast<int8*>(buf),
             reinterpret_cast<int8*>(locked_rect.pBits),
             bytesPerDstRow * dst_size.height());
    } else {
      for (int i = 0; i < dst_size.height(); ++i) {
        memcpy(reinterpret_cast<int8*>(buf) + bytesPerDstRow * i,
               reinterpret_cast<int8*>(locked_rect.pBits) + bytesPerSrcRow * i,
               bytesPerDstRow);
      }
    }
  }
  final_surface->UnlockRect();

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

#if defined(USE_AURA)
void AcceleratedPresenter::SetNewTargetWindow(gfx::PluginWindowHandle window) {
  window_ = window;
}
#endif

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
    const CopyCompletionTask& copy_completion_task,
    const PresentCompletionTask& present_completion_task) {
  TRACE_EVENT2(
      "gpu", "DoPresentAndAcknowledge",
      "width", size.width(),
      "height", size.height());

  HRESULT hr;

  base::AutoLock locked(lock_);

  // Initialize the device lazily since calling Direct3D can crash bots.
  present_thread_->InitDevice();

  if (!present_thread_->device()) {
    if (!copy_completion_task.is_null())
      copy_completion_task.Run(false);
    if (!present_completion_task.is_null())
      present_completion_task.Run(base::TimeTicks(), base::TimeDelta());
    TRACE_EVENT0("gpu", "EarlyOut_NoDevice");
    return;
  }

  // Ensure the task is always run and while the lock is taken.
  base::ScopedClosureRunner scoped_copy_completion_runner(
      base::Bind(copy_completion_task, true));
  base::ScopedClosureRunner scoped_present_completion_runner(
      base::Bind(present_completion_task,
      base::TimeTicks(),
      base::TimeDelta()));

  // If invalidated, do nothing, the window is gone.
  if (!window_) {
    TRACE_EVENT0("gpu", "EarlyOut_NoWindow");
    return;
  }

#if !defined(USE_AURA)
  // If the window is a different size than the swap chain that is being
  // presented then drop the frame.
  gfx::Size window_size = GetWindowSize();
  if (hidden_ && size != window_size) {
    TRACE_EVENT2("gpu", "EarlyOut_WrongWindowSize",
                 "backwidth", size.width(), "backheight", size.height());
    TRACE_EVENT2("gpu", "EarlyOut_WrongWindowSize2",
                 "windowwidth", window_size.width(),
                 "windowheight", window_size.height());
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

  // Acknowledge that the copy is complete and it is safe to modify the shared
  // texture.
  scoped_copy_completion_runner.Release();
  copy_completion_task.Run(true);

  present_size_ = size;

  static const base::TimeDelta swap_delay = GetSwapDelay();
  if (swap_delay.ToInternalValue())
    base::PlatformThread::Sleep(swap_delay);

  // If it is expected that Direct3D cannot be used reliably because the window
  // is resizing, fall back to presenting with GDI.
  if (CheckDirect3DWillWork()) {
    TRACE_EVENT0("gpu", "PresentD3D");

    hr = swap_chain_->Present(&rect, &rect, window_, NULL, 0);

    // For latency_tests.cc:
    UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete");

    if (FAILED(hr) &&
        FAILED(present_thread_->device()->CheckDeviceState(window_))) {
      present_thread_->ResetDevice();
    }
  } else {
    HDC dc = GetDC(window_);
    PresentWithGDI(dc);
    ReleaseDC(window_, dc);
  }

  // Early out if failed to reset device.
  if (!present_thread_->device())
    return;

  hidden_ = false;

  D3DDISPLAYMODE display_mode;
  hr = present_thread_->device()->GetDisplayMode(0, &display_mode);
  if (FAILED(hr))
    return;

  D3DRASTER_STATUS raster_status;
  hr = swap_chain_->GetRasterStatus(&raster_status);
  if (FAILED(hr))
    return;

  // I can't figure out how to determine how many scanlines are in the
  // vertical blank so clamp it such that scanline / height <= 1.
  int clamped_scanline = std::min(raster_status.ScanLine, display_mode.Height);

  // The Internet says that on some GPUs, the scanline is not available
  // while in the vertical blank.
  if (raster_status.InVBlank)
    clamped_scanline = display_mode.Height;

  base::TimeTicks current_time = base::TimeTicks::HighResNow();

  // Figure out approximately how far back in time the last vsync was based on
  // the ratio of the raster scanline to the display height.
  base::TimeTicks last_vsync_time;
  base::TimeDelta refresh_period;
  if (display_mode.Height) {
      last_vsync_time = current_time -
        base::TimeDelta::FromMilliseconds((clamped_scanline * 1000) /
            (display_mode.RefreshRate * display_mode.Height));
      refresh_period = base::TimeDelta::FromMicroseconds(
          1000000 / display_mode.RefreshRate);
  }

  scoped_present_completion_runner.Release();
  present_completion_task.Run(last_vsync_time, refresh_period);
}

void AcceleratedPresenter::DoSuspend() {
  base::AutoLock locked(lock_);
  swap_chain_ = NULL;
}

void AcceleratedPresenter::DoReleaseSurface() {
  base::AutoLock locked(lock_);
  present_thread_->InitDevice();
  source_texture_.Release();
}

void AcceleratedPresenter::PresentWithGDI(HDC dc) {
  TRACE_EVENT0("gpu", "PresentWithGDI");

  if (!present_thread_->device())
    return;

  if (!swap_chain_)
    return;

  base::win::ScopedComPtr<IDirect3DTexture9> system_texture;
  {
    TRACE_EVENT0("gpu", "CreateSystemTexture");
    HRESULT hr = present_thread_->device()->CreateTexture(
        quantized_size_.width(),
        quantized_size_.height(),
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_SYSTEMMEM,
        system_texture.Receive(),
        NULL);
    if (FAILED(hr))
      return;
  }

  base::win::ScopedComPtr<IDirect3DSurface9> system_surface;
  HRESULT hr = system_texture->GetSurfaceLevel(0, system_surface.Receive());
  DCHECK(SUCCEEDED(hr));

  base::win::ScopedComPtr<IDirect3DSurface9> back_buffer;
  hr = swap_chain_->GetBackBuffer(0,
                                  D3DBACKBUFFER_TYPE_MONO,
                                  back_buffer.Receive());
  DCHECK(SUCCEEDED(hr));

  {
    TRACE_EVENT0("gpu", "GetRenderTargetData");
    hr = present_thread_->device()->GetRenderTargetData(back_buffer,
                                                        system_surface);
    DCHECK(SUCCEEDED(hr));
  }

  D3DLOCKED_RECT locked_surface;
  hr = system_surface->LockRect(&locked_surface, NULL, D3DLOCK_READONLY);
  DCHECK(SUCCEEDED(hr));

  BITMAPINFO bitmap_info = {
    {
      sizeof(BITMAPINFOHEADER),
      quantized_size_.width(),
      -quantized_size_.height(),
      1,  // planes
      32,  // bitcount
      BI_RGB
    },
    {
      {0, 0, 0, 0}
    }
  };

  {
    TRACE_EVENT0("gpu", "StretchDIBits");
    StretchDIBits(dc,
                  0, 0,
                  present_size_.width(),
                  present_size_.height(),
                  0, 0,
                  present_size_.width(),
                  present_size_.height(),
                  locked_surface.pBits,
                  &bitmap_info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
  }

  system_surface->UnlockRect();

  // For latency_tests.cc:
  UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete");
}

gfx::Size AcceleratedPresenter::GetWindowSize() {
  RECT rect;
  GetClientRect(window_, &rect);
  return gfx::Rect(rect).size();
}

bool AcceleratedPresenter::CheckDirect3DWillWork() {
  gfx::Size window_size = GetWindowSize();
  if (window_size != last_window_size_ && last_window_size_.GetArea() != 0) {
    last_window_size_ = window_size;
    last_window_resize_time_ = base::Time::Now();
    return false;
  }

  return base::Time::Now() - last_window_resize_time_ >
      base::TimeDelta::FromMilliseconds(100);
}

AcceleratedSurface::AcceleratedSurface(gfx::PluginWindowHandle window)
    : presenter_(g_accelerated_presenter_map.Pointer()->CreatePresenter(
          window)) {
}

AcceleratedSurface::~AcceleratedSurface() {
  g_accelerated_presenter_map.Pointer()->RemovePresenter(presenter_);
  presenter_->Invalidate();
}

void AcceleratedSurface::Present(HDC dc) {
  presenter_->Present(dc);
}

void AcceleratedSurface::AsyncCopyTo(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    void* buf,
    const base::Callback<void(bool)>& callback) {
  presenter_->AsyncCopyTo(src_subrect, dst_size, buf, callback);
}

void AcceleratedSurface::Suspend() {
  presenter_->Suspend();
}

void AcceleratedSurface::WasHidden() {
  presenter_->WasHidden();
}
