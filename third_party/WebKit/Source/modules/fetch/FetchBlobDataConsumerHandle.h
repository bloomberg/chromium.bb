// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchBlobDataConsumerHandle_h
#define FetchBlobDataConsumerHandle_h

#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "platform/blob/BlobData.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class ExecutionContext;
// A FetchBlobDataConsumerHandle is created from a blob handle and it will
// return a valid handle from drainAsBlobDataHandle as much as possible.
class MODULES_EXPORT FetchBlobDataConsumerHandle final : public FetchDataConsumerHandle {
    WTF_MAKE_NONCOPYABLE(FetchBlobDataConsumerHandle);
public:
    class MODULES_EXPORT LoaderFactory : public GarbageCollectedFinalized<LoaderFactory> {
    public:
        virtual PassRefPtr<ThreadableLoader> create(ExecutionContext&, ThreadableLoaderClient*, const ResourceRequest&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&) = 0;
        virtual ~LoaderFactory() { }
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };

    static PassOwnPtr<FetchDataConsumerHandle> create(ExecutionContext*, PassRefPtr<BlobDataHandle>);

    // For testing.
    static PassOwnPtr<FetchDataConsumerHandle> create(ExecutionContext*, PassRefPtr<BlobDataHandle>, LoaderFactory*);

    ~FetchBlobDataConsumerHandle();

private:
    FetchBlobDataConsumerHandle(ExecutionContext*, PassRefPtr<BlobDataHandle>, LoaderFactory*);
    Reader* obtainReaderInternal(Client*) override;

    const char* debugName() const override { return "FetchBlobDataConsumerHandle"; }

    class ReaderContext;
    RefPtr<ReaderContext> m_readerContext;
};

} // namespace blink

#endif // FetchBlobDataConsumerHandle_h
