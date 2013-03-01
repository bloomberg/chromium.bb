// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/mocks/mock_webhyphenator.h"

#include "base/logging.h"
#include "base/memory/scoped_handle.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "third_party/hyphen/hyphen.h"

namespace webkit_glue {

MockWebHyphenator::MockWebHyphenator()
    : hyphen_dictionary_(NULL) {
}

MockWebHyphenator::~MockWebHyphenator() {
  if (hyphen_dictionary_)
    hnj_hyphen_free(hyphen_dictionary_);
}

void MockWebHyphenator::LoadDictionary(base::PlatformFile dict_file) {
  CHECK(!hyphen_dictionary_);
  // Initialize the hyphen library with a sample dictionary. To avoid test
  // flakiness, this code synchronously loads the dictionary.
  if (dict_file == base::kInvalidPlatformFileValue) {
    NOTREACHED();
    return;
  }
  ScopedStdioHandle dict_handle(base::FdopenPlatformFile(dict_file, "r"));
  if (!dict_handle.get()) {
    NOTREACHED();
    base::ClosePlatformFile(dict_file);
    return;
  }
  hyphen_dictionary_ = hnj_hyphen_load_file(dict_handle.get());
  DCHECK(hyphen_dictionary_);
}

bool MockWebHyphenator::canHyphenate(const WebKit::WebString& locale) {
  return locale.isEmpty()  || locale.equals("en") || locale.equals("en_US")  ||
      locale.equals("en_GB");
}

size_t MockWebHyphenator::computeLastHyphenLocation(
    const char16* characters,
    size_t length,
    size_t before_index,
    const WebKit::WebString& locale) {
  DCHECK(locale.isEmpty()  || locale.equals("en") || locale.equals("en_US")  ||
         locale.equals("en_GB"));
  if (!hyphen_dictionary_)
    return 0;

  // Retrieve the positions where we can insert hyphens. This function assumes
  // the input word is an English word so it can use the position returned by
  // the hyphen library without conversion.
  string16 word_utf16(characters, length);
  if (!IsStringASCII(word_utf16))
    return 0;
  std::string word = StringToLowerASCII(UTF16ToASCII(word_utf16));
  scoped_array<char> hyphens(new char[word.length() + 5]);
  char** rep = NULL;
  int* pos = NULL;
  int* cut = NULL;
  int error = hnj_hyphen_hyphenate2(hyphen_dictionary_,
                                    word.data(),
                                    static_cast<int>(word.length()),
                                    hyphens.get(),
                                    NULL,
                                    &rep,
                                    &pos,
                                    &cut);
  if (error)
    return 0;

  // Release all resources allocated by the hyphen library now because they are
  // not used when hyphenating English words.
  if (rep) {
    for (size_t i = 0; i < word.length(); ++i) {
      if (rep[i])
        free(rep[i]);
    }
    free(rep);
  }
  if (pos)
    free(pos);
  if (cut)
    free(cut);

  // Retrieve the last position where we can insert a hyphen before the given
  // index.
  if (before_index >= 2) {
    for (size_t index = before_index - 2; index > 0; --index) {
      if (hyphens[index] & 1)
        return index + 1;
    }
  }
  return 0;
}

}  // namespace webkit_glue
