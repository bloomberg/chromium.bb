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

#include "core/dom/Node.h"
#include "core/loader/cache/ResourcePtr.h"
#include "core/loader/cache/StyleSheetResourceClient.h"

namespace WebCore {

class StyleSheet;
class CSSStyleSheet;

class ProcessingInstruction FINAL : public Node, private StyleSheetResourceClient {
public:
    static PassRefPtr<ProcessingInstruction> create(Document*, const String& target, const String& data);
    virtual ~ProcessingInstruction();

    const String& target() const { return m_target; }
    const String& data() const { return m_data; }
    void setData(const String&);

    void setCreatedByParser(bool createdByParser) { m_createdByParser = createdByParser; }

    virtual void finishParsingChildren();

    const String& localHref() const { return m_localHref; }
    StyleSheet* sheet() const { return m_sheet.get(); }
    void setCSSStyleSheet(PassRefPtr<CSSStyleSheet>);

    bool isCSS() const { return m_isCSS; }
    bool isXSL() const { return m_isXSL; }

    bool isLoading() const;

private:
    ProcessingInstruction(Document*, const String& target, const String& data);

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String&);
    virtual PassRefPtr<Node> cloneNode(bool deep = true);
    virtual bool offsetInCharacters() const;
    virtual int maxCharacterOffset() const;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    void checkStyleSheet();
    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource*);
    virtual void setXSLStyleSheet(const String& href, const KURL& baseURL, const String& sheet);

    virtual bool sheetLoaded();

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    void parseStyleSheet(const String& sheet);

    String m_target;
    String m_data;
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

} //namespace

#endif
