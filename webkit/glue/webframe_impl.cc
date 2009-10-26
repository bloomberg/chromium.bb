/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |
//   Page O------- Frame (m_mainFrame) O-------O FrameView
//                   ||
//                   ||
//               FrameLoader O-------- WebFrame (via FrameLoaderClient)
//
// FrameLoader and Frame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// WebFrame is refcounted and has one ref on behalf of the FrameLoader/Frame.
// This is not a normal reference counted pointer because that would require
// changing WebKit code that we don't control. Instead, it is created with this
// ref initially and it is removed when the FrameLoader is getting destroyed.
//
// WebFrames are created in two places, first in WebViewImpl when the root
// frame is created, and second in WebFrame::CreateChildFrame when sub-frames
// are created. WebKit will hook up this object to the FrameLoader/Frame
// and the refcount will be correct.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each subframe.
//
// Frame going away causes the FrameLoader to get deleted. In FrameLoader's
// destructor, it notifies its client with frameLoaderDestroyed. This calls
// WebFrame::Closing and then derefs the WebFrame and will cause it to be
// deleted (unless an external someone is also holding a reference).

#include "config.h"

#include <algorithm>
#include <string>

#include "HTMLFormElement.h"  // need this before Document.h
#include "Chrome.h"
#include "ChromeClientChromium.h"
#include "ChromiumBridge.h"
#include "ClipboardUtilitiesChromium.h"
#include "Console.h"
#include "Document.h"
#include "DocumentFragment.h"  // Only needed for ReplaceSelectionCommand.h :(
#include "DocumentLoader.h"
#include "DocumentMarker.h"
#include "DOMWindow.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FormState.h"
#include "FrameChromium.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLCollection.h"
#include "HTMLHeadElement.h"
#include "HTMLInputElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#if PLATFORM(DARWIN)
#include "LocalCurrentGraphicsContext.h"
#endif
#include "markup.h"
#include "Page.h"
#include "PlatformContextSkia.h"
#include "PrintContext.h"
#include "RenderFrame.h"
#if PLATFORM(WIN_OS)
#include "RenderThemeChromiumWin.h"
#endif
#include "RenderView.h"
#include "RenderWidget.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "ScrollbarTheme.h"
#include "ScrollTypes.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SkiaUtils.h"
#include "SubstituteData.h"
#include "TextIterator.h"
#include "TextAffinity.h"
#include "XPathResult.h"
#include <wtf/CurrentTime.h>
#undef LOG

#include "webkit/api/public/WebConsoleMessage.h"
#include "webkit/api/public/WebFindOptions.h"
#include "webkit/api/public/WebForm.h"
#include "webkit/api/public/WebFrameClient.h"
#include "webkit/api/public/WebHistoryItem.h"
#include "webkit/api/public/WebRange.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/api/public/WebSecurityOrigin.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/api/src/DOMUtilitiesPrivate.h"
#include "webkit/api/src/PasswordAutocompleteListener.h"
#include "webkit/api/src/WebDataSourceImpl.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

#if PLATFORM(LINUX)
#include <gdk/gdk.h>
#endif

using WebCore::AtomicString;
using WebCore::ChromeClientChromium;
using WebCore::ChromiumBridge;
using WebCore::Color;
using WebCore::Document;
using WebCore::DocumentFragment;
using WebCore::DocumentLoader;
using WebCore::ExceptionCode;
using WebCore::GraphicsContext;
using WebCore::HTMLFrameOwnerElement;
using WebCore::HTMLInputElement;
using WebCore::Frame;
using WebCore::FrameLoader;
using WebCore::FrameLoadRequest;
using WebCore::FrameLoadType;
using WebCore::FrameTree;
using WebCore::FrameView;
using WebCore::HistoryItem;
using WebCore::HTMLFormElement;
using WebCore::IntRect;
using WebCore::KURL;
using WebCore::Node;
using WebCore::Range;
using WebCore::ReloadIgnoringCacheData;
using WebCore::RenderObject;
using WebCore::ResourceError;
using WebCore::ResourceHandle;
using WebCore::ResourceRequest;
using WebCore::ResourceResponse;
using WebCore::VisibleSelection;
using WebCore::ScriptValue;
using WebCore::SecurityOrigin;
using WebCore::SharedBuffer;
using WebCore::String;
using WebCore::SubstituteData;
using WebCore::TextIterator;
using WebCore::Timer;
using WebCore::VisiblePosition;
using WebCore::XPathResult;

using WebKit::PasswordAutocompleteListener;
using WebKit::WebCanvas;
using WebKit::WebConsoleMessage;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDataSourceImpl;
using WebKit::WebFindOptions;
using WebKit::WebFrame;
using WebKit::WebFrameClient;
using WebKit::WebHistoryItem;
using WebKit::WebForm;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScriptSource;
using WebKit::WebSecurityOrigin;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;
using WebKit::WebView;

// Key for a StatsCounter tracking how many WebFrames are active.
static const char* const kWebFrameActiveCount = "WebFrameActiveCount";

static const char* const kOSDType = "application/opensearchdescription+xml";
static const char* const kOSDRel = "search";

// The separator between frames when the frames are converted to plain text.
static const wchar_t kFrameSeparator[] = L"\n\n";
static const size_t kFrameSeparatorLen = arraysize(kFrameSeparator) - 1;

// Backend for contentAsPlainText, this is a recursive function that gets
// the text for the current frame and all of its subframes. It will append
// the text of each frame in turn to the |output| up to |max_chars| length.
//
// The |frame| must be non-NULL.
static void FrameContentAsPlainText(size_t max_chars, Frame* frame,
                                    Vector<UChar>* output) {
  Document* doc = frame->document();
  if (!doc)
    return;

  if (!frame->view())
    return;

  // TextIterator iterates over the visual representation of the DOM. As such,
  // it requires you to do a layout before using it (otherwise it'll crash).
  if (frame->view()->needsLayout())
    frame->view()->layout();

  // Select the document body.
  RefPtr<Range> range(doc->createRange());
  ExceptionCode exception = 0;
  range->selectNodeContents(doc->body(), exception);

  if (exception == 0) {
    // The text iterator will walk nodes giving us text. This is similar to
    // the plainText() function in TextIterator.h, but we implement the maximum
    // size and also copy the results directly into a wstring, avoiding the
    // string conversion.
    for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
      const UChar* chars = it.characters();
      if (!chars) {
        if (it.length() != 0) {
          // It appears from crash reports that an iterator can get into a state
          // where the character count is nonempty but the character pointer is
          // NULL. advance()ing it will then just add that many to the NULL
          // pointer which won't be caught in a NULL check but will crash.
          //
          // A NULL pointer and 0 length is common for some nodes.
          //
          // IF YOU CATCH THIS IN A DEBUGGER please let brettw know. We don't
          // currently understand the conditions for this to occur. Ideally, the
          // iterators would never get into the condition so we should fix them
          // if we can.
          ASSERT_NOT_REACHED();
          break;
        }

        // Just got a NULL node, we can forge ahead!
        continue;
      }
      size_t to_append = std::min(static_cast<size_t>(it.length()),
                                  max_chars - output->size());
      output->append(chars, to_append);
      if (output->size() >= max_chars)
        return;  // Filled up the buffer.
    }
  }

  // Recursively walk the children.
  FrameTree* frame_tree = frame->tree();
  for (Frame* cur_child = frame_tree->firstChild(); cur_child;
       cur_child = cur_child->tree()->nextSibling()) {
    // Make sure the frame separator won't fill up the buffer, and give up if
    // it will. The danger is if the separator will make the buffer longer than
    // max_chars. This will cause the computation above:
    //   max_chars - output->size()
    // to be a negative number which will crash when the subframe is added.
    if (output->size() >= max_chars - kFrameSeparatorLen)
      return;

    output->append(kFrameSeparator, kFrameSeparatorLen);
    FrameContentAsPlainText(max_chars, cur_child, output);
    if (output->size() >= max_chars)
      return;  // Filled up the buffer.
  }
}

