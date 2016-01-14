/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebMediaConstraints_h
#define WebMediaConstraints_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebString.h"
#include "WebVector.h"

namespace blink {

class WebMediaConstraintsPrivate;

class LongConstraint {
public:
    LongConstraint()
        : m_min()
        , m_max()
        , m_exact()
        , m_ideal()
    , m_hasMin(false)
    , m_hasMax(false)
    , m_hasExact(false)
    , m_hasIdeal(false)
    {
    }

    void setMin(long value)
    {
        m_min = value;
        m_hasMin = true;
    }

    void setMax(long value)
    {
        m_max = value;
        m_hasMax = true;
    }

    void setExact(long value)
    {
        m_exact = value;
        m_hasExact = true;
    }

    void setIdeal(long value)
    {
        m_ideal = value;
        m_hasIdeal = true;
    }

    BLINK_PLATFORM_EXPORT bool matches(long value) const;
    BLINK_PLATFORM_EXPORT bool isEmpty() const;

private:
    long m_min;
    long m_max;
    long m_exact;
    long m_ideal;
    unsigned m_hasMin : 1;
    unsigned m_hasMax : 1;
    unsigned m_hasExact : 1;
    unsigned m_hasIdeal : 1;
};

class DoubleConstraint {
public:
// Permit a certain leeway when comparing floats.
// The offset of 0.00001 is chosen based on observed behavior of
// doubles formatted with rtc::ToString.
    BLINK_PLATFORM_EXPORT static double kConstraintEpsilon;

    DoubleConstraint()
        : m_min()
        , m_max()
        , m_exact()
        , m_ideal()
        , m_hasMin(false)
        , m_hasMax(false)
        , m_hasExact(false)
        , m_hasIdeal(false)
    {
    }

    void setMin(double value)
    {
        m_min = value;
        m_hasMin = true;
    }

    void setMax(double value)
    {
        m_max = value;
        m_hasMax = true;
    }

    void setExact(double value)
    {
        m_exact = value;
        m_hasExact = true;
    }

    void setIdeal(double value)
    {
        m_ideal = value;
        m_hasIdeal = true;
    }

    BLINK_PLATFORM_EXPORT bool matches(double value) const;
    BLINK_PLATFORM_EXPORT bool isEmpty() const;

private:
    double m_min;
    double m_max;
    double m_exact;
    double m_ideal;
    unsigned m_hasMin : 1;
    unsigned m_hasMax : 1;
    unsigned m_hasExact : 1;
    unsigned m_hasIdeal : 1;
};

class StringConstraint {
public:
    // String-valued options don't have min or max, but can have multiple
    // values for ideal and exact.
    StringConstraint()
        : m_exact()
        , m_ideal()
    {
    }

    StringConstraint(const WebVector<WebString>& exact, const WebVector<WebString>& ideal)
        : m_exact(exact)
        , m_ideal(ideal)
    {
    }

    void setExact(const WebString& exact)
    {
        m_exact.assign(&exact, 1);
    }

    BLINK_PLATFORM_EXPORT bool matches(WebString value) const;
    BLINK_PLATFORM_EXPORT bool isEmpty() const;

private:
    WebVector<WebString> m_exact;
    WebVector<WebString> m_ideal;
};

class BooleanConstraint {
public:
    BooleanConstraint()
        : m_ideal(false)
        , m_exact(false)
        , m_hasIdeal(false)
        , m_hasExact(false)
    {
    }

    void setIdeal(bool value)
    {
        m_ideal = value;
        m_hasIdeal = true;
    }

    void setExact(bool value)
    {
        m_exact = value;
        m_hasExact = true;
    }

    BLINK_PLATFORM_EXPORT bool matches(bool value) const;
    BLINK_PLATFORM_EXPORT bool isEmpty() const;

private:
    unsigned m_ideal : 1;
    unsigned m_exact : 1;
    unsigned m_hasIdeal : 1;
    unsigned m_hasExact : 1;
};

struct WebMediaTrackConstraintSet {
public:
    LongConstraint width;
    LongConstraint height;
    DoubleConstraint aspectRatio;
    DoubleConstraint frameRate;
    StringConstraint facingMode;
    DoubleConstraint volume;
    LongConstraint sampleRate;
    LongConstraint sampleSize;
    BooleanConstraint echoCancellation;
    DoubleConstraint latency;
    LongConstraint channelCount;
    StringConstraint deviceId;
    StringConstraint groupId;
    // Constraints not exposed in Blink at the moment, only through
    // the legacy name interface.
    StringConstraint mediaStreamSource; // tab, screen, desktop, system
    BooleanConstraint renderToAssociatedSink;
    BooleanConstraint hotwordEnabled;
    BooleanConstraint googEchoCancellation;
    BooleanConstraint googExperimentalEchoCancellation;
    BooleanConstraint googAutoGainControl;
    BooleanConstraint googExperimentalAutoGainControl;
    BooleanConstraint googNoiseSuppression;
    BooleanConstraint googHighpassFilter;
    BooleanConstraint googTypingNoiseDetection;
    BooleanConstraint googExperimentalNoiseSuppression;
    BooleanConstraint googBeamforming;
    StringConstraint googArrayGeometry;
    BooleanConstraint googAudioMirroring;

    BLINK_PLATFORM_EXPORT bool isEmpty() const;
};

// Old type/value form of constraint. Will be deprecated.
struct WebMediaConstraint {
    WebMediaConstraint()
    {
    }

    WebMediaConstraint(WebString name, WebString value)
        : m_name(name)
        , m_value(value)
    {
    }

    WebString m_name;
    WebString m_value;
};

class WebMediaConstraints {
public:
    WebMediaConstraints() { }
    WebMediaConstraints(const WebMediaConstraints& other) { assign(other); }
    ~WebMediaConstraints() { reset(); }

    WebMediaConstraints& operator=(const WebMediaConstraints& other)
    {
        assign(other);
        return *this;
    }

    BLINK_PLATFORM_EXPORT void assign(const WebMediaConstraints&);

    BLINK_PLATFORM_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    BLINK_PLATFORM_EXPORT bool isEmpty() const;
    // Old accessors, will be deprecated
    BLINK_PLATFORM_EXPORT void getOptionalConstraints(WebVector<WebMediaConstraint>&) const;
    BLINK_PLATFORM_EXPORT void getMandatoryConstraints(WebVector<WebMediaConstraint>&) const;

    BLINK_PLATFORM_EXPORT bool getOptionalConstraintValue(const WebString& name, WebString& value) const;
    BLINK_PLATFORM_EXPORT bool getMandatoryConstraintValue(const WebString& name, WebString& value) const;
    // End of will be deprecated

    BLINK_PLATFORM_EXPORT void initialize();
    // Transition initializer, will be removed
    BLINK_PLATFORM_EXPORT void initialize(const WebVector<WebMediaConstraint>& optional, const WebVector<WebMediaConstraint>& mandatory, const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced);

    BLINK_PLATFORM_EXPORT void initialize(const WebMediaTrackConstraintSet& basic, const WebVector<WebMediaTrackConstraintSet>& advanced);

    BLINK_PLATFORM_EXPORT const WebMediaTrackConstraintSet& basic() const;
    BLINK_PLATFORM_EXPORT const WebVector<WebMediaTrackConstraintSet>& advanced() const;

private:
    WebPrivatePtr<WebMediaConstraintsPrivate> m_private;
};

} // namespace blink

#endif // WebMediaConstraints_h
