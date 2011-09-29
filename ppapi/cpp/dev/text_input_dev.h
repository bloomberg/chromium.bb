// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_TEXT_INPUT_DEV_H_
#define PPAPI_CPP_DEV_TEXT_INPUT_DEV_H_

#include "ppapi/c/dev/ppb_text_input_dev.h"

/// @file
/// This file defines the API for controlling text input methods.
namespace pp {

class Instance;
class Rect;

class TextInput_Dev {
 public:
  explicit TextInput_Dev(Instance* instance);
  virtual ~TextInput_Dev();

  void SetTextInputType(PP_TextInput_Type type);
  void UpdateCaretPosition(const Rect& caret, const Rect& bounding_box);
  void CancelCompositionText();

 private:
  Instance* instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_TEXT_INPUT_DEV_H_