// Simple class to override some of PrintContext behavior.
class ChromePrintContext : public WebCore::PrintContext {
 public:
  ChromePrintContext(Frame* frame)
      : PrintContext(frame),
        printed_page_width_(0) {
  }
  void begin(float width) {
    ASSERT(!printed_page_width_);
    printed_page_width_ = width;
    WebCore::PrintContext::begin(printed_page_width_);
  }
  float getPageShrink(int pageNumber) const {
    IntRect pageRect = m_pageRects[pageNumber];
    return printed_page_width_ / pageRect.width();
  }
  // Spools the printed page, a subrect of m_frame.
  // Skip the scale step. NativeTheme doesn't play well with scaling. Scaling
  // is done browser side instead.
  // Returns the scale to be applied.
  float spoolPage(GraphicsContext& ctx, int pageNumber) {
    IntRect pageRect = m_pageRects[pageNumber];
    float scale = printed_page_width_ / pageRect.width();

    ctx.save();
    ctx.translate(static_cast<float>(-pageRect.x()),
                  static_cast<float>(-pageRect.y()));
    ctx.clip(pageRect);
    m_frame->view()->paintContents(&ctx, pageRect);
    ctx.restore();
    return scale;
  }
 protected:
  // Set when printing.
  float printed_page_width_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromePrintContext);
};

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader) {
  return loader ? WebDataSourceImpl::fromDocumentLoader(loader) : NULL;
}


// WebFrame -------------------------------------------------------------------

class WebFrameImpl::DeferredScopeStringMatches {
 public:
  DeferredScopeStringMatches(WebFrameImpl* webframe,
                             int identifier,
                             const WebString& search_text,
                             const WebFindOptions& options,
                             bool reset)
      : timer_(this, &DeferredScopeStringMatches::DoTimeout),
        webframe_(webframe),
        identifier_(identifier),
        search_text_(search_text),
        options_(options),
        reset_(reset) {
    timer_.startOneShot(0.0);
  }

 private:
  void DoTimeout(Timer<DeferredScopeStringMatches>*) {
    webframe_->CallScopeStringMatches(
        this, identifier_, search_text_, options_, reset_);
  }

  Timer<DeferredScopeStringMatches> timer_;
  RefPtr<WebFrameImpl> webframe_;
  int identifier_;
  WebString search_text_;
  WebFindOptions options_;
  bool reset_;
};


// WebFrame -------------------------------------------------------------------

// static
WebFrame* WebFrame::frameForEnteredContext() {
  Frame* frame =
      WebCore::ScriptController::retrieveFrameForEnteredContext();
  return WebFrameImpl::FromFrame(frame);
}

// static
WebFrame* WebFrame::frameForCurrentContext() {
  Frame* frame =
      WebCore::ScriptController::retrieveFrameForCurrentContext();
  return WebFrameImpl::FromFrame(frame);
}

WebString WebFrameImpl::name() const {
  return webkit_glue::StringToWebString(frame_->tree()->name());
}

WebURL WebFrameImpl::url() const {
  const WebDataSource* ds = dataSource();
  if (!ds)
    return WebURL();
  return ds->request().url();
}

WebURL WebFrameImpl::favIconURL() const {
  WebCore::FrameLoader* frame_loader = frame_->loader();
  // The URL to the favicon may be in the header. As such, only
  // ask the loader for the favicon if it's finished loading.
  if (frame_loader->state() == WebCore::FrameStateComplete) {
    const KURL& url = frame_loader->iconURL();
    if (!url.isEmpty()) {
      return webkit_glue::KURLToWebURL(url);
    }
  }
  return WebURL();
}

WebURL WebFrameImpl::openSearchDescriptionURL() const {
  WebCore::FrameLoader* frame_loader = frame_->loader();
  if (frame_loader->state() == WebCore::FrameStateComplete &&
      frame_->document() && frame_->document()->head() &&
      !frame_->tree()->parent()) {
    WebCore::HTMLHeadElement* head = frame_->document()->head();
    if (head) {
      RefPtr<WebCore::HTMLCollection> children = head->children();
      for (Node* child = children->firstItem(); child != NULL;
           child = children->nextItem()) {
        WebCore::HTMLLinkElement* link_element =
            WebKit::toHTMLLinkElement(child);
        if (link_element && link_element->type() == kOSDType &&
            link_element->rel() == kOSDRel && !link_element->href().isEmpty()) {
          return webkit_glue::KURLToWebURL(link_element->href());
        }
      }
    }
  }
  return WebURL();
}

WebSize WebFrameImpl::scrollOffset() const {
  WebCore::FrameView* view = frameview();
  if (view)
    return webkit_glue::IntSizeToWebSize(view->scrollOffset());

  return WebSize();
}

WebSize WebFrameImpl::contentsSize() const {
  return webkit_glue::IntSizeToWebSize(frame()->view()->contentsSize());
}

int WebFrameImpl::contentsPreferredWidth() const {
  if ((frame_->document() != NULL) &&
      (frame_->document()->renderView() != NULL)) {
    return frame_->document()->renderView()->minPrefWidth();
  } else {
    return 0;
  }
}

bool WebFrameImpl::hasVisibleContent() const {
  return frame()->view()->visibleWidth() > 0 &&
         frame()->view()->visibleHeight() > 0;
}

WebView* WebFrameImpl::view() const {
  return GetWebViewImpl();
}

WebFrame* WebFrameImpl::opener() const {
  Frame* opener = NULL;
  if (frame_)
    opener = frame_->loader()->opener();
  return FromFrame(opener);
}

WebFrame* WebFrameImpl::parent() const {
  Frame *parent = NULL;
  if (frame_)
    parent = frame_->tree()->parent();
  return FromFrame(parent);
}

WebFrame* WebFrameImpl::top() const {
  if (frame_)
    return FromFrame(frame_->tree()->top());

  return NULL;
}

WebFrame* WebFrameImpl::firstChild() const {
  return FromFrame(frame()->tree()->firstChild());
}

WebFrame* WebFrameImpl::lastChild() const {
  return FromFrame(frame()->tree()->lastChild());
}

WebFrame* WebFrameImpl::nextSibling() const {
  return FromFrame(frame()->tree()->nextSibling());
}

WebFrame* WebFrameImpl::previousSibling() const {
  return FromFrame(frame()->tree()->previousSibling());
}

WebFrame* WebFrameImpl::traverseNext(bool wrap) const {
  return FromFrame(frame()->tree()->traverseNextWithWrap(wrap));
}

WebFrame* WebFrameImpl::traversePrevious(bool wrap) const {
  return FromFrame(frame()->tree()->traversePreviousWithWrap(wrap));
}

WebFrame* WebFrameImpl::findChildByName(const WebKit::WebString& name) const {
  return FromFrame(frame()->tree()->child(
      webkit_glue::WebStringToString(name)));
}

WebFrame* WebFrameImpl::findChildByExpression(
    const WebKit::WebString& xpath) const {
  if (xpath.isEmpty())
    return NULL;

  Document* document = frame_->document();

  ExceptionCode ec = 0;
  PassRefPtr<XPathResult> xpath_result =
      document->evaluate(webkit_glue::WebStringToString(xpath),
      document,
      NULL, /* namespace */
      XPathResult::ORDERED_NODE_ITERATOR_TYPE,
      NULL, /* XPathResult object */
      ec);
  if (!xpath_result.get())
    return NULL;

  Node* node = xpath_result->iterateNext(ec);

  if (!node || !node->isFrameOwnerElement())
    return NULL;
  HTMLFrameOwnerElement* frame_element =
    static_cast<HTMLFrameOwnerElement*>(node);
  return FromFrame(frame_element->contentFrame());
}

