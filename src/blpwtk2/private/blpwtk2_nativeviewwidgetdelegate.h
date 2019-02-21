/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_NATIVEVIEWWIDGETDELEGATE_H
#define INCLUDED_BLPWTK2_NATIVEVIEWWIDGETDELEGATE_H

#include <blpwtk2_config.h>

namespace aura {
class Window;
}

namespace blpwtk2 {

class NativeViewWidget;

// This interface is implemented by WebViewImpl to receive notifications from
// blpwtk2::NativeViewWidget.
class NativeViewWidgetDelegate {
  public:
    // Invoked when the NativeViewWidget is destroyed.  The NativeView has
    // already been detached from the widget.
    virtual void onDestroyed(NativeViewWidget* source) = 0;

    // Return true if an NC hit test result was set.  Returning false means the
    // default NC hit test behavior should be performed.  The hit test should be
    // performed using the most recent mouse coordinates.
    virtual bool OnNCHitTest(int* result) = 0;

    // Called when the user starts dragging in a non-client region.  Return true
    // if the delegate will handle the non-client drag, in which case,
    // OnNCDragMove will be called continuously until OnNCDragEnd is called.
    virtual bool OnNCDragBegin(int hit_test_code) = 0;

    // Called when the user is dragging in a non-client region.  This is only
    // called if the previous OnNCDragBegin returned true.
    virtual void OnNCDragMove() = 0;

    // Called when the user is finishes dragging in a non-client region.  This is
    // only called if the previous OnNCDragBegin returned true.
    virtual void OnNCDragEnd() = 0;

    // Called to get the default activation window.  Returning NULL will use
    // the widget's root view's window.
    virtual aura::Window* GetDefaultActivationWindow() = 0;

  protected:
    virtual ~NativeViewWidgetDelegate();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_NATIVEVIEWWIDGETDELEGATE_H

// vim: ts=4 et

