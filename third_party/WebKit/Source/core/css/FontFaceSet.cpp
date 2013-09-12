/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "core/css/FontFaceSet.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/Dictionary.h"
#include "core/css/CSSFontFaceLoadEvent.h"
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/page/FrameView.h"
#include "core/platform/HistogramSupport.h"

namespace WebCore {

static const int defaultFontSize = 10;
static const char* const defaultFontFamily = "sans-serif";

class LoadFontCallback : public CSSSegmentedFontFace::LoadFontCallback {
public:
    static PassRefPtr<LoadFontCallback> create(int numLoading, PassRefPtr<VoidCallback> loadCallback, PassRefPtr<VoidCallback> errorCallback)
    {
        return adoptRef<LoadFontCallback>(new LoadFontCallback(numLoading, loadCallback, errorCallback));
    }

    static PassRefPtr<LoadFontCallback> createFromParams(const Dictionary& params, const FontFamily& family)
    {
        RefPtr<VoidCallback> onsuccess;
        RefPtr<VoidCallback> onerror;
        params.get("onsuccess", onsuccess);
        params.get("onerror", onerror);
        if (!onsuccess && !onerror)
            return 0;
        int numFamilies = 0;
        for (const FontFamily* f = &family; f; f = f->next())
            numFamilies++;
        return LoadFontCallback::create(numFamilies, onsuccess, onerror);
    }

    virtual void notifyLoaded(CSSSegmentedFontFace*) OVERRIDE;
    virtual void notifyError(CSSSegmentedFontFace*) OVERRIDE;
    void loaded(Document*);
    void error(Document*);
private:
    LoadFontCallback(int numLoading, PassRefPtr<VoidCallback> loadCallback, PassRefPtr<VoidCallback> errorCallback)
        : m_numLoading(numLoading)
        , m_errorOccured(false)
        , m_loadCallback(loadCallback)
        , m_errorCallback(errorCallback)
    { }

