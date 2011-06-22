// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/ime/character_composer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/gtk+/gdk/gdkkeysyms.h"

namespace views {

namespace {

// Expects key is not filtered and no character is composed
void ExpectKeyNotFiltered(CharacterComposer* character_composer, uint key) {
  EXPECT_FALSE(character_composer->FilterKeyPress(key));
  EXPECT_TRUE(character_composer->GetComposedCharacter().empty());
}

// Expects key is filtered and no character is composed
void ExpectKeyFiltered(CharacterComposer* character_composer, uint key) {
  EXPECT_TRUE(character_composer->FilterKeyPress(key));
  EXPECT_TRUE(character_composer->GetComposedCharacter().empty());
}

// Expects |expected_character| is composed after sequence [key1, key2]
void ExpectCharacterComposed(CharacterComposer* character_composer,
                             uint key1,
                             uint key2,
                             const string16& expected_character) {
  ExpectKeyFiltered(character_composer, key1);
  EXPECT_TRUE(character_composer->FilterKeyPress(key2));
  EXPECT_EQ(character_composer->GetComposedCharacter(), expected_character);
}

// Expects |expected_character| is composed after sequence [key1, key2, key3]
void ExpectCharacterComposed(CharacterComposer* character_composer,
                             uint key1,
                             uint key2,
                             uint key3,
                             const string16& expected_character) {
  ExpectKeyFiltered(character_composer, key1);
  ExpectCharacterComposed(character_composer, key2, key3, expected_character);
}

// Expects |expected_character| is composed after sequence [key1, key2, key3,
// key 4]
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

} // anonymous namespace

TEST(CharacterComposerTest, InitialState) {
  CharacterComposer character_composer;
  EXPECT_TRUE(character_composer.GetComposedCharacter().empty());
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

  // Composition with sequence ['dead acute', '1'] will fail
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  EXPECT_TRUE(character_composer.FilterKeyPress(GDK_KEY_1));
  EXPECT_TRUE(character_composer.GetComposedCharacter().empty());

  // Composition with sequence ['dead acute', 'dead circumflex', '1'] will fail
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_circumflex);
  EXPECT_TRUE(character_composer.FilterKeyPress(GDK_KEY_1));
  EXPECT_TRUE(character_composer.GetComposedCharacter().empty());
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
  // Composition with sequence ['dead acute', 'dead circumflex', '1'] will fail
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_circumflex);
  EXPECT_TRUE(character_composer.FilterKeyPress(GDK_KEY_1));
  EXPECT_TRUE(character_composer.GetComposedCharacter().empty());
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
  EXPECT_TRUE(character_composer.GetComposedCharacter().empty());
}

TEST(CharacterComposerTest, CompositionStateIsClearedAfterReset) {
  CharacterComposer character_composer;
  // Even though sequence ['dead acute', 'a'] will compose 'a with acute',
  // no character is composed here because of reset.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute);
  character_composer.Reset();
  EXPECT_FALSE(character_composer.FilterKeyPress(GDK_KEY_a));
  EXPECT_TRUE(character_composer.GetComposedCharacter().empty());
}

}  // namespace views
