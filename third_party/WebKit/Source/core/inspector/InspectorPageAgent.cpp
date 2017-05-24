/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorPageAgent.h"

#include <memory>
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "core/HTMLNames.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/VoidCallback.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/DOMPatchSupport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/V8InspectorString.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/ScriptResource.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/PlatformResourceLoader.h"
#include "platform/UserGestureIndicator.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

using protocol::Response;

namespace PageAgentState {
static const char kPageAgentEnabled[] = "pageAgentEnabled";
static const char kPageAgentScriptsToEvaluateOnLoad[] =
    "pageAgentScriptsToEvaluateOnLoad";
static const char kScreencastEnabled[] = "screencastEnabled";
static const char kAutoAttachToCreatedPages[] = "autoAttachToCreatedPages";
}

namespace {

KURL UrlWithoutFragment(const KURL& url) {
  KURL result = url;
  result.RemoveFragmentIdentifier();
  return result;
}

String FrameId(LocalFrame* frame) {
  return frame ? IdentifiersFactory::FrameId(frame) : "";
}

String DialogTypeToProtocol(ChromeClient::DialogType dialog_type) {
  switch (dialog_type) {
    case ChromeClient::kAlertDialog:
      return protocol::Page::DialogTypeEnum::Alert;
    case ChromeClient::kConfirmDialog:
      return protocol::Page::DialogTypeEnum::Confirm;
    case ChromeClient::kPromptDialog:
      return protocol::Page::DialogTypeEnum::Prompt;
    case ChromeClient::kHTMLDialog:
      return protocol::Page::DialogTypeEnum::Beforeunload;
    case ChromeClient::kPrintDialog:
      NOTREACHED();
  }
  return protocol::Page::DialogTypeEnum::Alert;
}

}  // namespace

static bool PrepareResourceBuffer(Resource* cached_resource,
                                  bool* has_zero_size) {
  if (!cached_resource)
    return false;

  if (cached_resource->GetDataBufferingPolicy() == kDoNotBufferData)
    return false;

  // Zero-sized resources don't have data at all -- so fake the empty buffer,
  // instead of indicating error by returning 0.
  if (!cached_resource->EncodedSize()) {
    *has_zero_size = true;
    return true;
  }

  *has_zero_size = false;
  return true;
}

static bool HasTextContent(Resource* cached_resource) {
  Resource::Type type = cached_resource->GetType();
  return type == Resource::kCSSStyleSheet || type == Resource::kXSLStyleSheet ||
         type == Resource::kScript || type == Resource::kRaw ||
         type == Resource::kImportResource || type == Resource::kMainResource;
}

static std::unique_ptr<TextResourceDecoder> CreateResourceTextDecoder(
    const String& mime_type,
    const String& text_encoding_name) {
  if (!text_encoding_name.IsEmpty())
    return TextResourceDecoder::Create("text/plain", text_encoding_name);
  if (DOMImplementation::IsXMLMIMEType(mime_type)) {
    std::unique_ptr<TextResourceDecoder> decoder =
        TextResourceDecoder::Create("application/xml");
    decoder->UseLenientXMLDecoding();
    return decoder;
  }
  if (DeprecatedEqualIgnoringCase(mime_type, "text/html"))
    return TextResourceDecoder::Create("text/html", "UTF-8");
  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
      DOMImplementation::IsJSONMIMEType(mime_type))
    return TextResourceDecoder::Create("text/plain", "UTF-8");
  if (DOMImplementation::IsTextMIMEType(mime_type))
    return TextResourceDecoder::Create("text/plain", "ISO-8859-1");
  return std::unique_ptr<TextResourceDecoder>();
}

static void MaybeEncodeTextContent(const String& text_content,
                                   PassRefPtr<const SharedBuffer> buffer,
                                   String* result,
                                   bool* base64_encoded) {
  if (!text_content.IsNull() &&
      !text_content.Utf8(WTF::kStrictUTF8Conversion).IsNull()) {
    *result = text_content;
    *base64_encoded = false;
  } else if (buffer) {
    *result = Base64Encode(buffer->Data(), buffer->size());
    *base64_encoded = true;
  } else if (text_content.IsNull()) {
    *result = "";
    *base64_encoded = false;
  } else {
    DCHECK(!text_content.Is8Bit());
    *result = Base64Encode(text_content.Utf8(WTF::kLenientUTF8Conversion));
    *base64_encoded = true;
  }
}

