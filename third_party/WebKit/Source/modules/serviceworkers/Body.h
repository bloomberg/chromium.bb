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
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class ScriptState;

class Body
    : public GarbageCollectedFinalized<Body>
    , public ScriptWrappable
    , public ActiveDOMObject
    , public FileReaderLoaderClient {
    DEFINE_WRAPPERTYPEINFO();
public:
    explicit Body(ExecutionContext*);
    virtual ~Body() { }
    enum ResponseType {
        ResponseAsArrayBuffer,
        ResponseAsBlob,
        ResponseAsFormData,
        ResponseAsJSON,
        ResponseAsText
    };

    ScriptPromise arrayBuffer(ScriptState*);
    ScriptPromise blob(ScriptState*);
    ScriptPromise formData(ScriptState*);
    ScriptPromise json(ScriptState*);
    ScriptPromise text(ScriptState*);

    bool bodyUsed() const;

    // ActiveDOMObject override.
    virtual void stop() OVERRIDE;
    virtual bool hasPendingActivity() const OVERRIDE;

    virtual void trace(Visitor*) { }

protected:
    // Sets the bodyUsed flag to true. This signifies that the contents of the
    // body have been consumed and cannot be accessed again.
    void setBodyUsed();

private:
    ScriptPromise readAsync(ScriptState*, ResponseType);
    void resolveJSON();

    // FileReaderLoaderClient functions.
    virtual void didStartLoading() OVERRIDE;
    virtual void didReceiveData() OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFail(FileError::ErrorCode) OVERRIDE;

    virtual PassRefPtr<BlobDataHandle> blobDataHandle() = 0;

    OwnPtr<FileReaderLoader> m_loader;
    bool m_bodyUsed;
    ResponseType m_responseType;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

} // namespace blink

#endif // Body_h
