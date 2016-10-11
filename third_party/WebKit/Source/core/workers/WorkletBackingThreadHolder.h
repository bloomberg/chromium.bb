// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletBackingThreadHolder_h
#define WorkletBackingThreadHolder_h

#include "core/CoreExport.h"
#include "wtf/PtrUtil.h"

namespace blink {

class WorkerBackingThread;
class WaitableEvent;

class CORE_EXPORT WorkletBackingThreadHolder {
 public:
  WorkerBackingThread* thread() { return m_thread.get(); }

 protected:
  explicit WorkletBackingThreadHolder(std::unique_ptr<WorkerBackingThread>);
  virtual void initializeOnThread() = 0;
  void shutdownAndWait();
  void shutdownOnThread(WaitableEvent*);

  std::unique_ptr<WorkerBackingThread> m_thread;
  bool m_initialized;
};

}  // namespace blink

#endif  // WorkletBackingThreadHolder_h
