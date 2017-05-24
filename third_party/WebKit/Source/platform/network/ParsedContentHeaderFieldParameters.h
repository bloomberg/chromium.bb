// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParsedContentHeaderFieldParameters_h
#define ParsedContentHeaderFieldParameters_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class HeaderFieldTokenizer;

// Parses content header field parameters as specified in RFC2045 and stores
// them. It is used internally by ParsedContent* classes.
// FIXME: add support for comments.
class PLATFORM_EXPORT ParsedContentHeaderFieldParameters final {
  STACK_ALLOCATED();

 public:
  // When |Relaxed| is specified, the parser parses parameter values in a sloppy
  // manner, i.e., only ';' and '"' are treated as special characters.
  // See https://chromiumcodereview.appspot.com/23043002.
  // When |Strict| is specified, the parser does not allow multiple values
  // for the same parameter. Some RFCs based on RFC2045 (e.g. RFC6838) note that
  // "It is an error for a specific parameter to be specified more than once."
  enum class Mode {
    kNormal,
    kRelaxed,
    kStrict,
  };

  ParsedContentHeaderFieldParameters();

  // Note that in the case of multiple values for the same name, the last value
  // is returned.
  String ParameterValueForName(const String&) const;
  size_t ParameterCount() const;

  bool IsValid() const { return is_valid_; }

  void ParseParameters(HeaderFieldTokenizer, Mode);

  static ParsedContentHeaderFieldParameters CreateForTesting(
      const String&,
      Mode = Mode::kNormal);

 private:
  typedef HashMap<String, String> KeyValuePairs;
  bool is_valid_;
  KeyValuePairs parameters_;
};

}  // namespace blink

#endif
