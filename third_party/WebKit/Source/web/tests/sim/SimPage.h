// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimPage_h
#define SimPage_h

#include "platform/heap/Persistent.h"

namespace blink {

class Page;

class SimPage final {
 public:
  explicit SimPage();
  ~SimPage();

  void setPage(Page*);

  void setFocused(bool);
  bool isFocused() const;

 private:
  Persistent<Page> m_page;
};

}  // namespace blink

#endif
