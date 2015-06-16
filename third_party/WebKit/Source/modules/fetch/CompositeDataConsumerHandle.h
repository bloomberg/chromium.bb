// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositeDataConsumerHandle_h
#define CompositeDataConsumerHandle_h

#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

// This is a utility class to construct a composite data consumer handle. It
// owns a web data consumer handle and delegates methods. A user can update
// the handle by using |update| method.
class CompositeDataConsumerHandle final : public WebDataConsumerHandle {
public:
    // |handle| must not be null and must not be locked.
    static PassOwnPtr<CompositeDataConsumerHandle> create(PassOwnPtr<WebDataConsumerHandle> handle)
    {
        ASSERT(handle);
        return adoptPtr(new CompositeDataConsumerHandle(handle));
    }
    ~CompositeDataConsumerHandle() override;

    // This function should be called on the thread on which this object
    // was created. |handle| must not be null and must not be locked.
    void update(PassOwnPtr<WebDataConsumerHandle> /* handle */);

    // Utility static methods each of which provides a "basic block" of
    // a CompositeDataConsumerHandle. For example, a user can create
    // a CompositeDataConsumerHandle with a "waiting" handle to provide
    // WebDataConsumerHandle APIs while waiting for another handle. When that
    // handle arrives, the user will use |update| to replace the "waiting"
    // handle with the arrived one.
    //
    // Returns a handle that returns ShouldWait for read / beginRead and
    // UnexpectedError for endRead.
    static PassOwnPtr<WebDataConsumerHandle> createWaitingHandle();

    // Returns a handle that returns Done for read / beginRead and
    // UnexpectedError for endRead.
    static PassOwnPtr<WebDataConsumerHandle> createDoneHandle();

private:
    class Context;
    class ReaderImpl;
    Reader* obtainReaderInternal(Client*) override;

    explicit CompositeDataConsumerHandle(PassOwnPtr<WebDataConsumerHandle>);

    RefPtr<Context> m_context;
};

} // namespace blink

#endif // CompositeDataConsumerHandle_h
