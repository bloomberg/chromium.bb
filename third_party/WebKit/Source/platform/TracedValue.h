// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedValue_h
#define TracedValue_h

#include "platform/EventTracer.h"

#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {
class JSONArray;
class JSONObject;
class JSONValue;
template<class T> class TracedArray;

class PLATFORM_EXPORT TracedValueBase {
    WTF_MAKE_NONCOPYABLE(TracedValueBase);
protected:
    TracedValueBase();
    ~TracedValueBase();

    void setInteger(const char* name, int value);
    void setDouble(const char* name, double);
    void setBoolean(const char* name, bool value);
    void setString(const char* name, const String& value);
    void beginDictionaryNamed(const char* name);
    void beginArrayNamed(const char* name);
    void endCurrentDictionary();

    void pushInteger(int);
    void pushDouble(double);
    void pushBoolean(bool);
    void pushString(const String&);
    void pushArray();
    void pushDictionary();
    void endCurrentArray();

    JSONObject* currentDictionary() const;
    JSONArray* currentArray() const;

    Vector<RefPtr<JSONValue> > m_stack;
};

template <class OwnerType>
class TracedDictionary : public TracedValueBase {
    WTF_MAKE_NONCOPYABLE(TracedDictionary);
public:
    OwnerType& endDictionary()
    {
        ASSERT(m_stack.size() == nestingLevel);
        endCurrentDictionary();
        return *reinterpret_cast<OwnerType*>(this);
    }

    TracedDictionary<TracedDictionary<OwnerType> >& beginDictionary(const char* name)
    {
        beginDictionaryNamed(name);
        return *reinterpret_cast<TracedDictionary<TracedDictionary<OwnerType> >* >(this);
    }
    TracedArray<TracedDictionary<OwnerType> >& beginArray(const char* name)
    {
        beginArrayNamed(name);
        return *reinterpret_cast<TracedArray<TracedDictionary<OwnerType> >* >(this);
    }
    TracedDictionary<OwnerType>& setInteger(const char* name, int value)
    {
        TracedValueBase::setInteger(name, value);
        return *this;
    }
    TracedDictionary<OwnerType>& setDouble(const char* name, double value)
    {
        TracedValueBase::setDouble(name, value);
        return *this;
    }
    TracedDictionary<OwnerType>& setBoolean(const char* name, bool value)
    {
        TracedValueBase::setBoolean(name, value);
        return *this;
    }
    TracedDictionary<OwnerType>& setString(const char* name, const String& value)
    {
        TracedValueBase::setString(name, value);
        return *this;
    }

    static const size_t nestingLevel = OwnerType::nestingLevel + 1;

private:
    TracedDictionary();
    ~TracedDictionary();
};

template <class OwnerType>
class TracedArray : public TracedValueBase {
    WTF_MAKE_NONCOPYABLE(TracedArray);
public:
    TracedDictionary<TracedArray<OwnerType> >& beginDictionary()
    {
        pushDictionary();
        return *reinterpret_cast<TracedDictionary<TracedArray<OwnerType> >* >(this);
    }
    TracedDictionary<TracedArray<OwnerType> >& beginArray()
    {
        pushArray();
        return *reinterpret_cast<TracedDictionary<TracedArray<OwnerType> >* >(this);
    }
    OwnerType& endArray()
    {
        ASSERT(m_stack.size() == nestingLevel);
        endCurrentArray();
        return *reinterpret_cast<OwnerType*>(this);
    }

    TracedArray<OwnerType>& pushInteger(int value)
    {
        TracedValueBase::pushInteger(value);
        return *this;
    }
    TracedArray<OwnerType>& pushDouble(double value)
    {
        TracedValueBase::pushDouble(value);
        return *this;
    }
    TracedArray<OwnerType>& pushBoolean(bool value)
    {
        TracedValueBase::pushBoolean(value);
        return *this;
    }
    TracedArray<OwnerType>& pushString(const String& value)
    {
        TracedValueBase::pushString(value);
        return *this;
    }

    static const size_t nestingLevel = OwnerType::nestingLevel + 1;

private:
    TracedArray();
    ~TracedArray();
};

class PLATFORM_EXPORT TracedValue : public TracedValueBase {
    WTF_MAKE_NONCOPYABLE(TracedValue);
public:
    TracedValue();
    ~TracedValue();

    TracedDictionary<TracedValue>& beginDictionary(const char* name);
    TracedArray<TracedValue>& beginArray(const char* name);
    TracedValue& setInteger(const char* name, int value)
    {
        TracedValueBase::setInteger(name, value);
        return *this;
    }
    TracedValue& setDouble(const char* name, double value)
    {
        TracedValueBase::setDouble(name, value);
        return *this;
    }
    TracedValue& setBoolean(const char* name, bool value)
    {
        TracedValueBase::setBoolean(name, value);
        return *this;
    }
    TracedValue& setString(const char* name, const String& value)
    {
        TracedValueBase::setString(name, value);
        return *this;
    }
    PassRefPtr<TraceEvent::ConvertableToTraceFormat> finish();

    static const size_t nestingLevel = 1;
};

} // namespace WebCore

#endif // TracedValue_h
