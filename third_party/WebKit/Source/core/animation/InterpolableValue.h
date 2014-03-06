// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolableValue_h
#define InterpolableValue_h

#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class InterpolableValue {
public:
    virtual bool isNumber() const { return false; }
    virtual bool isBool() const { return false; }
    virtual bool isList() const { return false; }

    virtual ~InterpolableValue() { }
    virtual PassOwnPtr<InterpolableValue> clone() const = 0;

private:
    virtual PassOwnPtr<InterpolableValue> interpolate(const InterpolableValue &to, const double progress) const = 0;

    friend class Interpolation;

    // Keep interpolate private, but allow calls within the hierarchy without
    // knowledge of type.
    friend class InterpolableNumber;
    friend class InterpolableBool;
    friend class InterpolableList;
};

class InterpolableNumber : public InterpolableValue {
public:
    static PassOwnPtr<InterpolableNumber> create(double value)
    {
        return adoptPtr(new InterpolableNumber(value));
    }

    virtual bool isNumber() const OVERRIDE FINAL { return true; }
    double value() const { return m_value; }
    virtual PassOwnPtr<InterpolableValue> clone() const OVERRIDE FINAL { return create(m_value); }

private:
    virtual PassOwnPtr<InterpolableValue> interpolate(const InterpolableValue &to, const double progress) const OVERRIDE FINAL;
    double m_value;

    InterpolableNumber(double value)
        : m_value(value)
    { }

};

class InterpolableBool : public InterpolableValue {
public:
    static PassOwnPtr<InterpolableBool> create(bool value)
    {
        return adoptPtr(new InterpolableBool(value));
    }

    virtual bool isBool() const OVERRIDE FINAL { return true; }
    bool value() const { return m_value; }
    virtual PassOwnPtr<InterpolableValue> clone() const OVERRIDE FINAL { return create(m_value); }

private:
    virtual PassOwnPtr<InterpolableValue> interpolate(const InterpolableValue &to, const double progress) const OVERRIDE FINAL;
    bool m_value;

    InterpolableBool(bool value)
        : m_value(value)
    { }

};

class InterpolableList : public InterpolableValue {
public:
    static PassOwnPtr<InterpolableList> create(const InterpolableList &other)
    {
        return adoptPtr(new InterpolableList(other));
    }

    static PassOwnPtr<InterpolableList> create(size_t size)
    {
        return adoptPtr(new InterpolableList(size));
    }

    virtual bool isList() const OVERRIDE FINAL { return true; }
    void set(size_t position, PassOwnPtr<InterpolableValue> value)
    {
        ASSERT(position < m_size);
        m_values.get()[position] = value;
    }
    const InterpolableValue* get(size_t position) const
    {
        ASSERT(position < m_size);
        return m_values.get()[position].get();
    }
    size_t length() const { return m_size; }
    virtual PassOwnPtr<InterpolableValue> clone() const OVERRIDE FINAL { return create(*this); }

private:
    virtual PassOwnPtr<InterpolableValue> interpolate(const InterpolableValue &other, const double progress) const OVERRIDE FINAL;
    InterpolableList(size_t size)
        : m_size(size)
    {
        m_values = adoptArrayPtr(new OwnPtr<InterpolableValue>[size]);
    }

    InterpolableList(const InterpolableList& other)
        : m_size(other.m_size)
    {
        m_values = adoptArrayPtr(new OwnPtr<InterpolableValue>[m_size]);
        for (size_t i = 0; i < m_size; i++)
            set(i, other.m_values.get()[i]->clone());
    }

    size_t m_size;
    OwnPtr<OwnPtr<InterpolableValue>[]> m_values;
};

DEFINE_TYPE_CASTS(InterpolableNumber, InterpolableValue, value, value->isNumber(), value.isNumber());
DEFINE_TYPE_CASTS(InterpolableBool, InterpolableValue, value, value->isBool(), value.isBool());
DEFINE_TYPE_CASTS(InterpolableList, InterpolableValue, value, value->isList(), value.isList());

}

#endif
