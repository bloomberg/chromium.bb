// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoplayUmaHelper_h
#define AutoplayUmaHelper_h

#include "core/events/EventListener.h"
#include "platform/heap/Handle.h"

namespace blink {

// These values are used for histograms. Do not reorder.
enum class AutoplaySource {
    // Autoplay comes from HTMLMediaElement `autoplay` attribute.
    Attribute = 0,
    // Autoplay comes from `play()` method.
    Method = 1,
    // This enum value must be last.
    NumberOfSources = 2,
};

// These values are used for histograms. Do not reorder.
enum class AutoplayUnmuteActionStatus {
    Failure = 0,
    Success = 1,
    NumberOfStatus = 2,
};

class Document;
class ElementVisibilityObserver;
class HTMLMediaElement;

class AutoplayUmaHelper final : public EventListener {
public:
    static AutoplayUmaHelper* create(HTMLMediaElement*);

    ~AutoplayUmaHelper();

    bool operator==(const EventListener&) const override;

    void onAutoplayInitiated(AutoplaySource);

    void recordAutoplayUnmuteStatus(AutoplayUnmuteActionStatus);

    void onVisibilityChangedForVideoMutedPlayMethod(bool isVisible);

    void didMoveToNewDocument(Document& oldDocument);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit AutoplayUmaHelper(HTMLMediaElement*);

    void handleEvent(ExecutionContext*, Event*) override;

    void handlePlayingEvent();
    void handleUnloadEvent();

    // The autoplay source. Use AutoplaySource::NumberOfSources for invalid source.
    AutoplaySource m_source;
    // The media element this UMA helper is attached to. |m_element| owns |this|.
    WeakMember<HTMLMediaElement> m_element;
    // The observer is used to observe whether a muted video autoplaying by play() method become visible at some point.
    Member<ElementVisibilityObserver> m_videoMutedPlayMethodVisibilityObserver;
};

} // namespace blink

#endif // AutoplayUmaHelper_h