// static
bool InspectorPageAgent::SharedBufferContent(
    PassRefPtr<const SharedBuffer> buffer,
    const String& mime_type,
    const String& text_encoding_name,
    String* result,
    bool* base64_encoded) {
  if (!buffer)
    return false;

  String text_content;
  std::unique_ptr<TextResourceDecoder> decoder =
      CreateResourceTextDecoder(mime_type, text_encoding_name);
  WTF::TextEncoding encoding(text_encoding_name);

  if (decoder) {
    text_content = decoder->Decode(buffer->Data(), buffer->size());
    text_content = text_content + decoder->Flush();
  } else if (encoding.IsValid()) {
    text_content = encoding.Decode(buffer->Data(), buffer->size());
  }

  MaybeEncodeTextContent(text_content, std::move(buffer), result,
                         base64_encoded);
  return true;
}

// static
bool InspectorPageAgent::CachedResourceContent(Resource* cached_resource,
                                               String* result,
                                               bool* base64_encoded) {
  bool has_zero_size;
  if (!PrepareResourceBuffer(cached_resource, &has_zero_size))
    return false;

  if (!HasTextContent(cached_resource)) {
    RefPtr<const SharedBuffer> buffer = has_zero_size
                                            ? SharedBuffer::Create()
                                            : cached_resource->ResourceBuffer();
    if (!buffer)
      return false;
    *result = Base64Encode(buffer->Data(), buffer->size());
    *base64_encoded = true;
    return true;
  }

  if (has_zero_size) {
    *result = "";
    *base64_encoded = false;
    return true;
  }

  DCHECK(cached_resource);
  switch (cached_resource->GetType()) {
    case Resource::kCSSStyleSheet:
      MaybeEncodeTextContent(
          ToCSSStyleSheetResource(cached_resource)
              ->SheetText(CSSStyleSheetResource::MIMETypeCheck::kLax),
          cached_resource->ResourceBuffer(), result, base64_encoded);
      return true;
    case Resource::kScript:
      MaybeEncodeTextContent(
          cached_resource->ResourceBuffer()
              ? ToScriptResource(cached_resource)->DecodedText()
              : ToScriptResource(cached_resource)->SourceText(),
          cached_resource->ResourceBuffer(), result, base64_encoded);
      return true;
    default:
      String text_encoding_name =
          cached_resource->GetResponse().TextEncodingName();
      if (text_encoding_name.IsEmpty() &&
          cached_resource->GetType() != Resource::kRaw)
        text_encoding_name = "WinLatin1";
      return InspectorPageAgent::SharedBufferContent(
          cached_resource->ResourceBuffer(),
          cached_resource->GetResponse().MimeType(), text_encoding_name, result,
          base64_encoded);
  }
}

InspectorPageAgent* InspectorPageAgent::Create(
    InspectedFrames* inspected_frames,
    Client* client,
    InspectorResourceContentLoader* resource_content_loader,
    v8_inspector::V8InspectorSession* v8_session) {
  return new InspectorPageAgent(inspected_frames, client,
                                resource_content_loader, v8_session);
}

Resource* InspectorPageAgent::CachedResource(LocalFrame* frame,
                                             const KURL& url) {
  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;
  Resource* cached_resource = document->Fetcher()->CachedResource(url);
  if (!cached_resource) {
    HeapVector<Member<Document>> all_imports =
        InspectorPageAgent::ImportsForFrame(frame);
    for (Document* import : all_imports) {
      cached_resource = import->Fetcher()->CachedResource(url);
      if (cached_resource)
        break;
    }
  }
  if (!cached_resource)
    cached_resource = GetMemoryCache()->ResourceForURL(
        url, document->Fetcher()->GetCacheIdentifier());
  return cached_resource;
}

