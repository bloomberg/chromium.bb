/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
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

#include <blpwtk2_contextmenuitem.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>
#include <base/logging.h>

namespace blpwtk2 {

                        // ---------------------
                        // class ContextMenuItem
                        // ---------------------

ContextMenuItem::ContextMenuItem(mojom::ContextMenuItem *impl)
    : d_impl(impl)
{
}

StringRef ContextMenuItem::label() const
{
    return StringRef(d_impl->label);
}

StringRef ContextMenuItem::tooltip() const
{
    return StringRef(d_impl->tooltip);
}

ContextMenuItem::Type ContextMenuItem::type() const
{
    return static_cast<Type>(d_impl->type);
}

unsigned ContextMenuItem::action() const
{
    return d_impl->action;
}

ContextMenuItem::TextDirection ContextMenuItem::textDirection() const
{
    return static_cast<TextDirection>(d_impl->textDirection);
}

bool ContextMenuItem::hasDirectionalOverride() const
{
    return d_impl->hasDirectionalOverride;
}

bool ContextMenuItem::enabled() const
{
    return d_impl->enabled;
}

bool ContextMenuItem::checked() const
{
    return d_impl->checked;
}

int ContextMenuItem::numSubMenuItems() const
{
    return d_impl->subMenu.size();
}

const ContextMenuItem ContextMenuItem::subMenuItem(int index) const
{
    DCHECK(index >= 0 && index < (int)d_impl->subMenu.size());
    return ContextMenuItem(d_impl->subMenu[index].get());
}

}

// vim: ts=4 et

