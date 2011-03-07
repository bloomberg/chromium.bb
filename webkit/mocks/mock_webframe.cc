// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/mocks/mock_webframe.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"

namespace webkit_glue {

MockWebFrame::MockWebFrame() {}

MockWebFrame::~MockWebFrame() {}

WebString MockWebFrame::name() const {
  return WebString();
}

void MockWebFrame::setName(const WebString&) {}

long long MockWebFrame::identifier() const {
  return 0;
}

WebURL MockWebFrame::url() const {
  return WebURL();
}

WebURL MockWebFrame::favIconURL() const {
  return WebURL();
}

WebURL MockWebFrame::openSearchDescriptionURL() const {
  return WebURL();
}

WebString MockWebFrame::encoding() const {
  return WebString();
}

void MockWebFrame::setCanHaveScrollbars(bool) {}

WebSize MockWebFrame::scrollOffset() const {
  return WebSize(0,0);
}

void MockWebFrame::setScrollOffset(const WebSize&) {}

WebSize MockWebFrame::contentsSize() const {
  return WebSize();
}

int MockWebFrame::contentsPreferredWidth() const {
  return 0;
}

int MockWebFrame::documentElementScrollHeight() const {
  return 0;
}

bool MockWebFrame::hasVisibleContent() const {
  return false;
}

WebView* MockWebFrame::view() const {
  return NULL;
}

WebFrame* MockWebFrame::opener() const {
  return NULL;
}

void MockWebFrame::clearOpener() {
}

WebFrame* MockWebFrame::parent() const {
  return NULL;
}

WebFrame* MockWebFrame::top() const {
  return NULL;
}

WebFrame* MockWebFrame::firstChild() const {
  return NULL;
}

WebFrame* MockWebFrame::lastChild() const {
  return NULL;
}

WebFrame* MockWebFrame::nextSibling() const {
  return NULL;
}

WebFrame* MockWebFrame::previousSibling() const {
  return NULL;
}

WebFrame* MockWebFrame::traverseNext(bool wrap) const {
  return NULL;
}

WebFrame* MockWebFrame::traversePrevious(bool wrap) const {
  return NULL;
}

WebFrame* MockWebFrame::findChildByName(const WebString& name) const {
  return NULL;
}

WebFrame* MockWebFrame::findChildByExpression(const WebString& xpath) const {
  return NULL;
}

WebDocument MockWebFrame::document() const {
  return WebDocument();
}

void MockWebFrame::forms(WebVector<WebFormElement>&) const {}

WebAnimationController* MockWebFrame::animationController() {
  return NULL;
}

WebPerformance MockWebFrame::performance() const {
  return WebPerformance();
}

WebSecurityOrigin MockWebFrame::securityOrigin() const {
  return WebSecurityOrigin();
}

void MockWebFrame::grantUniversalAccess() {}

NPObject* MockWebFrame::windowObject() const {
  return NULL;
}

void MockWebFrame::bindToWindowObject(const WebString& name, NPObject*) {}

void MockWebFrame::executeScript(const WebScriptSource&) {}

void MockWebFrame::executeScriptInIsolatedWorld(
    int worldId, const WebScriptSource* sources, unsigned numSources,
    int extensionGroup) {}

void MockWebFrame::addMessageToConsole(const WebConsoleMessage&) {}

void MockWebFrame::collectGarbage() {}

#if WEBKIT_USING_V8
v8::Handle<v8::Value> MockWebFrame::executeScriptAndReturnValue(
    const WebScriptSource&) {
  return v8::Handle<v8::Value>();
}

v8::Local<v8::Context> MockWebFrame::mainWorldScriptContext() const {
  return v8::Local<v8::Context>();
}

v8::Handle<v8::Value> MockWebFrame::createFileSystem(
    int type, const WebString& name, const WebString& path) {
  return v8::Handle<v8::Value>();
}
#endif


bool MockWebFrame::insertStyleText(const WebString& styleText,
                                   const WebString& elementId) {
  return false;
}

void MockWebFrame::reload(bool ignoreCache) {}

void MockWebFrame::loadRequest(const WebURLRequest&) {}

void MockWebFrame::loadHistoryItem(const WebHistoryItem&) {}

void MockWebFrame::loadData(const WebData& data,
                            const WebString& mimeType,
                            const WebString& textEncoding,
                            const WebURL& baseURL,
                            const WebURL& unreachableURL,
                            bool replace) {}

void MockWebFrame::loadHTMLString(const WebData& html,
                                  const WebURL& baseURL,
                                  const WebURL& unreachableURL,
                                  bool replace) {}

bool MockWebFrame::isLoading() const {
  return false;
}

void MockWebFrame::stopLoading() {}

WebKit::WebDataSource* MockWebFrame::provisionalDataSource() const {
  return NULL;
}

WebKit::WebDataSource* MockWebFrame::dataSource() const {
  return NULL;
}

WebHistoryItem MockWebFrame::previousHistoryItem() const {
  return WebHistoryItem();
}

WebHistoryItem MockWebFrame::currentHistoryItem() const {
  return WebHistoryItem();
}

void MockWebFrame::enableViewSourceMode(bool) {}

bool MockWebFrame::isViewSourceModeEnabled() const {
  return false;
}

WebURLLoader* MockWebFrame::createAssociatedURLLoader() {
  return NULL;
}

void MockWebFrame::commitDocumentData(const char* data, size_t length) {}

unsigned MockWebFrame::unloadListenerCount() const {
  return 0;
}

bool MockWebFrame::isProcessingUserGesture() const {
  return false;
}

bool MockWebFrame::willSuppressOpenerInNewFrame() const {
  return false;
}

void MockWebFrame::replaceSelection(const WebString& text) {}

void MockWebFrame::insertText(const WebString& text) {}

void MockWebFrame::setMarkedText(const WebString& text,
                                 unsigned location,
                                 unsigned length) {}

void MockWebFrame::unmarkText() {}

bool MockWebFrame::hasMarkedText() const {
  return false;
}

WebRange MockWebFrame::markedRange() const {
  return WebRange();
}

bool MockWebFrame::firstRectForCharacterRange(unsigned location,
                                              unsigned length,
                                              WebRect&) const {
  return false;
}

bool MockWebFrame::executeCommand(const WebString&) {
  return false;
}

bool MockWebFrame::executeCommand(const WebString&, const WebString& value) {
  return false;
}

bool MockWebFrame::isCommandEnabled(const WebString&) const {
  return false;
}

void MockWebFrame::enableContinuousSpellChecking(bool) {}

bool MockWebFrame::isContinuousSpellCheckingEnabled() const {
  return false;
}

bool MockWebFrame::hasSelection() const {
  return false;
}

WebRange MockWebFrame::selectionRange() const {
  return WebRange();
}

WebString MockWebFrame::selectionAsText() const {
  return WebString();
}

WebString MockWebFrame::selectionAsMarkup() const {
  return WebString();
}

bool MockWebFrame::selectWordAroundCaret() {
  return false;
}

int MockWebFrame::printBegin(const WebSize& pageSize,
                             const WebNode& constrainToNode,
                             int printerDPI,
                             bool* useBrowserOverlays) {
  return 0;
}

float MockWebFrame::getPrintPageShrink(int page) {
  return 0;
}

float MockWebFrame::printPage(int pageToPrint, WebCanvas*) {
  return 0;
}

void MockWebFrame::printEnd() {}

bool MockWebFrame::isPageBoxVisible(int pageIndex) {
  return false;
}

void MockWebFrame::pageSizeAndMarginsInPixels(int pageIndex,
                                              WebSize& pageSize,
                                              int& marginTop,
                                              int& marginRight,
                                              int& marginBottom,
                                              int& marginLeft) {}

bool MockWebFrame::find(int identifier,
                        const WebString& searchText,
                        const WebFindOptions& options,
                        bool wrapWithinFrame,
                        WebRect* selectionRect) {
  return false;
}

void MockWebFrame::stopFinding(bool clearSelection) {}

void MockWebFrame::scopeStringMatches(int identifier,
                                      const WebString& searchText,
                                      const WebFindOptions& options,
                                      bool reset) {}

void MockWebFrame::cancelPendingScopingEffort() {}

void MockWebFrame::increaseMatchCount(int count, int identifier) {}

void MockWebFrame::resetMatchCount() {}

bool MockWebFrame::registerPasswordListener(
    WebInputElement,
    WebPasswordAutocompleteListener*) {
  return false;
}

void MockWebFrame::notifiyPasswordListenerOfAutocomplete(
    const WebInputElement&) {}

WebString MockWebFrame::contentAsText(size_t maxChars) const {
  return WebString();
}

WebString MockWebFrame::contentAsMarkup() const {
  return WebString();
}

WebString MockWebFrame::renderTreeAsText() const {
  return WebString();
}

WebString MockWebFrame::counterValueForElementById(const WebString& id) const {
  return WebString();
}

WebString MockWebFrame::markerTextForListItem(const WebElement&) const {
  return WebString();
}

int MockWebFrame::pageNumberForElementById(const WebString& id,
                                           float pageWidthInPixels,
                                           float pageHeightInPixels) const {
  return 0;
}

WebRect MockWebFrame::selectionBoundsRect() const {
  return WebRect();
}

bool MockWebFrame::selectionStartHasSpellingMarkerFor(int from,
                                                      int length) const {
  return false;
}

bool MockWebFrame::pauseSVGAnimation(const WebString& animationId,
                                     double time,
                                     const WebString& elementId) {
  return false;
}

WebString MockWebFrame::layerTreeAsText() const {
  return WebString();
}

}  // namespace webkit_glue
