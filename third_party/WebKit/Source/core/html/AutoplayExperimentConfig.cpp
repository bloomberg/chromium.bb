// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/AutoplayExperimentConfig.h"

#include "wtf/text/WTFString.h"

namespace blink {

AutoplayExperimentConfig::Mode AutoplayExperimentConfig::fromString(const String& mode)
{
    AutoplayExperimentConfig::Mode value = AutoplayExperimentConfig::Mode::Off;
    if (mode.contains("-forvideo"))
        value |= AutoplayExperimentConfig::Mode::ForVideo;
    if (mode.contains("-foraudio"))
        value |= AutoplayExperimentConfig::Mode::ForAudio;
    if (mode.contains("-ifmuted"))
        value |= AutoplayExperimentConfig::Mode::IfMuted;
    if (mode.contains("-ifmobile"))
        value |= AutoplayExperimentConfig::Mode::IfMobile;
    if (mode.contains("-playmuted"))
        value |= AutoplayExperimentConfig::Mode::PlayMuted;

    return value;
}

} // namespace blink
