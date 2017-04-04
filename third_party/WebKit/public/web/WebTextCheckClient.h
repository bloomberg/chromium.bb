// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTextCheckClient_h
#define WebTextCheckClient_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

class WebTextCheckingCompletion;

// TODO(xiaochengh): Rename WebTextCheckClient to WebSpellCheckClient.
class WebTextCheckClient {
 public:
  // The client should perform spell-checking on the given text. If the
  // text contains a misspelled word, then upon return misspelledOffset
  // will point to the start of the misspelled word, and misspelledLength
  // will indicates its length. Otherwise, if there was not a spelling
  // error, then upon return misspelledLength is 0. If optional_suggestions
  // is given, then it will be filled with suggested words (not a cheap step).
  virtual void checkSpelling(const WebString& text,
                             int& misspelledOffset,
                             int& misspelledLength,
                             WebVector<WebString>* optionalSuggestions) {}

  // Requests asynchronous spelling and grammar checking, whose result should be
  // returned by passed completion object.
  virtual void requestCheckingOfText(
      const WebString& textToCheck,
      WebTextCheckingCompletion* completionCallback) {}

  // Clear all stored references to requests, so that it will not become a
  // leak source.
  virtual void cancelAllPendingRequests() {}

 protected:
  virtual ~WebTextCheckClient() {}
};

}  // namespace blink

#endif
