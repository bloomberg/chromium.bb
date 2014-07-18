// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedValue_h
#define TracedValue_h

#include "platform/EventTracer.h"

#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {
class JSONArray;
class JSONObject;
class JSONValue;
class TracedArrayBase;
template<class T> class TracedArray;
template<class T> class TracedDictionary;

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

class TracedDictionaryBase : public TracedValueBase {
    WTF_MAKE_NONCOPYABLE(TracedDictionaryBase);
private:
    typedef TracedDictionaryBase SelfType;
public:
    TracedDictionary<SelfType>& beginDictionary(const char* name)
    {
        beginDictionaryNamed(name);
        return *reinterpret_cast<TracedDictionary<SelfType>*>(this);
    }
    TracedArray<SelfType>& beginArray(const char* name)
    {
        beginArrayNamed(name);
        return *reinterpret_cast<TracedArray<SelfType>*>(this);
    }
    SelfType& setInteger(const char* name, int value)
    {
        TracedValueBase::setInteger(name, value);
        return *this;
    }
    SelfType& setDouble(const char* name, double value)
    {
        TracedValueBase::setDouble(name, value);
        return *this;
    }
    SelfType& setBoolean(const char* name, bool value)
    {
        TracedValueBase::setBoolean(name, value);
        return *this;
    }
    SelfType& setString(const char* name, const String& value)
    {
        TracedValueBase::setString(name, value);
        return *this;
    }
private:
    TracedDictionaryBase();
    ~TracedDictionaryBase();
};

template <class OwnerType>
class TracedDictionary : public TracedDictionaryBase {
    WTF_MAKE_NONCOPYABLE(TracedDictionary);
private:
    typedef TracedDictionary<OwnerType> SelfType;
public:
    OwnerType& endDictionary()
    {
        TracedValueBase::endCurrentDictionary();
        return *reinterpret_cast<OwnerType*>(this);
    }

    TracedDictionary<SelfType>& beginDictionary(const char* name)
    {
        TracedDictionaryBase::beginDictionary(name);
        return *reinterpret_cast<TracedDictionary<SelfType>*>(this);
    }
    TracedArray<SelfType>& beginArray(const char* name)
    {
        TracedDictionaryBase::beginArray(name);
        return *reinterpret_cast<TracedArray<SelfType>*>(this);
    }
    SelfType& setInteger(const char* name, int value)
    {
        TracedDictionaryBase::setInteger(name, value);
        return *this;
    }
    SelfType& setDouble(const char* name, double value)
    {
        TracedDictionaryBase::setDouble(name, value);
        return *this;
    }
    SelfType& setBoolean(const char* name, bool value)
    {
        TracedDictionaryBase::setBoolean(name, value);
        return *this;
    }
    SelfType& setString(const char* name, const String& value)
    {
        TracedDictionaryBase::setString(name, value);
        return *this;
    }

private:
    TracedDictionary();
    ~TracedDictionary();
};

class TracedArrayBase : public TracedValueBase {
    WTF_MAKE_NONCOPYABLE(TracedArrayBase);
private:
    typedef TracedArrayBase SelfType;
public:
    TracedDictionary<SelfType>& beginDictionary()
    {
        pushDictionary();
        return *reinterpret_cast<TracedDictionary<SelfType>*>(this);
    }
    TracedArray<SelfType>& beginArray()
    {
        pushArray();
        return *reinterpret_cast<TracedArray<SelfType>*>(this);
    }

    SelfType& pushInteger(int value)
    {
        TracedValueBase::pushInteger(value);
        return *this;
    }
    SelfType& pushDouble(double value)
    {
        TracedValueBase::pushDouble(value);
        return *this;
    }
    SelfType& pushBoolean(bool value)
    {
        TracedValueBase::pushBoolean(value);
        return *this;
    }
    SelfType& pushString(const String& value)
    {
        TracedValueBase::pushString(value);
        return *this;
    }

private:
    TracedArrayBase();
    ~TracedArrayBase();
};

template <class OwnerType>
class TracedArray : public TracedArrayBase {
    WTF_MAKE_NONCOPYABLE(TracedArray);
private:
    typedef TracedArray<OwnerType> SelfType;
public:
    OwnerType& endArray()
    {
        TracedValueBase::endCurrentArray();
        return *reinterpret_cast<OwnerType*>(this);
    }

    TracedDictionary<SelfType >& beginDictionary()
    {
        TracedArrayBase::beginDictionary();
        return *reinterpret_cast<TracedDictionary<SelfType>*>(this);
    }
    TracedArray<SelfType >& beginArray()
    {
        TracedArrayBase::beginArray();
        return *reinterpret_cast<TracedArray<SelfType>*>(this);
    }

    SelfType& pushInteger(int value)
    {
        TracedArrayBase::pushInteger(value);
        return *this;
    }
    SelfType& pushDouble(double value)
    {
        TracedArrayBase::pushDouble(value);
        return *this;
    }
    SelfType& pushBoolean(bool value)
    {
        TracedArrayBase::pushBoolean(value);
        return *this;
    }
    SelfType& pushString(const String& value)
    {
        TracedArrayBase::pushString(value);
        return *this;
    }

private:
    TracedArray();
    ~TracedArray();
};

class PLATFORM_EXPORT TracedValue : public TracedValueBase {
    WTF_MAKE_NONCOPYABLE(TracedValue);
private:
    typedef TracedValue SelfType;
public:
    TracedValue();
    ~TracedValue();

    TracedDictionary<SelfType>& beginDictionary(const char* name);
    TracedArray<SelfType>& beginArray(const char* name);

    SelfType& setInteger(const char* name, int value)
    {
        TracedValueBase::setInteger(name, value);
        return *this;
    }
    SelfType& setDouble(const char* name, double value)
    {
        TracedValueBase::setDouble(name, value);
        return *this;
    }
    SelfType& setBoolean(const char* name, bool value)
    {
        TracedValueBase::setBoolean(name, value);
        return *this;
    }
    SelfType& setString(const char* name, const String& value)
    {
        TracedValueBase::setString(name, value);
        return *this;
    }

    PassRefPtr<TraceEvent::ConvertableToTraceFormat> finish();
};

} // namespace blink

#endif // TracedValue_h
