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

#include "core/css/MediaQueryMatcher.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryListEvent.h"
#include "core/css/MediaQueryListListener.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "platform/wtf/Vector.h"

namespace blink {

MediaQueryMatcher* MediaQueryMatcher::Create(Document& document) {
  return new MediaQueryMatcher(document);
}

MediaQueryMatcher::MediaQueryMatcher(Document& document)
    : document_(&document) {
  DCHECK(document_);
}

MediaQueryMatcher::~MediaQueryMatcher() {}

void MediaQueryMatcher::DocumentDetached() {
  document_ = nullptr;
  evaluator_ = nullptr;
}

MediaQueryEvaluator* MediaQueryMatcher::CreateEvaluator() const {
  if (!document_ || !document_->GetFrame())
    return nullptr;

  return new MediaQueryEvaluator(document_->GetFrame());
}

bool MediaQueryMatcher::Evaluate(const MediaQuerySet* media) {
  DCHECK(!document_ || document_->GetFrame() || !evaluator_);

  if (!media)
    return false;

  // Cache the evaluator to avoid allocating one per evaluation.
  if (!evaluator_)
    evaluator_ = CreateEvaluator();

  if (evaluator_)
    return evaluator_->Eval(*media);

  return false;
}

MediaQueryList* MediaQueryMatcher::MatchMedia(const String& query) {
  if (!document_)
    return nullptr;

  RefPtr<MediaQuerySet> media = MediaQuerySet::Create(query);
  return MediaQueryList::Create(document_, this, media);
}

void MediaQueryMatcher::AddMediaQueryList(MediaQueryList* query) {
  if (!document_)
    return;
  media_lists_.insert(query);
}

void MediaQueryMatcher::RemoveMediaQueryList(MediaQueryList* query) {
  if (!document_)
    return;
  media_lists_.erase(query);
}

void MediaQueryMatcher::AddViewportListener(MediaQueryListListener* listener) {
  if (!document_)
    return;
  viewport_listeners_.insert(listener);
}

void MediaQueryMatcher::RemoveViewportListener(
    MediaQueryListListener* listener) {
  if (!document_)
    return;
  viewport_listeners_.erase(listener);
}

void MediaQueryMatcher::MediaFeaturesChanged() {
  if (!document_)
    return;

  HeapVector<Member<MediaQueryListListener>> listeners_to_notify;
  for (const auto& list : media_lists_) {
    if (list->MediaFeaturesChanged(&listeners_to_notify)) {
      Event* event = MediaQueryListEvent::Create(list);
      event->SetTarget(list);
      document_->EnqueueUniqueAnimationFrameEvent(event);
    }
  }
  document_->EnqueueMediaQueryChangeListeners(listeners_to_notify);
}

void MediaQueryMatcher::ViewportChanged() {
  if (!document_)
    return;

  HeapVector<Member<MediaQueryListListener>> listeners_to_notify;
  for (const auto& listener : viewport_listeners_)
    listeners_to_notify.push_back(listener);

  document_->EnqueueMediaQueryChangeListeners(listeners_to_notify);
}

DEFINE_TRACE(MediaQueryMatcher) {
  visitor->Trace(document_);
  visitor->Trace(evaluator_);
  visitor->Trace(media_lists_);
  visitor->Trace(viewport_listeners_);
}

}  // namespace blink
