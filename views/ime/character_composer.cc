// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "character_composer.h"

#include <cstdlib>

#include "third_party/gtk+/gdk/gdkkeysyms.h"

namespace {

inline int CompareSequenceValue(unsigned int key, uint16 value) {
  return (key > value) ? 1 : ((key < value) ? -1 : 0);
}

class ComposeChecker {
 public:
  ComposeChecker(const uint16* data, int max_sequence_length, int n_sequences);
  bool CheckSequence(const std::vector<unsigned int>& sequence,
                     uint32* composed_character) const;

 private:
  static int CompareSequence(const void* key_void, const void* value_void);

  const uint16* data_;
  int max_sequence_length_;
  int n_sequences_;
  int row_stride_;

  DISALLOW_COPY_AND_ASSIGN(ComposeChecker);
};

ComposeChecker::ComposeChecker(const uint16* data,
                               int max_sequence_length,
                               int n_sequences)
    : data_(data),
      max_sequence_length_(max_sequence_length),
      n_sequences_(n_sequences),
      row_stride_(max_sequence_length + 2) {
}

bool ComposeChecker::CheckSequence(
    const std::vector<unsigned int>& sequence,
    uint32* composed_character) const {
  const int sequence_length = sequence.size();
  if (sequence_length > max_sequence_length_)
    return false;
  // Find sequence in the table
  const uint16* found = static_cast<const uint16*>(
      bsearch(&sequence, data_, n_sequences_,
              sizeof(uint16)*row_stride_, CompareSequence));
  if (!found)
    return false;
  // Ensure |found| is pointing the first matching element
  while (found > data_ &&
         CompareSequence(&sequence, found - row_stride_) == 0)
    found -= row_stride_;

  if (sequence_length == max_sequence_length_ || found[sequence_length] == 0) {
    // |found| is not partially matching. It's fully matching
    const uint16* data_end = data_ + row_stride_*n_sequences_;
    if (found + row_stride_ >= data_end ||
        CompareSequence(&sequence, found + row_stride_) != 0) {
      // There is no composition longer than |found| which matches to |sequence|
      const uint32 value = (found[max_sequence_length_] << 16) |
          found[max_sequence_length_ + 1];
      *composed_character = value;
    }
  }
  return true;
}

// static
int ComposeChecker::CompareSequence(const void* key_void,
                                    const void* value_void) {
  typedef std::vector<unsigned int> KeyType;
  const KeyType& key = *static_cast<const KeyType*>(key_void);
  const uint16* value = static_cast<const uint16*>(value_void);

  for(size_t i = 0; i < key.size(); ++i) {
    const int compare_result = CompareSequenceValue(key[i], value[i]);
    if(compare_result)
      return compare_result;
  }
  return 0;
}


class ComposeCheckerWithCompactTable {
 public:
  ComposeCheckerWithCompactTable(const uint16* data,
                                 int max_sequence_length,
                                 int index_size,
                                 int index_stride);
  bool CheckSequence(const std::vector<unsigned int>& sequence,
                     uint32* composed_character) const;

 private:
  static int CompareSequenceFront(const void* key_void, const void* value_void);
  static int CompareSequenceSkipFront(const void* key_void,
                                      const void* value_void);

