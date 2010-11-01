// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_SIZE_H_
#define PPAPI_CPP_SIZE_H_

#include "ppapi/c/pp_size.h"
#include "ppapi/cpp/logging.h"

namespace pp {

class Size {
 public:
  Size() {
    size_.width = 0;
    size_.height = 0;
  }
  Size(const PP_Size& s) {  // Implicit.
    // Want the >= 0 checking of the setter.
    set_width(s.width);
    set_height(s.height);
  }
  Size(int w, int h) {
    // Want the >= 0 checking of the setter.
    set_width(w);
    set_height(h);
  }

  ~Size() {
  }

  operator PP_Size() {
    return size_;
  }
  const PP_Size& pp_size() const {
    return size_;
  }
  PP_Size& pp_size() {
    return size_;
  }

  int width() const {
    return size_.width;
  }
  void set_width(int w) {
    if (w < 0) {
      PP_DCHECK(w >= 0);
      w = 0;
    }
    size_.width = w;
  }

  int height() const {
    return size_.height;
  }
  void set_height(int h) {
    if (h < 0) {
      PP_DCHECK(h >= 0);
      h = 0;
    }
    size_.height = h;
  }

  int GetArea() const {
    return width() * height();
  }

  void SetSize(int w, int h) {
    set_width(w);
    set_height(h);
  }

  void Enlarge(int w, int h) {
    set_width(width() + w);
    set_height(height() + h);
  }

  void swap(Size& other) {
    int32_t w = size_.width;
    int32_t h = size_.height;
    size_.width = other.size_.width;
    size_.height = other.size_.height;
    other.size_.width = w;
    other.size_.height = h;
  }

  bool IsEmpty() const {
    // Size doesn't allow negative dimensions, so testing for 0 is enough.
    return (width() == 0) || (height() == 0);
  }

 private:
  PP_Size size_;
};

}  // namespace pp

inline bool operator==(const pp::Size& lhs, const pp::Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const pp::Size& lhs, const pp::Size& rhs) {
  return !(lhs == rhs);
}

#endif  // PPAPI_CPP_SIZE_H_

