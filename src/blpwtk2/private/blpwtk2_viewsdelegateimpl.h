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

#ifndef INCLUDED_BLPWTK2_VIEWSDELEGATEIMPL_H
#define INCLUDED_BLPWTK2_VIEWSDELEGATEIMPL_H

#include <blpwtk2_config.h>

#include <ui/views/views_delegate.h>

namespace blpwtk2 {

// Our implementation of views::ViewsDelegate.  This is a singleton created by
// BrowserMainRunner.  Right now, all the overrides are implemented based on
// content_shell's ShellViewsDelegateAura.  We may add custom stuff in here in
// the future.
class ViewsDelegateImpl final : public views::ViewsDelegate {
public:
    ViewsDelegateImpl();
    ~ViewsDelegateImpl() override;
    ViewsDelegateImpl(const ViewsDelegateImpl&) = delete;
    ViewsDelegateImpl& operator=(const ViewsDelegateImpl&) = delete;

private:
    void SaveWindowPlacement(
        const views::Widget* widget,
        const std::string& window_name,
        const gfx::Rect& bounds,
        ui::WindowShowState show_state) override {}
    bool GetSavedWindowPlacement(
        const views::Widget* widget,
        const std::string& window_name,
        gfx::Rect* bounds,
        ui::WindowShowState* show_state) const override;
    void NotifyMenuItemFocused(
        const std::u16string& menu_name,
        const std::u16string& menu_item_name,
        int item_index,
        int item_count,
        bool has_submenu) override {}
#if defined(OS_WIN)
    HICON GetDefaultWindowIcon() const override;
    HICON GetSmallWindowIcon() const override;
    bool IsWindowInMetro(gfx::NativeWindow window) const override;
#endif
    std::unique_ptr<views::NonClientFrameView> CreateDefaultNonClientFrameView(
        views::Widget* widget) override;
    void AddRef() override { }
    void ReleaseRef() override { }
    void OnBeforeWidgetInit(
        views::Widget::InitParams* params,
        views::internal::NativeWidgetDelegate* delegate) override;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_VIEWSDELEGATEIMPL_H

// vim: ts=4 et

