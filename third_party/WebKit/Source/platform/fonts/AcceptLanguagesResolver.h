// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceptLanguagesResolver_h
#define AcceptLanguagesResolver_h

#include "platform/PlatformExport.h"
#include "platform/wtf/text/WTFString.h"

#include <unicode/uscript.h>

namespace blink {

class LayoutLocale;

class PLATFORM_EXPORT AcceptLanguagesResolver {
 public:
  static void AcceptLanguagesChanged(const String&);

  static const LayoutLocale* LocaleForHan();
  static const LayoutLocale* LocaleForHanFromAcceptLanguages(const String&);
};

}  // namespace blink

#endif  // AcceptLanguagesResolver_h
