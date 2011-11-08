// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/ime/character_composer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/gtk+/gdk/gdkkeysyms.h"
#include "ui/base/glib/glib_integers.h"

namespace views {

namespace {

// Expects key is not filtered and no character is composed.
void ExpectKeyNotFiltered(CharacterComposer* character_composer, uint key) {
  EXPECT_FALSE(character_composer->FilterKeyPress(key));
  EXPECT_TRUE(character_composer->composed_character().empty());
}

// Expects key is filtered and no character is composed.
void ExpectKeyFiltered(CharacterComposer* character_composer, uint key) {
  EXPECT_TRUE(character_composer->FilterKeyPress(key));
  EXPECT_TRUE(character_composer->composed_character().empty());
}

// Expects |expected_character| is composed after sequence [key1, key2].
void ExpectCharacterComposed(CharacterComposer* character_composer,
                             uint key1,
                             uint key2,
                             const string16& expected_character) {
  ExpectKeyFiltered(character_composer, key1);
  EXPECT_TRUE(character_composer->FilterKeyPress(key2));
  EXPECT_EQ(character_composer->composed_character(), expected_character);
}

// Expects |expected_character| is composed after sequence [key1, key2, key3].
void ExpectCharacterComposed(CharacterComposer* character_composer,
                             uint key1,
                             uint key2,
                             uint key3,
                             const string16& expected_character) {
  ExpectKeyFiltered(character_composer, key1);
  ExpectCharacterComposed(character_composer, key2, key3, expected_character);
}

// Expects |expected_character| is composed after sequence [key1, key2, key3,
// key 4].
void ExpectCharacterComposed(CharacterComposer* character_composer,
                             uint key1,
                             uint key2,
                             uint key3,
                             uint key4,
                             const string16& expected_character) {
  ExpectKeyFiltered(character_composer, key1);
  ExpectCharacterComposed(character_composer, key2, key3, key4,
                          expected_character);
}

} // namespace

TEST(CharacterComposerTest, InitialState) {
  CharacterComposer character_composer;
  EXPECT_TRUE(character_composer.composed_character().empty());
}

TEST(CharacterComposerTest, NormalKeyIsNotFiltered) {
  CharacterComposer character_composer;
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_B);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_Z);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_c);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_m);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_1);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_8);
}

TEST(CharacterComposerTest, PartiallyMatchingSequence) {
  CharacterComposer character_composer;

  // Composition with sequence ['dead acute', '1'] will fail.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  EXPECT_TRUE(character_composer.FilterKeyPress(GDK_KEY_1));
  EXPECT_TRUE(character_composer.composed_character().empty());

  // Composition with sequence ['dead acute', 'dead circumflex', '1'] will fail.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_circumflex);
  EXPECT_TRUE(character_composer.FilterKeyPress(GDK_KEY_1));
  EXPECT_TRUE(character_composer.composed_character().empty());
}

TEST(CharacterComposerTest, FullyMatchingSequences) {
  CharacterComposer character_composer;
  // LATIN SMALL LETTER A WITH ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_a,
                          string16(1, 0x00E1));
  // LATIN CAPITAL LETTER A WITH ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_A,
                          string16(1, 0x00C1));
  // GRAVE ACCENT
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_grave,
                          GDK_KEY_dead_grave, string16(1, 0x0060));
  // LATIN SMALL LETTER A WITH CIRCUMFLEX AND ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute,
                          GDK_KEY_dead_circumflex, GDK_KEY_a,
                          string16(1, 0x1EA5));
  // LATIN CAPITAL LETTER U WITH HORN AND GRAVE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_grave,
                          GDK_KEY_dead_horn, GDK_KEY_U, string16(1, 0x1EEA));
  // LATIN CAPITAL LETTER C WITH CEDILLA
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_C,
                          string16(1, 0x00C7));
  // LATIN SMALL LETTER C WITH CEDILLA
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_c,
                          string16(1, 0x00E7));
}

TEST(CharacterComposerTest, FullyMatchingSequencesAfterMatchingFailure) {
  CharacterComposer character_composer;
  // Composition with sequence ['dead acute', 'dead circumflex', '1'] will fail.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_circumflex);
  EXPECT_TRUE(character_composer.FilterKeyPress(GDK_KEY_1));
  EXPECT_TRUE(character_composer.composed_character().empty());
  // LATIN SMALL LETTER A WITH CIRCUMFLEX AND ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute,
                          GDK_KEY_dead_circumflex, GDK_KEY_a,
                          string16(1, 0x1EA5));
}

TEST(CharacterComposerTest, ComposedCharacterIsClearedAfterReset) {
  CharacterComposer character_composer;
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_a,
                          string16(1, 0x00E1));
  character_composer.Reset();
  EXPECT_TRUE(character_composer.composed_character().empty());
}

TEST(CharacterComposerTest, CompositionStateIsClearedAfterReset) {
  CharacterComposer character_composer;
  // Even though sequence ['dead acute', 'a'] will compose 'a with acute',
  // no character is composed here because of reset.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  character_composer.Reset();
  EXPECT_FALSE(character_composer.FilterKeyPress(GDK_KEY_a));
  EXPECT_TRUE(character_composer.composed_character().empty());
}

// ComposeCheckerWithCompactTable in character_composer.cc is depending on the
// assumption that the data in gtkimcontextsimpleseqs.h is correctly ordered.
TEST(CharacterComposerTest, MainTableIsCorrectlyOrdered) {
  // This file is included here intentionally, instead of the top of the file,
  // because including this file at the top of the file will define a
  // global constant and contaminate the global namespace.
#include "third_party/gtk+/gtk/gtkimcontextsimpleseqs.h"
  const int index_size = 26;
  const int index_stride = 6;

  // Verify that the index is correctly ordered
  for (int i = 1; i < index_size; ++i) {
    const int index_key_prev = gtk_compose_seqs_compact[(i - 1)*index_stride];
    const int index_key = gtk_compose_seqs_compact[i*index_stride];
    EXPECT_TRUE(index_key > index_key_prev);
  }

  // Verify that the sequenes are correctly ordered
  struct {
    int operator()(const uint16* l, const uint16* r, int length) const{
      for (int i = 0; i < length; ++i) {
        if (l[i] > r[i])
          return 1;
        if (l[i] < r[i])
          return -1;
      }
      return 0;
    }
  } compare_sequence;

  for (int i = 0; i < index_size; ++i) {
    for (int length = 1; length < index_stride - 1; ++length) {
      const int index_begin = gtk_compose_seqs_compact[i*index_stride + length];
      const int index_end =
          gtk_compose_seqs_compact[i*index_stride + length + 1];
      const int stride = length + 1;
      for (int index = index_begin + stride; index < index_end;
           index += stride) {
        const uint16* sequence = &gtk_compose_seqs_compact[index];
        const uint16* sequence_prev = sequence - stride;
        EXPECT_EQ(compare_sequence(sequence, sequence_prev, length), 1);
      }
    }
  }
}

}  // namespace views