String InspectorPageAgent::ResourceTypeJson(
    InspectorPageAgent::ResourceType resource_type) {
  switch (resource_type) {
    case kDocumentResource:
      return protocol::Page::ResourceTypeEnum::Document;
    case kFontResource:
      return protocol::Page::ResourceTypeEnum::Font;
    case kImageResource:
      return protocol::Page::ResourceTypeEnum::Image;
    case kMediaResource:
      return protocol::Page::ResourceTypeEnum::Media;
    case kScriptResource:
      return protocol::Page::ResourceTypeEnum::Script;
    case kStylesheetResource:
      return protocol::Page::ResourceTypeEnum::Stylesheet;
    case kTextTrackResource:
      return protocol::Page::ResourceTypeEnum::TextTrack;
    case kXHRResource:
      return protocol::Page::ResourceTypeEnum::XHR;
    case kFetchResource:
      return protocol::Page::ResourceTypeEnum::Fetch;
    case kEventSourceResource:
      return protocol::Page::ResourceTypeEnum::EventSource;
    case kWebSocketResource:
      return protocol::Page::ResourceTypeEnum::WebSocket;
    case kManifestResource:
      return protocol::Page::ResourceTypeEnum::Manifest;
    case kOtherResource:
      return protocol::Page::ResourceTypeEnum::Other;
  }
  return protocol::Page::ResourceTypeEnum::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::CachedResourceType(
    const Resource& cached_resource) {
  switch (cached_resource.GetType()) {
    case Resource::kImage:
      return InspectorPageAgent::kImageResource;
    case Resource::kFont:
      return InspectorPageAgent::kFontResource;
    case Resource::kMedia:
      return InspectorPageAgent::kMediaResource;
    case Resource::kManifest:
      return InspectorPageAgent::kManifestResource;
    case Resource::kTextTrack:
      return InspectorPageAgent::kTextTrackResource;
    case Resource::kCSSStyleSheet:
    // Fall through.
    case Resource::kXSLStyleSheet:
      return InspectorPageAgent::kStylesheetResource;
    case Resource::kScript:
      return InspectorPageAgent::kScriptResource;
    case Resource::kImportResource:
    // Fall through.
    case Resource::kMainResource:
      return InspectorPageAgent::kDocumentResource;
    default:
      break;
  }
  return InspectorPageAgent::kOtherResource;
}

String InspectorPageAgent::CachedResourceTypeJson(
    const Resource& cached_resource) {
  return ResourceTypeJson(CachedResourceType(cached_resource));
}

InspectorPageAgent::InspectorPageAgent(
    InspectedFrames* inspected_frames,
    Client* client,
    InspectorResourceContentLoader* resource_content_loader,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      v8_session_(v8_session),
      client_(client),
      last_script_identifier_(0),
      enabled_(false),
      reloading_(false),
      inspector_resource_content_loader_(resource_content_loader),
      resource_content_loader_client_id_(
          resource_content_loader->CreateClientId()) {}

void InspectorPageAgent::Restore() {
  if (state_->booleanProperty(PageAgentState::kPageAgentEnabled, false))
    enable();
}

Response InspectorPageAgent::enable() {
  enabled_ = true;
  state_->setBoolean(PageAgentState::kPageAgentEnabled, true);
  instrumenting_agents_->addInspectorPageAgent(this);

  // Tell the browser the ids for all existing frames.
  for (LocalFrame* frame : *inspected_frames_) {
    frame->Client()->SetDevToolsFrameId(FrameId(frame));
  }
  return Response::OK();
}

Response InspectorPageAgent::disable() {
  enabled_ = false;
  state_->setBoolean(PageAgentState::kPageAgentEnabled, false);
  state_->remove(PageAgentState::kPageAgentScriptsToEvaluateOnLoad);
  script_to_evaluate_on_load_once_ = String();
  pending_script_to_evaluate_on_load_once_ = String();
  instrumenting_agents_->removeInspectorPageAgent(this);
  inspector_resource_content_loader_->Cancel(
      resource_content_loader_client_id_);

  stopScreencast();

  FinishReload();
  return Response::OK();
}

Response InspectorPageAgent::addScriptToEvaluateOnLoad(const String& source,
                                                       String* identifier) {
  protocol::DictionaryValue* scripts =
      state_->getObject(PageAgentState::kPageAgentScriptsToEvaluateOnLoad);
  if (!scripts) {
    std::unique_ptr<protocol::DictionaryValue> new_scripts =
        protocol::DictionaryValue::create();
    scripts = new_scripts.get();
    state_->setObject(PageAgentState::kPageAgentScriptsToEvaluateOnLoad,
                      std::move(new_scripts));
  }
  // Assure we don't override existing ids -- m_lastScriptIdentifier could get
  // out of sync WRT actual scripts once we restored the scripts from the cookie
  // during navigation.
  do {
    *identifier = String::Number(++last_script_identifier_);
  } while (scripts->get(*identifier));
  scripts->setString(*identifier, source);
  return Response::OK();
}

Response InspectorPageAgent::removeScriptToEvaluateOnLoad(
    const String& identifier) {
  protocol::DictionaryValue* scripts =
      state_->getObject(PageAgentState::kPageAgentScriptsToEvaluateOnLoad);
  if (!scripts || !scripts->get(identifier))
    return Response::Error("Script not found");
  scripts->remove(identifier);
  return Response::OK();
}

Response InspectorPageAgent::setAutoAttachToCreatedPages(bool auto_attach) {
  state_->setBoolean(PageAgentState::kAutoAttachToCreatedPages, auto_attach);
  return Response::OK();
}

Response InspectorPageAgent::reload(
    Maybe<bool> optional_bypass_cache,
    Maybe<String> optional_script_to_evaluate_on_load) {
  pending_script_to_evaluate_on_load_once_ =
      optional_script_to_evaluate_on_load.fromMaybe("");
  v8_session_->setSkipAllPauses(true);
  reloading_ = true;
  inspected_frames_->Root()->Reload(optional_bypass_cache.fromMaybe(false)
                                        ? kFrameLoadTypeReloadBypassingCache
                                        : kFrameLoadTypeReload,
                                    ClientRedirectPolicy::kNotClientRedirect);
  return Response::OK();
}

Response InspectorPageAgent::navigate(const String& url,
                                      Maybe<String> referrer,
                                      Maybe<String> transitionType,
                                      String* out_frame_id) {
  *out_frame_id = FrameId(inspected_frames_->Root());
  return Response::OK();
}

Response InspectorPageAgent::stopLoading() {
  return Response::OK();
}

static void CachedResourcesForDocument(Document* document,
                                       HeapVector<Member<Resource>>& result,
                                       bool skip_xhrs) {
  const ResourceFetcher::DocumentResourceMap& all_resources =
      document->Fetcher()->AllResources();
  for (const auto& resource : all_resources) {
    Resource* cached_resource = resource.value.Get();
    if (!cached_resource)
      continue;

    // Skip images that were not auto loaded (images disabled in the user
    // agent), fonts that were referenced in CSS but never used/downloaded, etc.
    if (cached_resource->StillNeedsLoad())
      continue;
    if (cached_resource->GetType() == Resource::kRaw && skip_xhrs)
      continue;
    result.push_back(cached_resource);
  }
}

// static
HeapVector<Member<Document>> InspectorPageAgent::ImportsForFrame(
    LocalFrame* frame) {
  HeapVector<Member<Document>> result;
  Document* root_document = frame->GetDocument();

  if (HTMLImportsController* controller = root_document->ImportsController()) {
    for (size_t i = 0; i < controller->LoaderCount(); ++i) {
      if (Document* document = controller->LoaderAt(i)->GetDocument())
        result.push_back(document);
    }
  }

  return result;
}

static HeapVector<Member<Resource>> CachedResourcesForFrame(LocalFrame* frame,
                                                            bool skip_xhrs) {
  HeapVector<Member<Resource>> result;
  Document* root_document = frame->GetDocument();
  HeapVector<Member<Document>> loaders =
      InspectorPageAgent::ImportsForFrame(frame);

  CachedResourcesForDocument(root_document, result, skip_xhrs);
  for (size_t i = 0; i < loaders.size(); ++i)
    CachedResourcesForDocument(loaders[i], result, skip_xhrs);

  return result;
}

Response InspectorPageAgent::getResourceTree(
    std::unique_ptr<protocol::Page::FrameResourceTree>* object) {
  *object = BuildObjectForFrameTree(inspected_frames_->Root());
  return Response::OK();
}

void InspectorPageAgent::FinishReload() {
  if (!reloading_)
    return;
  reloading_ = false;
  v8_session_->setSkipAllPauses(false);
}

void InspectorPageAgent::GetResourceContentAfterResourcesContentLoaded(
    const String& frame_id,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame) {
    callback->sendFailure(Response::Error("No frame for given id found"));
    return;
  }
  String content;
  bool base64_encoded;
  if (InspectorPageAgent::CachedResourceContent(
          InspectorPageAgent::CachedResource(frame,
                                             KURL(kParsedURLString, url)),
          &content, &base64_encoded))
    callback->sendSuccess(content, base64_encoded);
  else
    callback->sendFailure(Response::Error("No resource with given URL found"));
}

void InspectorPageAgent::getResourceContent(
    const String& frame_id,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  if (!enabled_) {
    callback->sendFailure(Response::Error("Agent is not enabled."));
    return;
  }
  inspector_resource_content_loader_->EnsureResourcesContentLoaded(
      resource_content_loader_client_id_,
      WTF::Bind(
          &InspectorPageAgent::GetResourceContentAfterResourcesContentLoaded,
          WrapPersistent(this), frame_id, url,
          WTF::Passed(std::move(callback))));
}

void InspectorPageAgent::SearchContentAfterResourcesContentLoaded(
    const String& frame_id,
    const String& url,
    const String& query,
    bool case_sensitive,
    bool is_regex,
    std::unique_ptr<SearchInResourceCallback> callback) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame) {
    callback->sendFailure(Response::Error("No frame for given id found"));
    return;
  }
  String content;
  bool base64_encoded;
  if (!InspectorPageAgent::CachedResourceContent(
          InspectorPageAgent::CachedResource(frame,
                                             KURL(kParsedURLString, url)),
          &content, &base64_encoded)) {
    callback->sendFailure(Response::Error("No resource with given URL found"));
    return;
  }

  auto matches = v8_session_->searchInTextByLines(
      ToV8InspectorStringView(content), ToV8InspectorStringView(query),
      case_sensitive, is_regex);
  auto results = protocol::Array<
      v8_inspector::protocol::Debugger::API::SearchMatch>::create();
  for (size_t i = 0; i < matches.size(); ++i)
    results->addItem(std::move(matches[i]));
  callback->sendSuccess(std::move(results));
}

