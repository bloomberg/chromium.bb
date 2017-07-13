/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
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
 */

#include "core/xml/parser/XMLDocumentParser.h"

#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxslt/xslt.h>
#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/HTMLNames.h"
#include "core/XMLNSNames.h"
#include "core/dom/CDATASection.h"
#include "core/dom/ClassicPendingScript.h"
#include "core/dom/Comment.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/DocumentType.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TransformSource.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/parser/HTMLEntityParser.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ImageLoader.h"
#include "core/svg/graphics/SVGImage.h"
#include "core/xml/DocumentXMLTreeViewer.h"
#include "core/xml/DocumentXSLT.h"
#include "core/xml/parser/SharedBufferReader.h"
#include "core/xml/parser/XMLDocumentParserScope.h"
#include "core/xml/parser/XMLParserInput.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StringExtras.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/UTF8.h"

namespace blink {

using namespace HTMLNames;

// FIXME: HTMLConstructionSite has a limit of 512, should these match?
static const unsigned kMaxXMLTreeDepth = 5000;

static inline String ToString(const xmlChar* string, size_t length) {
  return String::FromUTF8(reinterpret_cast<const char*>(string), length);
}

static inline String ToString(const xmlChar* string) {
  return String::FromUTF8(reinterpret_cast<const char*>(string));
}

static inline AtomicString ToAtomicString(const xmlChar* string,
                                          size_t length) {
  return AtomicString::FromUTF8(reinterpret_cast<const char*>(string), length);
}

static inline AtomicString ToAtomicString(const xmlChar* string) {
  return AtomicString::FromUTF8(reinterpret_cast<const char*>(string));
}

static inline bool HasNoStyleInformation(Document* document) {
  if (document->SawElementsInKnownNamespaces() ||
      DocumentXSLT::HasTransformSourceDocument(*document))
    return false;

  if (!document->GetFrame() || !document->GetFrame()->GetPage())
    return false;

  if (document->GetFrame()->Tree().Parent())
    return false;  // This document is not in a top frame

  if (SVGImage::IsInSVGImage(document))
    return false;

  return true;
}

class PendingStartElementNSCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingStartElementNSCallback(const AtomicString& local_name,
                                const AtomicString& prefix,
                                const AtomicString& uri,
                                int namespace_count,
                                const xmlChar** namespaces,
                                int attribute_count,
                                int defaulted_count,
                                const xmlChar** attributes)
      : local_name_(local_name),
        prefix_(prefix),
        uri_(uri),
        namespace_count_(namespace_count),
        attribute_count_(attribute_count),
        defaulted_count_(defaulted_count) {
    namespaces_ = static_cast<xmlChar**>(
        xmlMalloc(sizeof(xmlChar*) * namespace_count * 2));
    for (int i = 0; i < namespace_count * 2; ++i)
      namespaces_[i] = xmlStrdup(namespaces[i]);
    attributes_ = static_cast<xmlChar**>(
        xmlMalloc(sizeof(xmlChar*) * attribute_count * 5));
    for (int i = 0; i < attribute_count; ++i) {
      // Each attribute has 5 elements in the array:
      // name, prefix, uri, value and an end pointer.
      for (int j = 0; j < 3; ++j)
        attributes_[i * 5 + j] = xmlStrdup(attributes[i * 5 + j]);
      int length = attributes[i * 5 + 4] - attributes[i * 5 + 3];
      attributes_[i * 5 + 3] = xmlStrndup(attributes[i * 5 + 3], length);
      attributes_[i * 5 + 4] = attributes_[i * 5 + 3] + length;
    }
  }

  ~PendingStartElementNSCallback() override {
    for (int i = 0; i < namespace_count_ * 2; ++i)
      xmlFree(namespaces_[i]);
    xmlFree(namespaces_);
    for (int i = 0; i < attribute_count_; ++i)
      for (int j = 0; j < 4; ++j)
        xmlFree(attributes_[i * 5 + j]);
    xmlFree(attributes_);
  }

  void Call(XMLDocumentParser* parser) override {
    parser->StartElementNs(local_name_, prefix_, uri_, namespace_count_,
                           const_cast<const xmlChar**>(namespaces_),
                           attribute_count_, defaulted_count_,
                           const_cast<const xmlChar**>(attributes_));
  }

 private:
  AtomicString local_name_;
  AtomicString prefix_;
  AtomicString uri_;
  int namespace_count_;
  xmlChar** namespaces_;
  int attribute_count_;
  int defaulted_count_;
  xmlChar** attributes_;
};

class PendingEndElementNSCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  explicit PendingEndElementNSCallback(TextPosition script_start_position)
      : script_start_position_(script_start_position) {}

  void Call(XMLDocumentParser* parser) override {
    parser->SetScriptStartPosition(script_start_position_);
    parser->EndElementNs();
  }

 private:
  TextPosition script_start_position_;
};

class PendingCharactersCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingCharactersCallback(const xmlChar* chars, int length)
      : chars_(xmlStrndup(chars, length)), length_(length) {}

  ~PendingCharactersCallback() override { xmlFree(chars_); }

  void Call(XMLDocumentParser* parser) override {
    parser->Characters(chars_, length_);
  }

 private:
  xmlChar* chars_;
  int length_;
};

class PendingProcessingInstructionCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingProcessingInstructionCallback(const String& target, const String& data)
      : target_(target), data_(data) {}

  void Call(XMLDocumentParser* parser) override {
    parser->GetProcessingInstruction(target_, data_);
  }

 private:
  String target_;
  String data_;
};

class PendingCDATABlockCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  explicit PendingCDATABlockCallback(const String& text) : text_(text) {}

  void Call(XMLDocumentParser* parser) override { parser->CdataBlock(text_); }

 private:
  String text_;
};

class PendingCommentCallback final : public XMLDocumentParser::PendingCallback {
 public:
  explicit PendingCommentCallback(const String& text) : text_(text) {}

  void Call(XMLDocumentParser* parser) override { parser->Comment(text_); }

 private:
  String text_;
};

class PendingInternalSubsetCallback final
    : public XMLDocumentParser::PendingCallback {
 public:
  PendingInternalSubsetCallback(const String& name,
                                const String& external_id,
                                const String& system_id)
      : name_(name), external_id_(external_id), system_id_(system_id) {}

  void Call(XMLDocumentParser* parser) override {
    parser->InternalSubset(name_, external_id_, system_id_);
  }

 private:
  String name_;
  String external_id_;
  String system_id_;
};

class PendingErrorCallback final : public XMLDocumentParser::PendingCallback {
 public:
  PendingErrorCallback(XMLErrors::ErrorType type,
                       const xmlChar* message,
                       OrdinalNumber line_number,
                       OrdinalNumber column_number)
      : type_(type),
        message_(xmlStrdup(message)),
        line_number_(line_number),
        column_number_(column_number) {}

  ~PendingErrorCallback() override { xmlFree(message_); }

  void Call(XMLDocumentParser* parser) override {
    parser->HandleError(type_, reinterpret_cast<char*>(message_),
                        TextPosition(line_number_, column_number_));
  }

 private:
  XMLErrors::ErrorType type_;
  xmlChar* message_;
  OrdinalNumber line_number_;
  OrdinalNumber column_number_;
};

