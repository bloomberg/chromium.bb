// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_MOCK_SPELLCHECK_H_
#define WEBKIT_TOOLS_TEST_SHELL_MOCK_SPELLCHECK_H_

#include <map>
#include <vector>

#include "base/string16.h"

// A mock implementation of a spell-checker used for WebKit tests.
// This class only implements the minimal functionarities required by WebKit
// tests, i.e. this class just compares the given string with known misspelled
// words in webkit tests and mark them as missspelled.
// Even though this is sufficent for webkit tests, this class is not suitable
// for any other usages.
class MockSpellCheck {
 public:
  MockSpellCheck();
  ~MockSpellCheck();

  // Checks the spellings of the specified text.
  // This function returns true if the text consists of valid words, and
  // returns false if it includes invalid words.
  // When the given text includes invalid words, this function sets the
  // position of the first invalid word to misspelledOffset, and the length of
  // the first invalid word to misspelledLength, respectively.
  // For example, when the given text is "   zz zz", this function sets 3 to
  // misspelledOffset and 2 to misspelledLength, respectively.
  bool SpellCheckWord(const base::string16& text,
                      int* misspelledOffset,
                      int* misspelledLength);

  // Emulates suggestions for misspelled words.
  // The suggestions are pushed to |suggestions| parameters.
  void FillSuggestions(const base::string16& word,
                       std::vector<base::string16>* suggestions);
 private:
  // Initialize the internal resources if we need to initialize it.
  // Initializing this object may take long time. To prevent from hurting
  // the performance of test_shell, we initialize this object when
  // SpellCheckWord() is called for the first time.
  // To be compliant with SpellCheck:InitializeIfNeeded(), this function
  // returns true if this object is downloading a dictionary, otherwise
  // it returns false.
  bool InitializeIfNeeded();

  // A table that consists of misspelled words.
  std::map<base::string16, bool> misspelled_words_;

  // A flag representing whether or not this object is initialized.
  bool initialized_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_MOCK_SPELLCHECK_H_
