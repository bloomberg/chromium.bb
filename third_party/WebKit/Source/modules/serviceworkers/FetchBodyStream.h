// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchBodyStream_h
#define FetchBodyStream_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

class ScriptState;

class FetchBodyStream FINAL
    : public RefCountedWillBeGarbageCollectedFinalized<FetchBodyStream>
    , public ScriptWrappable
    , public ActiveDOMObject
    , public FileReaderLoaderClient {
public:
    enum ResponseType {
        ResponseAsArrayBuffer,
        ResponseAsBlob,
        ResponseAsFormData,
        ResponseAsJSON,
        ResponseAsText
    };

    static PassRefPtrWillBeRawPtr<FetchBodyStream> create(ExecutionContext*, PassRefPtr<BlobDataHandle>);

    ScriptPromise asArrayBuffer(ScriptState*);
    ScriptPromise asBlob(ScriptState*);
    ScriptPromise asFormData(ScriptState*);
    ScriptPromise asJSON(ScriptState*);
    ScriptPromise asText(ScriptState*);

    // ActiveDOMObject override.
    virtual void stop() OVERRIDE;
    virtual bool hasPendingActivity() const OVERRIDE;

    void trace(Visitor*) { }

private:
    FetchBodyStream(ExecutionContext*, PassRefPtr<BlobDataHandle>);
    ScriptPromise readAsync(ScriptState*, ResponseType);
    void resolveJSON();

    // FileReaderLoaderClient functions.
    virtual void didStartLoading() OVERRIDE;
    virtual void didReceiveData() OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFail(FileError::ErrorCode) OVERRIDE;

    RefPtr<BlobDataHandle> m_blobDataHandle;
    OwnPtr<FileReaderLoader> m_loader;
    bool m_hasRead;
    ResponseType m_responseType;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

} // namespace blink

#endif // FetchBodyStream_h
