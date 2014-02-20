// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientation_h
#define ScreenOrientation_h

#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class Screen;

class ScreenOrientation FINAL : public Supplement<Screen>, DOMWindowProperty {
public:
    static ScreenOrientation& from(Screen&);
    virtual ~ScreenOrientation();

    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(orientationchange);

    static const AtomicString& orientation(Screen&);
    static bool lockOrientation(Screen&, const Vector<String>& orientations);
    static bool lockOrientation(Screen&, const AtomicString& orientation);
    static void unlockOrientation(Screen&);

private:
    explicit ScreenOrientation(Screen&);
    static const char* supplementName();
    Document& document() const;
};

} // namespace WebCore

#endif // ScreenOrientation_h
