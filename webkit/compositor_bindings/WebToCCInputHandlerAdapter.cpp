// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "WebToCCInputHandlerAdapter.h"

#include "IntPoint.h"
#include "IntSize.h"
#include "webcore_convert.h"
#include <public/WebInputHandlerClient.h>

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(int(WebKit::webkit_name) == int(WebCore::webcore_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusOnMainThread, CCInputHandlerClient::ScrollOnMainThread);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusStarted, CCInputHandlerClient::ScrollStarted);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusIgnored, CCInputHandlerClient::ScrollIgnored);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeGesture, CCInputHandlerClient::Gesture);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeWheel, CCInputHandlerClient::Wheel);

namespace WebKit {

PassOwnPtr<WebToCCInputHandlerAdapter> WebToCCInputHandlerAdapter::create(PassOwnPtr<WebInputHandler> handler)
{
    return adoptPtr(new WebToCCInputHandlerAdapter(handler));
}

WebToCCInputHandlerAdapter::WebToCCInputHandlerAdapter(PassOwnPtr<WebInputHandler> handler)
    : m_handler(handler)
{
}

WebToCCInputHandlerAdapter::~WebToCCInputHandlerAdapter()
{
}

class WebToCCInputHandlerAdapter::ClientAdapter : public WebInputHandlerClient {
public:
    ClientAdapter(WebCore::CCInputHandlerClient* client)
        : m_client(client)
    {
    }

    virtual ~ClientAdapter()
    {
    }

    virtual ScrollStatus scrollBegin(WebPoint point, ScrollInputType type) OVERRIDE
    {
        return static_cast<WebInputHandlerClient::ScrollStatus>(m_client->scrollBegin(convert(point), static_cast<WebCore::CCInputHandlerClient::ScrollInputType>(type)));
    }

    virtual void scrollBy(WebPoint point, WebSize offset) OVERRIDE
    {
        m_client->scrollBy(convert(point), convert(offset));
    }

    virtual void scrollEnd() OVERRIDE
    {
        m_client->scrollEnd();
    }

    virtual void pinchGestureBegin() OVERRIDE
    {
        m_client->pinchGestureBegin();
    }

    virtual void pinchGestureUpdate(float magnifyDelta, WebPoint anchor) OVERRIDE
    {
        m_client->pinchGestureUpdate(magnifyDelta, convert(anchor));
    }

    virtual void pinchGestureEnd() OVERRIDE
    {
        m_client->pinchGestureEnd();
    }

    virtual void startPageScaleAnimation(WebSize targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTime,
                                         double duration) OVERRIDE
    {
        m_client->startPageScaleAnimation(convert(targetPosition), anchorPoint, pageScale, startTime, duration);
    }

    virtual void scheduleAnimation() OVERRIDE
    {
        m_client->scheduleAnimation();
    }

private:
    WebCore::CCInputHandlerClient* m_client;
};


void WebToCCInputHandlerAdapter::bindToClient(WebCore::CCInputHandlerClient* client)
{
    m_clientAdapter = adoptPtr(new ClientAdapter(client));
    m_handler->bindToClient(m_clientAdapter.get());
}

void WebToCCInputHandlerAdapter::animate(double monotonicTime)
{
    m_handler->animate(monotonicTime);
}

}

