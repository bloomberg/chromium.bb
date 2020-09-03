/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_rendereriothread.h>

#include <base/bind.h>
#include <base/lazy_instance.h>
#include <base/threading/thread_local.h>
#include <float.h>

namespace blpwtk2 {

base::LazyInstance<base::ThreadLocalPointer<RendererIOThread>>::DestructorAtExit
    g_lazy_tls = LAZY_INSTANCE_INITIALIZER;

// static
LRESULT CALLBACK RendererIOThread::windowProcedure(NativeView window_handle,
                                                   UINT message,
                                                   WPARAM wparam,
                                                   LPARAM lparam) {
  if (WM_USER + 1 == message) {
    RendererIOThread* self = g_lazy_tls.Pointer()->Get();
    DCHECK(self);

    if (self) {
      self->wait_set_->Wait(
          self->ready_event_, self->num_ready_handles_, self->ready_handles_,
          self->ready_results_, self->signals_states_);
    }

    return S_OK;
  }

  return ::DefWindowProc(window_handle, message, wparam, lparam);
}

void RendererIOThread::ThreadMain() {
  static const wchar_t* class_name;

  if (nullptr == class_name) {
    class_name = L"blpwtk2::RendererIOThread";

    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = &windowProcedure;
    window_class.lpszClassName = class_name;

    ATOM atom = ::RegisterClassW(&window_class);
    CHECK(atom);
  }

  DCHECK(!g_lazy_tls.Pointer()->Get());
  g_lazy_tls.Pointer()->Set(this);

  window_ = ::CreateWindowW(class_name,    // lpClassName
                            0,             // lpWindowName
                            0,             // dwStyle
                            CW_DEFAULT,    // x
                            CW_DEFAULT,    // y
                            CW_DEFAULT,    // nWidth
                            CW_DEFAULT,    // nHeight
                            HWND_MESSAGE,  // hwndParent
                            0,             // hMenu
                            0,             // hInstance
                            0);            // lpParam

  DCHECK(window_);
  DCHECK(init_wait_event_);
  init_wait_event_->Signal();

  BOOL ret_val;
  MSG msg;
  while (0 != (ret_val = ::GetMessage(&msg, 0, 0, 0))) {
    if (-1 == ret_val || WM_DESTROY == msg.message) {
      break;
    } else {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }
}

RendererIOThread::RendererIOThread() {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  init_wait_event_ = &event;

  base::PlatformThread::Create(0, this, &thread_handle_);
  init_wait_event_->Wait();
  init_wait_event_ = 0;

  proxy_ = base::BindRepeating(&RendererIOThread::Wait,
                               base::Unretained(this));
}

RendererIOThread::~RendererIOThread() {
  ::DestroyWindow(window_);
  base::PlatformThread::Join(thread_handle_);
}

mojo::UIWaitProxy* RendererIOThread::proxy() {
  return &proxy_;
}

void RendererIOThread::Wait(
    mojo::WaitSet* wait_set,
    base::WaitableEvent** ready_event,
    size_t* num_ready_handles,
    mojo::Handle* ready_handles,
    MojoResult* ready_results,
    MojoHandleSignalsState* signals_states)
{
  wait_set_ = wait_set;
  ready_event_ = ready_event;
  num_ready_handles_ = num_ready_handles;
  ready_handles_ = ready_handles;
  ready_results_ = ready_results;
  signals_states_ = signals_states;
  ::SendMessage(window_, WM_USER + 1, 0, 0);
}

}  // namespace blpwtk2

// vim: ts=4 et