void XMLDocumentParser::PushCurrentNode(ContainerNode* n) {
  DCHECK(n);
  DCHECK(current_node_);
  current_node_stack_.push_back(current_node_);
  current_node_ = n;
  if (current_node_stack_.size() > kMaxXMLTreeDepth)
    HandleError(XMLErrors::kErrorTypeFatal, "Excessive node nesting.",
                GetTextPosition());
}

void XMLDocumentParser::PopCurrentNode() {
  if (!current_node_)
    return;
  DCHECK(current_node_stack_.size());
  current_node_ = current_node_stack_.back();
  current_node_stack_.pop_back();
}

void XMLDocumentParser::ClearCurrentNodeStack() {
  current_node_ = nullptr;
  leaf_text_node_ = nullptr;

  if (current_node_stack_.size()) {  // Aborted parsing.
    current_node_stack_.clear();
  }
}

void XMLDocumentParser::insert(const SegmentedString&) {
  NOTREACHED();
}

void XMLDocumentParser::Append(const String& input_source) {
  const SegmentedString source(input_source);
  if (saw_xsl_transform_ || !saw_first_element_)
    original_source_for_transform_.Append(source);

  if (IsStopped() || saw_xsl_transform_)
    return;

  if (parser_paused_) {
    pending_src_.Append(source);
    return;
  }

  DoWrite(source.ToString());
}

void XMLDocumentParser::HandleError(XMLErrors::ErrorType type,
                                    const char* formatted_message,
                                    TextPosition position) {
  xml_errors_.HandleError(type, formatted_message, position);
  if (type != XMLErrors::kErrorTypeWarning)
    saw_error_ = true;
  if (type == XMLErrors::kErrorTypeFatal)
    StopParsing();
}

void XMLDocumentParser::CreateLeafTextNodeIfNeeded() {
  if (leaf_text_node_)
    return;

  DCHECK_EQ(buffered_text_.size(), 0u);
  leaf_text_node_ = Text::Create(current_node_->GetDocument(), "");
  current_node_->ParserAppendChild(leaf_text_node_.Get());
}

bool XMLDocumentParser::UpdateLeafTextNode() {
  if (IsStopped())
    return false;

  if (!leaf_text_node_)
    return true;

  leaf_text_node_->appendData(
      ToString(buffered_text_.data(), buffered_text_.size()));
  buffered_text_.clear();
  leaf_text_node_ = nullptr;

  // Mutation event handlers executed by appendData() might detach this parser.
  return !IsStopped();
}

void XMLDocumentParser::Detach() {
  if (pending_script_) {
    pending_script_->StopWatchingForLoad();
    pending_script_ = nullptr;
  }
  ClearCurrentNodeStack();
  ScriptableDocumentParser::Detach();
}

void XMLDocumentParser::end() {
  TRACE_EVENT0("blink", "XMLDocumentParser::end");
  // XMLDocumentParserLibxml2 will do bad things to the document if doEnd() is
  // called.  I don't believe XMLDocumentParserQt needs doEnd called in the
  // fragment case.
  DCHECK(!parsing_fragment_);

  DoEnd();

  // doEnd() call above can detach the parser and null out its document.
  // In that case, we just bail out.
  if (IsDetached())
    return;

  // doEnd() could process a script tag, thus pausing parsing.
  if (parser_paused_)
    return;

  if (saw_error_)
    InsertErrorMessageBlock();
  else
    UpdateLeafTextNode();

  if (IsParsing())
    PrepareToStopParsing();
  GetDocument()->SetReadyState(Document::kInteractive);
  ClearCurrentNodeStack();
  GetDocument()->FinishedParsing();
}

void XMLDocumentParser::Finish() {
  // FIXME: We should DCHECK(!m_parserStopped) here, since it does not
  // makes sense to call any methods on DocumentParser once it's been stopped.
  // However, FrameLoader::stop calls DocumentParser::finish unconditionally.

  Flush();
  if (IsDetached())
    return;

  if (parser_paused_)
    finish_called_ = true;
  else
    end();
}

void XMLDocumentParser::InsertErrorMessageBlock() {
  xml_errors_.InsertErrorMessageBlock();
}

void XMLDocumentParser::PendingScriptFinished(
    PendingScript* unused_pending_script) {
  DCHECK_EQ(unused_pending_script, pending_script_);
  PendingScript* pending_script = pending_script_;
  pending_script_ = nullptr;

  pending_script->StopWatchingForLoad();

  Element* e = script_element_;
  script_element_ = nullptr;

  ScriptLoader* script_loader =
      ScriptElementBase::FromElementIfPossible(e)->Loader();
  DCHECK(script_loader);
  CHECK_EQ(script_loader->GetScriptType(), ScriptType::kClassic);

  if (!pending_script->ErrorOccurred()) {
    const double script_parser_blocking_time =
        pending_script->ParserBlockingLoadStartTime();
    if (script_parser_blocking_time > 0.0) {
      DocumentParserTiming::From(*GetDocument())
          .RecordParserBlockedOnScriptLoadDuration(
              MonotonicallyIncreasingTime() - script_parser_blocking_time,
              script_loader->WasCreatedDuringDocumentWrite());
    }
  }
  script_loader->ExecuteScriptBlock(pending_script, NullURL());

  script_element_ = nullptr;

  if (!IsDetached() && !requesting_script_)
    ResumeParsing();
}

bool XMLDocumentParser::IsWaitingForScripts() const {
  return pending_script_;
}

void XMLDocumentParser::PauseParsing() {
  if (!parsing_fragment_)
    parser_paused_ = true;
}

bool XMLDocumentParser::ParseDocumentFragment(
    const String& chunk,
    DocumentFragment* fragment,
    Element* context_element,
    ParserContentPolicy parser_content_policy) {
  if (!chunk.length())
    return true;

  // FIXME: We need to implement the HTML5 XML Fragment parsing algorithm:
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-xhtml-syntax.html#xml-fragment-parsing-algorithm
  // For now we have a hack for script/style innerHTML support:
  if (context_element &&
      (context_element->HasLocalName(scriptTag.LocalName()) ||
       context_element->HasLocalName(styleTag.LocalName()))) {
    fragment->ParserAppendChild(fragment->GetDocument().createTextNode(chunk));
    return true;
  }

  XMLDocumentParser* parser = XMLDocumentParser::Create(
      fragment, context_element, parser_content_policy);
  bool well_formed = parser->AppendFragmentSource(chunk);

  // Do not call finish(). Current finish() and doEnd() implementations touch
  // the main Document/loader and can cause crashes in the fragment case.

  // Allows ~DocumentParser to assert it was detached before destruction.
  parser->Detach();
  // appendFragmentSource()'s wellFormed is more permissive than wellFormed().
  return well_formed;
}

static int g_global_descriptor = 0;
static ThreadIdentifier g_libxml_loader_thread = 0;

static int MatchFunc(const char*) {
  // Only match loads initiated due to uses of libxml2 from within
  // XMLDocumentParser to avoid interfering with client applications that also
  // use libxml2. http://bugs.webkit.org/show_bug.cgi?id=17353
  return XMLDocumentParserScope::current_document_ &&
         CurrentThread() == g_libxml_loader_thread;
}

