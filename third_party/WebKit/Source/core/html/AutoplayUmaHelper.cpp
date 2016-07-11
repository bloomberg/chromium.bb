// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/AutoplayUmaHelper.h"

#include "core/dom/ElementVisibilityObserver.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "platform/Histogram.h"

namespace blink {

namespace {

void recordVideoAutoplayMutedPlayMethodBecomesVisibleUma(bool visible)
{
    DEFINE_STATIC_LOCAL(BooleanHistogram, histogram, ("Media.Video.Autoplay.Muted.PlayMethod.BecomesVisible"));
    histogram.count(visible);
}

} // namespace

AutoplayUmaHelper* AutoplayUmaHelper::create(HTMLMediaElement* element)
{
    return new AutoplayUmaHelper(element);
}

AutoplayUmaHelper::AutoplayUmaHelper(HTMLMediaElement* element)
    : EventListener(CPPEventListenerType)
    , m_source(AutoplaySource::NumberOfSources)
    , m_element(element)
    , m_videoMutedPlayMethodVisibilityObserver(nullptr) { }

AutoplayUmaHelper::~AutoplayUmaHelper() = default;

bool AutoplayUmaHelper::operator==(const EventListener& other) const
{
    return this == &other;
}

void AutoplayUmaHelper::onAutoplayInitiated(AutoplaySource source)
{
    DEFINE_STATIC_LOCAL(EnumerationHistogram, videoHistogram, ("Media.Video.Autoplay", static_cast<int>(AutoplaySource::NumberOfSources)));
    DEFINE_STATIC_LOCAL(EnumerationHistogram, mutedVideoHistogram, ("Media.Video.Autoplay.Muted", static_cast<int>(AutoplaySource::NumberOfSources)));
    DEFINE_STATIC_LOCAL(EnumerationHistogram, audioHistogram, ("Media.Audio.Autoplay", static_cast<int>(AutoplaySource::NumberOfSources)));

    m_source = source;

    if (m_element->isHTMLVideoElement()) {
        videoHistogram.count(static_cast<int>(m_source));
        if (m_element->muted())
            mutedVideoHistogram.count(static_cast<int>(m_source));
    } else {
        audioHistogram.count(static_cast<int>(m_source));
    }

    if (m_source == AutoplaySource::Method && m_element->isHTMLVideoElement() && m_element->muted())
        m_element->addEventListener(EventTypeNames::playing, this, false);
}

void AutoplayUmaHelper::onElementDestroyed()
{
    if (m_videoMutedPlayMethodVisibilityObserver) {
        recordVideoAutoplayMutedPlayMethodBecomesVisibleUma(false);
        m_videoMutedPlayMethodVisibilityObserver->stop();
        m_videoMutedPlayMethodVisibilityObserver = nullptr;
    }
}

void AutoplayUmaHelper::recordAutoplayUnmuteStatus(AutoplayUnmuteActionStatus status)
{
    DEFINE_STATIC_LOCAL(EnumerationHistogram, autoplayUnmuteHistogram, ("Media.Video.Autoplay.Muted.UnmuteAction", static_cast<int>(AutoplayUnmuteActionStatus::NumberOfStatus)));

    autoplayUnmuteHistogram.count(static_cast<int>(status));
}

void AutoplayUmaHelper::onVisibilityChangedForVideoMutedPlayMethod(bool isVisible)
{
    if (!isVisible)
        return;

    recordVideoAutoplayMutedPlayMethodBecomesVisibleUma(true);
    m_videoMutedPlayMethodVisibilityObserver->stop();
    m_videoMutedPlayMethodVisibilityObserver = nullptr;
}

void AutoplayUmaHelper::handleEvent(ExecutionContext* executionContext, Event* event)
{
    DCHECK(event->type() == EventTypeNames::playing);
    handlePlayingEvent();
}

void AutoplayUmaHelper::handlePlayingEvent()
{
    if (m_source == AutoplaySource::Method && m_element->isHTMLVideoElement() && m_element->muted()) {
        if (!m_videoMutedPlayMethodVisibilityObserver) {
            m_videoMutedPlayMethodVisibilityObserver = new ElementVisibilityObserver(m_element, WTF::bind(&AutoplayUmaHelper::onVisibilityChangedForVideoMutedPlayMethod, wrapPersistent(this)));
            m_videoMutedPlayMethodVisibilityObserver->start();
        }
    }
    m_element->removeEventListener(EventTypeNames::playing, this, false);
}

DEFINE_TRACE(AutoplayUmaHelper)
{
    EventListener::trace(visitor);
    visitor->trace(m_element);
    visitor->trace(m_videoMutedPlayMethodVisibilityObserver);
}

} // namespace blink
