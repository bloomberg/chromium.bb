// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/mock_spellcheck.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

MockSpellCheck::MockSpellCheck()
    : initialized_(false) {
}

MockSpellCheck::~MockSpellCheck() {
}

bool MockSpellCheck::SpellCheckWord(const base::string16& text,
                                    int* misspelledOffset,
                                    int* misspelledLength) {
  DCHECK(misspelledOffset && misspelledLength);

  // Initialize this spellchecker.
  InitializeIfNeeded();

  // Reset the result values as our spellchecker does.
  *misspelledOffset = 0;
  *misspelledLength = 0;

  // Extract the first possible English word from the given string.
  // The given string may include non-ASCII characters or numbers. So, we
  // should filter out such characters before start looking up our
  // misspelled-word table.
  // (This is a simple version of our SpellCheckWordIterator class.)
  // If the given string doesn't include any ASCII characters, we can treat the
  // string as valid one.
  // Unfortunately, This implementation splits a contraction, i.e. "isn't" is
  // split into two pieces "isn" and "t". This is OK because webkit tests
  // don't have misspelled contractions.
  static const char* kWordCharacters =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  base::string16 word_characters(ASCIIToUTF16(kWordCharacters));

  size_t word_offset = text.find_first_of(word_characters);
  if (word_offset == std::string::npos)
    return true;

  size_t word_end = text.find_first_not_of(word_characters, word_offset);
  size_t word_length = word_end == std::string::npos ?
      text.length() - word_offset : word_end - word_offset;

  // Look up our misspelled-word table to check if the extracted word is a
  // known misspelled word, and return the offset and the length of the
  // extracted word if this word is a known misspelled word.
  // (See the comment in MockSpellCheck::InitializeIfNeeded() why we use a
  // misspelled-word table.)
  base::string16 word(text, word_offset, word_length);
  std::map<base::string16, bool>::iterator it = misspelled_words_.find(word);
  if (it == misspelled_words_.end())
    return true;

  *misspelledOffset = static_cast<int>(word_offset);
  *misspelledLength = static_cast<int>(word_length);
  return false;
}

bool MockSpellCheck::InitializeIfNeeded() {
  // Exit if we have already initialized this object.
  if (initialized_)
    return false;

  // Create a table that consists of misspelled words used in WebKit layout
  // tests.
  // Since WebKit layout tests don't have so many misspelled words as
  // well-spelled words, it is easier to compare the given word with misspelled
  // ones than to compare with well-spelled ones.
  static const char* kMisspelledWords[] = {
    // These words are known misspelled words in webkit tests.
    // If there are other misspelled words in webkit tests, please add them in
    // this array.
    "foo",
    "Foo",
    "baz",
    "fo",
    "LibertyF",
    "chello",
    "xxxtestxxx",
    "XXxxx",
    "Textx",
    "blockquoted",
    "asd",
    "Lorem",
    "Nunc",
    "Curabitur",
    "eu",
    "adlj",
    "adaasj",
    "sdklj",
    "jlkds",
    "jsaada",
    "jlda",
    "zz",
    "contentEditable",
    // The following words are used by unit tests.
    "ifmmp",
    "qwertyuiopasd",
    "qwertyuiopasdf",
  };

  misspelled_words_.clear();
  for (size_t i = 0; i < arraysize(kMisspelledWords); ++i) {
    misspelled_words_.insert(std::make_pair<base::string16, bool>(
        ASCIIToUTF16(kMisspelledWords[i]), false));
  }

  // Mark as initialized to prevent this object from being initialized twice
  // or more.
  initialized_ = true;

  // Since this MockSpellCheck class doesn't download dictionaries, this
  // function always returns false.
  return false;
}

void MockSpellCheck::FillSuggestions(const base::string16& word,
                                     std::vector<base::string16>* suggestions) {
  if (word == ASCIIToUTF16("wellcome"))
    suggestions->push_back(ASCIIToUTF16("welcome"));
}
