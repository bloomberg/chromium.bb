// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamDataConsumerHandle_h
#define ReadableStreamDataConsumerHandle_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class ScriptState;

// This class is a FetchDataConsumerHandle pulling bytes from ReadableStream
// implemented with V8 Extras.
// The stream will be immediately locked by the handle and will never be
// released.
//
// The ReadableStreamReader handle held in a ReadableStreamDataConsumerHandle
// is weak. A user must guarantee that the ReadableStreamReader object is kept
// alive appropriately.
// TODO(yhirano): CURRENTLY THIS HANDLE SUPPORTS READING ONLY FROM THE THREAD ON
// WHICH IT IS CREATED. FIX THIS.
class MODULES_EXPORT ReadableStreamDataConsumerHandle final : public FetchDataConsumerHandle {
    WTF_MAKE_NONCOPYABLE(ReadableStreamDataConsumerHandle);
public:
    static PassOwnPtr<ReadableStreamDataConsumerHandle> create(ScriptState* scriptState, ScriptValue streamReader)
    {
        return adoptPtr(new ReadableStreamDataConsumerHandle(scriptState, streamReader));
    }
    ~ReadableStreamDataConsumerHandle() override;

private:
    class ReadingContext;
    ReadableStreamDataConsumerHandle(ScriptState*, ScriptValue streamReader);
    Reader* obtainReaderInternal(Client*) override;
    const char* debugName() const override { return "ReadableStreamDataConsumerHandle"; }

    RefPtr<ReadingContext> m_readingContext;
};

} // namespace blink

#endif // ReadableStreamDataConsumerHandle_h
