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
#include "V8FontFaceSet.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/ScriptState.h"
#include "core/css/CSSFontFaceLoadEvent.h"
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/page/FrameView.h"
#include "core/platform/HistogramSupport.h"

namespace WebCore {

static const int defaultFontSize = 10;
static const char* const defaultFontFamily = "sans-serif";

class LoadFontPromiseResolver : public CSSSegmentedFontFace::LoadFontCallback {
public:
    static PassRefPtr<LoadFontPromiseResolver> create(const FontFamily& family, ScriptExecutionContext* context)
    {
        int numFamilies = 0;
        for (const FontFamily* f = &family; f; f = f->next())
            numFamilies++;
        return adoptRef<LoadFontPromiseResolver>(new LoadFontPromiseResolver(numFamilies, context));
    }

    virtual void notifyLoaded(CSSSegmentedFontFace*) OVERRIDE;
    virtual void notifyError(CSSSegmentedFontFace*) OVERRIDE;
    void loaded(Document*);
    void error(Document*);
    void resolve();

    ScriptPromise promise()
    {
        ScriptPromise promise = m_resolver->promise();
        m_resolver->detachPromise();
        return promise;
    }

private:
    LoadFontPromiseResolver(int numLoading, ScriptExecutionContext* context)
        : m_numLoading(numLoading)
        , m_errorOccured(false)
        , m_scriptState(ScriptState::current())
        , m_resolver(ScriptPromiseResolver::create(context))
    { }

    int m_numLoading;
    bool m_errorOccured;
    ScriptState* m_scriptState;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

void LoadFontPromiseResolver::loaded(Document* document)
{
    m_numLoading--;
    if (m_numLoading || !document)
        return;

    document->fonts()->scheduleResolve(this);
}

void LoadFontPromiseResolver::error(Document* document)
{
    m_errorOccured = true;
    loaded(document);
}

void LoadFontPromiseResolver::notifyLoaded(CSSSegmentedFontFace* face)
{
    loaded(face->fontSelector()->document());
}

void LoadFontPromiseResolver::notifyError(CSSSegmentedFontFace* face)
{
    error(face->fontSelector()->document());
}

void LoadFontPromiseResolver::resolve()
{
    ScriptScope scope(m_scriptState);
    if (m_errorOccured)
        m_resolver->reject(ScriptValue::createNull());
    else
        m_resolver->fulfill(ScriptValue::createNull());
}

class FontsReadyPromiseResolver {
public:
    static PassOwnPtr<FontsReadyPromiseResolver> create(ScriptExecutionContext* context)
    {
        return adoptPtr(new FontsReadyPromiseResolver(context));
    }

    void call(PassRefPtr<FontFaceSet> fontFaceSet)
    {
        ScriptScope scope(m_scriptState);
        m_resolver->fulfill(fontFaceSet);
    }

