/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple, Inc. All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@webkit.org>
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

#include "core/xml/XSLTProcessor.h"

#include <libxslt/imports.h>
#include <libxslt/security.h>
#include <libxslt/variables.h>
#include <libxslt/xsltutils.h>
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/TransformSource.h"
#include "core/editing/serializers/Serialization.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/xml/XSLStyleSheet.h"
#include "core/xml/XSLTExtensions.h"
#include "core/xml/XSLTUnicodeSort.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/allocator/Partitions.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringBuffer.h"
#include "platform/wtf/text/UTF8.h"

namespace blink {

void XSLTProcessor::GenericErrorFunc(void*, const char*, ...) {
  // It would be nice to do something with this error message.
}

void XSLTProcessor::ParseErrorFunc(void* user_data, xmlError* error) {
  FrameConsole* console = static_cast<FrameConsole*>(user_data);
  if (!console)
    return;

  MessageLevel level;
  switch (error->level) {
    case XML_ERR_NONE:
      level = kVerboseMessageLevel;
      break;
    case XML_ERR_WARNING:
      level = kWarningMessageLevel;
      break;
    case XML_ERR_ERROR:
    case XML_ERR_FATAL:
    default:
      level = kErrorMessageLevel;
      break;
  }

  console->AddMessage(ConsoleMessage::Create(
      kXMLMessageSource, level, error->message,
      SourceLocation::Create(error->file, error->line, 0, nullptr)));
}

// FIXME: There seems to be no way to control the ctxt pointer for loading here,
// thus we have globals.
static XSLTProcessor* g_global_processor = nullptr;
static ResourceFetcher* g_global_resource_fetcher = nullptr;

static xmlDocPtr DocLoaderFunc(const xmlChar* uri,
                               xmlDictPtr,
                               int options,
                               void* ctxt,
                               xsltLoadType type) {
  if (!g_global_processor)
    return nullptr;

  switch (type) {
    case XSLT_LOAD_DOCUMENT: {
      xsltTransformContextPtr context = (xsltTransformContextPtr)ctxt;
      xmlChar* base = xmlNodeGetBase(context->document->doc, context->node);
      KURL url(KURL(kParsedURLString, reinterpret_cast<const char*>(base)),
               reinterpret_cast<const char*>(uri));
      xmlFree(base);

      ResourceLoaderOptions fetch_options;
      fetch_options.initiator_info.name = FetchInitiatorTypeNames::xml;
      FetchParameters params(ResourceRequest(url), fetch_options);
      params.SetOriginRestriction(FetchParameters::kRestrictToSameOrigin);
      Resource* resource =
          RawResource::FetchSynchronously(params, g_global_resource_fetcher);
      if (!resource || !g_global_processor)
        return nullptr;

      FrameConsole* console = nullptr;
      LocalFrame* frame =
          g_global_processor->XslStylesheet()->OwnerDocument()->GetFrame();
      if (frame)
        console = &frame->Console();
      xmlSetStructuredErrorFunc(console, XSLTProcessor::ParseErrorFunc);
      xmlSetGenericErrorFunc(console, XSLTProcessor::GenericErrorFunc);

      // We don't specify an encoding here. Neither Gecko nor WinIE respects
      // the encoding specified in the HTTP headers.
      RefPtr<const SharedBuffer> data = resource->ResourceBuffer();
      xmlDocPtr doc = data ? xmlReadMemory(data->Data(), data->size(),
                                           (const char*)uri, 0, options)
                           : nullptr;

      xmlSetStructuredErrorFunc(0, 0);
      xmlSetGenericErrorFunc(0, 0);

      return doc;
    }
    case XSLT_LOAD_STYLESHEET:
      return g_global_processor->XslStylesheet()->LocateStylesheetSubResource(
          ((xsltStylesheetPtr)ctxt)->doc, uri);
    default:
      break;
  }

  return nullptr;
}

static inline void SetXSLTLoadCallBack(xsltDocLoaderFunc func,
                                       XSLTProcessor* processor,
                                       ResourceFetcher* fetcher) {
  xsltSetLoaderFunc(func);
  g_global_processor = processor;
  g_global_resource_fetcher = fetcher;
}

static int WriteToStringBuilder(void* context, const char* buffer, int len) {
  StringBuilder& result_output = *static_cast<StringBuilder*>(context);

  if (!len)
    return 0;

  StringBuffer<UChar> string_buffer(len);
  UChar* buffer_u_char = string_buffer.Characters();
  UChar* buffer_u_char_end = buffer_u_char + len;

  const char* string_current = buffer;
  WTF::Unicode::ConversionResult result = WTF::Unicode::ConvertUTF8ToUTF16(
      &string_current, buffer + len, &buffer_u_char, buffer_u_char_end);
  if (result != WTF::Unicode::kConversionOK &&
      result != WTF::Unicode::kSourceExhausted) {
    NOTREACHED();
    return -1;
  }

  int utf16_length = buffer_u_char - string_buffer.Characters();
  result_output.Append(string_buffer.Characters(), utf16_length);
  return string_current - buffer;
}

static bool SaveResultToString(xmlDocPtr result_doc,
                               xsltStylesheetPtr sheet,
                               String& result_string) {
  xmlOutputBufferPtr output_buf = xmlAllocOutputBuffer(0);
  if (!output_buf)
    return false;

  StringBuilder result_builder;
  output_buf->context = &result_builder;
  output_buf->writecallback = WriteToStringBuilder;

  int retval = xsltSaveResultTo(output_buf, result_doc, sheet);
  xmlOutputBufferClose(output_buf);
  if (retval < 0)
    return false;

  // Workaround for <http://bugzilla.gnome.org/show_bug.cgi?id=495668>:
  // libxslt appends an extra line feed to the result.
  if (result_builder.length() > 0 &&
      result_builder[result_builder.length() - 1] == '\n')
    result_builder.Resize(result_builder.length() - 1);

  result_string = result_builder.ToString();

  return true;
}

static char* AllocateParameterArray(const char* data) {
  size_t length = strlen(data) + 1;
  char* parameter_array = static_cast<char*>(WTF::Partitions::FastMalloc(
      length, WTF_HEAP_PROFILER_TYPE_NAME(XSLTProcessor)));
  memcpy(parameter_array, data, length);
  return parameter_array;
}

static const char** XsltParamArrayFromParameterMap(
    XSLTProcessor::ParameterMap& parameters) {
  if (parameters.IsEmpty())
    return nullptr;

  const char** parameter_array = static_cast<const char**>(
      WTF::Partitions::FastMalloc(((parameters.size() * 2) + 1) * sizeof(char*),
                                  WTF_HEAP_PROFILER_TYPE_NAME(XSLTProcessor)));

  unsigned index = 0;
  for (auto& parameter : parameters) {
    parameter_array[index++] =
        AllocateParameterArray(parameter.key.Utf8().data());
    parameter_array[index++] =
        AllocateParameterArray(parameter.value.Utf8().data());
  }
  parameter_array[index] = 0;

  return parameter_array;
}

static void FreeXsltParamArray(const char** params) {
  const char** temp = params;
  if (!params)
    return;

  while (*temp) {
    WTF::Partitions::FastFree(const_cast<char*>(*(temp++)));
    WTF::Partitions::FastFree(const_cast<char*>(*(temp++)));
  }
  WTF::Partitions::FastFree(params);
}

static xsltStylesheetPtr XsltStylesheetPointer(
    Document* document,
    Member<XSLStyleSheet>& cached_stylesheet,
    Node* stylesheet_root_node) {
  if (!cached_stylesheet && stylesheet_root_node) {
    // When using importStylesheet, we will use the given document as the
    // imported stylesheet's owner.
    cached_stylesheet = XSLStyleSheet::CreateForXSLTProcessor(
        stylesheet_root_node->parentNode()
            ? &stylesheet_root_node->parentNode()->GetDocument()
            : document,
        stylesheet_root_node,
        stylesheet_root_node->GetDocument().Url().GetString(),
        stylesheet_root_node->GetDocument()
            .Url());  // FIXME: Should we use baseURL here?

    // According to Mozilla documentation, the node must be a Document node,
    // an xsl:stylesheet or xsl:transform element. But we just use text
    // content regardless of node type.
    cached_stylesheet->ParseString(CreateMarkup(stylesheet_root_node));
  }

  if (!cached_stylesheet || !cached_stylesheet->GetDocument())
    return nullptr;

  return cached_stylesheet->CompileStyleSheet();
}

static inline xmlDocPtr XmlDocPtrFromNode(Node* source_node,
                                          bool& should_delete) {
  Document* owner_document = &source_node->GetDocument();
  bool source_is_document = (source_node == owner_document);

  xmlDocPtr source_doc = nullptr;
  if (source_is_document && owner_document->GetTransformSource())
    source_doc =
        (xmlDocPtr)owner_document->GetTransformSource()->PlatformSource();
  if (!source_doc) {
    source_doc = (xmlDocPtr)XmlDocPtrForString(
        owner_document, CreateMarkup(source_node),
        source_is_document ? owner_document->Url().GetString() : String());
    should_delete = source_doc;
  }
  return source_doc;
}

static inline String ResultMIMEType(xmlDocPtr result_doc,
                                    xsltStylesheetPtr sheet) {
  // There are three types of output we need to be able to deal with:
  // HTML (create an HTML document), XML (create an XML document),
  // and text (wrap in a <pre> and create an XML document).

  const xmlChar* result_type = nullptr;
  XSLT_GET_IMPORT_PTR(result_type, sheet, method);
  if (!result_type && result_doc->type == XML_HTML_DOCUMENT_NODE)
    result_type = (const xmlChar*)"html";

  if (xmlStrEqual(result_type, (const xmlChar*)"html"))
    return "text/html";
  if (xmlStrEqual(result_type, (const xmlChar*)"text"))
    return "text/plain";

  return "application/xml";
}

bool XSLTProcessor::TransformToString(Node* source_node,
                                      String& mime_type,
                                      String& result_string,
                                      String& result_encoding) {
  Document* owner_document = &source_node->GetDocument();

  SetXSLTLoadCallBack(DocLoaderFunc, this, owner_document->Fetcher());
  xsltStylesheetPtr sheet = XsltStylesheetPointer(document_.Get(), stylesheet_,
                                                  stylesheet_root_node_.Get());
  if (!sheet) {
    SetXSLTLoadCallBack(0, 0, 0);
    stylesheet_ = nullptr;
    return false;
  }
  stylesheet_->ClearDocuments();

  xmlChar* orig_method = sheet->method;
  if (!orig_method && mime_type == "text/html")
    sheet->method = (xmlChar*)"html";

  bool success = false;
  bool should_free_source_doc = false;
  if (xmlDocPtr source_doc =
          XmlDocPtrFromNode(source_node, should_free_source_doc)) {
    // The XML declaration would prevent parsing the result as a fragment,
    // and it's not needed even for documents, as the result of this
    // function is always immediately parsed.
    sheet->omitXmlDeclaration = true;

    xsltTransformContextPtr transform_context =
        xsltNewTransformContext(sheet, source_doc);
    RegisterXSLTExtensions(transform_context);

    xsltSecurityPrefsPtr security_prefs = xsltNewSecurityPrefs();
    // Read permissions are checked by docLoaderFunc.
    CHECK_EQ(0, xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_WRITE_FILE,
                                     xsltSecurityForbid));
    CHECK_EQ(0,
             xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_CREATE_DIRECTORY,
                                  xsltSecurityForbid));
    CHECK_EQ(0, xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_WRITE_NETWORK,
                                     xsltSecurityForbid));
    CHECK_EQ(0, xsltSetCtxtSecurityPrefs(security_prefs, transform_context));

    // <http://bugs.webkit.org/show_bug.cgi?id=16077>: XSLT processor
    // <xsl:sort> algorithm only compares by code point.
    xsltSetCtxtSortFunc(transform_context, XsltUnicodeSortFunction);

    // This is a workaround for a bug in libxslt.
    // The bug has been fixed in version 1.1.13, so once we ship that this
    // can be removed.
    if (!transform_context->globalVars)
      transform_context->globalVars = xmlHashCreate(20);

    const char** params = XsltParamArrayFromParameterMap(parameters_);
    xsltQuoteUserParams(transform_context, params);
    xmlDocPtr result_doc =
        xsltApplyStylesheetUser(sheet, source_doc, 0, 0, 0, transform_context);

    xsltFreeTransformContext(transform_context);
    xsltFreeSecurityPrefs(security_prefs);
    FreeXsltParamArray(params);

    if (should_free_source_doc)
      xmlFreeDoc(source_doc);

    success = SaveResultToString(result_doc, sheet, result_string);
    if (success) {
      mime_type = ResultMIMEType(result_doc, sheet);
      result_encoding = (char*)result_doc->encoding;
    }
    xmlFreeDoc(result_doc);
  }

  sheet->method = orig_method;
  SetXSLTLoadCallBack(0, 0, 0);
  xsltFreeStylesheet(sheet);
  stylesheet_ = nullptr;

  return success;
}

}  // namespace blink
