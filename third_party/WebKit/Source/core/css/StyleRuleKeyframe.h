// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleRuleKeyframe_h
#define StyleRuleKeyframe_h

#include "core/css/StyleRule.h"

namespace blink {

class CSSParserValueList;
class MutableStylePropertySet;
class StylePropertySet;

class StyleRuleKeyframe final : public StyleRuleBase {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    static PassRefPtrWillBeRawPtr<StyleRuleKeyframe> create()
    {
        return adoptRefWillBeNoop(new StyleRuleKeyframe());
    }

    // Exposed to JavaScript.
    String keyText() const;
    bool setKeyText(const String&);

    // Used by StyleResolver.
    const Vector<double>& keys() const;
    // Used by BisonCSSParser when constructing a new StyleRuleKeyframe.
    void setKeys(PassOwnPtr<Vector<double>>);

    const StylePropertySet& properties() const { return *m_properties; }
    MutableStylePropertySet& mutableProperties();
    void setProperties(PassRefPtrWillBeRawPtr<StylePropertySet>);

    String cssText() const;

    DECLARE_TRACE_AFTER_DISPATCH();

    static PassOwnPtr<Vector<double>> createKeyList(CSSParserValueList*);

private:
    StyleRuleKeyframe();

    RefPtrWillBeMember<StylePropertySet> m_properties;
    Vector<double> m_keys;
};

DEFINE_STYLE_RULE_TYPE_CASTS(Keyframe);

} // namespace blink

#endif // StyleRuleKeyframe_h
