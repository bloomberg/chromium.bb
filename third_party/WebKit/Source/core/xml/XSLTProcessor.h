/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2007, 2008 Apple, Inc. All rights reserved.
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

#ifndef XSLTProcessor_h
#define XSLTProcessor_h

#include "core/dom/Node.h"
#include "core/xml/XSLStyleSheet.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/StringHash.h"

#include <libxml/parserInternals.h>
#include <libxslt/documents.h>

namespace blink {

class LocalFrame;
class Document;
class DocumentFragment;

class XSLTProcessor final : public GarbageCollectedFinalized<XSLTProcessor>,
                            public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static XSLTProcessor* Create(Document& document) {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    return new XSLTProcessor(document);
  }
  ~XSLTProcessor();

  void SetXSLStyleSheet(XSLStyleSheet* style_sheet) {
    stylesheet_ = style_sheet;
  }
  bool TransformToString(Node* source,
                         String& result_mime_type,
                         String& result_string,
                         String& result_encoding);
  Document* CreateDocumentFromSource(const String& source,
                                     const String& source_encoding,
                                     const String& source_mime_type,
                                     Node* source_node,
                                     LocalFrame*);

  // DOM methods
  void importStylesheet(Node* style) { stylesheet_root_node_ = style; }
  DocumentFragment* transformToFragment(Node* source, Document* ouput_doc);
  Document* transformToDocument(Node* source);

  void setParameter(const String& namespace_uri,
                    const String& local_name,
                    const String& value);
  String getParameter(const String& namespace_uri,
                      const String& local_name) const;
  void removeParameter(const String& namespace_uri, const String& local_name);
  void clearParameters() { parameters_.clear(); }

  void reset();

  static void ParseErrorFunc(void* user_data, xmlError*);
  static void GenericErrorFunc(void* user_data, const char* msg, ...);

  // Only for libXSLT callbacks
  XSLStyleSheet* XslStylesheet() const { return stylesheet_.Get(); }

  typedef HashMap<String, String> ParameterMap;

  void Trace(blink::Visitor*);

 private:
  XSLTProcessor(Document& document) : document_(&document) {}

  Member<XSLStyleSheet> stylesheet_;
  Member<Node> stylesheet_root_node_;
  Member<Document> document_;
  ParameterMap parameters_;
};

}  // namespace blink

#endif  // XSLTProcessor_h
