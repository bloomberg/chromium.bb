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

#ifndef INCLUDED_BLPWTK2_CONTEXTMENUPARAMS_H
#define INCLUDED_BLPWTK2_CONTEXTMENUPARAMS_H

#include <blpwtk2_config.h>

namespace blpwtk2 {

class ContextMenuItem;
class ContextMenuParams;
class StringRef;

namespace mojom {
class ContextMenuParams;
}

const mojom::ContextMenuParams *getContextMenuParamsImpl(const ContextMenuParams& obj);

                        // =======================
                        // class ContextMenuParams
                        // =======================

// This class contains parameters that are passed to the application whenever
// the user right clicks or presses the "Show Menu" key inside a WebView.  The
// application can use this information to show an appropriate context menu.
class BLPWTK2_EXPORT ContextMenuParams final
{
    // DATA
    const mojom::ContextMenuParams *d_impl;

    friend const mojom::ContextMenuParams *getContextMenuParamsImpl(const ContextMenuParams& obj);

  public:
    explicit ContextMenuParams(const mojom::ContextMenuParams *impl);

    const POINT pointOnScreen() const;
    bool canCut() const;
    bool canCopy() const;
    bool canPaste() const;
    bool canDelete() const;
    StringRef misspelledWord() const;

    int numCustomItems() const;
    const ContextMenuItem customItem(int index) const;

    int numSpellSuggestions() const;
    StringRef spellSuggestion(int index) const;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CONTEXTMENUPARAMS_H

// vim: ts=4 et

