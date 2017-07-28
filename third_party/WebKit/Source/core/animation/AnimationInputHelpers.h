// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationInputHelpers_h
#define AnimationInputHelpers_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class TimingFunction;
class QualifiedName;

class CORE_EXPORT AnimationInputHelpers {
  STATIC_ONLY(AnimationInputHelpers);

 public:
  static CSSPropertyID KeyframeAttributeToCSSProperty(const String&,
                                                      const Document&);
  static CSSPropertyID KeyframeAttributeToPresentationAttribute(const String&,
                                                                const Element&);
  static const QualifiedName* KeyframeAttributeToSVGAttribute(const String&,
                                                              Element&);
  static PassRefPtr<TimingFunction> ParseTimingFunction(const String&,
                                                        Document*,
                                                        ExceptionState&);
};

}  // namespace blink

#endif  // AnimationInputHelpers_h
