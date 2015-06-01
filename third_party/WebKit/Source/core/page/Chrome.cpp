/*
 * Copyright (C) 2006, 2007, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/page/Chrome.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/HitTestResult.h"
#include "core/page/ChromeClient.h"
#include "core/page/FrameTree.h"
#include "core/page/PopupOpeningObserver.h"
#include "core/page/ScopedPageLoadDeferrer.h"
#include "core/page/WindowFeatures.h"
#include "platform/FileChooser.h"
#include "platform/Logging.h"
#include "platform/geometry/IntRect.h"
#include "platform/network/NetworkHints.h"
#include "public/platform/WebScreenInfo.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include <algorithm>

namespace blink {

using namespace HTMLNames;

Chrome::Chrome(ChromeClient* client)
    : m_client(client)
{
    ASSERT(m_client);
}

Chrome::~Chrome()
{
}

PassOwnPtr<Chrome> Chrome::create(ChromeClient* client)
{
    return adoptPtr(new Chrome(client));
}

// TODO(tkent): Remove the following functions.

void Chrome::setWindowRect(const IntRect& pendingRect) const
{
    m_client->setWindowRect(pendingRect);
}

IntRect Chrome::windowRect() const
{
    return m_client->windowRect();
}

void Chrome::setWindowFeatures(const WindowFeatures& features) const
{
    m_client->setWindowFeatures(features);
}

bool Chrome::canRunBeforeUnloadConfirmPanel()
{
    return m_client->canRunBeforeUnloadConfirmPanel();
}

bool Chrome::runBeforeUnloadConfirmPanel(const String& message, LocalFrame* frame)
{
    return m_client->runBeforeUnloadConfirmPanel(message, frame);
}

void Chrome::runJavaScriptAlert(LocalFrame* frame, const String& message)
{
    m_client->runJavaScriptAlert(frame, message);
}

bool Chrome::runJavaScriptConfirm(LocalFrame* frame, const String& message)
{
    return m_client->runJavaScriptConfirm(frame, message);
}

bool Chrome::runJavaScriptPrompt(LocalFrame* frame, const String& prompt, const String& defaultValue, String& result)
{
    return m_client->runJavaScriptPrompt(frame, prompt, defaultValue, result);
}

void Chrome::mouseDidMoveOverElement(const HitTestResult& result)
{
    m_client->mouseDidMoveOverElement(result);
}

void Chrome::print(LocalFrame* frame)
{
    m_client->print(frame);
}

PassOwnPtrWillBeRawPtr<ColorChooser> Chrome::createColorChooser(LocalFrame* frame, ColorChooserClient* client, const Color& initialColor)
{
    return m_client->createColorChooser(frame, client, initialColor);
}

PassRefPtr<DateTimeChooser> Chrome::openDateTimeChooser(DateTimeChooserClient* client, const DateTimeChooserParameters& parameters)
{
    return m_client->openDateTimeChooser(client, parameters);
}

void Chrome::openTextDataListChooser(HTMLInputElement& input)
{
    m_client->openTextDataListChooser(input);
}

void Chrome::runOpenPanel(LocalFrame* frame, PassRefPtr<FileChooser> fileChooser)
{
    m_client->runOpenPanel(frame, fileChooser);
}

void Chrome::setCursor(const Cursor& cursor)
{
    m_client->setCursor(cursor);
}

Cursor Chrome::getLastSetCursorForTesting() const
{
    return m_client->getLastSetCursorForTesting();
}

// --------

PassRefPtrWillBeRawPtr<PopupMenu> Chrome::createPopupMenu(LocalFrame& frame, PopupMenuClient* client)
{
    return m_client->createPopupMenu(frame, client);
}

void Chrome::registerPopupOpeningObserver(PopupOpeningObserver* observer)
{
    m_client->registerPopupOpeningObserver(observer);
}

void Chrome::unregisterPopupOpeningObserver(PopupOpeningObserver* observer)
{
    m_client->unregisterPopupOpeningObserver(observer);
}

} // namespace blink
