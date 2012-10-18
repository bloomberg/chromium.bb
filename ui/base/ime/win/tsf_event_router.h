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
  typedef base::Callback<void ()> TextUpdatedCallback;
  typedef base::Callback<void (size_t window_count)>
      CandidateWindowCountChangedCallback;

  virtual ~TsfEventRouter();

  // Sets |manager| to be monitored. |manager| can be NULL.
  virtual void SetManager(ITfThreadMgr* manager) = 0;

  // Returns true if the IME is composing texts.
  virtual bool IsImeComposing() = 0;

  // Sets the callback function which is invoked when the text contents is
  // updated.
  virtual void SetTextUpdatedCallback(
      const TextUpdatedCallback& callback) = 0;

  // Sets the callback function which is invoked when the number of currently
  // candidate window opened is changed.
  virtual void SetCandidateWindowStatusChangedCallback(
      const CandidateWindowCountChangedCallback& callback) = 0;

  // Factory function, creates a new instance and retunrns ownership.
  static UI_EXPORT TsfEventRouter* Create();

 protected:
  // Create should be used instead.
  TsfEventRouter();

  DISALLOW_COPY_AND_ASSIGN(TsfEventRouter);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_
