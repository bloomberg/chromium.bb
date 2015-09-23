// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchFormDataConsumerHandle_h
#define FetchFormDataConsumerHandle_h

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"

namespace blink {

class ExecutionContext;

// FetchFormDataConsumerHandle is a handle made from an EncodedFormData. It
// provides drainAsFormData in the associated Reader.
class MODULES_EXPORT FetchFormDataConsumerHandle final : public FetchDataConsumerHandle {
    WTF_MAKE_NONCOPYABLE(FetchFormDataConsumerHandle);
public:
    static PassOwnPtr<FetchDataConsumerHandle> create(const String& body);
    static PassOwnPtr<FetchDataConsumerHandle> create(PassRefPtr<DOMArrayBuffer> body);
    static PassOwnPtr<FetchDataConsumerHandle> create(PassRefPtr<DOMArrayBufferView> body);
    static PassOwnPtr<FetchDataConsumerHandle> create(const void* data, size_t);
    static PassOwnPtr<FetchDataConsumerHandle> create(ExecutionContext*, PassRefPtr<EncodedFormData> body);
    // Use FetchBlobDataConsumerHandle for blobs.

    ~FetchFormDataConsumerHandle() override;

    static PassOwnPtr<FetchDataConsumerHandle> createForTest(
        ExecutionContext*,
        PassRefPtr<EncodedFormData> body,
        FetchBlobDataConsumerHandle::LoaderFactory*);

private:
    class Context;
    class SimpleContext;
    class ComplexContext;
    class ReaderImpl;

    explicit FetchFormDataConsumerHandle(const String& body);
    FetchFormDataConsumerHandle(const void*, size_t);
    FetchFormDataConsumerHandle(ExecutionContext*, const PassRefPtr<EncodedFormData> body, FetchBlobDataConsumerHandle::LoaderFactory* = nullptr);

    Reader* obtainReaderInternal(Client*) override;

    const char* debugName() const override { return "FetchFormDataConsumerHandle"; }

    RefPtr<Context> m_context;
};

} // namespace blink

#endif // FetchFormDataConsumerHandle_h