void WebFrameImpl::forms(WebVector<WebForm>& results) const {
  if (!frame_)
    return;

  RefPtr<WebCore::HTMLCollection> forms = frame_->document()->forms();
  size_t form_count = forms->length();

  WebVector<WebForm> temp(form_count);
  for (size_t i = 0; i < form_count; ++i) {
    Node* node = forms->item(i);
    // Strange but true, sometimes item can be NULL.
    if (node) {
      temp[i] = webkit_glue::HTMLFormElementToWebForm(
          static_cast<HTMLFormElement*>(node));
    }
  }
  results.swap(temp);
}

WebSecurityOrigin WebFrameImpl::securityOrigin() const {
  if (!frame_ || !frame_->document())
    return WebSecurityOrigin();

  return webkit_glue::SecurityOriginToWebSecurityOrigin(
      frame_->document()->securityOrigin());
}

void WebFrameImpl::grantUniversalAccess() {
  ASSERT(frame_ && frame_->document());
  if (frame_ && frame_->document()) {
    frame_->document()->securityOrigin()->grantUniversalAccess();
  }
}

NPObject* WebFrameImpl::windowObject() const {
  if (!frame_)
    return NULL;

  return frame_->script()->windowScriptNPObject();
}

void WebFrameImpl::bindToWindowObject(const WebString& name,
                                      NPObject* object) {
  ASSERT(frame_);
  if (!frame_ || !frame_->script()->isEnabled())
    return;

  String key = webkit_glue::WebStringToString(name);
#if USE(V8)
  frame_->script()->bindToWindowObject(frame_, key, object);
#else
  notImplemented();
#endif
}

void WebFrameImpl::executeScript(const WebScriptSource& source) {
  frame_->script()->executeScript(
      WebCore::ScriptSourceCode(
          webkit_glue::WebStringToString(source.code),
          webkit_glue::WebURLToKURL(source.url),
          source.startLine));
}

void WebFrameImpl::executeScriptInNewContext(
    const WebScriptSource* sources_in, unsigned num_sources,
    int extension_group) {
  Vector<WebCore::ScriptSourceCode> sources;

  for (unsigned i = 0; i < num_sources; ++i) {
    sources.append(WebCore::ScriptSourceCode(
        webkit_glue::WebStringToString(sources_in[i].code),
        webkit_glue::WebURLToKURL(sources_in[i].url),
        sources_in[i].startLine));
  }

  frame_->script()->evaluateInNewContext(sources, extension_group);
}

void WebFrameImpl::executeScriptInIsolatedWorld(
    int world_id, const WebScriptSource* sources_in, unsigned num_sources,
    int extension_group) {
  Vector<WebCore::ScriptSourceCode> sources;

  for (unsigned i = 0; i < num_sources; ++i) {
    sources.append(WebCore::ScriptSourceCode(
        webkit_glue::WebStringToString(sources_in[i].code),
        webkit_glue::WebURLToKURL(sources_in[i].url),
        sources_in[i].startLine));
  }

  frame_->script()->evaluateInIsolatedWorld(world_id, sources, extension_group);
}

void WebFrameImpl::addMessageToConsole(const WebConsoleMessage& message) {
  ASSERT(frame());

  WebCore::MessageLevel webcore_message_level;
  switch (message.level) {
    case WebConsoleMessage::LevelTip:
      webcore_message_level = WebCore::TipMessageLevel;
      break;
    case WebConsoleMessage::LevelLog:
      webcore_message_level = WebCore::LogMessageLevel;
      break;
    case WebConsoleMessage::LevelWarning:
      webcore_message_level = WebCore::WarningMessageLevel;
      break;
    case WebConsoleMessage::LevelError:
      webcore_message_level = WebCore::ErrorMessageLevel;
      break;
    default:
      ASSERT_NOT_REACHED();
      return;
  }

  frame()->domWindow()->console()->addMessage(
      WebCore::OtherMessageSource, WebCore::LogMessageType,
      webcore_message_level, webkit_glue::WebStringToString(message.text),
      1, String());
}

void WebFrameImpl::collectGarbage() {
  if (!frame_)
    return;
  if (!frame_->settings()->isJavaScriptEnabled())
    return;
  // TODO(mbelshe): Move this to the ScriptController and make it JS neutral.
#if USE(V8)
  frame_->script()->collectGarbage();
#else
  notImplemented();
#endif
}

#if USE(V8)
// Returns the V8 context for this frame, or an empty handle if there is none.
v8::Local<v8::Context> WebFrameImpl::mainWorldScriptContext() const {
  if (!frame_)
    return v8::Local<v8::Context>();

  return WebCore::V8Proxy::mainWorldContext(frame_);
}
#endif

bool WebFrameImpl::insertStyleText(
    const WebString& css, const WebString& id) {
  Document* document = frame()->document();
  if (!document)
    return false;
  WebCore::Element* document_element = document->documentElement();
  if (!document_element)
    return false;

  ExceptionCode err = 0;

  if (!id.isEmpty()) {
    WebCore::Element* old_element =
        document->getElementById(webkit_glue::WebStringToString(id));
    if (old_element) {
      Node* parent = old_element->parent();
      if (!parent)
        return false;
      parent->removeChild(old_element, err);
    }
  }

  RefPtr<WebCore::Element> stylesheet = document->createElement(
      WebCore::HTMLNames::styleTag, false);
  if (!id.isEmpty())
    stylesheet->setAttribute(WebCore::HTMLNames::idAttr,
                             webkit_glue::WebStringToString(id));
  stylesheet->setTextContent(webkit_glue::WebStringToString(css), err);
  ASSERT(!err);
  WebCore::Node* first = document_element->firstChild();
  bool success = document_element->insertBefore(stylesheet, first, err);
  ASSERT(success);
  return success;
}

void WebFrameImpl::reload() {
  frame_->loader()->history()->saveDocumentAndScrollState();

  stopLoading();  // Make sure existing activity stops.
  frame_->loader()->reload();
}

void WebFrameImpl::loadRequest(const WebURLRequest& request) {
  const ResourceRequest* resource_request =
      webkit_glue::WebURLRequestToResourceRequest(&request);
  ASSERT(resource_request);

  if (resource_request->url().protocolIs("javascript")) {
    LoadJavaScriptURL(resource_request->url());
    return;
  }

  stopLoading();  // Make sure existing activity stops.
  frame_->loader()->load(*resource_request, false);
}

void WebFrameImpl::loadHistoryItem(const WebHistoryItem& item) {
  RefPtr<HistoryItem> history_item =
      webkit_glue::WebHistoryItemToHistoryItem(item);
  ASSERT(history_item.get());

  stopLoading();  // Make sure existing activity stops.

  // If there is no current_item, which happens when we are navigating in
  // session history after a crash, we need to manufacture one otherwise WebKit
  // hoarks. This is probably the wrong thing to do, but it seems to work.
  RefPtr<HistoryItem> current_item = frame_->loader()->history()->currentItem();
  if (!current_item) {
    current_item = HistoryItem::create();
    current_item->setLastVisitWasFailure(true);
    frame_->loader()->history()->setCurrentItem(current_item.get());
    GetWebViewImpl()->SetCurrentHistoryItem(current_item.get());
  }

  frame_->loader()->history()->goToItem(history_item.get(),
                                        WebCore::FrameLoadTypeIndexedBackForward);
}

void WebFrameImpl::loadData(const WebData& data,
                            const WebString& mime_type,
                            const WebString& text_encoding,
                            const WebURL& base_url,
                            const WebURL& unreachable_url,
                            bool replace) {
  SubstituteData subst_data(
      webkit_glue::WebDataToSharedBuffer(data),
      webkit_glue::WebStringToString(mime_type),
      webkit_glue::WebStringToString(text_encoding),
      webkit_glue::WebURLToKURL(unreachable_url));
  ASSERT(subst_data.isValid());

  stopLoading();  // Make sure existing activity stops.
  frame_->loader()->load(ResourceRequest(webkit_glue::WebURLToKURL(base_url)),
                         subst_data, false);
  if (replace) {
    // Do this to force WebKit to treat the load as replacing the currently
    // loaded page.
    frame_->loader()->setReplacing();
  }
}

