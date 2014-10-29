// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindow_h
#define DOMWindow_h

#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"

namespace blink {

class ApplicationCache;
class BarProp;
class Console;
class DOMWindowCSS;
class Document;
class Frame;
class History;
class Location;
class Navigator;
class Performance;
class Screen;
class Storage;
class StyleMedia;

class DOMWindow : public RefCountedWillBeGarbageCollectedFinalized<DOMWindow>, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(DOMWindow);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DOMWindow);
public:
    // RefCountedWillBeGarbageCollectedFinalized overrides:
    void trace(Visitor* visitor) override
    {
        EventTargetWithInlineData::trace(visitor);
    }

    virtual Frame* frame() const = 0;

    // DOM Level 0
    virtual Screen* screen() const = 0;
    virtual History* history() const = 0;
    virtual BarProp* locationbar() const = 0;
    virtual BarProp* menubar() const = 0;
    virtual BarProp* personalbar() const = 0;
    virtual BarProp* scrollbars() const = 0;
    virtual BarProp* statusbar() const = 0;
    virtual BarProp* toolbar() const = 0;
    virtual Navigator* navigator() const = 0;
    Navigator* clientInformation() const { return navigator(); }
    virtual Location& location() const = 0;

    virtual bool offscreenBuffering() const = 0;

    virtual int outerHeight() const = 0;
    virtual int outerWidth() const = 0;
    virtual int innerHeight() const = 0;
    virtual int innerWidth() const = 0;
    virtual int screenX() const = 0;
    virtual int screenY() const = 0;
    int screenLeft() const { return screenX(); }
    int screenTop() const { return screenY(); }
    virtual double scrollX() const = 0;
    virtual double scrollY() const = 0;
    double pageXOffset() const { return scrollX(); }
    double pageYOffset() const { return scrollY(); }

    virtual bool closed() const = 0;

    virtual unsigned length() const = 0;

    virtual const AtomicString& name() const = 0;
    virtual void setName(const AtomicString&) = 0;

    virtual String status() const = 0;
    virtual void setStatus(const String&) = 0;
    virtual String defaultStatus() const = 0;
    virtual void setDefaultStatus(const String&) = 0;

    // Self-referential attributes
    virtual DOMWindow* self() const = 0;
    DOMWindow* window() const { return self(); }
    DOMWindow* frames() const { return self(); }

    virtual DOMWindow* opener() const = 0;
    virtual DOMWindow* parent() const = 0;
    virtual DOMWindow* top() const = 0;

    // DOM Level 2 AbstractView Interface
    virtual Document* document() const = 0;

    // CSSOM View Module
    virtual StyleMedia* styleMedia() const = 0;

    // WebKit extensions
    virtual double devicePixelRatio() const = 0;

    // HTML 5 key/value storage
    virtual Storage* sessionStorage(ExceptionState&) const = 0;
    virtual Storage* localStorage(ExceptionState&) const = 0;
    virtual ApplicationCache* applicationCache() const = 0;

    // This is the interface orientation in degrees. Some examples are:
    //  0 is straight up; -90 is when the device is rotated 90 clockwise;
    //  90 is when rotated counter clockwise.
    virtual int orientation() const = 0;

    virtual Console* console() const  = 0;

    virtual Performance* performance() const = 0;

    virtual DOMWindowCSS* css() const = 0;
};

} // namespace blink

#endif // DOMWindow_h
