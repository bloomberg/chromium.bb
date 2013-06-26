/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/testing/WebPreferences.h"

using namespace WebKit;

namespace WebTestRunner {

void WebPreferences::reset()
{
    defaultFontSize = 16;
    minimumFontSize = 0;
    DOMPasteAllowed = true;
    XSSAuditorEnabled = false;
    allowDisplayOfInsecureContent = true;
    allowFileAccessFromFileURLs = true;
    allowRunningOfInsecureContent = true;
    authorAndUserStylesEnabled = true;
    defaultTextEncodingName = WebString::fromUTF8("ISO-8859-1");
    experimentalWebGLEnabled = false;
    experimentalCSSRegionsEnabled = true;
    experimentalCSSGridLayoutEnabled = true;
    javaEnabled = false;
    javaScriptCanAccessClipboard = true;
    javaScriptCanOpenWindowsAutomatically = true;
    supportsMultipleWindows = true;
    javaScriptEnabled = true;
    loadsImagesAutomatically = true;
    offlineWebApplicationCacheEnabled = true;
    pluginsEnabled = true;
    userStyleSheetLocation = WebURL();
    caretBrowsingEnabled = false;

    // Allow those layout tests running as local files, i.e. under
    // LayoutTests/http/tests/local, to access http server.
    allowUniversalAccessFromFileURLs = true;

#ifdef __APPLE__
    editingBehavior = WebSettings::EditingBehaviorMac;
#else
    editingBehavior = WebSettings::EditingBehaviorWin;
#endif

    tabsToLinks = false;
    hyperlinkAuditingEnabled = false;
    cssCustomFilterEnabled = false;
    shouldRespectImageOrientation = false;
    asynchronousSpellCheckingEnabled = false;
}

}