void WebFrameImpl::loadHTMLString(const WebData& data,
                                  const WebURL& base_url,
                                  const WebURL& unreachable_url,
                                  bool replace) {
  loadData(data,
           WebString::fromUTF8("text/html"),
           WebString::fromUTF8("UTF-8"),
           base_url,
           unreachable_url,
           replace);
}

bool WebFrameImpl::isLoading() const {
  if (!frame_)
    return false;
  return frame_->loader()->isLoading();
}

void WebFrameImpl::stopLoading() {
  if (!frame_)
    return;

  // TODO(darin): Figure out what we should really do here.  It seems like a
  // bug that FrameLoader::stopLoading doesn't call stopAllLoaders.
  frame_->loader()->stopAllLoaders();
  frame_->loader()->stopLoading(WebCore::UnloadEventPolicyNone);
}

WebDataSource* WebFrameImpl::provisionalDataSource() const {
  FrameLoader* frame_loader = frame_->loader();

  // We regard the policy document loader as still provisional.
  DocumentLoader* doc_loader = frame_loader->provisionalDocumentLoader();
  if (!doc_loader)
    doc_loader = frame_loader->policyDocumentLoader();

  return DataSourceForDocLoader(doc_loader);
}

WebDataSource* WebFrameImpl::dataSource() const {
  return DataSourceForDocLoader(frame_->loader()->documentLoader());
}

WebHistoryItem WebFrameImpl::previousHistoryItem() const {
  // We use the previous item here because documentState (filled-out forms)
  // only get saved to history when it becomes the previous item.  The caller
  // is expected to query the history item after a navigation occurs, after
  // the desired history item has become the previous entry.
  return webkit_glue::HistoryItemToWebHistoryItem(
      GetWebViewImpl()->GetPreviousHistoryItem());
}

WebHistoryItem WebFrameImpl::currentHistoryItem() const {
  frame_->loader()->history()->saveDocumentAndScrollState();

  return webkit_glue::HistoryItemToWebHistoryItem(
      frame_->page()->backForwardList()->currentItem());
}

void WebFrameImpl::enableViewSourceMode(bool enable) {
  if (frame_)
    frame_->setInViewSourceMode(enable);
}

bool WebFrameImpl::isViewSourceModeEnabled() const {
  if (frame_)
    return frame_->inViewSourceMode();

  return false;
}

void WebFrameImpl::setReferrerForRequest(
    WebURLRequest& request, const WebURL& referrer_url) {
  String referrer;
  if (referrer_url.isEmpty()) {
    referrer = frame_->loader()->outgoingReferrer();
  } else {
    referrer = webkit_glue::WebStringToString(referrer_url.spec().utf16());
  }
  if (SecurityOrigin::shouldHideReferrer(
          webkit_glue::WebURLToKURL(request.url()), referrer))
    return;
  request.setHTTPHeaderField(WebString::fromUTF8("Referer"),
                             webkit_glue::StringToWebString(referrer));
}

void WebFrameImpl::dispatchWillSendRequest(WebURLRequest& request) {
  ResourceResponse response;
  frame_->loader()->client()->dispatchWillSendRequest(NULL, 0,
      *webkit_glue::WebURLRequestToMutableResourceRequest(&request),
      response);
}

void WebFrameImpl::commitDocumentData(const char* data, size_t data_len) {
  DocumentLoader* document_loader = frame_->loader()->documentLoader();

  // Set the text encoding.  This calls begin() for us.  It is safe to call
  // this multiple times (Mac does: page/mac/WebCoreFrameBridge.mm).
  bool user_chosen = true;
  String encoding = document_loader->overrideEncoding();
  if (encoding.isNull()) {
    user_chosen = false;
    encoding = document_loader->response().textEncodingName();
  }
  frame_->loader()->setEncoding(encoding, user_chosen);

  // NOTE: mac only does this if there is a document
  frame_->loader()->addData(data, data_len);
}

unsigned WebFrameImpl::unloadListenerCount() const {
  return frame()->domWindow()->pendingUnloadEventListeners();
}

bool WebFrameImpl::isProcessingUserGesture() const {
  return frame()->loader()->isProcessingUserGesture();
}

void WebFrameImpl::replaceSelection(const WebString& wtext) {
  String text = webkit_glue::WebStringToString(wtext);
  RefPtr<DocumentFragment> fragment = createFragmentFromText(
      frame()->selection()->toNormalizedRange().get(), text);
  WebCore::applyCommand(WebCore::ReplaceSelectionCommand::create(
      frame()->document(), fragment.get(), false, true, true));
}

void WebFrameImpl::insertText(const WebString& text) {
  frame()->editor()->insertText(webkit_glue::WebStringToString(text), NULL);
}

void WebFrameImpl::setMarkedText(
    const WebString& text, unsigned location, unsigned length) {
  WebCore::Editor* editor = frame()->editor();
  WebCore::String str = webkit_glue::WebStringToString(text);

  editor->confirmComposition(str);

  WTF::Vector<WebCore::CompositionUnderline> decorations;
  editor->setComposition(str, decorations, location, length);
}

void WebFrameImpl::unmarkText() {
  frame()->editor()->confirmCompositionWithoutDisturbingSelection();
}

bool WebFrameImpl::hasMarkedText() const {
  return frame()->editor()->hasComposition();
}

WebRange WebFrameImpl::markedRange() const {
  return webkit_glue::RangeToWebRange(frame()->editor()->compositionRange());
}

bool WebFrameImpl::executeCommand(const WebString& name) {
  ASSERT(frame());

  if (name.length() <= 2)
    return false;

  // Since we don't have NSControl, we will convert the format of command
  // string and call the function on Editor directly.
  String command = webkit_glue::WebStringToString(name);

  // Make sure the first letter is upper case.
  command.replace(0, 1, command.substring(0, 1).upper());

  // Remove the trailing ':' if existing.
  if (command[command.length() - 1] == UChar(':'))
    command = command.substring(0, command.length() - 1);

  bool rv = true;

  // Specially handling commands that Editor::execCommand does not directly
  // support.
  if (command == "DeleteToEndOfParagraph") {
    WebCore::Editor* editor = frame()->editor();
    if (!editor->deleteWithDirection(WebCore::SelectionController::FORWARD,
                                     WebCore::ParagraphBoundary,
                                     true,
                                     false)) {
      editor->deleteWithDirection(WebCore::SelectionController::FORWARD,
                                  WebCore::CharacterGranularity,
                                  true,
                                  false);
    }
  } else if (command == "Indent") {
    frame()->editor()->indent();
  } else if (command == "Outdent") {
    frame()->editor()->outdent();
  } else if (command == "DeleteBackward") {
    rv = frame()->editor()->command(AtomicString("BackwardDelete")).execute();
  } else if (command == "DeleteForward") {
    rv = frame()->editor()->command(AtomicString("ForwardDelete")).execute();
  } else if (command == "AdvanceToNextMisspelling") {
    // False must be passed here, or the currently selected word will never be
    // skipped.
    frame()->editor()->advanceToNextMisspelling(false);
  } else if (command == "ToggleSpellPanel") {
    frame()->editor()->showSpellingGuessPanel();
  } else {
    rv = frame()->editor()->command(command).execute();
  }
  return rv;
}

bool WebFrameImpl::executeCommand(const WebString& name,
                                  const WebString& value) {
  ASSERT(frame());
  WebCore::String web_name = webkit_glue::WebStringToString(name);

  // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit
  // for editable nodes.
  if (!frame()->editor()->canEdit() &&
      web_name == "moveToBeginningOfDocument") {
    return GetWebViewImpl()->PropagateScroll(WebCore::ScrollUp,
                                             WebCore::ScrollByDocument);
  } else if (!frame()->editor()->canEdit() &&
      web_name == "moveToEndOfDocument") {
    return GetWebViewImpl()->PropagateScroll(WebCore::ScrollDown,
                                             WebCore::ScrollByDocument);
  } else {
    return frame()->editor()->command(web_name).
        execute(webkit_glue::WebStringToString(value));
  }
}

