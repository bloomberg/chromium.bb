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

#include <blpwtk2_viewsdelegateimpl.h>

#include <ui/views/widget/desktop_aura/desktop_native_widget_aura.h>
#include <ui/views/widget/native_widget_aura.h>

namespace blpwtk2 {

ViewsDelegateImpl::ViewsDelegateImpl()
{
}

ViewsDelegateImpl::~ViewsDelegateImpl()
{
}

bool ViewsDelegateImpl::GetSavedWindowPlacement(
    const views::Widget* widget,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const
{
    return false;
}

#if defined(OS_WIN)
HICON ViewsDelegateImpl::GetDefaultWindowIcon() const
{
    return NULL;
}

HICON ViewsDelegateImpl::GetSmallWindowIcon() const
{
    return NULL;
}

bool ViewsDelegateImpl::IsWindowInMetro(gfx::NativeWindow window) const
{
    return false;
}
#endif

views::NonClientFrameView* ViewsDelegateImpl::CreateDefaultNonClientFrameView(
        views::Widget* widget)
{
    return NULL;
}

void ViewsDelegateImpl::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate)
{
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
    // If we already have a native_widget, we don't have to try to come
    // up with one.
    if (params->native_widget)
        return;

    if (params->parent && params->type != views::Widget::InitParams::TYPE_MENU) {
        params->native_widget = new views::NativeWidgetAura(delegate);
    }
    else if (!params->parent && !params->context) {
        params->native_widget = new views::DesktopNativeWidgetAura(delegate);
    }
#endif
}

}  // close namespace blpwtk2

// vim: ts=4 et

