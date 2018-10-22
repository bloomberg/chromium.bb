// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A stand-in for stuff that expects a weak pointer to a BaseTask for
// testing.

#ifndef JINGLE_NOTIFIER_FAKE_XMPP_CLIENT_H_
#define JINGLE_NOTIFIER_FAKE_XMPP_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "jingle/glue/task_pump.h"

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace notifier {

class FakeBaseTask {
 public:
  FakeBaseTask();
  ~FakeBaseTask();

  base::WeakPtr<buzz::XmppTaskParentInterface> AsWeakPtr();

 private:
  jingle_glue::TaskPump task_pump_;
  base::WeakPtr<buzz::XmppTaskParentInterface> base_task_;

  DISALLOW_COPY_AND_ASSIGN(FakeBaseTask);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_FAKE_XMPP_CLIENT_H_
