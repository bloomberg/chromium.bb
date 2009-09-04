/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebEditingClient_h
#define WebEditingClient_h

#include "WebEditingAction.h"
#include "WebTextAffinity.h"

namespace WebKit {
    class WebNode;
    class WebRange;
    class WebString;

    // This interface allow the client to intercept and overrule editing
    // operations.
    class WebEditingClient {
    public:
        virtual bool shouldBeginEditing(const WebRange&) = 0;
        virtual bool shouldEndEditing(const WebRange&) = 0;
        virtual bool shouldInsertNode(
            const WebNode&, const WebRange&, WebEditingAction) = 0;
        virtual bool shouldInsertText(
            const WebString&, const WebRange&, WebEditingAction) = 0;
        virtual bool shouldChangeSelectedRange(
            const WebRange& from, const WebRange& to, WebTextAffinity,
            bool stillSelecting) = 0;
        virtual bool shouldDeleteRange(const WebRange&) = 0;
        virtual bool shouldApplyStyle(const WebString& style, const WebRange&) = 0;

        virtual bool isSmartInsertDeleteEnabled() = 0;
        virtual bool isSelectTrailingWhitespaceEnabled() = 0;
        virtual void setInputMethodEnabled(bool enabled) = 0;

        virtual void didBeginEditing() = 0;
        virtual void didChangeSelection(bool isSelectionEmpty) = 0;
        virtual void didChangeContents() = 0;
        virtual void didExecuteCommand(const WebString& commandName) = 0;
        virtual void didEndEditing() = 0;
    };

} // namespace WebKit

#endif
