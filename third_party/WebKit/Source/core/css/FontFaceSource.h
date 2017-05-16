// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontFaceSource_h
#define FontFaceSource_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PassRefPtr.h"

namespace blink {

class Document;
class FontFaceSet;

class FontFaceSource {
  STATIC_ONLY(FontFaceSource);

 public:
  static FontFaceSet* fonts(Document&);
};

}  // namespace blink

#endif  // FontFaceSource_h
