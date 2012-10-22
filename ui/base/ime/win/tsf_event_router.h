// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_
#define UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_

#include <msctf.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_export.h"

struct ITfDocumentMgr;

namespace ui {

// This is an abstract interface that monitors events associated with TSF and
// forwards them to the observer. In order to manage the life cycle of this
// object by scoped_refptr and the implementation class of this interface is COM
// class anyway, this interface is derived from IUnknown.
class TsfEventRouter : public IUnknown {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the text contents are updated.
    virtual void OnTextUpdated() = 0;

    // Called when the number of currently opened candidate windows changes.
    virtual void OnCandidateWindowCountChanged(size_t window_count) = 0;
  };

  virtual ~TsfEventRouter();

  // Sets |manager| to be monitored and |observer| to be notified. |manager| and
  // |observer| can be NULL.
  virtual void SetManager(ITfThreadMgr* manager,
                          Observer* observer) = 0;

  // Returns true if the IME is composing texts.
  virtual bool IsImeComposing() = 0;

  // Factory function, creates a new instance which the caller owns.
  static UI_EXPORT TsfEventRouter* Create();

 protected:
  // Create should be used instead.
  TsfEventRouter();

  DISALLOW_COPY_AND_ASSIGN(TsfEventRouter);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_
