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

#include <blpwtk2_contextmenuparams.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>

#include <blpwtk2_contextmenuitem.h>
#include <blpwtk2_stringref.h>

#include <base/logging.h>

#include <vector>

namespace blpwtk2 {

                        // -----------------------
                        // class ContextMenuParams
                        // -----------------------

const mojom::ContextMenuParams* getContextMenuParamsImpl(const ContextMenuParams& obj)
{
    return obj.d_impl;
}

ContextMenuParams::ContextMenuParams(const mojom::ContextMenuParams *impl)
    : d_impl(impl)
{
}

const POINT ContextMenuParams::pointOnScreen() const
{
    const POINT point = {
        d_impl->x,
        d_impl->y
    };

    return point;
}

bool ContextMenuParams::canCut() const
{
    return d_impl->canCut;
}

bool ContextMenuParams::canCopy() const
{
    return d_impl->canCopy;
}

bool ContextMenuParams::canPaste() const
{
    return d_impl->canPaste;
}

bool ContextMenuParams::canDelete() const
{
    return d_impl->canDelete;
}

StringRef ContextMenuParams::misspelledWord() const
{
    return StringRef(d_impl->misspelledWord);
}

int ContextMenuParams::numCustomItems() const
{
    return d_impl->customItems.size();
}

const ContextMenuItem ContextMenuParams::customItem(int index) const
{
    DCHECK(index >= 0 && index < static_cast<int>(d_impl->customItems.size()));
    return ContextMenuItem(d_impl->customItems[index].get());
}

int ContextMenuParams::numSpellSuggestions() const
{
    return d_impl->spellSuggestions.size();
}

StringRef ContextMenuParams::spellSuggestion(int index) const
{
    DCHECK(index >= 0 && index < static_cast<int>(d_impl->spellSuggestions.size()));
    return StringRef(d_impl->spellSuggestions[index]);
}

}

// vim: ts=4 et

