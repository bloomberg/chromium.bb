// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SEARCH_BOX_MODEL_OBSERVER_H_
#define UI_APP_LIST_SEARCH_BOX_MODEL_OBSERVER_H_

#include "ui/app_list/app_list_export.h"

namespace app_list {

class APP_LIST_EXPORT SearchBoxModelObserver {
 public:
  // Invoked when the some properties of the speech recognition button is
  // changed.
  virtual void SpeechRecognitionButtonPropChanged() = 0;

  // Invoked when hint text is changed.
  virtual void HintTextChanged() = 0;

  // Invoked when selection model is changed.
  virtual void SelectionModelChanged() = 0;

  // Invoked when text or voice search flag is changed.
  virtual void Update() = 0;

 protected:
  virtual ~SearchBoxModelObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_SEARCH_BOX_MODEL_OBSERVER_H_
