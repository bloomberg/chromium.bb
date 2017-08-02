/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#ifndef XMLDocumentParser_h
#define XMLDocumentParser_h

#include <libxml/tree.h>
#include <memory>
#include "core/dom/ParserContentPolicy.h"
#include "core/dom/PendingScript.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/loader/resource/ScriptResource.h"
#include "core/xml/parser/XMLErrors.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/text/SegmentedString.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class ContainerNode;
class Document;
class DocumentFragment;
class Element;
class LocalFrameView;
class ScriptElementBase;
class Text;

class XMLParserContext : public RefCounted<XMLParserContext> {
 public:
  static RefPtr<XMLParserContext> CreateMemoryParser(xmlSAXHandlerPtr,
                                                     void* user_data,
                                                     const CString& chunk);
  static RefPtr<XMLParserContext> CreateStringParser(xmlSAXHandlerPtr,
                                                     void* user_data);
  ~XMLParserContext();
  xmlParserCtxtPtr Context() const { return context_; }

 private:
  XMLParserContext(xmlParserCtxtPtr context) : context_(context) {}

  xmlParserCtxtPtr context_;
};

class XMLDocumentParser final : public ScriptableDocumentParser,
                                public PendingScriptClient {
  USING_GARBAGE_COLLECTED_MIXIN(XMLDocumentParser);

 public:
  static XMLDocumentParser* Create(Document& document, LocalFrameView* view) {
    return new XMLDocumentParser(document, view);
  }
  static XMLDocumentParser* Create(DocumentFragment* fragment,
                                   Element* element,
                                   ParserContentPolicy parser_content_policy) {
    return new XMLDocumentParser(fragment, element, parser_content_policy);
  }
  ~XMLDocumentParser() override;
  DECLARE_VIRTUAL_TRACE();

  // Exposed for callbacks:
  void HandleError(XMLErrors::ErrorType, const char* message, TextPosition);

  void SetIsXHTMLDocument(bool is_xhtml) { is_xhtml_document_ = is_xhtml; }
  bool IsXHTMLDocument() const { return is_xhtml_document_; }

  bool IsCurrentlyParsing8BitChunk() {
    return is_currently_parsing8_bit_chunk_;
  }

  static bool ParseDocumentFragment(
      const String&,
      DocumentFragment*,
      Element* parent = 0,
      ParserContentPolicy = kAllowScriptingContent);

  // Used by the XMLHttpRequest to check if the responseXML was well formed.
  bool WellFormed() const override { return !saw_error_; }

  TextPosition GetTextPosition() const override;

  static bool SupportsXMLVersion(const String&);

  class PendingCallback {
    USING_FAST_MALLOC(PendingCallback);

   public:
    virtual ~PendingCallback() {}
    virtual void Call(XMLDocumentParser*) = 0;
  };

  void SetScriptStartPosition(TextPosition);

 private:
  explicit XMLDocumentParser(Document&, LocalFrameView* = 0);
  XMLDocumentParser(DocumentFragment*, Element*, ParserContentPolicy);

  // From DocumentParser
  void insert(const SegmentedString&) override;
  void Append(const String&) override;
  void Finish() override;
  bool IsWaitingForScripts() const override;
  void StopParsing() override;
  void Detach() override;
  OrdinalNumber LineNumber() const override;
  OrdinalNumber ColumnNumber() const;

  // from PendingScriptClient
  void PendingScriptFinished(PendingScript*) override;

  void end();

  void PauseParsing();
  void ResumeParsing();

  bool AppendFragmentSource(const String&);

 public:
  // Callbacks from parser SAX
  PRINTF_FORMAT(3, 0)
  void GetError(XMLErrors::ErrorType, const char* message, va_list args);
  void StartElementNs(const AtomicString& local_name,
                      const AtomicString& prefix,
                      const AtomicString& uri,
                      int namespace_count,
                      const xmlChar** namespaces,
                      int attribute_count,
                      int defaulted_count,
                      const xmlChar** libxml_attributes);
  void EndElementNs();
  void Characters(const xmlChar* chars, int length);
  void GetProcessingInstruction(const String& target, const String& data);
  void CdataBlock(const String&);
  void Comment(const String&);
  void StartDocument(const String& version,
                     const String& encoding,
                     int standalone);
  void InternalSubset(const String& name,
                      const String& external_id,
                      const String& system_id);
  void EndDocument();

 private:
  void InitializeParserContext(const CString& chunk = CString());

  void PushCurrentNode(ContainerNode*);
  void PopCurrentNode();
  void ClearCurrentNodeStack();

  void InsertErrorMessageBlock();

  void CreateLeafTextNodeIfNeeded();
  bool UpdateLeafTextNode();

  void DoWrite(const String&);
  void DoEnd();

  bool has_view_;

  SegmentedString original_source_for_transform_;

  xmlParserCtxtPtr Context() const {
    return context_ ? context_->Context() : 0;
  }
  RefPtr<XMLParserContext> context_;
  Deque<std::unique_ptr<PendingCallback>> pending_callbacks_;
  Vector<xmlChar> buffered_text_;

  Member<ContainerNode> current_node_;
  HeapVector<Member<ContainerNode>> current_node_stack_;

  Member<Text> leaf_text_node_;

  bool is_currently_parsing8_bit_chunk_;
  bool saw_error_;
  bool saw_css_;
  bool saw_xsl_transform_;
  bool saw_first_element_;
  bool is_xhtml_document_;
  bool parser_paused_;
  bool requesting_script_;
  bool finish_called_;

  XMLErrors xml_errors_;

  Member<PendingScript> pending_script_;
  Member<ScriptElementBase> script_element_;
  TextPosition script_start_position_;

  bool parsing_fragment_;
  AtomicString default_namespace_uri_;

  typedef HashMap<AtomicString, AtomicString> PrefixForNamespaceMap;
  PrefixForNamespaceMap prefix_to_namespace_map_;
  SegmentedString pending_src_;
};

xmlDocPtr XmlDocPtrForString(Document*,
                             const String& source,
                             const String& url);
HashMap<String, String> ParseAttributes(const String&, bool& attrs_ok);

}  // namespace blink

#endif  // XMLDocumentParser_h