void InspectorPageAgent::searchInResource(
    const String& frame_id,
    const String& url,
    const String& query,
    Maybe<bool> optional_case_sensitive,
    Maybe<bool> optional_is_regex,
    std::unique_ptr<SearchInResourceCallback> callback) {
  if (!enabled_) {
    callback->sendFailure(Response::Error("Agent is not enabled."));
    return;
  }
  inspector_resource_content_loader_->EnsureResourcesContentLoaded(
      resource_content_loader_client_id_,
      WTF::Bind(&InspectorPageAgent::SearchContentAfterResourcesContentLoaded,
                WrapPersistent(this), frame_id, url, query,
                optional_case_sensitive.fromMaybe(false),
                optional_is_regex.fromMaybe(false),
                WTF::Passed(std::move(callback))));
}

Response InspectorPageAgent::setDocumentContent(const String& frame_id,
                                                const String& html) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame)
    return Response::Error("No frame for given id found");

  Document* document = frame->GetDocument();
  if (!document)
    return Response::Error("No Document instance to set HTML for");
  DOMPatchSupport::PatchDocument(*document, html);
  return Response::OK();
}

void InspectorPageAgent::DidClearDocumentOfWindowObject(LocalFrame* frame) {
  if (!GetFrontend())
    return;

  protocol::DictionaryValue* scripts =
      state_->getObject(PageAgentState::kPageAgentScriptsToEvaluateOnLoad);
  if (scripts) {
    for (size_t i = 0; i < scripts->size(); ++i) {
      auto script = scripts->at(i);
      String script_text;
      if (script.second->asString(&script_text))
        frame->GetScriptController().ExecuteScriptInMainWorld(script_text);
    }
  }
  if (!script_to_evaluate_on_load_once_.IsEmpty()) {
    frame->GetScriptController().ExecuteScriptInMainWorld(
        script_to_evaluate_on_load_once_);
  }
}