    int m_numLoading;
    bool m_errorOccured;
    RefPtr<VoidCallback> m_loadCallback;
    RefPtr<VoidCallback> m_errorCallback;
};

void LoadFontCallback::loaded(Document* document)
{
    m_numLoading--;
    if (m_numLoading || !document)
        return;

    if (m_errorOccured) {
        if (m_errorCallback)
            document->fonts()->scheduleCallback(m_errorCallback.release());
    } else {
        if (m_loadCallback)
            document->fonts()->scheduleCallback(m_loadCallback.release());
    }
}

void LoadFontCallback::error(Document* document)
{
    m_errorOccured = true;
    loaded(document);
}

void LoadFontCallback::notifyLoaded(CSSSegmentedFontFace* face)
{
    loaded(face->fontSelector()->document());
}

void LoadFontCallback::notifyError(CSSSegmentedFontFace* face)
{
    error(face->fontSelector()->document());
}

FontFaceSet::FontFaceSet(Document* document)
    : ActiveDOMObject(document)
    , m_loadingCount(0)
    , m_timer(this, &FontFaceSet::timerFired)
{
    suspendIfNeeded();
}

FontFaceSet::~FontFaceSet()
{
}

Document* FontFaceSet::document() const
{
    return toDocument(scriptExecutionContext());
}

EventTargetData* FontFaceSet::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* FontFaceSet::ensureEventTargetData()
{
    return &m_eventTargetData;
}

const AtomicString& FontFaceSet::interfaceName() const
{
    return eventNames().interfaceForFontFaceSet;
}

ScriptExecutionContext* FontFaceSet::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void FontFaceSet::didLayout()
{
    Document* d = document();
    if (d->page() && d->page()->mainFrame() == d->frame())
        m_histogram.record();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    if (m_loadingCount || (!m_pendingDoneEvent && m_fontsReadyCallbacks.isEmpty()))
        return;
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void FontFaceSet::timerFired(Timer<FontFaceSet>*)
{
    firePendingEvents();
    firePendingCallbacks();
    fireDoneEventIfPossible();
}

void FontFaceSet::scheduleEvent(PassRefPtr<Event> event)
{
    m_pendingEvents.append(event);
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void FontFaceSet::firePendingEvents()
{
    if (m_pendingEvents.isEmpty())
        return;

    Vector<RefPtr<Event> > pendingEvents;
    m_pendingEvents.swap(pendingEvents);
    for (size_t index = 0; index < pendingEvents.size(); ++index)
        dispatchEvent(pendingEvents[index].release());
}

void FontFaceSet::scheduleCallback(PassRefPtr<VoidCallback> callback)
{
    m_pendingCallbacks.append(callback);
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void FontFaceSet::firePendingCallbacks()
{
    if (m_pendingCallbacks.isEmpty())
        return;

    Vector<RefPtr<VoidCallback> > pendingCallbacks;
    m_pendingCallbacks.swap(pendingCallbacks);
    for (size_t index = 0; index < pendingCallbacks.size(); ++index)
        pendingCallbacks[index]->handleEvent();
}

void FontFaceSet::beginFontLoading(CSSFontFaceRule* rule)
{
    m_histogram.incrementCount();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;

    ++m_loadingCount;
    if (m_loadingCount == 1 && !m_pendingDoneEvent)
        scheduleEvent(CSSFontFaceLoadEvent::createForFontFaceRule(eventNames().loadingEvent, rule));
    scheduleEvent(CSSFontFaceLoadEvent::createForFontFaceRule(eventNames().loadstartEvent, rule));
    m_pendingDoneEvent.clear();
}

void FontFaceSet::fontLoaded(CSSFontFaceRule* rule)
{
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    scheduleEvent(CSSFontFaceLoadEvent::createForFontFaceRule(eventNames().loadEvent, rule));
    queueDoneEvent(rule);
}

void FontFaceSet::loadError(CSSFontFaceRule* rule, CSSFontFaceSource* source)
{
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    // FIXME: We should report NetworkError in case of timeout, etc.
    String errorName = (source && source->isDecodeError()) ? "InvalidFontDataError" : DOMException::getErrorName(NotFoundError);
    scheduleEvent(CSSFontFaceLoadEvent::createForError(rule, DOMError::create(errorName)));
    queueDoneEvent(rule);
}

void FontFaceSet::queueDoneEvent(CSSFontFaceRule* rule)
{
    ASSERT(m_loadingCount > 0);
    --m_loadingCount;
    if (!m_loadingCount) {
        ASSERT(!m_pendingDoneEvent);
        m_pendingDoneEvent = CSSFontFaceLoadEvent::createForFontFaceRule(eventNames().loadingdoneEvent, rule);
    }
}

void FontFaceSet::notifyWhenFontsReady(PassRefPtr<VoidCallback> callback)
{
    m_fontsReadyCallbacks.append(callback);
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void FontFaceSet::fireDoneEventIfPossible()
{
    if (!m_pendingEvents.isEmpty() || !m_pendingCallbacks.isEmpty())
        return;
    if (m_loadingCount || (!m_pendingDoneEvent && m_fontsReadyCallbacks.isEmpty()))
        return;

    // If the layout was invalidated in between when we thought layout
    // was updated and when we're ready to fire the event, just wait
    // until after the next layout before firing events.
    Document* d = document();
    if (!d->view() || d->view()->needsLayout())
        return;

    if (m_pendingDoneEvent)
        dispatchEvent(m_pendingDoneEvent.release());

    if (!m_fontsReadyCallbacks.isEmpty()) {
        Vector<RefPtr<VoidCallback> > callbacks;
        m_fontsReadyCallbacks.swap(callbacks);
        for (size_t index = 0; index < callbacks.size(); ++index)
            callbacks[index]->handleEvent();
    }
}

void FontFaceSet::loadFont(const Dictionary& params)
{
    // FIXME: The text member of params is ignored.
    String fontString;
    if (!params.get("font", fontString))
        return;
    Font font;
    if (!resolveFontStyle(fontString, font))
        return;
    RefPtr<LoadFontCallback> callback = LoadFontCallback::createFromParams(params, font.family());

    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        Document* d = document();
        CSSSegmentedFontFace* face = d->styleResolver()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (!face) {
            if (callback)
                callback->error(d);
            continue;
        }
        face->loadFont(font.fontDescription(), callback);
    }
}

bool FontFaceSet::checkFont(const String& fontString, const String&)
{
    // FIXME: The second parameter (text) is ignored.
    Font font;
    if (!resolveFontStyle(fontString, font))
        return false;
    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = document()->styleResolver()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (!face || !face->checkFont())
            return false;
    }
    return true;
}

bool FontFaceSet::resolveFontStyle(const String& fontString, Font& font)
{
    // Interpret fontString in the same way as the 'font' attribute of CanvasRenderingContext2D.
    RefPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    CSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, fontString, true, CSSStrictMode, 0);
    if (parsedStyle->isEmpty())
        return false;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);
    if (fontValue == "inherit" || fontValue == "initial")
        return false;

    RefPtr<RenderStyle> style = RenderStyle::create();

    FontFamily fontFamily;
    fontFamily.setFamily(defaultFontFamily);

    FontDescription defaultFontDescription;
    defaultFontDescription.setFamily(fontFamily);
    defaultFontDescription.setSpecifiedSize(defaultFontSize);
    defaultFontDescription.setComputedSize(defaultFontSize);

    style->setFontDescription(defaultFontDescription);

    style->font().update(style->font().fontSelector());

    // Now map the font property longhands into the style.
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyFontFamily, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontStyle, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontVariant, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontWeight, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontSize, *parsedStyle),
        CSSPropertyValue(CSSPropertyLineHeight, *parsedStyle),
    };
    StyleResolver* styleResolver = document()->styleResolver();
    styleResolver->applyPropertiesToStyle(properties, WTF_ARRAY_LENGTH(properties), style.get());

    font = style->font();
    font.update(styleResolver->fontSelector());
    return true;
}

void FontFaceSet::FontLoadHistogram::record()
{
    if (m_recorded)
        return;
    m_recorded = true;
    HistogramSupport::histogramCustomCounts("WebFont.WebFontsInPage", m_count, 1, 100, 50);
}

} // namespace WebCore
