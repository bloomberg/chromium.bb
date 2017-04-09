// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPageImportanceSignals_h
#define WebPageImportanceSignals_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebViewClient;

// WebPageImportanceSignals indicate the importance of the page state to user.
// This signal is propagated to embedder so that it can prioritize preserving
// state of certain page over the others.
class WebPageImportanceSignals {
 public:
  WebPageImportanceSignals() { Reset(); }

  bool HadFormInteraction() const { return had_form_interaction_; }
  void SetHadFormInteraction();
  bool IssuedNonGetFetchFromScript() const {
    return issued_non_get_fetch_from_script_;
  }
  void SetIssuedNonGetFetchFromScript();

  BLINK_EXPORT void Reset();
#if BLINK_IMPLEMENTATION
  void OnCommitLoad();
#endif

  void SetObserver(WebViewClient* observer) { observer_ = observer; }

 private:
  bool had_form_interaction_ : 1;
  bool issued_non_get_fetch_from_script_ : 1;

  WebViewClient* observer_ = nullptr;
};

}  // namespace blink

#endif  // WebPageImportancesignals_h
