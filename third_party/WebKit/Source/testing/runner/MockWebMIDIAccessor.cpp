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

#include "MockWebMIDIAccessor.h"

#include "TestInterfaces.h"
#include "TestRunner.h"
#include "public/platform/WebMIDIAccessorClient.h"
#include "public/testing/WebTestDelegate.h"
#include "public/testing/WebTestRunner.h"

using namespace WebKit;

namespace {

class DidStartSessionTask : public WebTestRunner::WebMethodTask<WebTestRunner::MockWebMIDIAccessor> {
public:
    DidStartSessionTask(WebTestRunner::MockWebMIDIAccessor* object, WebKit::WebMIDIAccessorClient* client, bool result)
        : WebMethodTask<WebTestRunner::MockWebMIDIAccessor>(object)
        , m_client(client)
        , m_result(result)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_client->didStartSession(m_result);
    }

private:
    WebKit::WebMIDIAccessorClient* m_client;
    bool m_result;
};

} // namespace

namespace WebTestRunner {

MockWebMIDIAccessor::MockWebMIDIAccessor(WebKit::WebMIDIAccessorClient* client, TestInterfaces* interfaces)
    : m_client(client)
    , m_interfaces(interfaces)
{
}

MockWebMIDIAccessor::~MockWebMIDIAccessor()
{
}

void MockWebMIDIAccessor::startSession()
{
    // Add a mock input and output port.
    m_client->didAddInputPort("MockInputID", "MockInputManufacturer", "MockInputName", "MockInputVersion");
    m_client->didAddOutputPort("MockOutputID", "MockOutputManufacturer", "MockOutputName", "MockOutputVersion");
    m_interfaces->delegate()->postTask(new DidStartSessionTask(this, m_client, m_interfaces->testRunner()->midiAccessorResult()));
}

} // namespace WebTestRunner
