// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSyntaxDescriptor_h
#define CSSSyntaxDescriptor_h

#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

class CSSValue;

enum class CSSSyntaxType {
    TokenStream,
    Length,
    // TODO(timloh): Add all the other types
};

struct CSSSyntaxComponent {
    CSSSyntaxComponent(CSSSyntaxType type)
        : m_type(type)
    {
    }

    CSSSyntaxType m_type;
    // TODO(timloh): This will need to support arbitrary idents and list types
};

class CSSSyntaxDescriptor {
public:
    CSSSyntaxDescriptor(String syntax);

    const CSSValue* parse(CSSParserTokenRange) const;
    const CSSValue* parse(const String&) const;
    bool isValid() const { return !m_syntaxComponents.isEmpty(); }
    bool isTokenStream() const { return m_syntaxComponents.size() == 1 && m_syntaxComponents[0].m_type == CSSSyntaxType::TokenStream; }

private:
    Vector<CSSSyntaxComponent> m_syntaxComponents;
};

} // namespace blink

#endif // CSSSyntaxDescriptor_h
