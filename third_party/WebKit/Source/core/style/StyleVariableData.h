// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleVariableData_h
#define StyleVariableData_h

#include "core/css/CSSVariableData.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class StyleVariableData : public RefCounted<StyleVariableData> {
public:
    static PassRefPtr<StyleVariableData> create() { return adoptRef(new StyleVariableData()); }
    PassRefPtr<StyleVariableData> copy() const { return adoptRef(new StyleVariableData(*this)); }

    bool operator==(const StyleVariableData& other) const;
    bool operator!=(const StyleVariableData& other) const { return !(*this == other); }

    void setVariable(const AtomicString& name, PassRefPtr<CSSVariableData> value) { m_data.set(name, value); }
    CSSVariableData* getVariable(const AtomicString& name) const { return m_data.get(name); }
    void removeVariable(const AtomicString& name) { return m_data.remove(name); }

    const HashMap<AtomicString, RefPtr<CSSVariableData>>* getVariables() const { return &m_data; }
private:
    StyleVariableData() = default;
    StyleVariableData(const StyleVariableData& other) : RefCounted<StyleVariableData>(), m_data(other.m_data) { }

    friend class CSSVariableResolver;

    HashMap<AtomicString, RefPtr<CSSVariableData>> m_data;
};

} // namespace blink

#endif // StyleVariableData_h
