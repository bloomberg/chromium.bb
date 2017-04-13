// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextOffset_h
#define TextOffset_h

#include "platform/wtf/Forward.h"
#include "public/platform/Platform.h"

namespace blink {

class Text;

class TextOffset {
  STACK_ALLOCATED();

 public:
  TextOffset();
  TextOffset(Text*, int);
  TextOffset(const TextOffset&);

  Text* GetText() const { return text_.Get(); }
  int Offset() const { return offset_; }

  bool IsNull() const;
  bool IsNotNull() const;

 private:
  Member<Text> text_;
  int offset_;
};

}  // namespace blink

#endif  // TextOffset_h