bool WebFrameImpl::isCommandEnabled(const WebString& name) const {
  ASSERT(frame());
  return frame()->editor()->command(webkit_glue::WebStringToString(name)).
      isEnabled();
}

void WebFrameImpl::enableContinuousSpellChecking(bool enable) {
  if (enable == isContinuousSpellCheckingEnabled())
    return;
  frame()->editor()->toggleContinuousSpellChecking();
}

bool WebFrameImpl::isContinuousSpellCheckingEnabled() const {
  return frame()->editor()->isContinuousSpellCheckingEnabled();
}

bool WebFrameImpl::hasSelection() const {
  // frame()->selection()->isNone() never returns true.
  return (frame()->selection()->start() != frame()->selection()->end());
}

WebRange WebFrameImpl::selectionRange() const {
  return webkit_glue::RangeToWebRange(
      frame()->selection()->toNormalizedRange());
}

WebString WebFrameImpl::selectionAsText() const {
  RefPtr<Range> range = frame()->selection()->toNormalizedRange();
  if (!range.get())
    return WebString();

  String text = range->text();
#if PLATFORM(WIN_OS)
  WebCore::replaceNewlinesWithWindowsStyleNewlines(text);
#endif
  WebCore::replaceNBSPWithSpace(text);
  return webkit_glue::StringToWebString(text);
}

WebString WebFrameImpl::selectionAsMarkup() const {
  RefPtr<Range> range = frame()->selection()->toNormalizedRange();
  if (!range.get())
    return WebString();

  String markup = WebCore::createMarkup(range.get(), 0);
  return webkit_glue::StringToWebString(markup);
}

int WebFrameImpl::printBegin(const WebSize& page_size) {
  ASSERT(!frame()->document()->isFrameSet());

  print_context_.set(new ChromePrintContext(frame()));
  WebCore::FloatRect rect(0, 0,
                          static_cast<float>(page_size.width),
                          static_cast<float>(page_size.height));
  print_context_->begin(rect.width());
  float page_height;
  // We ignore the overlays calculation for now since they are generated in the
  // browser. page_height is actually an output parameter.
  print_context_->computePageRects(rect, 0, 0, 1.0, page_height);
  return print_context_->pageCount();
}

float WebFrameImpl::getPrintPageShrink(int page) {
  // Ensure correct state.
  if (!print_context_.get() || page < 0) {
    ASSERT_NOT_REACHED();
    return 0;
  }

  return print_context_->getPageShrink(page);
}

float WebFrameImpl::printPage(int page, WebCanvas* canvas) {
  // Ensure correct state.
  if (!print_context_.get() || page < 0 || !frame() || !frame()->document()) {
    ASSERT_NOT_REACHED();
    return 0;
  }

#if PLATFORM(WIN_OS) || PLATFORM(LINUX) || PLATFORM(FREEBSD)
  PlatformContextSkia context(canvas);
  GraphicsContext spool(&context);
#elif PLATFORM(DARWIN)
  GraphicsContext spool(canvas);
  WebCore::LocalCurrentGraphicsContext localContext(&spool);
#endif

  return print_context_->spoolPage(spool, page);
}

void WebFrameImpl::printEnd() {
  ASSERT(print_context_.get());
  if (print_context_.get())
    print_context_->end();
  print_context_.clear();
}

bool WebFrameImpl::find(int request_id,
                        const WebString& search_text,
                        const WebFindOptions& options,
                        bool wrap_within_frame,
                        WebRect* selection_rect) {
  WebCore::String webcore_string = webkit_glue::WebStringToString(search_text);

  WebFrameImpl* const main_frame_impl = GetWebViewImpl()->main_frame();

  if (!options.findNext)
    frame()->page()->unmarkAllTextMatches();
  else
    SetMarkerActive(active_match_.get(), false);  // Active match is changing.

  // Starts the search from the current selection.
  bool start_in_selection = true;

  // If the user has selected something since the last Find operation we want
  // to start from there. Otherwise, we start searching from where the last Find
  // operation left off (either a Find or a FindNext operation).
  VisibleSelection selection(frame()->selection()->selection());
  if (selection.isNone() && active_match_) {
    selection = VisibleSelection(active_match_.get());
    frame()->selection()->setSelection(selection);
  }

  ASSERT(frame() && frame()->view());
  bool found = frame()->findString(webcore_string, options.forward,
                                   options.matchCase, wrap_within_frame,
                                   start_in_selection);
  if (found) {
    // Store which frame was active. This will come in handy later when we
    // change the active match ordinal below.
    WebFrameImpl* old_active_frame = main_frame_impl->active_match_frame_;
    // Set this frame as the active frame (the one with the active highlight).
    main_frame_impl->active_match_frame_ = this;

    // We found something, so we can now query the selection for its position.
    VisibleSelection new_selection(frame()->selection()->selection());
    IntRect curr_selection_rect;

    // If we thought we found something, but it couldn't be selected (perhaps
    // because it was marked -webkit-user-select: none), we can't set it to
    // be active but we still continue searching. This matches Safari's
    // behavior, including some oddities when selectable and un-selectable text
    // are mixed on a page: see https://bugs.webkit.org/show_bug.cgi?id=19127.
    if (new_selection.isNone() ||
        (new_selection.start() == new_selection.end())) {
      active_match_ = NULL;
    } else {
      active_match_ = new_selection.toNormalizedRange();
      curr_selection_rect = active_match_->boundingBox();
      SetMarkerActive(active_match_.get(), true);  // Active.
      // WebKit draws the highlighting for all matches.
      executeCommand(WebString::fromUTF8("Unselect"));
    }

    if (!options.findNext) {
      // This is a Find operation, so we set the flag to ask the scoping effort
      // to find the active rect for us so we can update the ordinal (n of m).
      locating_active_rect_ = true;
    } else {
      if (old_active_frame != this) {
        // If the active frame has changed it means that we have a multi-frame
        // page and we just switch to searching in a new frame. Then we just
        // want to reset the index.
        if (options.forward)
          active_match_index_ = 0;
        else
          active_match_index_ = last_match_count_ - 1;
      } else {
        // We are still the active frame, so increment (or decrement) the
        // |active_match_index|, wrapping if needed (on single frame pages).
        options.forward ? ++active_match_index_ : --active_match_index_;
        if (active_match_index_ + 1 > last_match_count_)
          active_match_index_ = 0;
        if (active_match_index_ + 1 == 0)
          active_match_index_ = last_match_count_ - 1;
      }
      if (selection_rect) {
        WebRect rect = webkit_glue::IntRectToWebRect(
            frame()->view()->convertToContainingWindow(curr_selection_rect));
        rect.x -= frameview()->scrollOffset().width();
        rect.y -= frameview()->scrollOffset().height();
        *selection_rect = rect;

        ReportFindInPageSelection(rect,
                                  active_match_index_ + 1,
                                  request_id);
      }
    }
  } else {
    // Nothing was found in this frame.
    active_match_ = NULL;

    // Erase all previous tickmarks and highlighting.
    InvalidateArea(INVALIDATE_ALL);
  }

  return found;
}

void WebFrameImpl::stopFinding(bool clear_selection) {
  if (!clear_selection)
    SetFindEndstateFocusAndSelection();
  cancelPendingScopingEffort();

  // Remove all markers for matches found and turn off the highlighting.
  if (!parent())
    frame()->document()->removeMarkers(WebCore::DocumentMarker::TextMatch);
  frame()->setMarkedTextMatchesAreHighlighted(false);

  // Let the frame know that we don't want tickmarks or highlighting anymore.
  InvalidateArea(INVALIDATE_ALL);
}

