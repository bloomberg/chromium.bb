// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebToCCInputHandlerAdapter_h
#define WebToCCInputHandlerAdapter_h

#include "CCInputHandler.h"
#include <public/WebInputHandler.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class WebToCCInputHandlerAdapter : public cc::CCInputHandler {
public:
    static PassOwnPtr<WebToCCInputHandlerAdapter> create(PassOwnPtr<WebInputHandler>);
    virtual ~WebToCCInputHandlerAdapter();

    // cc::CCInputHandler implementation.
    virtual void bindToClient(cc::CCInputHandlerClient*) OVERRIDE;
    virtual void animate(double monotonicTime) OVERRIDE;

private:
    explicit WebToCCInputHandlerAdapter(PassOwnPtr<WebInputHandler>);

    class ClientAdapter;
    OwnPtr<ClientAdapter> m_clientAdapter;
    OwnPtr<WebInputHandler> m_handler;
};

}

#endif // WebToCCInputHandlerAdapter_h
