// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/BeaconLoader.h"

#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fileapi/File.h"
#include "core/frame/LocalFrame.h"
#include "core/html/FormData.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/MixedContentChecker.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/ParsedContentType.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

class Beacon {
    STACK_ALLOCATED();
public:
    virtual bool serialize(ResourceRequest&, int, int&) const = 0;
    virtual unsigned long long size() const = 0;
    virtual const AtomicString getContentType() const = 0;
};

class BeaconString final : public Beacon {
public:
    explicit BeaconString(const String& data)
        : m_data(data)
    {
    }

    unsigned long long size() const override
    {
        return m_data.charactersSizeInBytes();
    }

    bool serialize(ResourceRequest& request, int, int&) const override
    {
        RefPtr<EncodedFormData> entityBody = EncodedFormData::create(m_data.utf8());
        request.setHTTPBody(entityBody);
        request.setHTTPContentType(getContentType());
        return true;
    }

    const AtomicString getContentType() const { return AtomicString("text/plain;charset=UTF-8"); }

private:
    const String m_data;
};

class BeaconBlob final : public Beacon {
public:
    explicit BeaconBlob(Blob* data)
        : m_data(data)
    {
        const String& blobType = m_data->type();
        if (!blobType.isEmpty() && isValidContentType(blobType))
            m_contentType = AtomicString(blobType);
    }

    unsigned long long size() const override
    {
        return m_data->size();
    }

    bool serialize(ResourceRequest& request, int, int&) const override
    {
        ASSERT(m_data);
        RefPtr<EncodedFormData> entityBody = EncodedFormData::create();
        if (m_data->hasBackingFile())
            entityBody->appendFile(toFile(m_data)->path());
        else
            entityBody->appendBlob(m_data->uuid(), m_data->blobDataHandle());

        request.setHTTPBody(entityBody.release());

        if (!m_contentType.isEmpty())
            request.setHTTPContentType(m_contentType);

        return true;
    }

    const AtomicString getContentType() const { return m_contentType; }

private:
    const Member<Blob> m_data;
    AtomicString m_contentType;
};

class BeaconDOMArrayBufferView final : public Beacon {
public:
    explicit BeaconDOMArrayBufferView(DOMArrayBufferView* data)
        : m_data(data)
    {
    }

    unsigned long long size() const override
    {
        return m_data->byteLength();
    }

    bool serialize(ResourceRequest& request, int, int&) const override
    {
        ASSERT(m_data);
        RefPtr<EncodedFormData> entityBody = EncodedFormData::create(m_data->baseAddress(), m_data->byteLength());
        request.setHTTPBody(entityBody.release());

        // FIXME: a reasonable choice, but not in the spec; should it give a default?
        AtomicString contentType = AtomicString("application/octet-stream");
        request.setHTTPContentType(contentType);

        return true;
    }

    const AtomicString getContentType() const { return nullAtom; }

private:
    const Member<DOMArrayBufferView> m_data;
};

class BeaconFormData final : public Beacon {
public:
    explicit BeaconFormData(FormData* data)
        : m_data(data)
        , m_entityBody(m_data->encodeMultiPartFormData())
    {
        m_contentType = AtomicString("multipart/form-data; boundary=") + m_entityBody->boundary().data();
    }

    unsigned long long size() const override
    {
        // FormData's size cannot be determined until serialized.
        return 0;
    }

    bool serialize(ResourceRequest& request, int allowance, int& payloadLength) const override
    {
        unsigned long long entitySize = m_entityBody->sizeInBytes();
        if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
            return false;

        request.setHTTPBody(m_entityBody.get());
        request.setHTTPContentType(m_contentType);

        payloadLength = entitySize;
        return true;
    }

    const AtomicString getContentType() const { return m_contentType; }

private:
    const Member<FormData> m_data;
    RefPtr<EncodedFormData> m_entityBody;
    AtomicString m_contentType;
};

} // namespace

class BeaconLoader::Sender {
public:
    static bool send(LocalFrame* frame, int allowance, const KURL& beaconURL, const Beacon& beacon, int& payloadLength)
    {
        if (!frame->document())
            return false;

        unsigned long long entitySize = beacon.size();
        if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
            return false;

        ResourceRequest request(beaconURL);
        request.setRequestContext(WebURLRequest::RequestContextBeacon);
        request.setHTTPMethod(HTTPNames::POST);
        request.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
        request.setAllowStoredCredentials(true);
        frame->document()->fetcher()->context().addAdditionalRequestHeaders(request, FetchSubresource);
        frame->document()->fetcher()->context().populateRequestData(request);

        if (MixedContentChecker::shouldBlockFetch(frame, request, request.url()))
            return false;

        payloadLength = entitySize;
        if (!beacon.serialize(request, allowance, payloadLength))
            return false;

        const AtomicString contentType = beacon.getContentType();
        CORSEnabled corsEnabled = IsCORSEnabled;
        if (!contentType.isNull() && FetchUtils::isSimpleHeader(AtomicString("content-type"), contentType))
            corsEnabled = NotCORSEnabled;

        FetchInitiatorInfo initiatorInfo;
        initiatorInfo.name = FetchInitiatorTypeNames::beacon;

        // The loader keeps itself alive until it receives a response and disposes itself.
        BeaconLoader* loader = new BeaconLoader(frame, request, initiatorInfo, AllowStoredCredentials, corsEnabled);
        ASSERT_UNUSED(loader, loader);
        return true;
    }
};

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, const String& data, int& payloadLength)
{
    BeaconString beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, DOMArrayBufferView* data, int& payloadLength)
{
    BeaconDOMArrayBufferView beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, FormData* data, int& payloadLength)
{
    BeaconFormData beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, Blob* data, int& payloadLength)
{
    BeaconBlob beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

BeaconLoader::BeaconLoader(LocalFrame* frame, ResourceRequest& request, const FetchInitiatorInfo& initiatorInfo, StoredCredentials credentialsAllowed, CORSEnabled corsMode)
    : PingLoader(frame, request, initiatorInfo, credentialsAllowed)
    , m_beaconOrigin(frame->document()->getSecurityOrigin())
    , m_redirectsFollowCORS(corsMode == IsCORSEnabled)
{
}

void BeaconLoader::willFollowRedirect(WebURLLoader*, WebURLRequest& passedNewRequest, const WebURLResponse& passedRedirectResponse, int64_t encodedDataLength)
{
    passedNewRequest.setAllowStoredCredentials(true);
    if (!m_redirectsFollowCORS)
        return;

    ResourceRequest& newRequest(passedNewRequest.toMutableResourceRequest());
    const ResourceResponse& redirectResponse(passedRedirectResponse.toResourceResponse());

    ASSERT(!newRequest.isNull());
    ASSERT(!redirectResponse.isNull());

    String errorDescription;
    StoredCredentials withCredentials = AllowStoredCredentials;
    ResourceLoaderOptions options;
    if (!CrossOriginAccessControl::handleRedirect(m_beaconOrigin.get(), newRequest, redirectResponse, withCredentials, options, errorDescription)) {
        if (LocalFrame* localFrame = frame()) {
            if (localFrame->document())
                localFrame->document()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, errorDescription));
        }
        // Cancel the load and self destruct.
        dispose();
        // Signal WebURLLoader that the redirect musn't be followed.
        passedNewRequest = WebURLRequest();
        return;
    }
    // FIXME: http://crbug.com/427429 is needed to correctly propagate
    // updates of Origin: following this successful redirect.
}

} // namespace blink
