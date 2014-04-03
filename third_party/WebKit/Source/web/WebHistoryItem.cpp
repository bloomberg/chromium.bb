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

#include "config.h"
#include "WebHistoryItem.h"

#include "WebSerializedScriptValue.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/html/forms/FormController.h"
#include "core/loader/HistoryItem.h"
#include "platform/network/FormData.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebHTTPBody.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "wtf/text/StringHash.h"

using namespace WebCore;

namespace blink {
namespace {

void addReferencedFilePaths(HistoryItem* item, HashSet<String>& results)
{
    const FormData* formData = item->formData();
    if (formData) {
        for (size_t i = 0; i < formData->elements().size(); ++i) {
            const FormDataElement& element = formData->elements()[i];
            if (element.m_type == FormDataElement::encodedFile)
                results.add(element.m_filename);
        }
    }

    const Vector<String>& filePaths = FormController::getReferencedFilePaths(item->documentState());
    for (size_t i = 0; i < filePaths.size(); ++i)
        results.add(filePaths[i]);

    const HistoryItemVector& children = item->deprecatedChildren();
    for (size_t i = 0; i < children.size(); ++i)
        addReferencedFilePaths(children[i].get(), results);
}

} // namespace

void WebHistoryItem::initialize()
{
    m_private = HistoryItem::create();
}

void WebHistoryItem::reset()
{
    m_private.reset();
}

void WebHistoryItem::assign(const WebHistoryItem& other)
{
    m_private = other.m_private;
}

WebString WebHistoryItem::urlString() const
{
    return m_private->urlString();
}

void WebHistoryItem::setURLString(const WebString& url)
{
    m_private->setURLString(KURL(ParsedURLString, url).string());
}

WebString WebHistoryItem::referrer() const
{
    return m_private->referrer().referrer;
}

WebReferrerPolicy WebHistoryItem::referrerPolicy() const
{
    return static_cast<WebReferrerPolicy>(m_private->referrer().referrerPolicy);
}

void WebHistoryItem::setReferrer(const WebString& referrer, WebReferrerPolicy referrerPolicy)
{
    m_private->setReferrer(Referrer(referrer, static_cast<ReferrerPolicy>(referrerPolicy)));
}

WebString WebHistoryItem::target() const
{
    return m_private->target();
}

void WebHistoryItem::setTarget(const WebString& target)
{
    m_private->setTarget(target);
}

WebPoint WebHistoryItem::scrollOffset() const
{
    return m_private->scrollPoint();
}

void WebHistoryItem::setScrollOffset(const WebPoint& scrollOffset)
{
    m_private->setScrollPoint(scrollOffset);
}

float WebHistoryItem::pageScaleFactor() const
{
    return m_private->pageScaleFactor();
}

void WebHistoryItem::setPageScaleFactor(float scale)
{
    m_private->setPageScaleFactor(scale);
}

WebVector<WebString> WebHistoryItem::documentState() const
{
    return m_private->documentState();
}

void WebHistoryItem::setDocumentState(const WebVector<WebString>& state)
{
    // FIXME: would be nice to avoid the intermediate copy
    Vector<String> ds;
    for (size_t i = 0; i < state.size(); ++i)
        ds.append(state[i]);
    m_private->setDocumentState(ds);
}

long long WebHistoryItem::itemSequenceNumber() const
{
    return m_private->itemSequenceNumber();
}

void WebHistoryItem::setItemSequenceNumber(long long itemSequenceNumber)
{
    m_private->setItemSequenceNumber(itemSequenceNumber);
}

long long WebHistoryItem::documentSequenceNumber() const
{
    return m_private->documentSequenceNumber();
}

void WebHistoryItem::setDocumentSequenceNumber(long long documentSequenceNumber)
{
    m_private->setDocumentSequenceNumber(documentSequenceNumber);
}

WebSerializedScriptValue WebHistoryItem::stateObject() const
{
    return WebSerializedScriptValue(m_private->stateObject());
}

void WebHistoryItem::setStateObject(const WebSerializedScriptValue& object)
{
    m_private->setStateObject(object);
}

WebString WebHistoryItem::httpContentType() const
{
    return m_private->formContentType();
}

void WebHistoryItem::setHTTPContentType(const WebString& httpContentType)
{
    m_private->setFormContentType(httpContentType);
}

WebHTTPBody WebHistoryItem::httpBody() const
{
    return WebHTTPBody(m_private->formData());
}

void WebHistoryItem::setHTTPBody(const WebHTTPBody& httpBody)
{
    m_private->setFormData(httpBody);
}

WebVector<WebHistoryItem> WebHistoryItem::children() const
{
    return m_private->deprecatedChildren();
}

void WebHistoryItem::setChildren(const WebVector<WebHistoryItem>& items)
{
    m_private->deprecatedClearChildren();
    for (size_t i = 0; i < items.size(); ++i)
        m_private->deprecatedAddChildItem(items[i]);
}

void WebHistoryItem::appendToChildren(const WebHistoryItem& item)
{
    m_private->deprecatedAddChildItem(item);
}

WebVector<WebString> WebHistoryItem::getReferencedFilePaths() const
{
    HashSet<String> filePaths;
    addReferencedFilePaths(m_private.get(), filePaths);

    Vector<String> results;
    copyToVector(filePaths, results);
    return results;
}

WebHistoryItem::WebHistoryItem(const PassRefPtr<HistoryItem>& item)
    : m_private(item)
{
}

WebHistoryItem& WebHistoryItem::operator=(const PassRefPtr<HistoryItem>& item)
{
    m_private = item;
    return *this;
}

WebHistoryItem::operator PassRefPtr<HistoryItem>() const
{
    return m_private.get();
}

} // namespace blink
