// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoplayExperimentConfig_h
#define AutoplayExperimentConfig_h

namespace WTF {
class String;
}

namespace blink {

class AutoplayExperimentConfig {
    public:
    // Experiment configuration bits.  These maybe combined.  For example,
    // ForVideo|IfMuted will override the user gesture requirement for
    // playing video that has no audio or is muted.  ForVideo, by itself,
    // will entirely override the user gesture requirement for all video
    // elements, but not for audio elements.
    enum Mode {
        // Do not enable the autoplay experiment.
        Off        = 0,
        // Enable gestureless autoplay for video elements.
        ForVideo   = 1 << 0,
        // Enable gestureless autoplay for audio elements.
        ForAudio   = 1 << 1,
        // Restrict gestureless autoplay to audio-less or muted media.
        IfMuted    = 1 << 2,
        // Restrict gestureless autoplay to sites which contain the
        // viewport tag.
        IfMobile   = 1 << 3,
        // If gestureless autoplay is allowed, then mute the media before
        // starting to play.
        PlayMuted  = 1 << 4,
    };

    static Mode fromString(const WTF::String& token);
};

inline AutoplayExperimentConfig::Mode& operator|=(AutoplayExperimentConfig::Mode& a, const AutoplayExperimentConfig::Mode& b)
{
    a = static_cast<AutoplayExperimentConfig::Mode>(
        static_cast<int>(a) | static_cast<int>(b));
    return a;
}

} // namespace blink

#endif // AutoplayExperimentConfig_h
