/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/MediaQueryList.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/MediaQueryListListener.h"
#include "core/css/MediaQueryMatcher.h"

namespace blink {

PassRefPtrWillBeRawPtr<MediaQueryList> MediaQueryList::create(PassRefPtrWillBeRawPtr<MediaQueryMatcher> matcher, PassRefPtrWillBeRawPtr<MediaQuerySet> media)
{
    return adoptRefWillBeNoop(new MediaQueryList(matcher, media));
}

MediaQueryList::MediaQueryList(PassRefPtrWillBeRawPtr<MediaQueryMatcher> matcher, PassRefPtrWillBeRawPtr<MediaQuerySet> media)
    : m_matcher(matcher)
    , m_media(media)
    , m_matchesDirty(true)
    , m_matches(false)
{
    m_matcher->addMediaQueryList(this);
    updateMatches();
}

MediaQueryList::~MediaQueryList()
{
#if !ENABLE(OILPAN)
    m_matcher->removeMediaQueryList(this);
#endif
}

String MediaQueryList::media() const
{
    return m_media->mediaText();
}

void MediaQueryList::addListener(PassRefPtrWillBeRawPtr<MediaQueryListListener> listener)
{
    if (!listener)
        return;

    listener->setMediaQueryList(this);
    m_listeners.add(listener);
}

void MediaQueryList::removeListener(PassRefPtrWillBeRawPtr<MediaQueryListListener> listener)
{
    if (!listener)
        return;

    RefPtrWillBeRawPtr<MediaQueryList> protect(this);
    listener->clearMediaQueryList();

    for (ListenerList::iterator it = m_listeners.begin(), end = m_listeners.end(); it != end; ++it) {
        // We can't just use m_listeners.remove() here, because we get a new wrapper for the
        // listener callback every time. We have to use MediaQueryListListener::operator==.
        if (**it == *listener.get()) {
            m_listeners.remove(it);
            break;
        }
    }
}

void MediaQueryList::documentDetached()
{
    m_listeners.clear();
}

void MediaQueryList::mediaFeaturesChanged(WillBeHeapVector<RefPtrWillBeMember<MediaQueryListListener> >* listenersToNotify)
{
    m_matchesDirty = true;
    if (!updateMatches())
        return;
    for (ListenerList::const_iterator it = m_listeners.begin(), end = m_listeners.end(); it != end; ++it) {
        listenersToNotify->append(*it);
    }
}

bool MediaQueryList::updateMatches()
{
    m_matchesDirty = false;
    if (m_matches != m_matcher->evaluate(m_media.get())) {
        m_matches = !m_matches;
        return true;
    }
    return false;
}

bool MediaQueryList::matches()
{
    updateMatches();
    return m_matches;
}

void MediaQueryList::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_matcher);
    visitor->trace(m_media);
    visitor->trace(m_listeners);
#endif
}

}