void InspectorPageAgent::DomContentLoadedEventFired(LocalFrame* frame) {
  if (frame != inspected_frames_->Root())
    return;
  GetFrontend()->domContentEventFired(MonotonicallyIncreasingTime());
}

void InspectorPageAgent::LoadEventFired(LocalFrame* frame) {
  if (frame != inspected_frames_->Root())
    return;
  GetFrontend()->loadEventFired(MonotonicallyIncreasingTime());
}

void InspectorPageAgent::DidCommitLoad(LocalFrame*, DocumentLoader* loader) {
  if (loader->GetFrame() == inspected_frames_->Root()) {
    FinishReload();
    script_to_evaluate_on_load_once_ = pending_script_to_evaluate_on_load_once_;
    pending_script_to_evaluate_on_load_once_ = String();
  }
  GetFrontend()->frameNavigated(BuildObjectForFrame(loader->GetFrame()));
}

void InspectorPageAgent::FrameAttachedToParent(LocalFrame* frame) {
  Frame* parent_frame = frame->Tree().Parent();
  if (!parent_frame->IsLocalFrame())
    parent_frame = 0;
  std::unique_ptr<SourceLocation> location =
      SourceLocation::CaptureWithFullStackTrace();
  String frame_id = FrameId(frame);
  frame->Client()->SetDevToolsFrameId(frame_id);
  GetFrontend()->frameAttached(
      frame_id, FrameId(ToLocalFrame(parent_frame)),
      location ? location->BuildInspectorObject() : nullptr);
}

