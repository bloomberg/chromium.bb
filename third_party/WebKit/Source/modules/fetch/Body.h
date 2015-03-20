// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Body_h
#define Body_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/streams/ReadableStreamImpl.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class BodyStreamBuffer;
class DOMArrayBuffer;
class DOMArrayBufferView;
class ReadableStreamReader;
class ScriptState;

class Body
    : public GarbageCollectedFinalized<Body>
    , public ScriptWrappable
    , public ActiveDOMObject
    , public FileReaderLoaderClient {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Body);
public:
    enum ResponseType {
        ResponseUnknown,
        ResponseAsArrayBuffer,
        ResponseAsBlob,
        ResponseAsFormData,
        ResponseAsJSON,
        ResponseAsText
    };
    explicit Body(ExecutionContext*);
    virtual ~Body() { }

    ScriptPromise arrayBuffer(ScriptState*);
    ScriptPromise blob(ScriptState*);
    ScriptPromise formData(ScriptState*);
    ScriptPromise json(ScriptState*);
    ScriptPromise text(ScriptState*);
    ReadableStream* body();

    // Sets the bodyUsed flag to true. This signifies that the contents of the
    // body have been consumed and cannot be accessed again.
    void setBodyUsed();
    bool bodyUsed() const;

    bool streamAccessed() const;

    // Creates a new BodyStreamBuffer to drain the data from the ReadableStream.
    BodyStreamBuffer* createDrainingStream();

    // ActiveDOMObject override.
    virtual void stop() override;
    virtual bool hasPendingActivity() const override;

    DECLARE_VIRTUAL_TRACE();

protected:
    // Copy constructor for clone() implementations
    explicit Body(const Body&);

private:
    class ReadableStreamSource;
    class BlobHandleReceiver;

    void pullSource();
    void readAllFromStream();
    ScriptPromise readAsync(ScriptState*, ResponseType);
    void readAsyncFromBlob(PassRefPtr<BlobDataHandle>);
    void resolveJSON(const String&);

    // FileReaderLoaderClient functions.
    virtual void didStartLoading() override;
    virtual void didReceiveData() override;
    virtual void didFinishLoading() override;
    virtual void didFail(FileError::ErrorCode) override;

    void didBlobHandleReceiveError(PassRefPtrWillBeRawPtr<DOMException>);

    // We use BlobDataHandle or BodyStreamBuffer as data container of the Body.
    // BodyStreamBuffer is used only when the Response object is created by
    // fetch() API.
    // FIXME: We should seek a cleaner way to handle the data.
    virtual PassRefPtr<BlobDataHandle> blobDataHandle() const = 0;
    virtual BodyStreamBuffer* buffer() const = 0;
    virtual String contentTypeForBuffer() const = 0;

    void didFinishLoadingViaStream(PassRefPtr<DOMArrayBuffer>);

    OwnPtr<FileReaderLoader> m_loader;
    bool m_bodyUsed;
    ResponseType m_responseType;
    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
    Member<ReadableStreamSource> m_streamSource;
    Member<ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBufferView>>> m_stream;
    Member<ReadableStreamReader> m_streamReader;
};

} // namespace blink

#endif // Body_h
