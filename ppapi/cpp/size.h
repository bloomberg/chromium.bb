// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_SIZE_H_
#define PPAPI_CPP_SIZE_H_

#include "ppapi/c/pp_size.h"
#include "ppapi/cpp/logging.h"

/// @file
/// This file defines the API to create a size based on width
/// and height.

namespace pp {

/// A size of an object based on width and height.
class Size {
 public:

  /// The default constructor. Initializes the width and height to 0.
  Size() {
    size_.width = 0;
    size_.height = 0;
  }

  /// A constructor accepting a pointer to a PP_Size and converting the
  /// PP_Size to a Size. This is an implicit conversion constructor.
  /// @param[in] s A pointer to a PP_Size.
  Size(const PP_Size& s) {  // Implicit.
    // Want the >= 0 checking of the setter.
    set_width(s.width);
    set_height(s.height);
  }

  /// A constructor accepting two int values for width and height and
  /// converting them to a Size.
  /// @param[in] w An int value representing a width.
  /// @param[in] h An int value representing a height.
  Size(int w, int h) {
    // Want the >= 0 checking of the setter.
    set_width(w);
    set_height(h);
  }

  /// Destructor.
  ~Size() {
  }

  /// A function allowing implicit conversion of a Size to a PP_Size.
  /// @return A Size.
  operator PP_Size() {
    return size_;
  }

  /// Getter function for returning the internal PP_Size struct.
  /// @return A const reference to the internal PP_Size struct.
  const PP_Size& pp_size() const {
    return size_;
  }

  /// Getter function for returning the internal PP_Size struct.
  /// @return A mutable reference to the PP_Size struct.
  PP_Size& pp_size() {
    return size_;
  }

  /// Getter function for returning the value of width.
  /// @return The value of width for this Size.
  int width() const {
    return size_.width;
  }

  /// Setter function for setting the value of width.
  /// @param[in] w A new width value.
  void set_width(int w) {
    if (w < 0) {
      PP_DCHECK(w >= 0);
      w = 0;
    }
    size_.width = w;
  }

  /// Getter function for returning the value of height.
  /// @return The value of height for this Size.
  int height() const {
    return size_.height;
  }

  /// Setter function for setting the value of height.
  /// @param[in] h A new height value.
  void set_height(int h) {
    if (h < 0) {
      PP_DCHECK(h >= 0);
      h = 0;
    }
    size_.height = h;
  }

  /// A function for determining the area (width * height).
  /// @return The area
  int GetArea() const {
    return width() * height();
  }

  /// Setter function for setting the value of width and height.
  /// @param[in] w A new width value.
  /// @param[in] h A new height value.
  void SetSize(int w, int h) {
    set_width(w);
    set_height(h);
  }

  /// A function used to enlarge the size of an object.
  /// @param[in] w A width to add the current width.
  /// @param[in] h A height to add to the current height.
  void Enlarge(int w, int h) {
    set_width(width() + w);
    set_height(height() + h);
  }

  /// A function used to determine if the size is zero.
  /// @return true if the size is zero.
  bool IsEmpty() const {
    // Size doesn't allow negative dimensions, so testing for 0 is enough.
    return (width() == 0) || (height() == 0);
  }

 private:
  PP_Size size_;
};

}  // namespace pp

/// Determines whether the width and height values of two sizes are equal.
/// @param[in] lhs The Size on the left-hand side of the equation.
/// @param[in] rhs The Size on the right-hand side of the equation.
/// @return true if they are equal, false if unequal.
inline bool operator==(const pp::Size& lhs, const pp::Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

/// Determines whether two Sizes are not equal.
/// @param[in] lhs The Size on the left-hand side of the equation.
/// @param[in] rhs The Size on the right-hand side of the equation.
/// @return true if the Size of lhs are equal to the Size
/// of rhs, otherwise false.
inline bool operator!=(const pp::Size& lhs, const pp::Size& rhs) {
  return !(lhs == rhs);
}

#endif  // PPAPI_CPP_SIZE_H_

