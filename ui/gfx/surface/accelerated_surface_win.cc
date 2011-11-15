// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/surface/accelerated_surface_win.h"

#include <windows.h>

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
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

#pragma comment(lib, "d3d9.lib")

namespace {

const int64 kPollQueryInterval = 1;

class QuerySyncThread
    : public base::Thread,
      public base::RefCounted<QuerySyncThread> {
 public:
  explicit QuerySyncThread(const char* name);
  virtual ~QuerySyncThread();

  // Invoke the completion task when the query completes.
  void AcknowledgeQuery(const base::win::ScopedComPtr<IDirect3DQuery9>& query,
                        const base::Closure& completion_task);

  // Acknowledge all pending queries for the given device early then invoke
  // the given task.
  void AcknowledgeEarly(
      const base::win::ScopedComPtr<IDirect3DDevice9Ex>& device,
      const base::Closure& completion_task);

 private:
  void PollQueries();

  struct PendingQuery {
    base::win::ScopedComPtr<IDirect3DQuery9> query;
    base::Closure completion_task;
  };

  typedef std::list<PendingQuery> PendingQueries;
  PendingQueries pending_queries_;
  base::WeakPtrFactory<QuerySyncThread> poll_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuerySyncThread);
};

class PresentThreadPool {
 public:
  static const int kNumPresentThreads = 4;

  PresentThreadPool();

  int NextThread();

  void PostTask(int thread,
                const tracked_objects::Location& from_here,
                const base::Closure& task);

  void AcknowledgeQuery(const tracked_objects::Location& from_here,
                        const base::win::ScopedComPtr<IDirect3DQuery9>& query,
                        const base::Closure& completion_task);

  void AcknowledgeEarly(
      const tracked_objects::Location& from_here,
      const base::win::ScopedComPtr<IDirect3DDevice9Ex>& device,
      const base::Closure& completion_task);

 private:
  int next_thread_;
  scoped_ptr<base::Thread> present_threads_[kNumPresentThreads];
  scoped_refptr<QuerySyncThread> query_sync_thread_;

  DISALLOW_COPY_AND_ASSIGN(PresentThreadPool);
};

base::LazyInstance<PresentThreadPool>
    g_present_thread_pool = LAZY_INSTANCE_INITIALIZER;

QuerySyncThread::QuerySyncThread(const char* name)
    : base::Thread(name),
      poll_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

QuerySyncThread::~QuerySyncThread() {
}

void QuerySyncThread::AcknowledgeQuery(
    const base::win::ScopedComPtr<IDirect3DQuery9>& query,
    const base::Closure& completion_task) {
  PendingQuery pending_query;
  pending_query.query = query;
  pending_query.completion_task = completion_task;
  pending_queries_.push_back(pending_query);

  // Cancel any pending poll tasks. There should only ever be one pending at a
  // time.
  poll_factory_.InvalidateWeakPtrs();

  PollQueries();
}

void QuerySyncThread::AcknowledgeEarly(
    const base::win::ScopedComPtr<IDirect3DDevice9Ex>& device,
    const base::Closure& completion_task) {
  TRACE_EVENT0("surface", "AcknowledgeEarly");

  PendingQueries::iterator it = pending_queries_.begin();
  while (it != pending_queries_.end()) {
    const PendingQuery& pending_query = *it;

    base::win::ScopedComPtr<IDirect3DDevice9> query_device;
    pending_query.query->GetDevice(query_device.Receive());

    base::win::ScopedComPtr<IDirect3DDevice9Ex> query_device_ex;
    query_device_ex.QueryFrom(query_device.get());

    if (query_device_ex.get() != device.get()) {
      ++it;
    } else {
      pending_query.completion_task.Run();
      it = pending_queries_.erase(it);
    }
  }

  if (!completion_task.is_null())
    completion_task.Run();
}


void QuerySyncThread::PollQueries() {
  TRACE_EVENT0("surface", "PollQueries");

  PendingQueries::iterator it = pending_queries_.begin();
  while (it != pending_queries_.end()) {
    const PendingQuery& pending_query = *it;

    HRESULT hr = pending_query.query->GetData(NULL, 0, D3DGETDATA_FLUSH);
    if (hr == S_FALSE) {
      ++it;
    } else {
      pending_query.completion_task.Run();
      it = pending_queries_.erase(it);
    }
  }

  // Try again later if there are incomplete queries. Otherwise don't poll again
  // until AcknowledgeQuery is called with a new query.
  if (!pending_queries_.empty()) {
    message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&QuerySyncThread::PollQueries, poll_factory_.GetWeakPtr()),
        kPollQueryInterval);
  }
}

