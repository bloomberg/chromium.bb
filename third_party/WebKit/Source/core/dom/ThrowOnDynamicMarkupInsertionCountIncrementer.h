// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThrowOnDynamicMarkupInsertionCountIncrementer_h
#define ThrowOnDynamicMarkupInsertionCountIncrementer_h

#include "core/dom/Document.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ThrowOnDynamicMarkupInsertionCountIncrementer {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ThrowOnDynamicMarkupInsertionCountIncrementer);

 public:
  explicit ThrowOnDynamicMarkupInsertionCountIncrementer(Document* document)
      : count_(document ? &document->throw_on_dynamic_markup_insertion_count_
                        : 0) {
    if (!count_)
      return;
    ++(*count_);
  }

  ~ThrowOnDynamicMarkupInsertionCountIncrementer() {
    if (!count_)
      return;
    --(*count_);
  }

 private:
  unsigned* count_;
};

}  // namespace blink

#endif