static inline void SetAttributes(Element* element,
                                 Vector<Attribute>& attribute_vector,
                                 ParserContentPolicy parser_content_policy) {
  if (!ScriptingContentIsAllowed(parser_content_policy))
    element->StripScriptingAttributes(attribute_vector);
  element->ParserSetAttributes(attribute_vector);
}

static void SwitchEncoding(xmlParserCtxtPtr ctxt, bool is8_bit) {
  // Make sure we don't call xmlSwitchEncoding in an error state.
  if ((ctxt->errNo != XML_ERR_OK) && (ctxt->disableSAX == 1))
    return;

  // Hack around libxml2's lack of encoding overide support by manually
  // resetting the encoding to UTF-16 before every chunk. Otherwise libxml
  // will detect <?xml version="1.0" encoding="<encoding name>"?> blocks and
  // switch encodings, causing the parse to fail.
  if (is8_bit) {
    xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_8859_1);
    return;
  }

  const UChar kBOM = 0xFEFF;
  const unsigned char bom_high_byte =
      *reinterpret_cast<const unsigned char*>(&kBOM);
  xmlSwitchEncoding(ctxt, bom_high_byte == 0xFF ? XML_CHAR_ENCODING_UTF16LE
                                                : XML_CHAR_ENCODING_UTF16BE);
}

static void ParseChunk(xmlParserCtxtPtr ctxt, const String& chunk) {
  bool is8_bit = chunk.Is8Bit();
  SwitchEncoding(ctxt, is8_bit);
  if (is8_bit)
    xmlParseChunk(ctxt, reinterpret_cast<const char*>(chunk.Characters8()),
                  sizeof(LChar) * chunk.length(), 0);
  else
    xmlParseChunk(ctxt, reinterpret_cast<const char*>(chunk.Characters16()),
                  sizeof(UChar) * chunk.length(), 0);
}

static void FinishParsing(xmlParserCtxtPtr ctxt) {
  xmlParseChunk(ctxt, 0, 0, 1);
}

#define xmlParseChunk \
  #error "Use parseChunk instead to select the correct encoding."

static bool IsLibxmlDefaultCatalogFile(const String& url_string) {
  // On non-Windows platforms libxml with catalogs enabled asks for
  // this URL, the "XML_XML_DEFAULT_CATALOG", on initialization.
  if (url_string == "file:///etc/xml/catalog")
    return true;

  // On Windows, libxml with catalogs enabled computes a URL relative
  // to where its DLL resides.
  if (url_string.StartsWithIgnoringASCIICase("file:///") &&
      url_string.EndsWithIgnoringASCIICase("/etc/catalog"))
    return true;
  return false;
}

static bool ShouldAllowExternalLoad(const KURL& url) {
  String url_string = url.GetString();

  // libxml should not be configured with catalogs enabled, so it
  // should not be asking to load default catalogs.
  CHECK(!IsLibxmlDefaultCatalogFile(url));

  // The most common DTD. There isn't much point in hammering www.w3c.org by
  // requesting this URL for every XHTML document.
  if (url_string.StartsWithIgnoringASCIICase("http://www.w3.org/TR/xhtml"))
    return false;

  // Similarly, there isn't much point in requesting the SVG DTD.
  if (url_string.StartsWithIgnoringASCIICase("http://www.w3.org/Graphics/SVG"))
    return false;

  // The libxml doesn't give us a lot of context for deciding whether to allow
  // this request. In the worst case, this load could be for an external
  // entity and the resulting document could simply read the retrieved
  // content. If we had more context, we could potentially allow the parser to
  // load a DTD. As things stand, we take the conservative route and allow
  // same-origin requests only.
  if (!XMLDocumentParserScope::current_document_->GetSecurityOrigin()
           ->CanRequest(url)) {
    // FIXME: This is copy/pasted. We should probably build console logging into
    // canRequest().
    if (!url.IsNull()) {
      String message =
          "Unsafe attempt to load URL " + url.ElidedString() +
          " from frame with URL " +
          XMLDocumentParserScope::current_document_->Url().ElidedString() +
          ". Domains, protocols and ports must match.\n";
      XMLDocumentParserScope::current_document_->AddConsoleMessage(
          ConsoleMessage::Create(kSecurityMessageSource, kErrorMessageLevel,
                                 message));
    }
    return false;
  }

  return true;
}

static void* OpenFunc(const char* uri) {
  DCHECK(XMLDocumentParserScope::current_document_);
  DCHECK_EQ(CurrentThread(), g_libxml_loader_thread);

  KURL url(NullURL(), uri);

  if (!ShouldAllowExternalLoad(url))
    return &g_global_descriptor;

  KURL final_url;
  RefPtr<const SharedBuffer> data;

  {
    Document* document = XMLDocumentParserScope::current_document_;
    XMLDocumentParserScope scope(0);
    // FIXME: We should restore the original global error handler as well.
    ResourceLoaderOptions options;
    options.initiator_info.name = FetchInitiatorTypeNames::xml;
    FetchParameters params(ResourceRequest(url), options);
    Resource* resource =
        RawResource::FetchSynchronously(params, document->Fetcher());
    if (resource && !resource->ErrorOccurred()) {
      data = resource->ResourceBuffer();
      final_url = resource->GetResponse().Url();
    }
  }

  // We have to check the URL again after the load to catch redirects.
  // See <https://bugs.webkit.org/show_bug.cgi?id=21963>.
  if (!ShouldAllowExternalLoad(final_url))
    return &g_global_descriptor;

  UseCounter::Count(XMLDocumentParserScope::current_document_,
                    WebFeature::kXMLExternalResourceLoad);

  return new SharedBufferReader(data);
}

static int ReadFunc(void* context, char* buffer, int len) {
  // Do 0-byte reads in case of a null descriptor
  if (context == &g_global_descriptor)
    return 0;

  SharedBufferReader* data = static_cast<SharedBufferReader*>(context);
  return data->ReadData(buffer, len);
}

static int WriteFunc(void*, const char*, int) {
  // Always just do 0-byte writes
  return 0;
}

static int CloseFunc(void* context) {
  if (context != &g_global_descriptor) {
    SharedBufferReader* data = static_cast<SharedBufferReader*>(context);
    delete data;
  }
  return 0;
}

static void ErrorFunc(void*, const char*, ...) {
  // FIXME: It would be nice to display error messages somewhere.
}

static void InitializeLibXMLIfNecessary() {
  static bool did_init = false;
  if (did_init)
    return;

  xmlInitParser();
  xmlRegisterInputCallbacks(MatchFunc, OpenFunc, ReadFunc, CloseFunc);
  xmlRegisterOutputCallbacks(MatchFunc, OpenFunc, WriteFunc, CloseFunc);
  g_libxml_loader_thread = CurrentThread();
  did_init = true;
}

PassRefPtr<XMLParserContext> XMLParserContext::CreateStringParser(
    xmlSAXHandlerPtr handlers,
    void* user_data) {
  InitializeLibXMLIfNecessary();
  xmlParserCtxtPtr parser = xmlCreatePushParserCtxt(handlers, 0, 0, 0, 0);
  xmlCtxtUseOptions(parser, XML_PARSE_HUGE);
  parser->_private = user_data;
  parser->replaceEntities = true;
  return AdoptRef(new XMLParserContext(parser));
}

