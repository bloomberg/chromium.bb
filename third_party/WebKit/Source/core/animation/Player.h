/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Player_h
#define Player_h

#include "core/animation/TimedItem.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DocumentTimeline;

class Player FINAL : public RefCounted<Player> {

public:
    ~Player();
    static PassRefPtr<Player> create(DocumentTimeline*, TimedItem*);

    // Returns whether this player is still current or in effect.
    bool update();
    void cancel();
    double currentTime() const;
    void setCurrentTime(double);
    bool paused() const { return !isNull(m_pauseStartTime); }
    void setPaused(bool);
    double playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(double);
    double startTime() const { return m_startTime; }
    double timeDrift() const;
    DocumentTimeline* timeline() { return m_timeline; }

private:
    Player(DocumentTimeline*, TimedItem*);
    static double effectiveTime(double time) { return isNull(time) ? 0 : time; }
    inline double pausedTimeDrift() const;
    inline double currentTimeBeforeDrift() const;

    double m_pauseStartTime;
    double m_playbackRate;
    double m_timeDrift;
    const double m_startTime;

    RefPtr<TimedItem> m_content;
    DocumentTimeline* const m_timeline;
};

} // namespace

#endif