    ScriptPromise promise()
    {
        ScriptPromise promise = m_resolver->promise();
        m_resolver->detachPromise();
        return promise;
    }

private:
    FontsReadyPromiseResolver(ScriptExecutionContext* context)
        : m_scriptState(ScriptState::current())
        , m_resolver(ScriptPromiseResolver::create(context))
    { }
    ScriptState* m_scriptState;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

FontFaceSet::FontFaceSet(Document* document)
    : ActiveDOMObject(document)
    , m_loadingCount(0)
    , m_shouldFireDoneEvent(false)
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

AtomicString FontFaceSet::status() const
{
    DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded", AtomicString::ConstructFromLiteral));
    return (m_loadingCount > 0 || m_shouldFireDoneEvent) ? loading : loaded;
}

void FontFaceSet::didLayout()
{
    Document* d = document();
    if (d->page() && d->page()->mainFrame() == d->frame())
        m_histogram.record();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    if (m_loadingCount || (!m_shouldFireDoneEvent && m_readyResolvers.isEmpty()))
        return;
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void FontFaceSet::timerFired(Timer<FontFaceSet>*)
{
    firePendingEvents();
    resolvePendingLoadPromises();
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

void FontFaceSet::scheduleResolve(LoadFontPromiseResolver* resolver)
{
    m_pendingLoadResolvers.append(resolver);
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void FontFaceSet::resolvePendingLoadPromises()
{
    if (m_pendingLoadResolvers.isEmpty())
        return;

    Vector<RefPtr<LoadFontPromiseResolver> > resolvers;
    m_pendingLoadResolvers.swap(resolvers);
    for (size_t index = 0; index < resolvers.size(); ++index)
        resolvers[index]->resolve();
}

void FontFaceSet::beginFontLoading(FontFace* fontFace)
{
    m_histogram.incrementCount();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;

    ++m_loadingCount;
    if (m_loadingCount == 1 && !m_shouldFireDoneEvent)
        scheduleEvent(CSSFontFaceLoadEvent::createForFontFaces(eventNames().loadingEvent));
    m_shouldFireDoneEvent = false;
}

void FontFaceSet::fontLoaded(FontFace* fontFace)
{
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    m_loadedFonts.append(fontFace);
    queueDoneEvent(fontFace);
}

void FontFaceSet::loadError(FontFace* fontFace)
{
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    m_failedFonts.append(fontFace);
    queueDoneEvent(fontFace);
}

void FontFaceSet::queueDoneEvent(FontFace* fontFace)
{
    ASSERT(m_loadingCount > 0);
    --m_loadingCount;
    if (!m_loadingCount) {
        ASSERT(!m_shouldFireDoneEvent);
        m_shouldFireDoneEvent = true;
        if (!m_timer.isActive())
            m_timer.startOneShot(0);
    }
}

ScriptPromise FontFaceSet::ready()
{
    OwnPtr<FontsReadyPromiseResolver> resolver = FontsReadyPromiseResolver::create(scriptExecutionContext());
    ScriptPromise promise = resolver->promise();
    m_readyResolvers.append(resolver.release());
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
    return promise;
}

void FontFaceSet::fireDoneEventIfPossible()
{
    if (!m_pendingEvents.isEmpty() || !m_pendingLoadResolvers.isEmpty())
        return;
    if (m_loadingCount || (!m_shouldFireDoneEvent && m_readyResolvers.isEmpty()))
        return;

    // If the layout was invalidated in between when we thought layout
    // was updated and when we're ready to fire the event, just wait
    // until after the next layout before firing events.
    Document* d = document();
    if (!d->view() || d->view()->needsLayout())
        return;

    if (m_shouldFireDoneEvent) {
        m_shouldFireDoneEvent = false;
        RefPtr<CSSFontFaceLoadEvent> doneEvent;
        RefPtr<CSSFontFaceLoadEvent> errorEvent;
        doneEvent = CSSFontFaceLoadEvent::createForFontFaces(eventNames().loadingdoneEvent, m_loadedFonts);
        m_loadedFonts.clear();
        if (!m_failedFonts.isEmpty()) {
            errorEvent = CSSFontFaceLoadEvent::createForFontFaces(eventNames().loadingerrorEvent, m_failedFonts);
            m_failedFonts.clear();
        }
        dispatchEvent(doneEvent);
        if (errorEvent)
            dispatchEvent(errorEvent);
    }

    if (!m_readyResolvers.isEmpty()) {
        Vector<OwnPtr<FontsReadyPromiseResolver> > resolvers;
        m_readyResolvers.swap(resolvers);
        for (size_t index = 0; index < resolvers.size(); ++index)
            resolvers[index]->call(this);
    }
}

Vector<RefPtr<FontFace> > FontFaceSet::match(const String& fontString, const String&, ExceptionState& es)
{
    // FIXME: The second parameter (text) is ignored.
    Vector<RefPtr<FontFace> > matchedFonts;

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        es.throwUninformativeAndGenericDOMException(SyntaxError);
        return matchedFonts;
    }

    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = document()->styleResolver()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (face)
            matchedFonts.append(face->fontFaces());
    }
    return matchedFonts;
}

ScriptPromise FontFaceSet::load(const String& fontString, const String&, ExceptionState& es)
{
    // FIXME: The second parameter (text) is ignored.
    Font font;
    if (!resolveFontStyle(fontString, font)) {
        es.throwUninformativeAndGenericDOMException(SyntaxError);
        return ScriptPromise();
    }

    Document* d = document();
    RefPtr<LoadFontPromiseResolver> resolver = LoadFontPromiseResolver::create(font.family(), scriptExecutionContext());
    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = d->styleResolver()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (!face) {
            resolver->error(d);
            continue;
        }
        face->loadFont(font.fontDescription(), resolver);
    }
    return resolver->promise();
}

bool FontFaceSet::check(const String& fontString, const String&, ExceptionState& es)
{
    // FIXME: The second parameter (text) is ignored.
    Font font;
    if (!resolveFontStyle(fontString, font)) {
        es.throwUninformativeAndGenericDOMException(SyntaxError);
        return false;
    }

    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = document()->styleResolver()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (!face || !face->checkFont())
            return false;
    }
    return true;
}

bool FontFaceSet::resolveFontStyle(const String& fontString, Font& font)
{
    if (fontString.isEmpty())
        return false;

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
