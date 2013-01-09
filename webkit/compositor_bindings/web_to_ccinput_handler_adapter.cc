// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_to_ccinput_handler_adapter.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebInputHandlerClient.h"

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, cc_name) \
    COMPILE_ASSERT(int(WebKit::webkit_name) == int(cc::cc_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusOnMainThread, InputHandlerClient::ScrollOnMainThread);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusStarted, InputHandlerClient::ScrollStarted);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusIgnored, InputHandlerClient::ScrollIgnored);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeGesture, InputHandlerClient::Gesture);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeWheel, InputHandlerClient::Wheel);

namespace WebKit {

scoped_ptr<WebToCCInputHandlerAdapter> WebToCCInputHandlerAdapter::create(scoped_ptr<WebInputHandler> handler)
{
    return scoped_ptr<WebToCCInputHandlerAdapter>(new WebToCCInputHandlerAdapter(handler.Pass()));
}

WebToCCInputHandlerAdapter::WebToCCInputHandlerAdapter(scoped_ptr<WebInputHandler> handler)
    : m_handler(handler.Pass())
{
}

WebToCCInputHandlerAdapter::~WebToCCInputHandlerAdapter()
{
}

class WebToCCInputHandlerAdapter::ClientAdapter : public WebInputHandlerClient {
public:
    ClientAdapter(cc::InputHandlerClient* client)
        : m_client(client)
    {
    }

    virtual ~ClientAdapter()
    {
    }

    virtual ScrollStatus scrollBegin(WebPoint point, ScrollInputType type) OVERRIDE
    {
        return static_cast<WebInputHandlerClient::ScrollStatus>(m_client->scrollBegin(point, static_cast<cc::InputHandlerClient::ScrollInputType>(type)));
    }

    virtual bool scrollByIfPossible(WebPoint point, WebSize offset) OVERRIDE
    {
      return m_client->scrollBy(point, offset);
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
        m_client->pinchGestureUpdate(magnifyDelta, anchor);
    }

    virtual void pinchGestureEnd() OVERRIDE
    {
        m_client->pinchGestureEnd();
    }

    virtual void startPageScaleAnimation(WebSize targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTimeSec,
                                         double durationSec) OVERRIDE
    {
        base::TimeTicks startTime = base::TimeTicks::FromInternalValue(startTimeSec * base::Time::kMicrosecondsPerSecond);
        base::TimeDelta duration = base::TimeDelta::FromMicroseconds(durationSec * base::Time::kMicrosecondsPerSecond);
        m_client->startPageScaleAnimation(targetPosition, anchorPoint, pageScale, startTime, duration);
    }

    virtual void scheduleAnimation() OVERRIDE
    {
        m_client->scheduleAnimation();
    }

    virtual bool haveTouchEventHandlersAt(WebPoint point)
    {
        return m_client->haveTouchEventHandlersAt(point);
    }

private:
    cc::InputHandlerClient* m_client;
};


void WebToCCInputHandlerAdapter::bindToClient(cc::InputHandlerClient* client)
{
    m_clientAdapter.reset(new ClientAdapter(client));
    m_handler->bindToClient(m_clientAdapter.get());
}

void WebToCCInputHandlerAdapter::animate(base::TimeTicks time)
{
    double monotonicTimeSeconds = (time - base::TimeTicks()).InSecondsF();
    m_handler->animate(monotonicTimeSeconds);
}

void WebToCCInputHandlerAdapter::mainThreadHasStoppedFlinging()
{
    m_handler->mainThreadHasStoppedFlinging();
}

}  // namespace WebKit
