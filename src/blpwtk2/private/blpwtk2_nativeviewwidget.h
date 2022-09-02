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

#ifndef INCLUDED_BLPWTK2_NATIVEVIEWWIDGET_H
#define INCLUDED_BLPWTK2_NATIVEVIEWWIDGET_H

#include <blpwtk2_config.h>

#include <ui/views/widget/widget_delegate.h>

namespace views {
class NativeViewHost;
class Widget;
}  // close namespace views

namespace blpwtk2 {

class NativeViewWidgetDelegate;

// This class wraps a views::Widget that contains a gfx::NativeView (in our
// case, this is an aura::Window).  The widget can be parented into an HWND.
class NativeViewWidget final : private views::WidgetDelegateView {
  public:
    NativeViewWidget(gfx::NativeView contents,
                     NativeViewWidgetDelegate* delegate,
                     int width,
                     int height,
                     bool activatable,
                     bool rerouteMouseWheelToAnyRelatedWindow);
    ~NativeViewWidget() override;
    NativeViewWidget(const NativeViewWidget&) = delete;
    NativeViewWidget& operator=(const NativeViewWidget&) = delete;

    void destroy();
    void setDelegate(NativeViewWidgetDelegate* delegate);
    int setParent(blpwtk2::NativeView parent);
    void show();
    void hide();
    void move(int x, int y, int width, int height);

    // Return false when focus failed
    bool focus();

    blpwtk2::NativeView getNativeWidgetView() const;
    void setRegion(blpwtk2::NativeRegion);
    void compositionChanged();

    gfx::NativeWindow GetNativeWindow();

  private:
    // views::WidgetDelegate overrides
    void WindowClosing() override;
    aura::Window* GetDefaultActivationWindow() override;
    views::View* GetContentsView() override;
    bool OnNCHitTest(int* result, const gfx::Point& point) override;
    bool OnNCDragBegin(int hit_test_code) override;
    void OnNCDragMove() override;
    void OnNCDragEnd() override;
    void OnNCDoubleClick() override;
    bool OnPreHandleMessage(unsigned window,
                            unsigned message,
                            unsigned w_param,
                            long l_param,
                            LONG_PTR *result) override;

  private:
    NativeViewWidgetDelegate* d_delegate;  // held, not owned
    views::NativeViewHost* d_nativeViewHost;  // owned (by views::View)
    views::Widget* d_impl;  // owned by native widget
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_NATIVEVIEWWIDGET_H

// vim: ts=4 et

