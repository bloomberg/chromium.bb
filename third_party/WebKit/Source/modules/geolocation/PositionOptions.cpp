// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/geolocation/PositionOptions.h"

#include "bindings/core/v8/Dictionary.h"
#include <limits.h>

namespace blink {

PositionOptions* PositionOptions::create(const Dictionary& options)
{
    return new PositionOptions(options);
}

PositionOptions::PositionOptions(const Dictionary& options)
    : m_highAccuracy(false)
    , m_maximumAge(0)
    , m_timeout(std::numeric_limits<unsigned>::max())
{
    if (options.hasProperty("enableHighAccuracy")) {
        bool highAccuracy;
        if (DictionaryHelper::get(options, "enableHighAccuracy", highAccuracy))
            m_highAccuracy = highAccuracy;
    }
    if (options.hasProperty("maximumAge")) {
        double maximumAge;
        if (DictionaryHelper::get(options, "maximumAge", maximumAge)) {
            if (maximumAge < 0)
                m_maximumAge = 0;
            else if (maximumAge > std::numeric_limits<unsigned>::max())
                m_maximumAge = std::numeric_limits<unsigned>::max();
            else
                m_maximumAge = maximumAge;
        }
    }
    if (options.hasProperty("timeout")) {
        double timeout;
        if (DictionaryHelper::get(options, "timeout", timeout)) {
            if (timeout < 0)
                m_timeout = 0;
            else if (timeout > std::numeric_limits<unsigned>::max())
                m_timeout = std::numeric_limits<unsigned>::max();
            else
                m_timeout = timeout;
        }
    }
}

} // namespace blink