void InspectorPageAgent::FrameDetachedFromParent(LocalFrame* frame) {
  GetFrontend()->frameDetached(FrameId(frame));
}

bool InspectorPageAgent::ScreencastEnabled() {
  return enabled_ &&
         state_->booleanProperty(PageAgentState::kScreencastEnabled, false);
}

void InspectorPageAgent::FrameStartedLoading(LocalFrame* frame, FrameLoadType) {
  GetFrontend()->frameStartedLoading(FrameId(frame));
}

void InspectorPageAgent::FrameStoppedLoading(LocalFrame* frame) {
  GetFrontend()->frameStoppedLoading(FrameId(frame));
}

void InspectorPageAgent::FrameScheduledNavigation(LocalFrame* frame,
                                                  double delay) {
  GetFrontend()->frameScheduledNavigation(FrameId(frame), delay);
}

void InspectorPageAgent::FrameClearedScheduledNavigation(LocalFrame* frame) {
  GetFrontend()->frameClearedScheduledNavigation(FrameId(frame));
}

void InspectorPageAgent::WillRunJavaScriptDialog(
    const String& message,
    ChromeClient::DialogType dialog_type) {
  GetFrontend()->javascriptDialogOpening(message,
                                         DialogTypeToProtocol(dialog_type));
  GetFrontend()->flush();
}

void InspectorPageAgent::DidRunJavaScriptDialog(bool result) {
  GetFrontend()->javascriptDialogClosed(result);
  GetFrontend()->flush();
}

void InspectorPageAgent::DidResizeMainFrame() {
  if (!inspected_frames_->Root()->IsMainFrame())
    return;
#if !OS(ANDROID)
  PageLayoutInvalidated(true);
#endif
  GetFrontend()->frameResized();
}

void InspectorPageAgent::DidChangeViewport() {
  PageLayoutInvalidated(false);
}

void InspectorPageAgent::Will(const probe::UpdateLayout&) {}

void InspectorPageAgent::Did(const probe::UpdateLayout&) {
  PageLayoutInvalidated(false);
}

