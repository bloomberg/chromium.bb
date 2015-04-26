// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Body_h
#define Body_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "modules/ModulesExport.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class BodyStreamBuffer;
class BodyStreamSource;
class DOMException;
class ReadableByteStream;
class ScriptState;

class MODULES_EXPORT Body
    : public GarbageCollectedFinalized<Body>
    , public ScriptWrappable
    , public ActiveDOMObject
    , public FileReaderLoaderClient {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Body);
public:
    class ReadableStreamSource;
    enum ResponseType {
        ResponseUnknown,
        ResponseAsArrayBuffer,
        ResponseAsBlob,
        ResponseAsFormData,
        ResponseAsJSON,
        ResponseAsText
    };
    enum LockBodyOption {
        LockBodyOptionNone,
        // Setting "body passed" flag in addition to acquiring a lock.
        PassBody,
    };
    explicit Body(ExecutionContext*);
    virtual ~Body() { }

    ScriptPromise arrayBuffer(ScriptState*);
    ScriptPromise blob(ScriptState*);
    ScriptPromise formData(ScriptState*);
    ScriptPromise json(ScriptState*);
    ScriptPromise text(ScriptState*);
    ReadableByteStream* body();

    bool bodyUsed() const;
    void lockBody(LockBodyOption = LockBodyOptionNone);

    // Returns true if the body stream is (possibly partially) consumed.
    bool isBodyConsumed() const;
    // Sets |m_stream| to a newly created stream from |source|.
    void setBody(ReadableStreamSource* /* source */);

    void setBody(PassRefPtr<BlobDataHandle> handle)
    {
        setBody(createBodySource(handle));
    }
    void setBody(BodyStreamBuffer* buffer)
    {
        setBody(createBodySource(buffer));
    }

    // Creates a new BodyStreamBuffer to drain the data from the ReadableStream.
    BodyStreamBuffer* createDrainingStream();

    // ActiveDOMObject override.
    virtual void stop() override;
    virtual bool hasPendingActivity() const override;

    ReadableStreamSource* createBodySource(PassRefPtr<BlobDataHandle>);
    ReadableStreamSource* createBodySource(BodyStreamBuffer*);

    DECLARE_VIRTUAL_TRACE();

    BodyStreamBuffer* bufferForTest() const { return buffer(); }

private:
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

    void didBlobHandleReceiveError(DOMException*);

    // We use BlobDataHandle or BodyStreamBuffer as data container of the Body.
    // BodyStreamBuffer is used only when the Response object is created by
    // fetch() API.
    // FIXME: We should seek a cleaner way to handle the data.
    virtual PassRefPtr<BlobDataHandle> blobDataHandle() const = 0;
    virtual BodyStreamBuffer* buffer() const = 0;
    virtual String mimeType() const = 0;

    void didFinishLoadingViaStream(PassRefPtr<DOMArrayBuffer>);

    OwnPtr<FileReaderLoader> m_loader;
    bool m_bodyUsed;
    ResponseType m_responseType;
    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
    Member<ReadableStreamSource> m_streamSource;
    Member<ReadableByteStream> m_stream;
};

} // namespace blink

#endif // Body_h
