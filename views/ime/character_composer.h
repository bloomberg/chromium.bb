// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_IME_CHARACTER_COMPOSER_H_
#define VIEWS_IME_CHARACTER_COMPOSER_H_
#pragma once

#include <vector>
#include "base/basictypes.h"
#include "base/string_util.h"

namespace views {

// A class to recognize compose and dead key sequence.
// Outputs composed character.
//
// TODO(hashimoto): support unicode character composition starting with
// Ctrl-Shift-U. http://crosbug.com/15925
class CharacterComposer {
 public:
  CharacterComposer();

  void Reset();

  // Filters keypress.  Returns true if the keypress is handled.
  bool FilterKeyPress(unsigned int keycode);

  // Returns a string consisting of composed character.
  // Empty is returned when there is no composition result.
  const string16& GetComposedCharacter() const {
    return composed_character_;
  }

 private:
  std::vector<unsigned int> compose_buffer_;
  string16 composed_character_;

  DISALLOW_COPY_AND_ASSIGN(CharacterComposer);
};

}  // namespace views

#endif  // VIEWS_IME_CHARACTER_COMPOSER_H_
