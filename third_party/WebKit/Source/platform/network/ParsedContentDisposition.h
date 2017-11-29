// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParsedContentDisposition_h
#define ParsedContentDisposition_h

#include "platform/PlatformExport.h"
#include "platform/network/ParsedContentHeaderFieldParameters.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// Parses the content of a Content-Disposition header field into disposition
// type and parameters and stores them.
class PLATFORM_EXPORT ParsedContentDisposition final {
  STACK_ALLOCATED();

 public:
  using Mode = ParsedContentHeaderFieldParameters::Mode;

  explicit ParsedContentDisposition(const String&, Mode = Mode::kNormal);

  String Type() const { return type_; }
  String Filename() const;

  // Note that in the case of multiple values for the same name, the last value
  // is returned.
  String ParameterValueForName(const String& name) const {
    return IsValid() ? parameters_->ParameterValueForName(name) : String();
  }
  bool IsValid() const { return !!parameters_; }

 private:
  String type_;
  WTF::Optional<ParsedContentHeaderFieldParameters> parameters_;
};

}  // namespace blink

#endif