void WebFrameImpl::scopeStringMatches(int request_id,
                                      const WebString& search_text,
                                      const WebFindOptions& options,
                                      bool reset) {
  if (!ShouldScopeMatches(search_text))
    return;

  WebFrameImpl* main_frame_impl = GetWebViewImpl()->main_frame();

  if (reset) {
    // This is a brand new search, so we need to reset everything.
    // Scoping is just about to begin.
    scoping_complete_ = false;
    // Clear highlighting for this frame.
    if (frame()->markedTextMatchesAreHighlighted())
      frame()->page()->unmarkAllTextMatches();
    // Clear the counters from last operation.
    last_match_count_ = 0;
    next_invalidate_after_ = 0;

    resume_scoping_from_range_ = NULL;

    main_frame_impl->frames_scoping_count_++;

    // Now, defer scoping until later to allow find operation to finish quickly.
    ScopeStringMatchesSoon(
        request_id,
        search_text,
        options,
        false);  // false=we just reset, so don't do it again.
    return;
  }

  WebCore::String webcore_string = webkit_glue::String16ToString(search_text);

  RefPtr<Range> search_range(rangeOfContents(frame()->document()));

  ExceptionCode ec = 0, ec2 = 0;
  if (resume_scoping_from_range_.get()) {
    // This is a continuation of a scoping operation that timed out and didn't
    // complete last time around, so we should start from where we left off.
    search_range->setStart(resume_scoping_from_range_->startContainer(),
                           resume_scoping_from_range_->startOffset(ec2) + 1,
                           ec);
    if (ec != 0 || ec2 != 0) {
      if (ec2 != 0)  // A non-zero |ec| happens when navigating during search.
        ASSERT_NOT_REACHED();
      return;
    }
  }

  // This timeout controls how long we scope before releasing control.  This
  // value does not prevent us from running for longer than this, but it is
  // periodically checked to see if we have exceeded our allocated time.
  const double kTimeout = 0.1;  // seconds

  int match_count = 0;
  bool timeout = false;
  double start_time = currentTime();
  do {
    // Find next occurrence of the search string.
    // TODO(finnur): (http://b/1088245) This WebKit operation may run
    // for longer than the timeout value, and is not interruptible as it is
    // currently written. We may need to rewrite it with interruptibility in
    // mind, or find an alternative.
    RefPtr<Range> result_range(findPlainText(search_range.get(),
                                            webcore_string,
                                            true,
                                            options.matchCase));
    if (result_range->collapsed(ec)) {
      if (!result_range->startContainer()->isInShadowTree())
        break;

      search_range = rangeOfContents(frame()->document());
      search_range->setStartAfter(
          result_range->startContainer()->shadowAncestorNode(), ec);
      continue;
    }

    // A non-collapsed result range can in some funky whitespace cases still not
    // advance the range's start position (4509328). Break to avoid infinite
    // loop. (This function is based on the implementation of
    // Frame::markAllMatchesForText, which is where this safeguard comes from).
    VisiblePosition new_start = endVisiblePosition(result_range.get(),
                                                   WebCore::DOWNSTREAM);
    if (new_start == startVisiblePosition(search_range.get(),
                                          WebCore::DOWNSTREAM))
      break;

    // Only treat the result as a match if it is visible
    if (frame()->editor()->insideVisibleArea(result_range.get())) {
      ++match_count;

      setStart(search_range.get(), new_start);
      Node* shadow_tree_root = search_range->shadowTreeRootNode();
      if (search_range->collapsed(ec) && shadow_tree_root)
        search_range->setEnd(shadow_tree_root,
                             shadow_tree_root->childNodeCount(), ec);

      // Catch a special case where Find found something but doesn't know what
      // the bounding box for it is. In this case we set the first match we find
      // as the active rect.
      IntRect result_bounds = result_range->boundingBox();
      IntRect active_selection_rect;
      if (locating_active_rect_) {
        active_selection_rect = active_match_.get() ?
            active_match_->boundingBox() : result_bounds;
      }

      // If the Find function found a match it will have stored where the
      // match was found in active_selection_rect_ on the current frame. If we
      // find this rect during scoping it means we have found the active
      // tickmark.
      bool found_active_match = false;
      if (locating_active_rect_ && (active_selection_rect == result_bounds)) {
        // We have found the active tickmark frame.
        main_frame_impl->active_match_frame_ = this;
        found_active_match = true;
        // We also know which tickmark is active now.
        active_match_index_ = match_count - 1;
        // To stop looking for the active tickmark, we set this flag.
        locating_active_rect_ = false;

        // Notify browser of new location for the selected rectangle.
        result_bounds.move(-frameview()->scrollOffset().width(),
                           -frameview()->scrollOffset().height());
        ReportFindInPageSelection(
            webkit_glue::IntRectToWebRect(
                frame()->view()->convertToContainingWindow(result_bounds)),
            active_match_index_ + 1,
            request_id);
      }

      AddMarker(result_range.get(), found_active_match);
    }

    resume_scoping_from_range_ = result_range;
    timeout = (currentTime() - start_time) >= kTimeout;
  } while (!timeout);

  // Remember what we search for last time, so we can skip searching if more
  // letters are added to the search string (and last outcome was 0).
  last_search_string_ = search_text;

  if (match_count > 0) {
    frame()->setMarkedTextMatchesAreHighlighted(true);

    last_match_count_ += match_count;

    // Let the mainframe know how much we found during this pass.
    main_frame_impl->increaseMatchCount(match_count, request_id);
  }

  if (timeout) {
    // If we found anything during this pass, we should redraw. However, we
    // don't want to spam too much if the page is extremely long, so if we
    // reach a certain point we start throttling the redraw requests.
    if (match_count > 0)
      InvalidateIfNecessary();

    // Scoping effort ran out of time, lets ask for another time-slice.
    ScopeStringMatchesSoon(
        request_id,
        search_text,
        options,
        false);  // don't reset.
    return;  // Done for now, resume work later.
  }

  // This frame has no further scoping left, so it is done. Other frames might,
  // of course, continue to scope matches.
  scoping_complete_ = true;
  main_frame_impl->frames_scoping_count_--;

  // If this is the last frame to finish scoping we need to trigger the final
  // update to be sent.
  if (main_frame_impl->frames_scoping_count_ == 0)
    main_frame_impl->increaseMatchCount(0, request_id);

  // This frame is done, so show any scrollbar tickmarks we haven't drawn yet.
  InvalidateArea(INVALIDATE_SCROLLBAR);
}

void WebFrameImpl::cancelPendingScopingEffort() {
  deleteAllValues(deferred_scoping_work_);
  deferred_scoping_work_.clear();

  active_match_index_ = -1;
}

void WebFrameImpl::increaseMatchCount(int count, int request_id) {
  // This function should only be called on the mainframe.
  ASSERT(!parent());

  total_matchcount_ += count;

  // Update the UI with the latest findings.
  if (client()) {
    client()->reportFindInPageMatchCount(
        request_id, total_matchcount_, frames_scoping_count_ == 0);
  }
}

void WebFrameImpl::ReportFindInPageSelection(const WebRect& selection_rect,
                                             int active_match_ordinal,
                                             int request_id) {
  // Update the UI with the latest selection rect.
  if (client()) {
    client()->reportFindInPageSelection(
        request_id, OrdinalOfFirstMatchForFrame(this) + active_match_ordinal,
        selection_rect);
  }
}

void WebFrameImpl::resetMatchCount() {
  total_matchcount_ = 0;
  frames_scoping_count_ = 0;
}

WebURL WebFrameImpl::completeURL(const WebString& url) const {
  if (!frame_ || !frame_->document())
    return WebURL();

  return webkit_glue::KURLToWebURL(
      frame_->document()->completeURL(webkit_glue::WebStringToString(url)));
}

WebString WebFrameImpl::contentAsText(size_t max_chars) const {
  if (!frame_)
    return WebString();

  Vector<UChar> text;
  FrameContentAsPlainText(max_chars, frame_, &text);
  return webkit_glue::StringToWebString(String::adopt(text));
}

WebString WebFrameImpl::contentAsMarkup() const {
  return webkit_glue::StringToWebString(createFullMarkup(frame_->document()));
}

