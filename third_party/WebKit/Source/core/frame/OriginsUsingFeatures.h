// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginsUsingFeatures_h
#define OriginsUsingFeatures_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class EventTarget;
class ScriptState;

class CORE_EXPORT OriginsUsingFeatures {
    DISALLOW_NEW();
public:
    ~OriginsUsingFeatures();

    // Features for RAPPOR. Do not reorder or remove!
    enum class Feature {
        ElementCreateShadowRoot,
        DocumentRegisterElement,
        EventPath,
        DeviceMotionInsecureOrigin,
        DeviceOrientationInsecureOrigin,
        FullscreenInsecureOrigin,
        GeolocationInsecureOrigin,
        GetUserMediaInsecureOrigin,
        GetUserMediaSecureOrigin,
        ElementAttachShadow,

        NumberOfFeatures // This must be the last item.
    };

    static void countAnyWorld(Document&, Feature);
    static void countMainWorldOnly(const ScriptState*, Document&, Feature);
    static void countOriginOrIsolatedWorldHumanReadableName(const ScriptState*, EventTarget&, Feature);

    void documentDetached(Document&);
    void updateMeasurementsAndClear();

    class CORE_EXPORT Value {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        Value();

        bool isEmpty() const { return !m_countBits; }
        void clear() { m_countBits = 0; }

        void count(Feature);
        bool get(Feature feature) const { return m_countBits & (1 << static_cast<unsigned>(feature)); }

        void aggregate(Value);
        void recordOriginToRappor(const String& origin);
        void recordNameToRappor(const String& name);

    private:
        unsigned m_countBits : static_cast<unsigned>(Feature::NumberOfFeatures);
    };

    void countName(Feature, const String&);
    HashMap<String, Value>& valueByName() { return m_valueByName; }
    void clear();

private:
    void recordOriginsToRappor();
    void recordNamesToRappor();

    Vector<std::pair<String, OriginsUsingFeatures::Value>, 1> m_originAndValues;
    HashMap<String, OriginsUsingFeatures::Value> m_valueByName;
};

} // namespace blink

#endif // OriginsUsingFeatures_h