// Chunk should be encoded in UTF-8
PassRefPtr<XMLParserContext> XMLParserContext::CreateMemoryParser(
    xmlSAXHandlerPtr handlers,
    void* user_data,
    const CString& chunk) {
  InitializeLibXMLIfNecessary();

  // appendFragmentSource() checks that the length doesn't overflow an int.
  xmlParserCtxtPtr parser =
      xmlCreateMemoryParserCtxt(chunk.data(), chunk.length());

  if (!parser)
    return nullptr;

  // Copy the sax handler
  memcpy(parser->sax, handlers, sizeof(xmlSAXHandler));

  // Set parser options.
  // XML_PARSE_NODICT: default dictionary option.
  // XML_PARSE_NOENT: force entities substitutions.
  // XML_PARSE_HUGE: don't impose arbitrary limits on document size.
  xmlCtxtUseOptions(parser,
                    XML_PARSE_NODICT | XML_PARSE_NOENT | XML_PARSE_HUGE);

  // Internal initialization
  parser->sax2 = 1;
  parser->instate = XML_PARSER_CONTENT;  // We are parsing a CONTENT
  parser->depth = 0;
  parser->str_xml = xmlDictLookup(parser->dict, BAD_CAST "xml", 3);
  parser->str_xmlns = xmlDictLookup(parser->dict, BAD_CAST "xmlns", 5);
  parser->str_xml_ns = xmlDictLookup(parser->dict, XML_XML_NAMESPACE, 36);
  parser->_private = user_data;

  return AdoptRef(new XMLParserContext(parser));
}

// --------------------------------

bool XMLDocumentParser::SupportsXMLVersion(const String& version) {
  return version == "1.0";
}

XMLDocumentParser::XMLDocumentParser(Document& document,
                                     LocalFrameView* frame_view)
    : ScriptableDocumentParser(document),
      has_view_(frame_view),
      context_(nullptr),
      current_node_(&document),
      is_currently_parsing8_bit_chunk_(false),
      saw_error_(false),
      saw_css_(false),
      saw_xsl_transform_(false),
      saw_first_element_(false),
      is_xhtml_document_(false),
      parser_paused_(false),
      requesting_script_(false),
      finish_called_(false),
      xml_errors_(&document),
      script_start_position_(TextPosition::BelowRangePosition()),
      parsing_fragment_(false) {
  // This is XML being used as a document resource.
  if (frame_view && document.IsXMLDocument())
    UseCounter::Count(document, WebFeature::kXMLDocument);
}

XMLDocumentParser::XMLDocumentParser(DocumentFragment* fragment,
                                     Element* parent_element,
                                     ParserContentPolicy parser_content_policy)
    : ScriptableDocumentParser(fragment->GetDocument(), parser_content_policy),
      has_view_(false),
      context_(nullptr),
      current_node_(fragment),
      is_currently_parsing8_bit_chunk_(false),
      saw_error_(false),
      saw_css_(false),
      saw_xsl_transform_(false),
      saw_first_element_(false),
      is_xhtml_document_(false),
      parser_paused_(false),
      requesting_script_(false),
      finish_called_(false),
      xml_errors_(&fragment->GetDocument()),
      script_start_position_(TextPosition::BelowRangePosition()),
      parsing_fragment_(true) {
  // Add namespaces based on the parent node
  HeapVector<Member<Element>> elem_stack;
  while (parent_element) {
    elem_stack.push_back(parent_element);

    Element* grand_parent_element = parent_element->parentElement();
    if (!grand_parent_element)
      break;
    parent_element = grand_parent_element;
  }

  if (elem_stack.IsEmpty())
    return;

  for (; !elem_stack.IsEmpty(); elem_stack.pop_back()) {
    Element* element = elem_stack.back();
    AttributeCollection attributes = element->Attributes();
    for (auto& attribute : attributes) {
      if (attribute.LocalName() == g_xmlns_atom)
        default_namespace_uri_ = attribute.Value();
      else if (attribute.Prefix() == g_xmlns_atom)
        prefix_to_namespace_map_.Set(attribute.LocalName(), attribute.Value());
    }
  }

  // If the parent element is not in document tree, there may be no xmlns
  // attribute; just default to the parent's namespace.
  if (default_namespace_uri_.IsNull() && !parent_element->isConnected())
    default_namespace_uri_ = parent_element->namespaceURI();
}

XMLParserContext::~XMLParserContext() {
  if (context_->myDoc)
    xmlFreeDoc(context_->myDoc);
  xmlFreeParserCtxt(context_);
}

XMLDocumentParser::~XMLDocumentParser() {
  DCHECK(!pending_script_);
}

DEFINE_TRACE(XMLDocumentParser) {
  visitor->Trace(current_node_);
  visitor->Trace(current_node_stack_);
  visitor->Trace(leaf_text_node_);
  visitor->Trace(xml_errors_);
  visitor->Trace(pending_script_);
  visitor->Trace(script_element_);
  ScriptableDocumentParser::Trace(visitor);
  PendingScriptClient::Trace(visitor);
}

void XMLDocumentParser::DoWrite(const String& parse_string) {
  TRACE_EVENT0("blink", "XMLDocumentParser::doWrite");
  DCHECK(!IsDetached());
  if (!context_)
    InitializeParserContext();

  // Protect the libxml context from deletion during a callback
  RefPtr<XMLParserContext> context = context_;

  // libXML throws an error if you try to switch the encoding for an empty
  // string.
  if (parse_string.length()) {
    XMLDocumentParserScope scope(GetDocument());
    AutoReset<bool> encoding_scope(&is_currently_parsing8_bit_chunk_,
                                   parse_string.Is8Bit());
    ParseChunk(context->Context(), parse_string);

    // JavaScript (which may be run under the parseChunk callstack) may
    // cause the parser to be stopped or detached.
    if (IsStopped())
      return;
  }

  // FIXME: Why is this here? And why is it after we process the passed
  // source?
  if (GetDocument()->SawDecodingError()) {
    // If the decoder saw an error, report it as fatal (stops parsing)
    TextPosition position(
        OrdinalNumber::FromOneBasedInt(context->Context()->input->line),
        OrdinalNumber::FromOneBasedInt(context->Context()->input->col));
    HandleError(XMLErrors::kErrorTypeFatal, "Encoding error", position);
  }
}

struct xmlSAX2Namespace {
  const xmlChar* prefix;
  const xmlChar* uri;
};

static inline void HandleNamespaceAttributes(
    Vector<Attribute>& prefixed_attributes,
    const xmlChar** libxml_namespaces,
    int nb_namespaces,
    ExceptionState& exception_state) {
  xmlSAX2Namespace* namespaces =
      reinterpret_cast<xmlSAX2Namespace*>(libxml_namespaces);
  for (int i = 0; i < nb_namespaces; ++i) {
    AtomicString namespace_q_name = g_xmlns_atom;
    AtomicString namespace_uri = ToAtomicString(namespaces[i].uri);
    if (namespaces[i].prefix)
      namespace_q_name =
          WTF::g_xmlns_with_colon + ToAtomicString(namespaces[i].prefix);

    QualifiedName parsed_name = g_any_name;
    if (!Element::ParseAttributeName(parsed_name, XMLNSNames::xmlnsNamespaceURI,
                                     namespace_q_name, exception_state))
      return;

    prefixed_attributes.push_back(Attribute(parsed_name, namespace_uri));
  }
}

