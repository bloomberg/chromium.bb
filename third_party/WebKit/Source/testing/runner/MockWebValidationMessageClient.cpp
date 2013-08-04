/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "MockWebValidationMessageClient.h"

#include "public/platform/WebCString.h"
#include "public/platform/WebString.h"
#include "public/testing/WebTestDelegate.h"

using namespace WebKit;

namespace WebTestRunner {

MockWebValidationMessageClient::MockWebValidationMessageClient()
    : m_delegate(0)
{
}

MockWebValidationMessageClient::~MockWebValidationMessageClient()
{
}

void MockWebValidationMessageClient::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

void MockWebValidationMessageClient::showValidationMessage(const WebRect&, const WebString& message, const WebString& subMessage, WebTextDirection)
{
    m_delegate->printMessage(std::string("ValidationMessageClient: main-message=") + std::string(message.utf8()) + " sub-message=" + std::string(subMessage.utf8()) + "\n");
}

void MockWebValidationMessageClient::hideValidationMessage()
{
}

void MockWebValidationMessageClient::moveValidationMessage(const WebRect&)
{
}

}