// WebFrameImpl public ---------------------------------------------------------

int WebFrameImpl::live_object_count_ = 0;

PassRefPtr<WebFrameImpl> WebFrameImpl::create(WebFrameClient* client) {
  return adoptRef(new WebFrameImpl(ClientHandle::create(client)));
}

WebFrameImpl::WebFrameImpl(PassRefPtr<ClientHandle> client_handle)
  : ALLOW_THIS_IN_INITIALIZER_LIST(frame_loader_client_(this)),
    client_handle_(client_handle),
    active_match_frame_(NULL),
    active_match_index_(-1),
    locating_active_rect_(false),
    resume_scoping_from_range_(NULL),
    last_match_count_(-1),
    total_matchcount_(-1),
    frames_scoping_count_(-1),
    scoping_complete_(false),
    next_invalidate_after_(0) {
  ChromiumBridge::incrementStatsCounter(kWebFrameActiveCount);
  live_object_count_++;
}

WebFrameImpl::~WebFrameImpl() {
  ChromiumBridge::decrementStatsCounter(kWebFrameActiveCount);
  live_object_count_--;

  cancelPendingScopingEffort();
  ClearPasswordListeners();
}

void WebFrameImpl::InitMainFrame(WebViewImpl* webview_impl) {
  RefPtr<Frame> frame =
      Frame::create(webview_impl->page(), 0, &frame_loader_client_);
  frame_ = frame.get();

  // Add reference on behalf of FrameLoader.  See comments in
  // WebFrameLoaderClient::frameLoaderDestroyed for more info.
  ref();

  // We must call init() after frame_ is assigned because it is referenced
  // during init().
  frame_->init();
}

PassRefPtr<Frame> WebFrameImpl::CreateChildFrame(
    const FrameLoadRequest& request, HTMLFrameOwnerElement* owner_element) {
  // TODO(darin): share code with initWithName()

  RefPtr<WebFrameImpl> webframe(adoptRef(new WebFrameImpl(client_handle_)));

  // Add an extra ref on behalf of the Frame/FrameLoader, which references the
  // WebFrame via the FrameLoaderClient interface. See the comment at the top
  // of this file for more info.
  webframe->ref();

  RefPtr<Frame> child_frame = Frame::create(
      frame_->page(), owner_element, &webframe->frame_loader_client_);
  webframe->frame_ = child_frame.get();

  child_frame->tree()->setName(request.frameName());

  frame_->tree()->appendChild(child_frame);

  // Frame::init() can trigger onload event in the parent frame,
  // which may detach this frame and trigger a null-pointer access
  // in FrameTree::removeChild. Move init() after appendChild call
  // so that webframe->frame_ is in the tree before triggering
  // onload event handler.
  // Because the event handler may set webframe->frame_ to null,
  // it is necessary to check the value after calling init() and
  // return without loading URL.
  // (b:791612)
  child_frame->init();      // create an empty document
  if (!child_frame->tree()->parent())
    return NULL;

  frame_->loader()->loadURLIntoChildFrame(
      request.resourceRequest().url(),
      request.resourceRequest().httpReferrer(),
      child_frame.get());

  // A synchronous navigation (about:blank) would have already processed
  // onload, so it is possible for the frame to have already been destroyed by
  // script in the page.
  if (!child_frame->tree()->parent())
    return NULL;

  return child_frame.release();
}

void WebFrameImpl::Layout() {
  // layout this frame
  FrameView* view = frame_->view();
  if (view)
    view->layoutIfNeededRecursive();
}

void WebFrameImpl::Paint(WebCanvas* canvas, const WebRect& rect) {
  if (rect.isEmpty())
    return;
  IntRect dirty_rect(webkit_glue::WebRectToIntRect(rect));
#if WEBKIT_USING_CG
  GraphicsContext gc(canvas);
  WebCore::LocalCurrentGraphicsContext localContext(&gc);
#elif WEBKIT_USING_SKIA
  PlatformContextSkia context(canvas);

  // PlatformGraphicsContext is actually a pointer to PlatformContextSkia
  GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#else
  notImplemented();
#endif
  gc.save();
  if (frame_->document() && frameview()) {
    gc.clip(dirty_rect);
    frameview()->paint(&gc, dirty_rect);
    frame_->page()->inspectorController()->drawNodeHighlight(gc);
  } else {
    gc.fillRect(dirty_rect, Color::white);
  }
  gc.restore();
}

void WebFrameImpl::CreateFrameView() {
  ASSERT(frame_);  // If frame_ doesn't exist, we probably didn't init properly.

  WebCore::Page* page = frame_->page();
  ASSERT(page);

  ASSERT(page->mainFrame() != NULL);

  bool is_main_frame = frame_ == page->mainFrame();
  if (is_main_frame && frame_->view())
    frame_->view()->setParentVisible(false);

  frame_->setView(0);

  WebViewImpl* web_view = GetWebViewImpl();

  RefPtr<WebCore::FrameView> view;
  if (is_main_frame) {
    IntSize size = webkit_glue::WebSizeToIntSize(web_view->size());
    view = FrameView::create(frame_, size);
  } else {
    view = FrameView::create(frame_);
  }

  frame_->setView(view);

  if (web_view->isTransparent())
    view->setTransparent(true);

  // TODO(darin): The Mac code has a comment about this possibly being
  // unnecessary.  See installInFrame in WebCoreFrameBridge.mm
  if (frame_->ownerRenderer())
    frame_->ownerRenderer()->setWidget(view.get());

  if (HTMLFrameOwnerElement* owner = frame_->ownerElement()) {
    view->setCanHaveScrollbars(
        owner->scrollingMode() != WebCore::ScrollbarAlwaysOff);
  }

  if (is_main_frame)
    view->setParentVisible(true);
}

// static
WebFrameImpl* WebFrameImpl::FromFrame(WebCore::Frame* frame) {
  if (!frame)
    return NULL;
  return static_cast<WebFrameLoaderClient*>(
      frame->loader()->client())->webframe();
}

WebViewImpl* WebFrameImpl::GetWebViewImpl() const {
  if (!frame_ || !frame_->page())
    return NULL;

  // There are cases where a Frame may outlive its associated Page.  Get the
  // WebViewImpl by accessing it indirectly through the Frame's Page so that we
  // don't have to worry about cleaning up the WebFrameImpl -> WebViewImpl
  // pointer. WebCore already clears the Frame's Page pointer when the Page is
  // destroyed by the WebViewImpl.
  return static_cast<ChromeClientImpl*>(
      frame_->page()->chrome()->client())->webview();
}

WebDataSourceImpl* WebFrameImpl::GetDataSourceImpl() const {
  return static_cast<WebDataSourceImpl*>(dataSource());
}

WebDataSourceImpl* WebFrameImpl::GetProvisionalDataSourceImpl() const {
  return static_cast<WebDataSourceImpl*>(provisionalDataSource());
}

void WebFrameImpl::SetFindEndstateFocusAndSelection() {
  WebFrameImpl* main_frame_impl = GetWebViewImpl()->main_frame();

  if (this == main_frame_impl->active_match_frame() &&
      active_match_.get()) {
    // If the user has set the selection since the match was found, we
    // don't focus anything.
    VisibleSelection selection(frame()->selection()->selection());
    if (!selection.isNone())
      return;

    // Try to find the first focusable node up the chain, which will, for
    // example, focus links if we have found text within the link.
    Node* node = active_match_->firstNode();
    while (node && !node->isFocusable() && node != frame()->document())
      node = node->parent();

    if (node && node != frame()->document()) {
      // Found a focusable parent node. Set focus to it.
      frame()->document()->setFocusedNode(node);
    } else {
      // Iterate over all the nodes in the range until we find a focusable node.
      // This, for example, sets focus to the first link if you search for
      // text and text that is within one or more links.
      node = active_match_->firstNode();
      while (node && node != active_match_->pastLastNode()) {
        if (node->isFocusable()) {
          frame()->document()->setFocusedNode(node);
          break;
        }
        node = node->traverseNextNode();
      }
    }
  }
}

