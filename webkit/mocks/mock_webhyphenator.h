// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MOCKS_MOCK_WEBHYPHENATOR_H_
#define WEBKIT_MOCKS_MOCK_WEBHYPHENATOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/platform_file.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHyphenator.h"

typedef struct _HyphenDict HyphenDict;

namespace webkit_glue {

// Implements a simple WebHyphenator that only supports en-US. It is used for
// layout tests that expect that hyphenator to be available synchronously.
// Therefore, this class supports synchronous loading of the dictionary as well.
class MockWebHyphenator : public WebKit::WebHyphenator {
 public:
  MockWebHyphenator();
  virtual ~MockWebHyphenator();

  // Loads the hyphenation dictionary. |dict_file| should be an open fd to
  // third_party/hyphen/hyph_en_US.dic.
  void LoadDictionary(base::PlatformFile dict_file);

  // WebHyphenator implementation.
  virtual bool canHyphenate(const WebKit::WebString& locale) OVERRIDE;
  virtual size_t computeLastHyphenLocation(
      const char16* characters,
      size_t length,
      size_t before_index,
      const WebKit::WebString& locale) OVERRIDE;

 private:
  HyphenDict* hyphen_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(MockWebHyphenator);
};

}  // namespace webkit_glue

#endif  // WEBKIT_MOCKS_MOCK_WEBHYPHENATOR_H_
