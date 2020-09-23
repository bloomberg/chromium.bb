/*
 * Copyright (C) 2015 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_RENDERERUTIL_H
#define INCLUDED_BLPWTK2_RENDERERUTIL_H

#include <blpwtk2_config.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_string.h>
#include <content/public/renderer/render_view.h>

namespace content {

class RenderWidget;
class RenderView;

}

namespace blpwtk2 {

struct RendererUtil
{
    static void handleInputEvents(content::RenderWidget     *rw,
                                  const WebView::InputEvent *events,
                                  size_t                     eventsCount);



    // patch section: screen printing
    static void drawContentsToBlob(content::RenderView        *rv,
                                   Blob                       *blob,
                                   const WebView::DrawParams&  params);


    // patch section: docprinter
    static String printToPDF(content::RenderView* renderView);



};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERERUTIL_H

// vim: ts=4 et