struct xmlSAX2Attributes {
  const xmlChar* localname;
  const xmlChar* prefix;
  const xmlChar* uri;
  const xmlChar* value;
  const xmlChar* end;
};

static inline void HandleElementAttributes(
    Vector<Attribute>& prefixed_attributes,
    const xmlChar** libxml_attributes,
    int nb_attributes,
    const HashMap<AtomicString, AtomicString>& initial_prefix_to_namespace_map,
    ExceptionState& exception_state) {
  xmlSAX2Attributes* attributes =
      reinterpret_cast<xmlSAX2Attributes*>(libxml_attributes);
  for (int i = 0; i < nb_attributes; ++i) {
    int value_length =
        static_cast<int>(attributes[i].end - attributes[i].value);
    AtomicString attr_value = ToAtomicString(attributes[i].value, value_length);
    AtomicString attr_prefix = ToAtomicString(attributes[i].prefix);
    AtomicString attr_uri;
    if (!attr_prefix.IsEmpty()) {
      // If provided, use the namespace URI from libxml2 because libxml2
      // updates its namespace table as it parses whereas the
      // initialPrefixToNamespaceMap is the initial map from namespace
      // prefixes to namespace URIs created by the XMLDocumentParser
      // constructor (in the case where we are parsing an XML fragment).
      if (attributes[i].uri) {
        attr_uri = ToAtomicString(attributes[i].uri);
      } else {
        const HashMap<AtomicString, AtomicString>::const_iterator it =
            initial_prefix_to_namespace_map.find(attr_prefix);
        if (it != initial_prefix_to_namespace_map.end())
          attr_uri = it->value;
        else
          attr_uri = AtomicString();
      }
    }
    AtomicString attr_q_name =
        attr_prefix.IsEmpty()
            ? ToAtomicString(attributes[i].localname)
            : attr_prefix + ":" + ToString(attributes[i].localname);

    QualifiedName parsed_name = g_any_name;
    if (!Element::ParseAttributeName(parsed_name, attr_uri, attr_q_name,
                                     exception_state))
      return;

    prefixed_attributes.push_back(Attribute(parsed_name, attr_value));
  }
}

void XMLDocumentParser::StartElementNs(const AtomicString& local_name,
                                       const AtomicString& prefix,
                                       const AtomicString& uri,
                                       int nb_namespaces,
                                       const xmlChar** libxml_namespaces,
                                       int nb_attributes,
                                       int nb_defaulted,
                                       const xmlChar** libxml_attributes) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    script_start_position_ = GetTextPosition();
    pending_callbacks_.push_back(
        WTF::WrapUnique(new PendingStartElementNSCallback(
            local_name, prefix, uri, nb_namespaces, libxml_namespaces,
            nb_attributes, nb_defaulted, libxml_attributes)));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  AtomicString adjusted_uri = uri;
  if (parsing_fragment_ && adjusted_uri.IsNull()) {
    if (!prefix.IsNull())
      adjusted_uri = prefix_to_namespace_map_.at(prefix);
    else
      adjusted_uri = default_namespace_uri_;
  }

  bool is_first_element = !saw_first_element_;
  saw_first_element_ = true;

  QualifiedName q_name(prefix, local_name, adjusted_uri);
  Element* new_element =
      current_node_->GetDocument().createElement(q_name, kCreatedByParser);
  if (!new_element) {
    StopParsing();
    return;
  }

  Vector<Attribute> prefixed_attributes;
  DummyExceptionStateForTesting exception_state;
  HandleNamespaceAttributes(prefixed_attributes, libxml_namespaces,
                            nb_namespaces, exception_state);
  if (exception_state.HadException()) {
    SetAttributes(new_element, prefixed_attributes, GetParserContentPolicy());
    StopParsing();
    return;
  }

  HandleElementAttributes(prefixed_attributes, libxml_attributes, nb_attributes,
                          prefix_to_namespace_map_, exception_state);
  SetAttributes(new_element, prefixed_attributes, GetParserContentPolicy());
  if (exception_state.HadException()) {
    StopParsing();
    return;
  }

  new_element->BeginParsingChildren();

  if (new_element->IsScriptElement())
    script_start_position_ = GetTextPosition();

  current_node_->ParserAppendChild(new_element);

  // Event handlers may synchronously trigger removal of the
  // document and cancellation of this parser.
  if (IsStopped()) {
    StopParsing();
    return;
  }

  if (isHTMLTemplateElement(*new_element))
    PushCurrentNode(toHTMLTemplateElement(*new_element).content());
  else
    PushCurrentNode(new_element);

  // Note: |insertedByParser| will perform dispatching if this is an
  // HTMLHtmlElement.
  if (isHTMLHtmlElement(*new_element) && is_first_element) {
    toHTMLHtmlElement(*new_element).InsertedByParser();
  } else if (!parsing_fragment_ && is_first_element &&
             GetDocument()->GetFrame()) {
    GetDocument()->GetFrame()->Loader().DispatchDocumentElementAvailable();
    GetDocument()->GetFrame()->Loader().RunScriptsAtDocumentElementAvailable();
    // runScriptsAtDocumentElementAvailable might have invalidated the document.
  }
}

void XMLDocumentParser::EndElementNs() {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        WTF::MakeUnique<PendingEndElementNSCallback>(script_start_position_));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  ContainerNode* n = current_node_;
  if (current_node_->IsElementNode())
    ToElement(n)->FinishParsingChildren();

  ScriptElementBase* script_element_base =
      n->IsElementNode()
          ? ScriptElementBase::FromElementIfPossible(ToElement(n))
          : nullptr;
  if (!ScriptingContentIsAllowed(GetParserContentPolicy()) &&
      script_element_base) {
    PopCurrentNode();
    n->remove(IGNORE_EXCEPTION_FOR_TESTING);
    return;
  }

  if (!n->IsElementNode() || !has_view_) {
    PopCurrentNode();
    return;
  }

  Element* element = ToElement(n);

  // The element's parent may have already been removed from document.
  // Parsing continues in this case, but scripts aren't executed.
  if (!element->isConnected()) {
    PopCurrentNode();
    return;
  }

  ScriptLoader* script_loader =
      script_element_base ? script_element_base->Loader() : nullptr;
  if (!script_loader) {
    PopCurrentNode();
    return;
  }

  // Don't load external scripts for standalone documents (for now).
  DCHECK(!pending_script_);
  requesting_script_ = true;

  bool success = script_loader->PrepareScript(
      script_start_position_, ScriptLoader::kAllowLegacyTypeInTypeAttribute);

  if (script_loader->GetScriptType() != ScriptType::kClassic) {
    // XMLDocumentParser does not support a module script, and thus ignores it.
    success = false;
    GetDocument()->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel,
                               "Module scripts in XML documents are currently "
                               "not supported. See crbug.com/717643"));
  }

  if (success) {
    // FIXME: Script execution should be shared between
    // the libxml2 and Qt XMLDocumentParser implementations.

    if (script_loader->ReadyToBeParserExecuted()) {
      // 5th Clause, Step 23 of https://html.spec.whatwg.org/#prepare-a-script
      script_loader->ExecuteScriptBlock(
          ClassicPendingScript::Create(script_element_base,
                                       script_start_position_),
          GetDocument()->Url());
    } else if (script_loader->WillBeParserExecuted()) {
      // 1st/2nd Clauses, Step 23 of
      // https://html.spec.whatwg.org/#prepare-a-script
      pending_script_ = script_loader->CreatePendingScript();
      pending_script_->MarkParserBlockingLoadStartTime();
      script_element_ = element;
      pending_script_->WatchForLoad(this);
      // pending_script_ will be null if script was already ready.
      if (pending_script_)
        PauseParsing();
    } else {
      script_element_ = nullptr;
    }

    // JavaScript may have detached the parser
    if (IsDetached())
      return;
  }
  requesting_script_ = false;
  PopCurrentNode();
}