void WebFrameImpl::DidFail(const ResourceError& error, bool was_provisional) {
  if (!client())
    return;
  const WebURLError& web_error =
      webkit_glue::ResourceErrorToWebURLError(error);
  if (was_provisional) {
    client()->didFailProvisionalLoad(this, web_error);
  } else {
    client()->didFailLoad(this, web_error);
  }
}

void WebFrameImpl::SetAllowsScrolling(bool flag) {
  frame_->view()->setCanHaveScrollbars(flag);
}

void WebFrameImpl::RegisterPasswordListener(
    PassRefPtr<HTMLInputElement> input_element,
    PasswordAutocompleteListener* listener) {
  RefPtr<HTMLInputElement> element = input_element;
  ASSERT(password_listeners_.find(element) == password_listeners_.end());
  password_listeners_.set(element, listener);
}

PasswordAutocompleteListener* WebFrameImpl::GetPasswordListener(
    HTMLInputElement* input_element) {
  return password_listeners_.get(RefPtr<HTMLInputElement>(input_element));
}

// WebFrameImpl protected ------------------------------------------------------

void WebFrameImpl::Closing() {
  frame_ = NULL;
}

// WebFrameImpl private --------------------------------------------------------

void WebFrameImpl::InvalidateArea(AreaToInvalidate area) {
  ASSERT(frame() && frame()->view());
  FrameView* view = frame()->view();

  if ((area & INVALIDATE_ALL) == INVALIDATE_ALL) {
    view->invalidateRect(view->frameRect());
  } else {
    if ((area & INVALIDATE_CONTENT_AREA) == INVALIDATE_CONTENT_AREA) {
      IntRect content_area(
          view->x(), view->y(), view->visibleWidth(), view->visibleHeight());
      view->invalidateRect(content_area);
    }

    if ((area & INVALIDATE_SCROLLBAR) == INVALIDATE_SCROLLBAR) {
      // Invalidate the vertical scroll bar region for the view.
      IntRect scroll_bar_vert(
          view->x() + view->visibleWidth(), view->y(),
          WebCore::ScrollbarTheme::nativeTheme()->scrollbarThickness(),
          view->visibleHeight());
      view->invalidateRect(scroll_bar_vert);
    }
  }
}

void WebFrameImpl::AddMarker(WebCore::Range* range, bool active_match) {
  // Use a TextIterator to visit the potentially multiple nodes the range
  // covers.
  TextIterator markedText(range);
  for (; !markedText.atEnd(); markedText.advance()) {
    RefPtr<Range> textPiece = markedText.range();
    int exception = 0;

    WebCore::DocumentMarker marker = {
        WebCore::DocumentMarker::TextMatch,
        textPiece->startOffset(exception),
        textPiece->endOffset(exception),
        "",
        active_match };

    if (marker.endOffset > marker.startOffset) {
      // Find the node to add a marker to and add it.
      Node* node = textPiece->startContainer(exception);
      frame()->document()->addMarker(node, marker);

      // Rendered rects for markers in WebKit are not populated until each time
      // the markers are painted. However, we need it to happen sooner, because
      // the whole purpose of tickmarks on the scrollbar is to show where
      // matches off-screen are (that haven't been painted yet).
      Vector<WebCore::DocumentMarker> markers =
          frame()->document()->markersForNode(node);
      frame()->document()->setRenderedRectForMarker(
          textPiece->startContainer(exception),
          markers[markers.size() - 1],
          range->boundingBox());
    }
  }
}

void WebFrameImpl::SetMarkerActive(WebCore::Range* range, bool active) {
  if (!range)
    return;

  frame()->document()->setMarkersActive(range, active);
}

int WebFrameImpl::OrdinalOfFirstMatchForFrame(WebFrameImpl* frame) const {
  int ordinal = 0;
  WebFrameImpl* main_frame_impl = GetWebViewImpl()->main_frame();
  // Iterate from the main frame up to (but not including) |frame| and
  // add up the number of matches found so far.
  for (WebFrameImpl* it = main_frame_impl;
       it != frame;
       it = static_cast<WebFrameImpl*>(it->traverseNext(true))) {
    if (it->last_match_count_ > 0)
      ordinal += it->last_match_count_;
  }

  return ordinal;
}

bool WebFrameImpl::ShouldScopeMatches(const string16& search_text) {
  // Don't scope if we can't find a frame or if the frame is not visible.
  // The user may have closed the tab/application, so abort.
  if (!frame() || !hasVisibleContent())
    return false;

  ASSERT(frame()->document() && frame()->view());

  // If the frame completed the scoping operation and found 0 matches the last
  // time it was searched, then we don't have to search it again if the user is
  // just adding to the search string or sending the same search string again.
  if (scoping_complete_ &&
      !last_search_string_.empty() && last_match_count_ == 0) {
    // Check to see if the search string prefixes match.
    string16 previous_search_prefix =
        search_text.substr(0, last_search_string_.length());

    if (previous_search_prefix == last_search_string_) {
      return false;  // Don't search this frame, it will be fruitless.
    }
  }

  return true;
}

void WebFrameImpl::ScopeStringMatchesSoon(
    int identifier, const WebString& search_text,
    const WebFindOptions& options, bool reset) {
  deferred_scoping_work_.append(new DeferredScopeStringMatches(
      this, identifier, search_text, options, reset));
}

void WebFrameImpl::CallScopeStringMatches(
    DeferredScopeStringMatches* caller, int identifier,
    const WebString& search_text, const WebFindOptions& options, bool reset) {
  deferred_scoping_work_.remove(deferred_scoping_work_.find(caller));

  scopeStringMatches(identifier, search_text, options, reset);

  // This needs to happen last since search_text is passed by reference.
  delete caller;
}

void WebFrameImpl::InvalidateIfNecessary() {
  if (last_match_count_ > next_invalidate_after_) {
    // TODO(finnur): (http://b/1088165) Optimize the drawing of the
    // tickmarks and remove this. This calculation sets a milestone for when
    // next to invalidate the scrollbar and the content area. We do this so that
    // we don't spend too much time drawing the scrollbar over and over again.
    // Basically, up until the first 500 matches there is no throttle. After the
    // first 500 matches, we set set the milestone further and further out (750,
    // 1125, 1688, 2K, 3K).
    static const int start_slowing_down_after = 500;
    static const int slowdown = 750;
    int i = (last_match_count_ / start_slowing_down_after);
    next_invalidate_after_ += i * slowdown;

    InvalidateArea(INVALIDATE_SCROLLBAR);
  }
}

void WebFrameImpl::ClearPasswordListeners() {
  for (PasswordListenerMap::iterator iter = password_listeners_.begin();
       iter != password_listeners_.end(); ++iter) {
    delete iter->second;
  }
  password_listeners_.clear();
}

void WebFrameImpl::LoadJavaScriptURL(const KURL& url) {
  // This is copied from FrameLoader::executeIfJavaScriptURL.  Unfortunately,
  // we cannot just use that method since it is private, and it also doesn't
  // quite behave as we require it to for bookmarklets.  The key difference is
  // that we need to suppress loading the string result from evaluating the JS
  // URL if executing the JS URL resulted in a location change.  We also allow
  // a JS URL to be loaded even if scripts on the page are otherwise disabled.

  if (!frame_->document() || !frame_->page())
    return;

  String script =
      decodeURLEscapeSequences(url.string().substring(strlen("javascript:")));
  ScriptValue result = frame_->script()->executeScript(script, true);

  String script_result;
  if (!result.getString(script_result))
    return;

  SecurityOrigin* security_origin = frame_->document()->securityOrigin();

  if (!frame_->redirectScheduler()->locationChangePending()) {
    frame_->loader()->stopAllLoaders();
    frame_->loader()->begin(frame_->loader()->url(), true, security_origin);
    frame_->loader()->write(script_result);
    frame_->loader()->end();
  }
}
