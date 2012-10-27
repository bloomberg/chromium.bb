// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_
#define UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <msctf.h>

#include <set>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_export.h"

struct ITfDocumentMgr;

namespace ui {

class TsfEventRouterObserver {
 public:
  TsfEventRouterObserver() {}

  // Called when the text contents are updated.
  virtual void OnTextUpdated() = 0;

  // Called when the number of currently opened candidate windows changes.
  virtual void OnCandidateWindowCountChanged(size_t window_count) = 0;

 protected:
  virtual ~TsfEventRouterObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TsfEventRouterObserver);
};

// This class monitores TSF related events and forwards them to given
// |observer|.
class UI_EXPORT TsfEventRouter {
 public:
  // Do not pass NULL to |observer|.
  explicit TsfEventRouter(TsfEventRouterObserver* observer);
  virtual ~TsfEventRouter();

  // Sets |thread_manager| to be monitored. |thread_manager| can be NULL.
  void SetManager(ITfThreadMgr* thread_manager);

  // Returns true if the IME is composing text.
  bool IsImeComposing();

  // Callbacks from the TsfEventRouterDelegate:
  void OnTextUpdated();
  void OnCandidateWindowCountChanged(size_t window_count);

 private:
  class TsfEventRouterDelegate;

  CComPtr<TsfEventRouterDelegate> delegate_;

  TsfEventRouterObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(TsfEventRouter);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_
