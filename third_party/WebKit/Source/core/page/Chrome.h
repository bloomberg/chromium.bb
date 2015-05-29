/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef Chrome_h
#define Chrome_h

#include "core/CoreExport.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/Cursor.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFocusType.h"
#include "wtf/Forward.h"

namespace blink {

class ChromeClient;
class ColorChooser;
class ColorChooserClient;
class DateTimeChooser;
class DateTimeChooserClient;
class FileChooser;
class LocalFrame;
class HTMLInputElement;
class HitTestResult;
class IntRect;
class Node;
class Page;
class PopupMenu;
class PopupMenuClient;
class PopupOpeningObserver;

struct DateTimeChooserParameters;
struct ViewportDescription;
struct WindowFeatures;

class CORE_EXPORT Chrome final {
public:
    virtual ~Chrome();

    static PassOwnPtr<Chrome> create(Page*, ChromeClient*);

    ChromeClient& client() { return *m_client; }

    void setCursor(const Cursor&);
    Cursor getLastSetCursorForTesting() const;

    void setWindowRect(const IntRect&) const;
    IntRect windowRect() const;

    void setWindowFeatures(const WindowFeatures&) const;

    bool canRunBeforeUnloadConfirmPanel();
    bool runBeforeUnloadConfirmPanel(const String& message, LocalFrame*);

    void runJavaScriptAlert(LocalFrame*, const String&);
    bool runJavaScriptConfirm(LocalFrame*, const String&);
    bool runJavaScriptPrompt(LocalFrame*, const String& message, const String& defaultValue, String& result);

    void mouseDidMoveOverElement(const HitTestResult&);

    void setToolTip(const HitTestResult&);

    void print(LocalFrame*);

    PassOwnPtrWillBeRawPtr<ColorChooser> createColorChooser(LocalFrame*, ColorChooserClient*, const Color& initialColor);
    PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&);
    void openTextDataListChooser(HTMLInputElement&);

    void runOpenPanel(LocalFrame*, PassRefPtr<FileChooser>);

    PassRefPtrWillBeRawPtr<PopupMenu> createPopupMenu(LocalFrame&, PopupMenuClient*);

    void registerPopupOpeningObserver(PopupOpeningObserver*);
    void unregisterPopupOpeningObserver(PopupOpeningObserver*);

private:
    Chrome(Page*, ChromeClient*);
    void notifyPopupOpeningObservers() const;

    Page* m_page;
    ChromeClient* m_client;
    Vector<PopupOpeningObserver*> m_popupOpeningObservers;
    Cursor m_lastSetMouseCursorForTesting;
};

} // namespace blink

#endif // Chrome_h
