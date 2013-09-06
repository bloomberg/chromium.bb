/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ProcessingInstruction_h
#define ProcessingInstruction_h

#include "core/dom/CharacterData.h"
#include "core/fetch/ResourcePtr.h"
#include "core/fetch/StyleSheetResourceClient.h"

namespace WebCore {

class StyleSheet;
class CSSStyleSheet;

class ProcessingInstruction FINAL : public CharacterData, private StyleSheetResourceClient {
public:
    static PassRefPtr<ProcessingInstruction> create(Document&, const String& target, const String& data);
    virtual ~ProcessingInstruction();

    const String& target() const { return m_target; }

    void setCreatedByParser(bool createdByParser) { m_createdByParser = createdByParser; }

    virtual void finishParsingChildren();

    const String& localHref() const { return m_localHref; }
    StyleSheet* sheet() const { return m_sheet.get(); }
    void setCSSStyleSheet(PassRefPtr<CSSStyleSheet>);

    bool isCSS() const { return m_isCSS; }
    bool isXSL() const { return m_isXSL; }

    bool isLoading() const;

private:
    friend class CharacterData;
    ProcessingInstruction(Document&, const String& target, const String& data);

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep = true);

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    void checkStyleSheet();
    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource*);
    virtual void setXSLStyleSheet(const String& href, const KURL& baseURL, const String& sheet);

    virtual bool sheetLoaded();

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    void parseStyleSheet(const String& sheet);

    String m_target;
    String m_localHref;
    String m_title;
    String m_media;
    ResourcePtr<Resource> m_resource;
    RefPtr<StyleSheet> m_sheet;
    bool m_loading;
    bool m_alternate;
    bool m_createdByParser;
    bool m_isCSS;
    bool m_isXSL;
};

inline ProcessingInstruction* toProcessingInstruction(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->nodeType() == Node::PROCESSING_INSTRUCTION_NODE);
    return static_cast<ProcessingInstruction*>(node);
}

} //namespace

#endif