void XMLDocumentParser::SetScriptStartPosition(TextPosition text_position) {
  script_start_position_ = text_position;
}

void XMLDocumentParser::Characters(const xmlChar* chars, int length) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        WTF::MakeUnique<PendingCharactersCallback>(chars, length));
    return;
  }

  CreateLeafTextNodeIfNeeded();
  buffered_text_.Append(chars, length);
}

void XMLDocumentParser::GetError(XMLErrors::ErrorType type,
                                 const char* message,
                                 va_list args) {
  if (IsStopped())
    return;

  char formatted_message[1024];
  vsnprintf(formatted_message, sizeof(formatted_message) - 1, message, args);

  if (parser_paused_) {
    pending_callbacks_.push_back(WTF::WrapUnique(new PendingErrorCallback(
        type, reinterpret_cast<const xmlChar*>(formatted_message), LineNumber(),
        ColumnNumber())));
    return;
  }

  HandleError(type, formatted_message, GetTextPosition());
}

void XMLDocumentParser::GetProcessingInstruction(const String& target,
                                                 const String& data) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        WTF::MakeUnique<PendingProcessingInstructionCallback>(target, data));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  // ### handle exceptions
  DummyExceptionStateForTesting exception_state;
  ProcessingInstruction* pi =
      current_node_->GetDocument().createProcessingInstruction(target, data,
                                                               exception_state);
  if (exception_state.HadException())
    return;

  current_node_->ParserAppendChild(pi);

  if (pi->IsCSS())
    saw_css_ = true;

  if (!RuntimeEnabledFeatures::XSLTEnabled())
    return;

  saw_xsl_transform_ = !saw_first_element_ && pi->IsXSL();
  if (saw_xsl_transform_ &&
      !DocumentXSLT::HasTransformSourceDocument(*GetDocument())) {
    // This behavior is very tricky. We call stopParsing() here because we
    // want to stop processing the document until we're ready to apply the
    // transform, but we actually still want to be fed decoded string pieces
    // to accumulate in m_originalSourceForTransform. So, we call
    // stopParsing() here and check isStopped() in element callbacks.
    // FIXME: This contradicts the contract of DocumentParser.
    StopParsing();
  }
}

void XMLDocumentParser::CdataBlock(const String& text) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(
        WTF::MakeUnique<PendingCDATABlockCallback>(text));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  current_node_->ParserAppendChild(
      CDATASection::Create(current_node_->GetDocument(), text));
}

void XMLDocumentParser::Comment(const String& text) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(WTF::MakeUnique<PendingCommentCallback>(text));
    return;
  }

  if (!UpdateLeafTextNode())
    return;

  current_node_->ParserAppendChild(
      Comment::Create(current_node_->GetDocument(), text));
}

enum StandaloneInfo {
  kStandaloneUnspecified = -2,
  kNoXMlDeclaration,
  kStandaloneNo,
  kStandaloneYes
};

void XMLDocumentParser::StartDocument(const String& version,
                                      const String& encoding,
                                      int standalone) {
  StandaloneInfo standalone_info = static_cast<StandaloneInfo>(standalone);
  if (standalone_info == kNoXMlDeclaration) {
    GetDocument()->SetHasXMLDeclaration(false);
    return;
  }

  // Silently ignore XML version mismatch in the prologue.
  // https://www.w3.org/TR/xml/#sec-prolog-dtd note says:
  // "When an XML 1.0 processor encounters a document that specifies a 1.x
  // version number other than '1.0', it will process it as a 1.0 document. This
  // means that an XML 1.0 processor will accept 1.x documents provided they do
  // not use any non-1.0 features."
  if (!version.IsNull() && SupportsXMLVersion(version)) {
    GetDocument()->setXMLVersion(version, ASSERT_NO_EXCEPTION);
  }
  if (standalone != kStandaloneUnspecified)
    GetDocument()->setXMLStandalone(standalone_info == kStandaloneYes,
                                    ASSERT_NO_EXCEPTION);
  if (!encoding.IsNull())
    GetDocument()->SetXMLEncoding(encoding);
  GetDocument()->SetHasXMLDeclaration(true);
}

void XMLDocumentParser::EndDocument() {
  UpdateLeafTextNode();
}

void XMLDocumentParser::InternalSubset(const String& name,
                                       const String& external_id,
                                       const String& system_id) {
  if (IsStopped())
    return;

  if (parser_paused_) {
    pending_callbacks_.push_back(WTF::WrapUnique(
        new PendingInternalSubsetCallback(name, external_id, system_id)));
    return;
  }

  if (GetDocument())
    GetDocument()->ParserAppendChild(
        DocumentType::Create(GetDocument(), name, external_id, system_id));
}

static inline XMLDocumentParser* GetParser(void* closure) {
  xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
  return static_cast<XMLDocumentParser*>(ctxt->_private);
}

static void StartElementNsHandler(void* closure,
                                  const xmlChar* local_name,
                                  const xmlChar* prefix,
                                  const xmlChar* uri,
                                  int nb_namespaces,
                                  const xmlChar** namespaces,
                                  int nb_attributes,
                                  int nb_defaulted,
                                  const xmlChar** libxml_attributes) {
  GetParser(closure)->StartElementNs(
      ToAtomicString(local_name), ToAtomicString(prefix), ToAtomicString(uri),
      nb_namespaces, namespaces, nb_attributes, nb_defaulted,
      libxml_attributes);
}

static void EndElementNsHandler(void* closure,
                                const xmlChar*,
                                const xmlChar*,
                                const xmlChar*) {
  GetParser(closure)->EndElementNs();
}

static void CharactersHandler(void* closure, const xmlChar* chars, int length) {
  GetParser(closure)->Characters(chars, length);
}

static void ProcessingInstructionHandler(void* closure,
                                         const xmlChar* target,
                                         const xmlChar* data) {
  GetParser(closure)->GetProcessingInstruction(ToString(target),
                                               ToString(data));
}

static void CdataBlockHandler(void* closure, const xmlChar* text, int length) {
  GetParser(closure)->CdataBlock(ToString(text, length));
}