  const uint16* data_;
  int max_sequence_length_;
  int index_size_;
  int index_stride_;
};

ComposeCheckerWithCompactTable::ComposeCheckerWithCompactTable(
    const uint16* data,
    int max_sequence_length,
    int index_size,
    int index_stride)
    : data_(data),
      max_sequence_length_(max_sequence_length),
      index_size_(index_size),
      index_stride_(index_stride) {
}

bool ComposeCheckerWithCompactTable::CheckSequence(
    const std::vector<unsigned int>& sequence,
    uint32* composed_character) const {
  const int compose_length = sequence.size();
  if (compose_length > max_sequence_length_)
    return false;
  // Find corresponding index for the first keypress
  const uint16* index = static_cast<const uint16*>(
      bsearch(&sequence, data_, index_size_,
              sizeof(uint16)*index_stride_, CompareSequenceFront));
  if (!index)
    return false;
  if (compose_length == 1)
    return true;
  // Check for composition sequences
  for (int length = compose_length - 1; length < max_sequence_length_;
       ++length) {
    const uint16* table = data_ + index[length];
    const uint16* table_next = data_ + index[length + 1];
    if (table_next > table) {
      // There are composition sequences for this |length|
      const int row_stride = length + 1;
      const int n_sequences = (table_next - table)/row_stride;
      const uint16* seq = static_cast<const uint16*>(
          bsearch(&sequence, table, n_sequences,
                  sizeof(uint16)*row_stride, CompareSequenceSkipFront));
      if (seq) {
        if (length == compose_length - 1) // exact match
          *composed_character = seq[length];
        return true;
      }
    }
  }
  return false;
}

// static
int ComposeCheckerWithCompactTable::CompareSequenceFront(
    const void* key_void,
    const void* value_void) {
  typedef std::vector<unsigned int> KeyType;
  const KeyType& key = *static_cast<const KeyType*>(key_void);
  const uint16* value = static_cast<const uint16*>(value_void);

  return CompareSequenceValue(key[0], value[0]);
}

// static
int ComposeCheckerWithCompactTable::CompareSequenceSkipFront(
    const void* key_void,
    const void* value_void) {
  typedef std::vector<unsigned int> KeyType;
  const KeyType& key = *static_cast<const KeyType*>(key_void);
  const uint16* value = static_cast<const uint16*>(value_void);

  for(size_t i = 1; i < key.size(); ++i) {
    const int compare_result = CompareSequenceValue(key[i], value[i - 1]);
    if(compare_result)
      return compare_result;
  }
  return 0;
}

// Main table
typedef uint16 guint16;
#include "third_party/gtk+/gtk/gtkimcontextsimpleseqs.h"

// Additional table

// The difference between this and the default input method is the handling
// of C+acute - this method produces C WITH CEDILLA rather than C WITH ACUTE.
// For languages that use CCedilla and not acute, this is the preferred mapping,
// and is particularly important for pt_BR, where the us-intl keyboard is
// used extensively.

const uint16 cedilla_compose_seqs[] = {
  // LATIN_CAPITAL_LETTER_C_WITH_CEDILLA
  GDK_KEY_dead_acute, GDK_KEY_C, 0, 0, 0, 0x00C7,
  // LATIN_SMALL_LETTER_C_WITH_CEDILLA
  GDK_KEY_dead_acute, GDK_KEY_c, 0, 0, 0, 0x00E7,
  // LATIN_CAPITAL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_apostrophe, GDK_KEY_C, 0, 0, 0x00C7,
  // LATIN_SMALL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_apostrophe, GDK_KEY_c, 0, 0, 0x00E7,
  // LATIN_CAPITAL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_C, GDK_KEY_apostrophe, 0, 0, 0x00C7,
  // LATIN_SMALL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_c, GDK_KEY_apostrophe, 0, 0, 0x00E7,
};

bool KeypressShouldBeIgnored(unsigned int keycode) {
  switch(keycode) {
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
    case GDK_KEY_Caps_Lock:
    case GDK_KEY_Shift_Lock:
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
    case GDK_KEY_Hyper_L:
    case GDK_KEY_Hyper_R:
    case GDK_KEY_Mode_switch:
    case GDK_KEY_ISO_Level3_Shift:
      return true;
    default:
      return false;
  }
}

bool CheckCharacterComposeTable(const std::vector<unsigned int>& sequence,
                                uint32* composed_character) {
  // Check cedilla compose table
  const ComposeChecker kCedillaComposeChecker(
    cedilla_compose_seqs, 4, arraysize(cedilla_compose_seqs)/(4 + 2));
  if (kCedillaComposeChecker.CheckSequence(sequence, composed_character))
    return true;

  // Check main compose table
  const ComposeCheckerWithCompactTable kMainComposeChecker(
      gtk_compose_seqs_compact, 5, 24, 6);
  if (kMainComposeChecker.CheckSequence(sequence, composed_character))
    return true;

  return false;
}

}  // anonymous namespace

namespace views {

CharacterComposer::CharacterComposer() {
}

void CharacterComposer::Reset() {
  compose_buffer_.clear();
  composed_character_.clear();
}

bool CharacterComposer::FilterKeyPress(unsigned int keycode) {
  if(KeypressShouldBeIgnored(keycode))
    return false;

  compose_buffer_.push_back(keycode);

  // Check compose table
  composed_character_.clear();
  uint32 composed_character_utf32 = 0;
  if (CheckCharacterComposeTable(compose_buffer_, &composed_character_utf32)) {
    // Key press is recognized as a part of composition
    if (composed_character_utf32 !=0) {
      // We get a composed character
      compose_buffer_.clear();
      // We assume that composed character is in BMP
      if (composed_character_utf32 <= 0xffff)
        composed_character_ += static_cast<char16>(composed_character_utf32);
    }
    return true;
  }
  // Key press is not a part of composition
  compose_buffer_.pop_back(); // remove the keypress added this time
  if (!compose_buffer_.empty()) {
    compose_buffer_.clear();
    return true;
  }
  return false;
}

}  // namespace views
