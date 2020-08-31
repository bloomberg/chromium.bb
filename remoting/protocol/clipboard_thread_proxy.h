// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ClipboardThreadProxy is used to allow a Clipboard on the UI thread to invoke
// a ClipboardStub on the network thread.

#ifndef REMOTING_PROTOCOL_CLIPBOARD_THREAD_PROXY_H_
#define REMOTING_PROTOCOL_CLIPBOARD_THREAD_PROXY_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {
namespace protocol {

class ClipboardThreadProxy : public ClipboardStub {
 public:
  ~ClipboardThreadProxy() override;

  // Constructs a proxy for |clipboard_stub| which will trampoline invocations
  // to |clipboard_stub_task_runner|.
  ClipboardThreadProxy(
      const base::WeakPtr<ClipboardStub>& clipboard_stub,
      scoped_refptr<base::TaskRunner> clipboard_stub_task_runner);

  // ClipboardStub implementation.
  void InjectClipboardEvent(const ClipboardEvent& event) override;

 private:
  // Injects a clipboard event into a stub, if the given weak pointer to the
  // stub is valid.
  static void InjectClipboardEventStatic(
      const base::WeakPtr<ClipboardStub>& clipboard_stub,
      const ClipboardEvent& event);

  base::WeakPtr<ClipboardStub> clipboard_stub_;
  scoped_refptr<base::TaskRunner> clipboard_stub_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardThreadProxy);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIPBOARD_THREAD_PROXY_H_