void InspectorPageAgent::Will(const probe::RecalculateStyle&) {}

void InspectorPageAgent::Did(const probe::RecalculateStyle&) {
  PageLayoutInvalidated(false);
}

void InspectorPageAgent::PageLayoutInvalidated(bool resized) {
  if (enabled_ && client_)
    client_->PageLayoutInvalidated(resized);
}

void InspectorPageAgent::WindowCreated(LocalFrame* created) {
  if (enabled_ &&
      state_->booleanProperty(PageAgentState::kAutoAttachToCreatedPages, false))
    client_->WaitForCreateWindow(created);
}

std::unique_ptr<protocol::Page::Frame> InspectorPageAgent::BuildObjectForFrame(
    LocalFrame* frame) {
  std::unique_ptr<protocol::Page::Frame> frame_object =
      protocol::Page::Frame::create()
          .setId(FrameId(frame))
          .setLoaderId(
              IdentifiersFactory::LoaderId(frame->Loader().GetDocumentLoader()))
          .setUrl(UrlWithoutFragment(frame->GetDocument()->Url()).GetString())
          .setMimeType(frame->Loader().GetDocumentLoader()->ResponseMIMEType())
          .setSecurityOrigin(
              frame->GetDocument()->GetSecurityOrigin()->ToRawString())
          .build();
  // FIXME: This doesn't work for OOPI.
  Frame* parent_frame = frame->Tree().Parent();
  if (parent_frame && parent_frame->IsLocalFrame())
    frame_object->setParentId(FrameId(ToLocalFrame(parent_frame)));
  if (frame->DeprecatedLocalOwner()) {
    AtomicString name = frame->DeprecatedLocalOwner()->GetNameAttribute();
    if (name.IsEmpty())
      name = frame->DeprecatedLocalOwner()->getAttribute(HTMLNames::idAttr);
    frame_object->setName(name);
  }

  return frame_object;
}

std::unique_ptr<protocol::Page::FrameResourceTree>
InspectorPageAgent::BuildObjectForFrameTree(LocalFrame* frame) {
  std::unique_ptr<protocol::Page::Frame> frame_object =
      BuildObjectForFrame(frame);
  std::unique_ptr<protocol::Array<protocol::Page::FrameResource>> subresources =
      protocol::Array<protocol::Page::FrameResource>::create();

  HeapVector<Member<Resource>> all_resources =
      CachedResourcesForFrame(frame, true);
  for (Resource* cached_resource : all_resources) {
    std::unique_ptr<protocol::Page::FrameResource> resource_object =
        protocol::Page::FrameResource::create()
            .setUrl(UrlWithoutFragment(cached_resource->Url()).GetString())
            .setType(CachedResourceTypeJson(*cached_resource))
            .setMimeType(cached_resource->GetResponse().MimeType())
            .setContentSize(cached_resource->GetResponse().DecodedBodyLength())
            .build();
    double last_modified = cached_resource->GetResponse().LastModified();
    if (!std::isnan(last_modified))
      resource_object->setLastModified(last_modified);
    if (cached_resource->WasCanceled())
      resource_object->setCanceled(true);
    else if (cached_resource->GetStatus() == ResourceStatus::kLoadError)
      resource_object->setFailed(true);
    subresources->addItem(std::move(resource_object));
  }

  HeapVector<Member<Document>> all_imports =
      InspectorPageAgent::ImportsForFrame(frame);
  for (Document* import : all_imports) {
    std::unique_ptr<protocol::Page::FrameResource> resource_object =
        protocol::Page::FrameResource::create()
            .setUrl(UrlWithoutFragment(import->Url()).GetString())
            .setType(ResourceTypeJson(InspectorPageAgent::kDocumentResource))
            .setMimeType(import->SuggestedMIMEType())
            .build();
    subresources->addItem(std::move(resource_object));
  }

  std::unique_ptr<protocol::Page::FrameResourceTree> result =
      protocol::Page::FrameResourceTree::create()
          .setFrame(std::move(frame_object))
          .setResources(std::move(subresources))
          .build();

  std::unique_ptr<protocol::Array<protocol::Page::FrameResourceTree>>
      children_array;
  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (!child->IsLocalFrame())
      continue;
    if (!children_array)
      children_array =
          protocol::Array<protocol::Page::FrameResourceTree>::create();
    children_array->addItem(BuildObjectForFrameTree(ToLocalFrame(child)));
  }
  result->setChildFrames(std::move(children_array));
  return result;
}

