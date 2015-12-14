// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamDataConsumerHandle_h
#define ReadableStreamDataConsumerHandle_h

#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace blink {

class ScriptState;

// This class is a FetchDataConsumerHandle pulling bytes from ReadableStream
// implemented with V8 Extras.
// The stream will be immediately locked by the handle and will never be
// released.
// TODO(yhirano): CURRENTLY THIS HANDLE SUPPORTS READING ONLY FROM THE THREAD ON
// WHICH IT IS CREATED. FIX THIS.
// TODO(yhirano): This implementation may cause leaks because the handle and
// the reader own a strong reference to the associated ReadableStreamReader.
// Fix it.
class MODULES_EXPORT ReadableStreamDataConsumerHandle final : public FetchDataConsumerHandle {
    WTF_MAKE_NONCOPYABLE(ReadableStreamDataConsumerHandle);
public:
    static PassOwnPtr<ReadableStreamDataConsumerHandle> create(ScriptState* scriptState, v8::Local<v8::Value> stream)
    {
        return adoptPtr(new ReadableStreamDataConsumerHandle(scriptState, stream));
    }
    ~ReadableStreamDataConsumerHandle() override;

private:
    class ReadingContext;
    ReadableStreamDataConsumerHandle(ScriptState*, v8::Local<v8::Value> stream);
    Reader* obtainReaderInternal(Client*) override;
    const char* debugName() const override { return "ReadableStreamDataConsumerHandle"; }

    RefPtr<ReadingContext> m_readingContext;
};

} // namespace blink

#endif // ReadableStreamDataConsumerHandle_h
