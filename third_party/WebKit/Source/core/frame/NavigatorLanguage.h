// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorLanguage_h
#define NavigatorLanguage_h

#include "core/CoreExport.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class CORE_EXPORT NavigatorLanguage {
 public:
  NavigatorLanguage();

  AtomicString language();
  virtual Vector<String> languages() = 0;
  bool hasLanguagesChanged() const;
  void SetLanguagesChanged();

 protected:
  bool languages_changed_ = true;
};

}  // namespace blink

#endif  // NavigatorLanguage_h
