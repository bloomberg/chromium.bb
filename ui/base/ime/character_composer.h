// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHARACTER_COMPOSER_H_
#define UI_BASE_IME_CHARACTER_COMPOSER_H_
#pragma once

#include <vector>

#include "base/string_util.h"
#include "ui/base/ui_export.h"

namespace ui {

// A class to recognize compose and dead key sequence.
// Outputs composed character.
//
// TODO(hashimoto): support unicode character composition starting with
// Ctrl-Shift-U. http://crosbug.com/15925
class UI_EXPORT CharacterComposer {
 public:
  CharacterComposer();
  ~CharacterComposer();

  void Reset();

  // Filters keypress.
  // Returns true if the keypress is recognized as a part of composition
  // sequence.
  // |keyval| must be a GDK_KEY_* constants.
  // |flags| must be a combination of ui::EF_* flags.
  bool FilterKeyPress(unsigned int keyval, unsigned int flags);

  // Returns a string consisting of composed character.
  // Empty string is returned when there is no composition result.
  const string16& composed_character() const {
    return composed_character_;
  }

 private:
  // An enum to describe composition mode.
  enum CompositionMode {
    // This is the initial state.
    // Composite a character with dead-keys and compose-key.
    KEY_SEQUENCE_MODE,
    // Composite a character with a hexadecimal unicode sequence.
    HEX_MODE,
  };

  // Commit a character composed from hexadecimal uncode sequence
  void CommitHex();

  // Remembers keypresses previously filtered.
  std::vector<unsigned int> compose_buffer_;

  // A string representing the composed character.
  string16 composed_character_;

  // Composition mode which this instance is in.
  CompositionMode composition_mode_;

  DISALLOW_COPY_AND_ASSIGN(CharacterComposer);
};

}  // namespace ui

#endif  // UI_BASE_IME_CHARACTER_COMPOSER_H_
