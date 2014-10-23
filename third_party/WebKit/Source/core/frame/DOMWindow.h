// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

class ScriptWrappable;

class DOMWindow {
public:
    // Ideally, DOMWindow would be a ScriptWrappable. However, LocalDOMWindow
    // already has a ScriptWrappable subclass via EventTargetWithInlineData. As
    // a result, making DOMWindow a ScriptWrappable would force callers of
    // ScriptWrappable methods on DOMWindow methods to disambiguate which base
    // class to access the methods through. To avoid this trouble, DOMWindow
    // exposes an interface method to convert the DOMWindow to a ScriptWrappable
    // instead.
    virtual ScriptWrappable* toScriptWrappable() = 0;
};

} // namespace blink