PresentThreadPool::PresentThreadPool() : next_thread_(0) {
  for (int i = 0; i < kNumPresentThreads; ++i) {
    present_threads_[i].reset(new base::Thread(
        base::StringPrintf("PresentThread #%d", i).c_str()));
    present_threads_[i]->Start();
  }

  query_sync_thread_ = new QuerySyncThread("QuerySyncThread");
  query_sync_thread_->Start();
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

void PresentThreadPool::AcknowledgeQuery(
    const tracked_objects::Location& from_here,
    const base::win::ScopedComPtr<IDirect3DQuery9>& query,
    const base::Closure& completion_task) {
  query_sync_thread_->message_loop()->PostTask(
      from_here,
      base::Bind(&QuerySyncThread::AcknowledgeQuery,
                 query_sync_thread_,
                 query,
                 completion_task));
}

void PresentThreadPool::AcknowledgeEarly(
    const tracked_objects::Location& from_here,
    const base::win::ScopedComPtr<IDirect3DDevice9Ex>& device,
    const base::Closure& completion_task) {
  query_sync_thread_->message_loop()->PostTask(
      from_here,
      base::Bind(&QuerySyncThread::AcknowledgeEarly,
                 query_sync_thread_,
                 device,
                 completion_task));
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
  g_present_thread_pool.Pointer()->AcknowledgeEarly(
      FROM_HERE,
      device_,
      base::Bind(&AcceleratedSurface::QueriesDestroyed, this));
}

void AcceleratedSurface::AsyncPresentAndAcknowledge(
    const gfx::Size& size,
    int64 surface_id,
    base::Closure completion_task) {
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

void AcceleratedSurface::Present() {
  TRACE_EVENT0("surface", "Present");

  HRESULT hr;

  base::AutoLock locked(lock_);

  if (!device_)
    return;

  RECT rect;
  if (!GetClientRect(window_, &rect))
    return;

  {
    TRACE_EVENT0("surface", "PresentEx");
    hr = device_->PresentEx(&rect,
                            &rect,
                            NULL,
                            NULL,
                            D3DPRESENT_INTERVAL_IMMEDIATE);
    if (FAILED(hr))
      return;
  }

  hr = query_->Issue(D3DISSUE_END);
  if (FAILED(hr))
    return;

  {
    TRACE_EVENT0("surface", "spin");
    do {
      hr = query_->GetData(NULL, 0, D3DGETDATA_FLUSH);

      if (hr == S_FALSE)
        Sleep(0);
    } while (hr == S_FALSE);
  }
}

void AcceleratedSurface::DoInitialize() {
  TRACE_EVENT0("surface", "DoInitialize");

  HRESULT hr;

  base::win::ScopedComPtr<IDirect3D9Ex> d3d;
  hr = Direct3DCreate9Ex(D3D_SDK_VERSION, d3d.Receive());
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

void AcceleratedSurface::QueriesDestroyed() {
  g_present_thread_pool.Pointer()->PostTask(
      thread_affinity_,
      FROM_HERE,
      base::Bind(&AcceleratedSurface::DoDestroy,
                 this));
}

void AcceleratedSurface::DoDestroy() {
  TRACE_EVENT0("surface", "DoDestroy");

  base::AutoLock locked(lock_);

  device_ = NULL;
  query_ = NULL;
}

void AcceleratedSurface::DoResize(const gfx::Size& size) {
  TRACE_EVENT0("surface", "DoResize");

  HRESULT hr;

  base::AtomicRefCountDec(&num_pending_resizes_);

  D3DPRESENT_PARAMETERS parameters = { 0 };
  parameters.BackBufferWidth = size.width();
  parameters.BackBufferHeight = size.height();
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

  size_ = size;

  device_->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 0, 0);
}

void AcceleratedSurface::DoPresentAndAcknowledge(
    const gfx::Size& size,
    int64 surface_id,
    base::Closure completion_task) {
  TRACE_EVENT1("surface", "DoPresentAndAcknowledge", "surface_id", surface_id);

  HRESULT hr;

  base::AutoLock locked(lock_);

  // Ensure the task is always run and while the lock is taken.
  base::ScopedClosureRunner scoped_completion_runner(completion_task);

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
          SWP_ASYNCWINDOWPOS);

  scoped_completion_runner.Release();
  if (!completion_task.is_null()) {
    g_present_thread_pool.Pointer()->AcknowledgeQuery(FROM_HERE,
                                                      query_,
                                                      completion_task);
  }

  {
    TRACE_EVENT0("surface", "Present");
    hr = device_->Present(&rect, &rect, NULL, NULL);
    if (FAILED(hr))
      return;
  }
}