static void CommentHandler(void* closure, const xmlChar* text) {
  GetParser(closure)->Comment(ToString(text));
}

PRINTF_FORMAT(2, 3)
static void WarningHandler(void* closure, const char* message, ...) {
  va_list args;
  va_start(args, message);
  GetParser(closure)->GetError(XMLErrors::kErrorTypeWarning, message, args);
  va_end(args);
}

PRINTF_FORMAT(2, 3)
static void NormalErrorHandler(void* closure, const char* message, ...) {
  va_list args;
  va_start(args, message);
  GetParser(closure)->GetError(XMLErrors::kErrorTypeNonFatal, message, args);
  va_end(args);
}

// Using a static entity and marking it XML_INTERNAL_PREDEFINED_ENTITY is a hack
// to avoid malloc/free. Using a global variable like this could cause trouble
// if libxml implementation details were to change
static xmlChar g_shared_xhtml_entity_result[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

static xmlEntityPtr SharedXHTMLEntity() {
  static xmlEntity entity;
  if (!entity.type) {
    entity.type = XML_ENTITY_DECL;
    entity.orig = g_shared_xhtml_entity_result;
    entity.content = g_shared_xhtml_entity_result;
    entity.etype = XML_INTERNAL_PREDEFINED_ENTITY;
  }
  return &entity;
}

static size_t ConvertUTF16EntityToUTF8(const UChar* utf16_entity,
                                       size_t number_of_code_units,
                                       char* target,
                                       size_t target_size) {
  const char* original_target = target;
  WTF::Unicode::ConversionResult conversion_result =
      WTF::Unicode::ConvertUTF16ToUTF8(&utf16_entity,
                                       utf16_entity + number_of_code_units,
                                       &target, target + target_size);
  if (conversion_result != WTF::Unicode::kConversionOK)
    return 0;

  // Even though we must pass the length, libxml expects the entity string to be
  // null terminated.
  DCHECK_GT(target, original_target + 1);
  *target = '\0';
  return target - original_target;
}

static xmlEntityPtr GetXHTMLEntity(const xmlChar* name) {
  UChar utf16_decoded_entity[4];
  size_t number_of_code_units = DecodeNamedEntityToUCharArray(
      reinterpret_cast<const char*>(name), utf16_decoded_entity);
  if (!number_of_code_units)
    return 0;

  DCHECK_LE(number_of_code_units, 4u);
  size_t entity_length_in_utf8 = ConvertUTF16EntityToUTF8(
      utf16_decoded_entity, number_of_code_units,
      reinterpret_cast<char*>(g_shared_xhtml_entity_result),
      WTF_ARRAY_LENGTH(g_shared_xhtml_entity_result));
  if (!entity_length_in_utf8)
    return 0;

  xmlEntityPtr entity = SharedXHTMLEntity();
  entity->length = entity_length_in_utf8;
  entity->name = name;
  return entity;
}

static xmlEntityPtr GetEntityHandler(void* closure, const xmlChar* name) {
  xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
  xmlEntityPtr ent = xmlGetPredefinedEntity(name);
  if (ent) {
    ent->etype = XML_INTERNAL_PREDEFINED_ENTITY;
    return ent;
  }

  ent = xmlGetDocEntity(ctxt->myDoc, name);
  if (!ent && GetParser(closure)->IsXHTMLDocument()) {
    ent = GetXHTMLEntity(name);
    if (ent)
      ent->etype = XML_INTERNAL_GENERAL_ENTITY;
  }

  return ent;
}

static void StartDocumentHandler(void* closure) {
  xmlParserCtxt* ctxt = static_cast<xmlParserCtxt*>(closure);
  XMLDocumentParser* parser = GetParser(closure);
  SwitchEncoding(ctxt, parser->IsCurrentlyParsing8BitChunk());
  parser->StartDocument(ToString(ctxt->version), ToString(ctxt->encoding),
                        ctxt->standalone);
  xmlSAX2StartDocument(closure);
}

static void EndDocumentHandler(void* closure) {
  GetParser(closure)->EndDocument();
  xmlSAX2EndDocument(closure);
}

static void InternalSubsetHandler(void* closure,
                                  const xmlChar* name,
                                  const xmlChar* external_id,
                                  const xmlChar* system_id) {
  GetParser(closure)->InternalSubset(ToString(name), ToString(external_id),
                                     ToString(system_id));
  xmlSAX2InternalSubset(closure, name, external_id, system_id);
}

static void ExternalSubsetHandler(void* closure,
                                  const xmlChar*,
                                  const xmlChar* external_id,
                                  const xmlChar*) {
  String ext_id = ToString(external_id);
  if (ext_id == "-//W3C//DTD XHTML 1.0 Transitional//EN" ||
      ext_id == "-//W3C//DTD XHTML 1.1//EN" ||
      ext_id == "-//W3C//DTD XHTML 1.0 Strict//EN" ||
      ext_id == "-//W3C//DTD XHTML 1.0 Frameset//EN" ||
      ext_id == "-//W3C//DTD XHTML Basic 1.0//EN" ||
      ext_id == "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN" ||
      ext_id == "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN" ||
      ext_id == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" ||
      ext_id == "-//WAPFORUM//DTD XHTML Mobile 1.1//EN" ||
      ext_id == "-//WAPFORUM//DTD XHTML Mobile 1.2//EN") {
    // Controls if we replace entities or not.
    GetParser(closure)->SetIsXHTMLDocument(true);
  }
}

static void IgnorableWhitespaceHandler(void*, const xmlChar*, int) {
  // Nothing to do, but we need this to work around a crasher.
  // http://bugzilla.gnome.org/show_bug.cgi?id=172255
  // http://bugs.webkit.org/show_bug.cgi?id=5792
}

void XMLDocumentParser::InitializeParserContext(const CString& chunk) {
  xmlSAXHandler sax;
  memset(&sax, 0, sizeof(sax));

  // According to http://xmlsoft.org/html/libxml-tree.html#xmlSAXHandler and
  // http://xmlsoft.org/html/libxml-parser.html#fatalErrorSAXFunc the SAX
  // fatalError callback is unused; error gets all the errors. Use
  // normalErrorHandler for both the error and fatalError callbacks.
  sax.error = NormalErrorHandler;
  sax.fatalError = NormalErrorHandler;
  sax.characters = CharactersHandler;
  sax.processingInstruction = ProcessingInstructionHandler;
  sax.cdataBlock = CdataBlockHandler;
  sax.comment = CommentHandler;
  sax.warning = WarningHandler;
  sax.startElementNs = StartElementNsHandler;
  sax.endElementNs = EndElementNsHandler;
  sax.getEntity = GetEntityHandler;
  sax.startDocument = StartDocumentHandler;
  sax.endDocument = EndDocumentHandler;
  sax.internalSubset = InternalSubsetHandler;
  sax.externalSubset = ExternalSubsetHandler;
  sax.ignorableWhitespace = IgnorableWhitespaceHandler;
  sax.entityDecl = xmlSAX2EntityDecl;
  sax.initialized = XML_SAX2_MAGIC;
  saw_error_ = false;
  saw_css_ = false;
  saw_xsl_transform_ = false;
  saw_first_element_ = false;

  XMLDocumentParserScope scope(GetDocument());
  if (parsing_fragment_) {
    context_ = XMLParserContext::CreateMemoryParser(&sax, this, chunk);
  } else {
    DCHECK(!chunk.data());
    context_ = XMLParserContext::CreateStringParser(&sax, this);
  }
}

void XMLDocumentParser::DoEnd() {
  if (!IsStopped()) {
    if (context_) {
      // Tell libxml we're done.
      {
        XMLDocumentParserScope scope(GetDocument());
        FinishParsing(Context());
      }

      context_ = nullptr;
    }
  }

  bool xml_viewer_mode = !saw_error_ && !saw_css_ && !saw_xsl_transform_ &&
                         HasNoStyleInformation(GetDocument());
  if (xml_viewer_mode) {
    GetDocument()->SetIsViewSource(true);
    TransformDocumentToXMLTreeView(*GetDocument());
  } else if (saw_xsl_transform_) {
    xmlDocPtr doc = XmlDocPtrForString(
        GetDocument(), original_source_for_transform_.ToString(),
        GetDocument()->Url().GetString());
    GetDocument()->SetTransformSource(WTF::MakeUnique<TransformSource>(doc));
    DocumentParser::StopParsing();
  }
}

xmlDocPtr XmlDocPtrForString(Document* document,
                             const String& source,
                             const String& url) {
  if (source.IsEmpty())
    return 0;
  // Parse in a single chunk into an xmlDocPtr
  // FIXME: Hook up error handlers so that a failure to parse the main
  // document results in good error messages.
  XMLDocumentParserScope scope(document, ErrorFunc, 0);
  XMLParserInput input(source);
  return xmlReadMemory(input.Data(), input.size(), url.Latin1().data(),
                       input.Encoding(), XSLT_PARSE_OPTIONS);
}

OrdinalNumber XMLDocumentParser::LineNumber() const {
  return OrdinalNumber::FromOneBasedInt(Context() ? Context()->input->line : 1);
}

OrdinalNumber XMLDocumentParser::ColumnNumber() const {
  return OrdinalNumber::FromOneBasedInt(Context() ? Context()->input->col : 1);
}

TextPosition XMLDocumentParser::GetTextPosition() const {
  xmlParserCtxtPtr context = this->Context();
  if (!context)
    return TextPosition::MinimumPosition();
  return TextPosition(OrdinalNumber::FromOneBasedInt(context->input->line),
                      OrdinalNumber::FromOneBasedInt(context->input->col));
}

void XMLDocumentParser::StopParsing() {
  DocumentParser::StopParsing();
  if (Context())
    xmlStopParser(Context());
}

void XMLDocumentParser::ResumeParsing() {
  DCHECK(!IsDetached());
  DCHECK(parser_paused_);

  parser_paused_ = false;

  // First, execute any pending callbacks
  while (!pending_callbacks_.IsEmpty()) {
    std::unique_ptr<PendingCallback> callback = pending_callbacks_.TakeFirst();
    callback->Call(this);

    // A callback paused the parser
    if (parser_paused_)
      return;
  }

  // Then, write any pending data
  SegmentedString rest = pending_src_;
  pending_src_.Clear();
  // There is normally only one string left, so toString() shouldn't copy.
  // In any case, the XML parser runs on the main thread and it's OK if
  // the passed string has more than one reference.
  Append(rest.ToString().Impl());

  // Finally, if finish() has been called and write() didn't result
  // in any further callbacks being queued, call end()
  if (finish_called_ && pending_callbacks_.IsEmpty())
    end();
}

bool XMLDocumentParser::AppendFragmentSource(const String& chunk) {
  DCHECK(!context_);
  DCHECK(parsing_fragment_);

  CString chunk_as_utf8 = chunk.Utf8();

  // libxml2 takes an int for a length, and therefore can't handle XML chunks
  // larger than 2 GiB.
  if (chunk_as_utf8.length() > INT_MAX)
    return false;

  TRACE_EVENT0("blink", "XMLDocumentParser::appendFragmentSource");
  InitializeParserContext(chunk_as_utf8);
  xmlParseContent(Context());
  EndDocument();  // Close any open text nodes.

  // FIXME: If this code is actually needed, it should probably move to
  // finish()
  // XMLDocumentParserQt has a similar check (m_stream.error() ==
  // QXmlStreamReader::PrematureEndOfDocumentError) in doEnd(). Check if all
  // the chunk has been processed.
  long bytes_processed = xmlByteConsumed(Context());
  if (bytes_processed == -1 ||
      static_cast<unsigned long>(bytes_processed) != chunk_as_utf8.length()) {
    // FIXME: I don't believe we can hit this case without also having seen
    // an error or a null byte. If we hit this DCHECK, we've found a test
    // case which demonstrates the need for this code.
    DCHECK(saw_error_ ||
           (bytes_processed >= 0 && !chunk_as_utf8.data()[bytes_processed]));
    return false;
  }

  // No error if the chunk is well formed or it is not but we have no error.
  return Context()->wellFormed || !xmlCtxtGetLastError(Context());
}

// --------------------------------

struct AttributeParseState {
  HashMap<String, String> attributes;
  bool got_attributes;
};

static void AttributesStartElementNsHandler(void* closure,
                                            const xmlChar* xml_local_name,
                                            const xmlChar* /*xmlPrefix*/,
                                            const xmlChar* /*xmlURI*/,
                                            int /*nbNamespaces*/,
                                            const xmlChar** /*namespaces*/,
                                            int nb_attributes,
                                            int /*nbDefaulted*/,
                                            const xmlChar** libxml_attributes) {
  if (strcmp(reinterpret_cast<const char*>(xml_local_name), "attrs") != 0)
    return;

  xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
  AttributeParseState* state =
      static_cast<AttributeParseState*>(ctxt->_private);

  state->got_attributes = true;

  xmlSAX2Attributes* attributes =
      reinterpret_cast<xmlSAX2Attributes*>(libxml_attributes);
  for (int i = 0; i < nb_attributes; ++i) {
    String attr_local_name = ToString(attributes[i].localname);
    int value_length = (int)(attributes[i].end - attributes[i].value);
    String attr_value = ToString(attributes[i].value, value_length);
    String attr_prefix = ToString(attributes[i].prefix);
    String attr_q_name = attr_prefix.IsEmpty()
                             ? attr_local_name
                             : attr_prefix + ":" + attr_local_name;

    state->attributes.Set(attr_q_name, attr_value);
  }
}

HashMap<String, String> ParseAttributes(const String& string, bool& attrs_ok) {
  AttributeParseState state;
  state.got_attributes = false;

  xmlSAXHandler sax;
  memset(&sax, 0, sizeof(sax));
  sax.startElementNs = AttributesStartElementNsHandler;
  sax.initialized = XML_SAX2_MAGIC;
  RefPtr<XMLParserContext> parser =
      XMLParserContext::CreateStringParser(&sax, &state);
  String parse_string = "<?xml version=\"1.0\"?><attrs " + string + " />";
  ParseChunk(parser->Context(), parse_string);
  FinishParsing(parser->Context());
  attrs_ok = state.got_attributes;
  return state.attributes;
}

}  // namespace blink