Response InspectorPageAgent::startScreencast(Maybe<String> format,
                                             Maybe<int> quality,
                                             Maybe<int> max_width,
                                             Maybe<int> max_height,
                                             Maybe<int> every_nth_frame) {
  state_->setBoolean(PageAgentState::kScreencastEnabled, true);
  return Response::OK();
}

Response InspectorPageAgent::stopScreencast() {
  state_->setBoolean(PageAgentState::kScreencastEnabled, false);
  return Response::OK();
}

Response InspectorPageAgent::getLayoutMetrics(
    std::unique_ptr<protocol::Page::LayoutViewport>* out_layout_viewport,
    std::unique_ptr<protocol::Page::VisualViewport>* out_visual_viewport,
    std::unique_ptr<protocol::DOM::Rect>* out_content_size) {
  LocalFrame* main_frame = inspected_frames_->Root();
  VisualViewport& visual_viewport = main_frame->GetPage()->GetVisualViewport();

  main_frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  IntRect visible_contents = main_frame->View()->VisibleContentRect();
  *out_layout_viewport = protocol::Page::LayoutViewport::create()
                             .setPageX(visible_contents.X())
                             .setPageY(visible_contents.Y())
                             .setClientWidth(visible_contents.Width())
                             .setClientHeight(visible_contents.Height())
                             .build();

  FrameView* frame_view = main_frame->View();
  ScrollOffset page_offset = frame_view->GetScrollableArea()->GetScrollOffset();
  float page_zoom = main_frame->PageZoomFactor();
  FloatRect visible_rect = visual_viewport.VisibleRect();
  float scale = visual_viewport.Scale();
  float scrollbar_width = frame_view->VerticalScrollbarWidth() / scale;
  float scrollbar_height = frame_view->HorizontalScrollbarHeight() / scale;

  IntSize content_size = frame_view->GetScrollableArea()->ContentsSize();
  *out_content_size = protocol::DOM::Rect::create()
                          .setX(0)
                          .setY(0)
                          .setWidth(content_size.Width())
                          .setHeight(content_size.Height())
                          .build();

  *out_visual_viewport =
      protocol::Page::VisualViewport::create()
          .setOffsetX(AdjustScrollForAbsoluteZoom(visible_rect.X(), page_zoom))
          .setOffsetY(AdjustScrollForAbsoluteZoom(visible_rect.Y(), page_zoom))
          .setPageX(AdjustScrollForAbsoluteZoom(page_offset.Width(), page_zoom))
          .setPageY(
              AdjustScrollForAbsoluteZoom(page_offset.Height(), page_zoom))
          .setClientWidth(visible_rect.Width() - scrollbar_width)
          .setClientHeight(visible_rect.Height() - scrollbar_height)
          .setScale(scale)
          .build();
  return Response::OK();
}

protocol::Response InspectorPageAgent::createIsolatedWorld(
    const String& frame_id,
    Maybe<String> world_name,
    Maybe<bool> grant_universal_access) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame)
    return Response::Error("No frame for given id found");

  int world_id = frame->GetScriptController().CreateNewDInspectorIsolatedWorld(
      world_name.fromMaybe(""));
  if (world_id == DOMWrapperWorld::kInvalidWorldId)
    return Response::Error("Could not create isolated world");

  if (grant_universal_access.fromMaybe(false)) {
    RefPtr<SecurityOrigin> security_origin =
        frame->GetSecurityContext()->GetSecurityOrigin()->IsolatedCopy();
    security_origin->GrantUniversalAccess();
    DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(world_id, security_origin);
  }
  return Response::OK();
}

DEFINE_TRACE(InspectorPageAgent) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(inspector_resource_content_loader_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
