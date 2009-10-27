// Copyright (c) 2007-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "AXObjectCache.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Cursor.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DragController.h"
#include "DragData.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FontDescription.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HitTestResult.h"
#include "Image.h"
#include "InspectorController.h"
#include "IntRect.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageGroup.h"
#include "Pasteboard.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginInfoStore.h"
#include "PopupMenuChromium.h"
#include "PopupMenuClient.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "SecurityOrigin.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TypingCommand.h"
#if PLATFORM(WIN_OS)
#include "KeyboardCodesWin.h"
#include "RenderThemeChromiumWin.h"
#else
#include "KeyboardCodesPosix.h"
#include "RenderTheme.h"
#endif
#undef LOG

#include "webkit/api/public/WebAccessibilityObject.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebMediaPlayerAction.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/api/public/WebViewClient.h"
#include "webkit/api/src/DOMUtilitiesPrivate.h"
#include "webkit/api/src/WebInputEventConversion.h"
#include "webkit/api/src/WebPopupMenuImpl.h"
#include "webkit/api/src/WebSettingsImpl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview_impl.h"

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;

using WebKit::ChromeClientImpl;
using WebKit::PlatformKeyboardEventBuilder;
using WebKit::PlatformMouseEventBuilder;
using WebKit::PlatformWheelEventBuilder;
using WebKit::WebAccessibilityObject;
using WebKit::WebCanvas;
using WebKit::WebCompositionCommand;
using WebKit::WebCompositionCommandConfirm;
using WebKit::WebCompositionCommandDiscard;
using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsAgentClient;
using WebKit::WebDragData;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationsMask;
using WebKit::WebFrame;
using WebKit::WebFrameClient;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMediaPlayerAction;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebNavigationPolicy;
using WebKit::WebNode;
using WebKit::WebPoint;
using WebKit::WebPopupMenuImpl;
using WebKit::WebRect;
using WebKit::WebSettings;
using WebKit::WebSettingsImpl;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebTextDirectionDefault;
using WebKit::WebTextDirectionLeftToRight;
using WebKit::WebTextDirectionRightToLeft;
using WebKit::WebURL;
using WebKit::WebVector;
using WebKit::WebViewClient;

using webkit_glue::AccessibilityObjectToWebAccessibilityObject;

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
static const double kTextSizeMultiplierRatio = 1.2;
static const double kMinTextSizeMultiplier = 0.5;
static const double kMaxTextSizeMultiplier = 3.0;

// The group name identifies a namespace of pages.  Page group is used on OSX
// for some programs that use HTML views to display things that don't seem like
// web pages to the user (so shouldn't have visited link coloring).  We only use
// one page group.
// FIXME: This needs to go into the WebKit API implementation and be hidden
//         from the API's users.
const char* pageGroupName = "default";

// Ensure that the WebKit::WebDragOperation enum values stay in sync with
// the original WebCore::DragOperation constants.
#define COMPILE_ASSERT_MATCHING_ENUM(webcore_name) \
   COMPILE_ASSERT(int(WebCore::webcore_name) == int(WebKit::Web##webcore_name),\
                  dummy##webcore_name)
COMPILE_ASSERT_MATCHING_ENUM(DragOperationNone);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationCopy);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationLink);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationGeneric);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationPrivate);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationMove);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationDelete);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationEvery);

// AutocompletePopupMenuClient
class AutocompletePopupMenuClient : public WebCore::PopupMenuClient {
 public:
  AutocompletePopupMenuClient(WebViewImpl* webview) : text_field_(NULL),
                                                      selected_index_(0),
                                                      webview_(webview) {
  }

  void Init(WebCore::HTMLInputElement* text_field,
            const WebVector<WebString>& suggestions,
            int default_suggestion_index) {
    ASSERT(default_suggestion_index < static_cast<int>(suggestions.size()));
    text_field_ = text_field;
    selected_index_ = default_suggestion_index;
    SetSuggestions(suggestions);

    FontDescription font_description;
    webview_->theme()->systemFont(CSSValueWebkitControl, font_description);
    // Use a smaller font size to match IE/Firefox.
    // TODO(jcampan): http://crbug.com/7376 use the system size instead of a
    //                fixed font size value.
    font_description.setComputedSize(12.0);
    Font font(font_description, 0, 0);
    font.update(text_field->document()->styleSelector()->fontSelector());
    // The direction of text in popup menu is set the same as the direction of
    // the input element: text_field.
    style_.set(new PopupMenuStyle(Color::black, Color::white, font, true,
        Length(WebCore::Fixed), text_field->renderer()->style()->direction()));
  }

  virtual ~AutocompletePopupMenuClient() {
  }

  // WebCore::PopupMenuClient implementation.
  virtual void valueChanged(unsigned listIndex, bool fireEvents = true) {
    text_field_->setValue(suggestions_[listIndex]);
    EditorClientImpl* editor =
        static_cast<EditorClientImpl*>(webview_->page()->editorClient());
    ASSERT(editor);
    editor->OnAutofillSuggestionAccepted(
        static_cast<WebCore::HTMLInputElement*>(text_field_.get()));
  }

  virtual WebCore::String itemText(unsigned list_index) const {
    return suggestions_[list_index];
  }

  virtual WebCore::String itemToolTip(unsigned last_index) const {
    notImplemented();
    return WebCore::String();
  }

  virtual bool itemIsEnabled(unsigned listIndex) const {
    return true;
  }

  virtual PopupMenuStyle itemStyle(unsigned listIndex) const {
    return *style_;
  }

  virtual PopupMenuStyle menuStyle() const {
    return *style_;
  }

  virtual int clientInsetLeft() const {
    return 0;
  }
  virtual int clientInsetRight() const {
    return 0;
  }
  virtual int clientPaddingLeft() const {
    // Bug http://crbug.com/7708 seems to indicate the style can be NULL.
    WebCore::RenderStyle* style = GetTextFieldStyle();
    return style ? webview_->theme()->popupInternalPaddingLeft(style) : 0;
  }
  virtual int clientPaddingRight() const {
    // Bug http://crbug.com/7708 seems to indicate the style can be NULL.
    WebCore::RenderStyle* style = GetTextFieldStyle();
    return style ? webview_->theme()->popupInternalPaddingRight(style) : 0;
  }
  virtual int listSize() const {
    return suggestions_.size();
  }
  virtual int selectedIndex() const {
    return selected_index_;
  }
  virtual void popupDidHide() {
    webview_->AutoCompletePopupDidHide();
  }
  virtual bool itemIsSeparator(unsigned listIndex) const {
    return false;
  }
  virtual bool itemIsLabel(unsigned listIndex) const {
    return false;
  }
  virtual bool itemIsSelected(unsigned listIndex) const {
    return false;
  }
  virtual bool shouldPopOver() const {
    return false;
  }
  virtual bool valueShouldChangeOnHotTrack() const {
    return false;
  }

  virtual void setTextFromItem(unsigned listIndex) {
    text_field_->setValue(suggestions_[listIndex]);
  }

  virtual FontSelector* fontSelector() const {
    return text_field_->document()->styleSelector()->fontSelector();
  }

  virtual HostWindow* hostWindow() const {
    return text_field_->document()->view()->hostWindow();
  }

  virtual PassRefPtr<Scrollbar> createScrollbar(
      ScrollbarClient* client,
      ScrollbarOrientation orientation,
      ScrollbarControlSize size) {
    RefPtr<Scrollbar> widget = Scrollbar::createNativeScrollbar(client,
                                                                orientation,
                                                                size);
    return widget.release();
  }

  // AutocompletePopupMenuClient specific methods:
  void SetSuggestions(const WebVector<WebString>& suggestions) {
    suggestions_.clear();
    for (size_t i = 0; i < suggestions.size(); ++i)
      suggestions_.append(webkit_glue::WebStringToString(suggestions[i]));
    // Try to preserve selection if possible.
    if (selected_index_ >= static_cast<int>(suggestions.size()))
      selected_index_ = -1;
  }

  void RemoveItemAtIndex(int index) {
    ASSERT(index >= 0 && index < static_cast<int>(suggestions_.size()));
    suggestions_.remove(index);
  }

  WebCore::HTMLInputElement* text_field() const {
    return text_field_.get();
  }

  WebCore::RenderStyle* GetTextFieldStyle() const {
    WebCore::RenderStyle* style = text_field_->computedStyle();
    if (!style) {
      // It seems we can only have an NULL style in a TextField if the node is
      // dettached, in which case we the popup shoud not be showing.  Please
      // report this in http://crbug.com/7708 and include the page you were
      // visiting.
      ASSERT_NOT_REACHED();
    }
    return style;
  }

 private:
  RefPtr<WebCore::HTMLInputElement> text_field_;
  Vector<WebCore::String> suggestions_;
  int selected_index_;
  WebViewImpl* webview_;
  OwnPtr<PopupMenuStyle> style_;
};

// Note that focusOnShow is false so that the autocomplete popup is shown not
// activated.  We need the page to still have focus so the user can keep typing
// while the popup is showing.
static const WebCore::PopupContainerSettings kAutocompletePopupSettings = {
  false,  // focusOnShow
  false,  // setTextOnIndexChange
  false,  // acceptOnAbandon
  true,   // loopSelectionNavigation
  true,   // restrictWidthOfListBox. Same as other browser (Fx, IE, and safari)
  // For autocomplete, we use the direction of the input field as the direction
  // of the popup items. The main reason is to keep the display of items in
  // drop-down the same as the items in the input field.
  WebCore::PopupContainerSettings::DOMElementDirection,
};

// WebView ----------------------------------------------------------------

namespace WebKit {

// static
WebView* WebView::create(WebViewClient* client) {
  return new WebViewImpl(client);
}

// static
void WebView::updateVisitedLinkState(unsigned long long link_hash) {
  WebCore::Page::visitedStateChanged(
      WebCore::PageGroup::pageGroup(pageGroupName), link_hash);
}

// static
void WebView::resetVisitedLinkState() {
  WebCore::Page::allVisitedStateChanged(
      WebCore::PageGroup::pageGroup(pageGroupName));
}

}  // namespace WebKit

void WebViewImpl::initializeMainFrame(WebFrameClient* frame_client) {
  // NOTE: The WebFrameImpl takes a reference to itself within InitMainFrame
  // and releases that reference once the corresponding Frame is destroyed.
  RefPtr<WebFrameImpl> main_frame = WebFrameImpl::create(frame_client);

  main_frame->InitMainFrame(this);

  if (client_) {
    WebDevToolsAgentClient* tools_client = client_->devToolsAgentClient();
    if (tools_client)
      devtools_agent_.set(new WebDevToolsAgentImpl(this, tools_client));
  }

  // Restrict the access to the local file system
  // (see WebView.mm WebView::_commonInitializationWithFrameName).
  SecurityOrigin::setLocalLoadPolicy(
      SecurityOrigin::AllowLocalLoadsForLocalOnly);
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : client_(client),
      ALLOW_THIS_IN_INITIALIZER_LIST(back_forward_list_client_impl_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(chrome_client_impl_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_menu_client_impl_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(drag_client_impl_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(editor_client_impl_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(inspector_client_impl_(this)),
      observed_new_navigation_(false),
#ifndef NDEBUG
      new_navigation_loader_(NULL),
#endif
      zoom_level_(0),
      context_menu_allowed_(false),
      doing_drag_and_drop_(false),
      ignore_input_events_(false),
      suppress_next_keypress_event_(false),
      initial_navigation_policy_(WebKit::WebNavigationPolicyIgnore),
      ime_accept_events_(true),
      drag_target_dispatch_(false),
      drag_identity_(0),
      drop_effect_(DROP_EFFECT_DEFAULT),
      operations_allowed_(WebKit::WebDragOperationNone),
      drag_operation_(WebKit::WebDragOperationNone),
      autocomplete_popup_showing_(false),
      is_transparent_(false),
      tabs_to_links_(false) {
  // WebKit/win/WebView.cpp does the same thing, except they call the
  // KJS specific wrapper around this method. We need to have threading
  // initialized because CollatorICU requires it.
  WTF::initializeThreading();

  // set to impossible point so we always get the first mouse pos
  last_mouse_position_ = WebPoint(-1, -1);

  // the page will take ownership of the various clients
  page_.set(new Page(&chrome_client_impl_,
                     &context_menu_client_impl_,
                     &editor_client_impl_,
                     &drag_client_impl_,
                     &inspector_client_impl_,
                     NULL));

  page_->backForwardList()->setClient(&back_forward_list_client_impl_);
  page_->setGroupName(pageGroupName);
}

WebViewImpl::~WebViewImpl() {
  ASSERT(page_ == NULL);
}

RenderTheme* WebViewImpl::theme() const {
  return page_.get() ? page_->theme() : RenderTheme::defaultTheme().get();
}

bool WebViewImpl::tabKeyCyclesThroughElements() const {
  ASSERT(page_.get());
  return page_->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value) {
  if (page_ != NULL) {
    page_->setTabKeyCyclesThroughElements(value);
  }
}

void WebViewImpl::MouseMove(const WebMouseEvent& event) {
  if (!main_frame() || !main_frame()->frameview())
    return;

  last_mouse_position_ = WebPoint(event.x, event.y);

  // We call mouseMoved here instead of handleMouseMovedEvent because we need
  // our ChromeClientImpl to receive changes to the mouse position and
  // tooltip text, and mouseMoved handles all of that.
  main_frame()->frame()->eventHandler()->mouseMoved(
      PlatformMouseEventBuilder(main_frame()->frameview(), event));
}

void WebViewImpl::MouseLeave(const WebMouseEvent& event) {
  // This event gets sent as the main frame is closing.  In that case, just
  // ignore it.
  if (!main_frame() || !main_frame()->frameview())
    return;

  client_->setMouseOverURL(WebURL());

  main_frame()->frame()->eventHandler()->handleMouseMoveEvent(
      PlatformMouseEventBuilder(main_frame()->frameview(), event));
}

void WebViewImpl::MouseDown(const WebMouseEvent& event) {
  if (!main_frame() || !main_frame()->frameview())
    return;

  last_mouse_down_point_ = WebPoint(event.x, event.y);

  // If a text field that has focus is clicked again, we should display the
  // autocomplete popup.
  RefPtr<Node> clicked_node;
  if (event.button == WebMouseEvent::ButtonLeft) {
    RefPtr<Node> focused_node = GetFocusedNode();
    if (focused_node.get() &&
        WebKit::toHTMLInputElement(focused_node.get())) {
      IntPoint point(event.x, event.y);
      point = page_->mainFrame()->view()->windowToContents(point);
      HitTestResult result(point);
      result = page_->mainFrame()->eventHandler()->hitTestResultAtPoint(point,
                                                                        false);
      if (result.innerNonSharedNode() == focused_node) {
        // Already focused text field was clicked, let's remember this.  If
        // focus has not changed after the mouse event is processed, we'll
        // trigger the autocomplete.
        clicked_node = focused_node;
      }
    }
  }

  main_frame()->frame()->eventHandler()->handleMousePressEvent(
      PlatformMouseEventBuilder(main_frame()->frameview(), event));

  if (clicked_node.get() && clicked_node == GetFocusedNode()) {
    // Focus has not changed, show the autocomplete popup.
    static_cast<EditorClientImpl*>(page_->editorClient())->
        ShowFormAutofillForNode(clicked_node.get());
  }

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  // On Windows, we handle it on mouse up, not down.
#if PLATFORM(DARWIN)
  if (event.button == WebMouseEvent::ButtonRight ||
      (event.button == WebMouseEvent::ButtonLeft &&
       event.modifiers & WebMouseEvent::ControlKey)) {
    MouseContextMenu(event);
  }
#elif PLATFORM(LINUX)
  if (event.button == WebMouseEvent::ButtonRight)
    MouseContextMenu(event);
#endif
}

void WebViewImpl::MouseContextMenu(const WebMouseEvent& event) {
  if (!main_frame() || !main_frame()->frameview())
    return;

  page_->contextMenuController()->clearContextMenu();

  PlatformMouseEventBuilder pme(main_frame()->frameview(), event);

  // Find the right target frame. See issue 1186900.
  HitTestResult result = HitTestResultForWindowPos(pme.pos());
  Frame* target_frame;
  if (result.innerNonSharedNode())
    target_frame = result.innerNonSharedNode()->document()->frame();
  else
    target_frame = page_->focusController()->focusedOrMainFrame();

#if PLATFORM(WIN_OS)
  target_frame->view()->setCursor(pointerCursor());
#endif

  context_menu_allowed_ = true;
  target_frame->eventHandler()->sendContextMenuEvent(pme);
  context_menu_allowed_ = false;
  // Actually showing the context menu is handled by the ContextMenuClient
  // implementation...
}

void WebViewImpl::MouseUp(const WebMouseEvent& event) {
  if (!main_frame() || !main_frame()->frameview())
    return;

#if PLATFORM(LINUX)
  // If the event was a middle click, attempt to copy text into the focused
  // frame. We execute this before we let the page have a go at the event
  // because the page may change what is focused during in its event handler.
  //
  // This code is in the mouse up handler. There is some debate about putting
  // this here, as opposed to the mouse down handler.
  //   xterm: pastes on up.
  //   GTK: pastes on down.
  //   Firefox: pastes on up.
  //   Midori: couldn't paste at all with 0.1.2
  //
  // There is something of a webcompat angle to this well, as highlighted by
  // crbug.com/14608. Pages can clear text boxes 'onclick' and, if we paste on
  // down then the text is pasted just before the onclick handler runs and
  // clears the text box. So it's important this happens after the
  // handleMouseReleaseEvent() earlier in this function
  if (event.button == WebMouseEvent::ButtonMiddle) {
    Frame* focused = GetFocusedWebCoreFrame();
    IntPoint click_point(last_mouse_down_point_.x, last_mouse_down_point_.y);
    click_point = page_->mainFrame()->view()->windowToContents(click_point);
    HitTestResult hit_test_result =
        focused->eventHandler()->hitTestResultAtPoint(click_point, false, false,
                                                      ShouldHitTestScrollbars);
    // We don't want to send a paste when middle clicking a scroll bar or a
    // link (which will navigate later in the code).
    if (!hit_test_result.scrollbar() && !hit_test_result.isLiveLink() &&
        focused) {
      Editor* editor = focused->editor();
      Pasteboard* pasteboard = Pasteboard::generalPasteboard();
      bool oldSelectionMode = pasteboard->isSelectionMode();
      pasteboard->setSelectionMode(true);
      editor->command(AtomicString("Paste")).execute();
      pasteboard->setSelectionMode(oldSelectionMode);
    }
  }
#endif

  mouseCaptureLost();
  main_frame()->frame()->eventHandler()->handleMouseReleaseEvent(
      PlatformMouseEventBuilder(main_frame()->frameview(), event));

#if PLATFORM(WIN_OS)
  // Dispatch the contextmenu event regardless of if the click was swallowed.
  // On Mac/Linux, we handle it on mouse down, not up.
  if (event.button == WebMouseEvent::ButtonRight)
    MouseContextMenu(event);
#endif
}

void WebViewImpl::MouseWheel(const WebMouseWheelEvent& event) {
  PlatformWheelEventBuilder platform_event(main_frame()->frameview(), event);
  main_frame()->frame()->eventHandler()->handleWheelEvent(platform_event);
}

bool WebViewImpl::KeyEvent(const WebKeyboardEvent& event) {
  ASSERT((event.type == WebInputEvent::RawKeyDown) ||
         (event.type == WebInputEvent::KeyDown) ||
         (event.type == WebInputEvent::KeyUp));

  // Please refer to the comments explaining the suppress_next_keypress_event_
  // member.
  // The suppress_next_keypress_event_ is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  suppress_next_keypress_event_ = false;

  // Give autocomplete a chance to consume the key events it is interested in.
  if (AutocompleteHandleKeyEvent(event))
    return true;

  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  EventHandler* handler = frame->eventHandler();
  if (!handler)
    return KeyEventDefault(event);

#if PLATFORM(WIN_OS) || PLATFORM(LINUX)
  if (((event.modifiers == 0) && (event.windowsKeyCode == VKEY_APPS)) ||
      ((event.modifiers == WebInputEvent::ShiftKey) &&
       (event.windowsKeyCode == VKEY_F10))) {
    SendContextMenuEvent(event);
    return true;
  }
#endif

  // It's not clear if we should continue after detecting a capslock keypress.
  // I'll err on the side of continuing, which is the pre-existing behaviour.
  if (event.windowsKeyCode == VKEY_CAPITAL)
    handler->capsLockStateMayHaveChanged();

  PlatformKeyboardEventBuilder evt(event);

  if (handler->keyEvent(evt)) {
    if (WebInputEvent::RawKeyDown == event.type && !evt.isSystemKey())
      suppress_next_keypress_event_ = true;
    return true;
  }

  return KeyEventDefault(event);
}

bool WebViewImpl::AutocompleteHandleKeyEvent(const WebKeyboardEvent& event) {
  if (!autocomplete_popup_showing_ ||
      // Home and End should be left to the text field to process.
      event.windowsKeyCode == VKEY_HOME ||
      event.windowsKeyCode == VKEY_END) {
    return false;
  }

  // Pressing delete triggers the removal of the selected suggestion from the
  // DB.
  if (event.windowsKeyCode == VKEY_DELETE &&
      autocomplete_popup_->selectedIndex() != -1) {
    Node* node = GetFocusedNode();
    if (!node || (node->nodeType() != WebCore::Node::ELEMENT_NODE)) {
      ASSERT_NOT_REACHED();
      return false;
    }
    WebCore::Element* element = static_cast<WebCore::Element*>(node);
    if (!element->hasLocalName(WebCore::HTMLNames::inputTag)) {
      ASSERT_NOT_REACHED();
      return false;
    }

    int selected_index = autocomplete_popup_->selectedIndex();
    WebCore::HTMLInputElement* input_element =
        static_cast<WebCore::HTMLInputElement*>(element);
    const WebString& name = webkit_glue::StringToWebString(
        input_element->name());
    const WebString& value = webkit_glue::StringToWebString(
        autocomplete_popup_client_->itemText(selected_index));
    client_->removeAutofillSuggestions(name, value);
    // Update the entries in the currently showing popup to reflect the
    // deletion.
    autocomplete_popup_client_->RemoveItemAtIndex(selected_index);
    RefreshAutofillPopup();
    return false;
  }

  if (!autocomplete_popup_->isInterestedInEventForKey(event.windowsKeyCode))
    return false;

  if (autocomplete_popup_->handleKeyEvent(
          PlatformKeyboardEventBuilder(event))) {
      // We need to ignore the next Char event after this otherwise pressing
      // enter when selecting an item in the menu will go to the page.
      if (WebInputEvent::RawKeyDown == event.type)
        suppress_next_keypress_event_ = true;
      return true;
    }

  return false;
}

bool WebViewImpl::CharEvent(const WebKeyboardEvent& event) {
  ASSERT(event.type == WebInputEvent::Char);

  // Please refer to the comments explaining the suppress_next_keypress_event_
  // member.
  // The suppress_next_keypress_event_ is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as it only applies
  // to the current keyPress event.
  if (suppress_next_keypress_event_) {
    suppress_next_keypress_event_ = false;
    return true;
  }

  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  EventHandler* handler = frame->eventHandler();
  if (!handler)
    return KeyEventDefault(event);

  PlatformKeyboardEventBuilder evt(event);
  if (!evt.isCharacterKey())
    return true;

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (evt.isSystemKey())
    return handler->handleAccessKey(evt);

  if (!handler->keyEvent(evt))
    return KeyEventDefault(event);

  return true;
}

/*
* The WebViewImpl::SendContextMenuEvent function is based on the Webkit
* function
* bool WebView::handleContextMenuEvent(WPARAM wParam, LPARAM lParam) in
* webkit\webkit\win\WebView.cpp. The only significant change in this
* function is the code to convert from a Keyboard event to the Right
* Mouse button up event.
*
* This function is an ugly copy/paste and should be cleaned up when the
* WebKitWin version is cleaned: https://bugs.webkit.org/show_bug.cgi?id=20438
*/
#if PLATFORM(WIN_OS) || PLATFORM(LINUX)
// TODO(pinkerton): implement on non-windows
bool WebViewImpl::SendContextMenuEvent(const WebKeyboardEvent& event) {
  static const int kContextMenuMargin = 1;
  Frame* main_frame = page()->mainFrame();
  FrameView* view = main_frame->view();
  if (!view)
    return false;

  IntPoint coords(-1, -1);
#if PLATFORM(WIN_OS)
  int right_aligned = ::GetSystemMetrics(SM_MENUDROPALIGNMENT);
#else
  int right_aligned = 0;
#endif
  IntPoint location;

  // The context menu event was generated from the keyboard, so show the
  // context menu by the current selection.
  Position start = main_frame->selection()->selection().start();
  Position end = main_frame->selection()->selection().end();

  if (!start.node() || !end.node()) {
    location =
        IntPoint(right_aligned ? view->contentsWidth() - kContextMenuMargin
                     : kContextMenuMargin, kContextMenuMargin);
  } else {
    RenderObject* renderer = start.node()->renderer();
    if (!renderer)
      return false;

    RefPtr<Range> selection = main_frame->selection()->toNormalizedRange();
    IntRect first_rect = main_frame->firstRectForRange(selection.get());

    int x = right_aligned ? first_rect.right() : first_rect.x();
    location = IntPoint(x, first_rect.bottom());
  }

  location = view->contentsToWindow(location);
  // FIXME: The IntSize(0, -1) is a hack to get the hit-testing to result in
  // the selected element. Ideally we'd have the position of a context menu
  // event be separate from its target node.
  coords = location + IntSize(0, -1);

  // The contextMenuController() holds onto the last context menu that was
  // popped up on the page until a new one is created. We need to clear
  // this menu before propagating the event through the DOM so that we can
  // detect if we create a new menu for this event, since we won't create
  // a new menu if the DOM swallows the event and the defaultEventHandler does
  // not run.
  page()->contextMenuController()->clearContextMenu();

  Frame* focused_frame = page()->focusController()->focusedOrMainFrame();
  focused_frame->view()->setCursor(pointerCursor());
  WebMouseEvent mouse_event;
  mouse_event.button = WebMouseEvent::ButtonRight;
  mouse_event.x = coords.x();
  mouse_event.y = coords.y();
  mouse_event.type = WebInputEvent::MouseUp;

  PlatformMouseEventBuilder platform_event(view, mouse_event);

  context_menu_allowed_ = true;
  bool handled =
      focused_frame->eventHandler()->sendContextMenuEvent(platform_event);
  context_menu_allowed_ = false;
  return handled;
}
#endif

bool WebViewImpl::KeyEventDefault(const WebKeyboardEvent& event) {
  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  switch (event.type) {
    case WebInputEvent::Char: {
      if (event.windowsKeyCode == VKEY_SPACE) {
        int key_code = ((event.modifiers & WebInputEvent::ShiftKey) ?
                         VKEY_PRIOR : VKEY_NEXT);
        return ScrollViewWithKeyboard(key_code, event.modifiers);
      }
      break;
    }

    case WebInputEvent::RawKeyDown: {
      if (event.modifiers == WebInputEvent::ControlKey) {
        switch (event.windowsKeyCode) {
          case 'A':
            focusedFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
            return true;
          case VKEY_INSERT:
          case 'C':
            focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
            return true;
          // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
          // key combinations which affect scrolling. Safari is buggy in the
          // sense that it scrolls the page for all Ctrl+scrolling key
          // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
          case VKEY_HOME:
          case VKEY_END:
            break;
          default:
            return false;
        }
      }
      if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey)) {
        return ScrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
      }
      break;
    }

    default:
      break;
  }
  return false;
}

bool WebViewImpl::ScrollViewWithKeyboard(int key_code, int modifiers) {
  ScrollDirection scroll_direction;
  ScrollGranularity scroll_granularity;

  switch (key_code) {
    case VKEY_LEFT:
      scroll_direction = ScrollLeft;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_RIGHT:
      scroll_direction = ScrollRight;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_UP:
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_DOWN:
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_HOME:
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByDocument;
      break;
    case VKEY_END:
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByDocument;
      break;
    case VKEY_PRIOR:  // page up
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByPage;
      break;
    case VKEY_NEXT:  // page down
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByPage;
      break;
    default:
      return false;
  }

  return PropagateScroll(scroll_direction, scroll_granularity);
}

bool WebViewImpl::PropagateScroll(
    WebCore::ScrollDirection scroll_direction,
    WebCore::ScrollGranularity scroll_granularity) {

  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  bool scroll_handled =
      frame->eventHandler()->scrollOverflow(scroll_direction,
                                            scroll_granularity);
  Frame* current_frame = frame;
  while (!scroll_handled && current_frame) {
    scroll_handled = current_frame->view()->scroll(scroll_direction,
                                                   scroll_granularity);
    current_frame = current_frame->tree()->parent();
  }
  return scroll_handled;
}

Frame* WebViewImpl::GetFocusedWebCoreFrame() {
  return page_.get() ? page_->focusController()->focusedOrMainFrame() : NULL;
}

// static
WebViewImpl* WebViewImpl::FromPage(WebCore::Page* page) {
  if (!page)
    return NULL;

  return static_cast<ChromeClientImpl*>(page->chrome()->client())->webview();
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close() {
  RefPtr<WebFrameImpl> main_frame;

  if (page_.get()) {
    // Initiate shutdown for the entire frameset.  This will cause a lot of
    // notifications to be sent.
    if (page_->mainFrame()) {
      main_frame = WebFrameImpl::FromFrame(page_->mainFrame());
      page_->mainFrame()->loader()->frameDetached();
    }
    page_.clear();
  }

  // Should happen after page_.reset().
  if (devtools_agent_.get())
    devtools_agent_.clear();

  // We drop the client after the page has been destroyed to support the
  // WebFrameClient::didDestroyScriptContext method.
  if (main_frame)
    main_frame->drop_client();

  // Reset the delegate to prevent notifications being sent as we're being
  // deleted.
  client_ = NULL;

  deref();  // Balances ref() acquired in WebView::create
}

void WebViewImpl::resize(const WebSize& new_size) {
  if (size_ == new_size)
    return;
  size_ = new_size;

  if (main_frame()->frameview()) {
    main_frame()->frameview()->resize(size_.width, size_.height);
    main_frame()->frame()->eventHandler()->sendResizeEvent();
  }

  if (client_) {
    WebRect damaged_rect(0, 0, size_.width, size_.height);
    client_->didInvalidateRect(damaged_rect);
  }
}

void WebViewImpl::layout() {
  WebFrameImpl* webframe = main_frame();
  if (webframe) {
    // In order for our child HWNDs (NativeWindowWidgets) to update properly,
    // they need to be told that we are updating the screen.  The problem is
    // that the native widgets need to recalculate their clip region and not
    // overlap any of our non-native widgets.  To force the resizing, call
    // setFrameRect().  This will be a quick operation for most frames, but
    // the NativeWindowWidgets will update a proper clipping region.
    FrameView* view = webframe->frameview();
    if (view)
      view->setFrameRect(view->frameRect());

    // setFrameRect may have the side-effect of causing existing page
    // layout to be invalidated, so layout needs to be called last.

    webframe->Layout();
  }
}

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  WebFrameImpl* webframe = main_frame();
  if (webframe)
    webframe->Paint(canvas, rect);
}

// TODO(eseidel): g_current_input_event should be removed once
// ChromeClient:show() can get the current-event information from WebCore.
/* static */
const WebInputEvent* WebViewImpl::g_current_input_event = NULL;

bool WebViewImpl::handleInputEvent(const WebInputEvent& input_event) {
  // If we've started a drag and drop operation, ignore input events until
  // we're done.
  if (doing_drag_and_drop_)
    return true;

  if (ignore_input_events_)
    return true;

  // TODO(eseidel): Remove g_current_input_event.
  // This only exists to allow ChromeClient::show() to know which mouse button
  // triggered a window.open event.
  // Safari must perform a similar hack, ours is in our WebKit glue layer
  // theirs is in the application.  This should go when WebCore can be fixed
  // to pass more event information to ChromeClient::show()
  g_current_input_event = &input_event;

  bool handled = true;

  // TODO(jcampan): WebKit seems to always return false on mouse events
  // processing methods. For now we'll assume it has processed them (as we are
  // only interested in whether keyboard events are processed).
  switch (input_event.type) {
    case WebInputEvent::MouseMove:
      MouseMove(*static_cast<const WebMouseEvent*>(&input_event));
      break;

    case WebInputEvent::MouseLeave:
      MouseLeave(*static_cast<const WebMouseEvent*>(&input_event));
      break;

    case WebInputEvent::MouseWheel:
      MouseWheel(*static_cast<const WebMouseWheelEvent*>(&input_event));
      break;

    case WebInputEvent::MouseDown:
      MouseDown(*static_cast<const WebMouseEvent*>(&input_event));
      break;

    case WebInputEvent::MouseUp:
      MouseUp(*static_cast<const WebMouseEvent*>(&input_event));
      break;

    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      handled = KeyEvent(*static_cast<const WebKeyboardEvent*>(&input_event));
      break;

    case WebInputEvent::Char:
      handled = CharEvent(*static_cast<const WebKeyboardEvent*>(&input_event));
      break;
    default:
      handled = false;
  }

  g_current_input_event = NULL;

  return handled;
}

void WebViewImpl::mouseCaptureLost() {
}

void WebViewImpl::setFocus(bool enable) {
  page_->focusController()->setFocused(enable);
  if (enable) {
    // Note that we don't call setActive() when disabled as this cause extra
    // focus/blur events to be dispatched.
    page_->focusController()->setActive(true);
    ime_accept_events_ = true;
  } else {
    HideAutoCompletePopup();

    // Clear focus on the currently focused frame if any.
    if (!page_.get())
      return;

    Frame* frame = page_->mainFrame();
    if (!frame)
      return;

    RefPtr<Frame> focused_frame = page_->focusController()->focusedFrame();
    if (focused_frame.get()) {
      // Finish an ongoing composition to delete the composition node.
      Editor* editor = focused_frame->editor();
      if (editor && editor->hasComposition())
        editor->confirmComposition();
      ime_accept_events_ = false;
    }
  }
}

bool WebViewImpl::handleCompositionEvent(WebCompositionCommand command,
                                         int cursor_position,
                                         int target_start,
                                         int target_end,
                                         const WebString& ime_string) {
  Frame* focused = GetFocusedWebCoreFrame();
  if (!focused || !ime_accept_events_) {
    return false;
  }
  Editor* editor = focused->editor();
  if (!editor)
    return false;
  if (!editor->canEdit()) {
    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    if (!editor->hasComposition())
      return false;
  }

  // We should verify the parent node of this IME composition node are
  // editable because JavaScript may delete a parent node of the composition
  // node. In this case, WebKit crashes while deleting texts from the parent
  // node, which doesn't exist any longer.
  PassRefPtr<Range> range = editor->compositionRange();
  if (range) {
    const Node* node = range->startPosition().node();
    if (!node || !node->isContentEditable())
      return false;
  }

  if (command == WebCompositionCommandDiscard) {
    // A browser process sent an IPC message which does not contain a valid
    // string, which means an ongoing composition has been canceled.
    // If the ongoing composition has been canceled, replace the ongoing
    // composition string with an empty string and complete it.
    WebCore::String empty_string;
    WTF::Vector<WebCore::CompositionUnderline> empty_underlines;
    editor->setComposition(empty_string, empty_underlines, 0, 0);
  } else {
    // A browser process sent an IPC message which contains a string to be
    // displayed in this Editor object.
    // To display the given string, set the given string to the
    // m_compositionNode member of this Editor object and display it.
    if (target_start < 0)
      target_start = 0;
    if (target_end < 0)
      target_end = static_cast<int>(ime_string.length());
    WebCore::String composition_string(
        webkit_glue::WebStringToString(ime_string));
    // Create custom underlines.
    // To emphasize the selection, the selected region uses a solid black
    // for its underline while other regions uses a pale gray for theirs.
    WTF::Vector<WebCore::CompositionUnderline> underlines(3);
    underlines[0].startOffset = 0;
    underlines[0].endOffset = target_start;
    underlines[0].thick = true;
    underlines[0].color.setRGB(0xd3, 0xd3, 0xd3);
    underlines[1].startOffset = target_start;
    underlines[1].endOffset = target_end;
    underlines[1].thick = true;
    underlines[1].color.setRGB(0x00, 0x00, 0x00);
    underlines[2].startOffset = target_end;
    underlines[2].endOffset = static_cast<int>(ime_string.length());
    underlines[2].thick = true;
    underlines[2].color.setRGB(0xd3, 0xd3, 0xd3);
    // When we use custom underlines, WebKit ("InlineTextBox.cpp" Line 282)
    // prevents from writing a text in between 'selectionStart' and
    // 'selectionEnd' somehow.
    // Therefore, we use the 'cursor_position' for these arguments so that
    // there are not any characters in the above region.
    editor->setComposition(composition_string, underlines,
                           cursor_position, cursor_position);
    // The given string is a result string, which means the ongoing
    // composition has been completed. I have to call the
    // Editor::confirmCompletion() and complete this composition.
    if (command == WebCompositionCommandConfirm)
      editor->confirmComposition();
  }

  return editor->hasComposition();
}

bool WebViewImpl::queryCompositionStatus(bool* enable_ime,
                                         WebRect* caret_rect) {
  // Store whether the selected node needs IME and the caret rectangle.
  // This process consists of the following four steps:
  //  1. Retrieve the selection controller of the focused frame;
  //  2. Retrieve the caret rectangle from the controller;
  //  3. Convert the rectangle, which is relative to the parent view, to the
  //     one relative to the client window, and;
  //  4. Store the converted rectangle.
  const Frame* focused = GetFocusedWebCoreFrame();
  if (!focused)
    return false;

  const Editor* editor = focused->editor();
  if (!editor || !editor->canEdit())
    return false;

  SelectionController* controller = focused->selection();
  if (!controller)
    return false;

  const Node* node = controller->start().node();
  if (!node)
    return false;

  *enable_ime = node->shouldUseInputMethod() &&
      !controller->isInPasswordField();
  const FrameView* view = node->document()->view();
  if (!view)
    return false;

  *caret_rect = webkit_glue::IntRectToWebRect(
      view->contentsToWindow(controller->absoluteCaretBounds()));
  return true;
}

void WebViewImpl::setTextDirection(WebTextDirection direction) {
  // The Editor::setBaseWritingDirection() function checks if we can change
  // the text direction of the selected node and updates its DOM "dir"
  // attribute and its CSS "direction" property.
  // So, we just call the function as Safari does.
  const Frame* focused = GetFocusedWebCoreFrame();
  if (!focused)
    return;

  Editor* editor = focused->editor();
  if (!editor || !editor->canEdit())
    return;

  switch (direction) {
    case WebTextDirectionDefault:
      editor->setBaseWritingDirection(WebCore::NaturalWritingDirection);
      break;

    case WebTextDirectionLeftToRight:
      editor->setBaseWritingDirection(WebCore::LeftToRightWritingDirection);
      break;

    case WebTextDirectionRightToLeft:
      editor->setBaseWritingDirection(WebCore::RightToLeftWritingDirection);
      break;

    default:
      notImplemented();
      break;
  }
}

// WebView --------------------------------------------------------------------

WebSettings* WebViewImpl::settings() {
  if (!web_settings_.get())
    web_settings_.set(new WebSettingsImpl(page_->settings()));
  ASSERT(web_settings_.get());
  return web_settings_.get();
}

WebString WebViewImpl::pageEncoding() const {
  if (!page_.get())
    return WebString();

  String encoding_name = page_->mainFrame()->loader()->encoding();
  return webkit_glue::StringToWebString(encoding_name);
}

void WebViewImpl::setPageEncoding(const WebString& encoding_name) {
  if (!page_.get())
    return;

  // Only change override encoding, don't change default encoding.
  // Note that the new encoding must be NULL if it isn't supposed to be set.
  String new_encoding_name;
  if (!encoding_name.isEmpty())
    new_encoding_name = webkit_glue::WebStringToString(encoding_name);
  page_->mainFrame()->loader()->reloadWithOverrideEncoding(new_encoding_name);
}

bool WebViewImpl::dispatchBeforeUnloadEvent() {
  // TODO(creis): This should really cause a recursive depth-first walk of all
  // frames in the tree, calling each frame's onbeforeunload.  At the moment,
  // we're consistent with Safari 3.1, not IE/FF.
  Frame* frame = page_->focusController()->focusedOrMainFrame();
  if (!frame)
    return true;

  return frame->shouldClose();
}

void WebViewImpl::dispatchUnloadEvent() {
  // Run unload handlers.
  page_->mainFrame()->loader()->closeURL();
}

WebFrame* WebViewImpl::mainFrame() {
  return main_frame();
}

WebFrame* WebViewImpl::findFrameByName(
    const WebString& name, WebFrame* relative_to_frame) {
  String name_str = webkit_glue::WebStringToString(name);
  if (!relative_to_frame)
    relative_to_frame = mainFrame();
  Frame* frame = static_cast<WebFrameImpl*>(relative_to_frame)->frame();
  frame = frame->tree()->find(name_str);
  return WebFrameImpl::FromFrame(frame);
}

WebFrame* WebViewImpl::focusedFrame() {
  return WebFrameImpl::FromFrame(GetFocusedWebCoreFrame());
}

void WebViewImpl::setFocusedFrame(WebFrame* frame) {
  if (!frame) {
    // Clears the focused frame if any.
    Frame* frame = GetFocusedWebCoreFrame();
    if (frame)
      frame->selection()->setFocused(false);
    return;
  }
  WebFrameImpl* frame_impl = static_cast<WebFrameImpl*>(frame);
  WebCore::Frame* webcore_frame = frame_impl->frame();
  webcore_frame->page()->focusController()->setFocusedFrame(webcore_frame);
}

void WebViewImpl::setInitialFocus(bool reverse) {
  if (!page_.get())
    return;

  // Since we don't have a keyboard event, we'll create one.
  WebKeyboardEvent keyboard_event;
  keyboard_event.type = WebInputEvent::RawKeyDown;
  if (reverse)
    keyboard_event.modifiers = WebInputEvent::ShiftKey;

  // VK_TAB which is only defined on Windows.
  keyboard_event.windowsKeyCode = 0x09;
  PlatformKeyboardEventBuilder platform_event(keyboard_event);
  RefPtr<KeyboardEvent> webkit_event =
      KeyboardEvent::create(platform_event, NULL);
  page()->focusController()->setInitialFocus(
      reverse ? WebCore::FocusDirectionBackward :
                WebCore::FocusDirectionForward,
      webkit_event.get());
}

void WebViewImpl::clearFocusedNode() {
  if (!page_.get())
    return;

  RefPtr<Frame> frame = page_->mainFrame();
  if (!frame.get())
    return;

  RefPtr<Document> document = frame->document();
  if (!document.get())
    return;

  RefPtr<Node> old_focused_node = document->focusedNode();

  // Clear the focused node.
  document->setFocusedNode(NULL);

  if (!old_focused_node.get())
    return;

  // If a text field has focus, we need to make sure the selection controller
  // knows to remove selection from it. Otherwise, the text field is still
  // processing keyboard events even though focus has been moved to the page and
  // keystrokes get eaten as a result.
  if (old_focused_node->hasTagName(HTMLNames::textareaTag) ||
      (old_focused_node->hasTagName(HTMLNames::inputTag) &&
       static_cast<HTMLInputElement*>(old_focused_node.get())->isTextField())) {
    // Clear the selection.
    SelectionController* selection = frame->selection();
    selection->clear();
  }
}

void WebViewImpl::zoomIn(bool text_only) {
  Frame* frame = main_frame()->frame();
  double multiplier = std::min(std::pow(kTextSizeMultiplierRatio,
                                        zoom_level_ + 1),
                               kMaxTextSizeMultiplier);
  float zoom_factor = static_cast<float>(multiplier);
  if (zoom_factor != frame->zoomFactor()) {
    ++zoom_level_;
    frame->setZoomFactor(zoom_factor, text_only);
  }
}

void WebViewImpl::zoomOut(bool text_only) {
  Frame* frame = main_frame()->frame();
  double multiplier = std::max(std::pow(kTextSizeMultiplierRatio,
                                        zoom_level_ - 1),
                               kMinTextSizeMultiplier);
  float zoom_factor = static_cast<float>(multiplier);
  if (zoom_factor != frame->zoomFactor()) {
    --zoom_level_;
    frame->setZoomFactor(zoom_factor, text_only);
  }
}

void WebViewImpl::zoomDefault() {
  // We don't change the zoom mode (text only vs. full page) here. We just want
  // to reset whatever is already set.
  zoom_level_ = 0;
  main_frame()->frame()->setZoomFactor(
      1.0f,
      main_frame()->frame()->isZoomFactorTextOnly());
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location) {
  HitTestResult result =
      HitTestResultForWindowPos(webkit_glue::WebPointToIntPoint(location));
  WTF::RefPtr<WebCore::Node> node = result.innerNonSharedNode();
  if (!node->hasTagName(WebCore::HTMLNames::videoTag) &&
      !node->hasTagName(WebCore::HTMLNames::audioTag))
    return;

  WTF::RefPtr<WebCore::HTMLMediaElement> media_element =
      static_pointer_cast<WebCore::HTMLMediaElement>(node);
  switch (action.type) {
    case WebMediaPlayerAction::Play:
      if (action.enable) {
        media_element->play();
      } else {
        media_element->pause();
      }
      break;
    case WebMediaPlayerAction::Mute:
      media_element->setMuted(action.enable);
      break;
    case WebMediaPlayerAction::Loop:
      media_element->setLoop(action.enable);
      break;
    default:
      ASSERT_NOT_REACHED();
  }
}

void WebViewImpl::copyImageAt(const WebPoint& point) {
  if (!page_.get())
    return;

  HitTestResult result =
      HitTestResultForWindowPos(webkit_glue::WebPointToIntPoint(point));

  if (result.absoluteImageURL().isEmpty()) {
    // There isn't actually an image at these coordinates.  Might be because
    // the window scrolled while the context menu was open or because the page
    // changed itself between when we thought there was an image here and when
    // we actually tried to retreive the image.
    //
    // TODO: implement a cache of the most recent HitTestResult to avoid having
    //       to do two hit tests.
    return;
  }

  page_->mainFrame()->editor()->copyImage(result);
}

void WebViewImpl::dragSourceEndedAt(
    const WebPoint& client_point,
    const WebPoint& screen_point,
    WebDragOperation operation) {
  PlatformMouseEvent pme(webkit_glue::WebPointToIntPoint(client_point),
                         webkit_glue::WebPointToIntPoint(screen_point),
                         LeftButton, MouseEventMoved, 0, false, false, false,
                         false, 0);
  page_->mainFrame()->eventHandler()->dragSourceEndedAt(pme,
      static_cast<WebCore::DragOperation>(operation));
}

void WebViewImpl::dragSourceMovedTo(
    const WebPoint& client_point,
    const WebPoint& screen_point) {
  PlatformMouseEvent pme(webkit_glue::WebPointToIntPoint(client_point),
                         webkit_glue::WebPointToIntPoint(screen_point),
                         LeftButton, MouseEventMoved, 0, false, false, false,
                         false, 0);
  page_->mainFrame()->eventHandler()->dragSourceMovedTo(pme);
}

void WebViewImpl::dragSourceSystemDragEnded() {
  // It's possible for us to get this callback while not doing a drag if
  // it's from a previous page that got unloaded.
  if (doing_drag_and_drop_) {
    page_->dragController()->dragEnded();
    doing_drag_and_drop_ = false;
  }
}

WebDragOperation WebViewImpl::dragTargetDragEnter(
    const WebDragData& web_drag_data, int identity,
    const WebPoint& client_point,
    const WebPoint& screen_point,
    WebDragOperationsMask operations_allowed) {
  ASSERT(!current_drag_data_.get());

  current_drag_data_ =
      webkit_glue::WebDragDataToChromiumDataObject(web_drag_data);
  drag_identity_ = identity;
  operations_allowed_ = operations_allowed;

  DragData drag_data(
      current_drag_data_.get(),
      webkit_glue::WebPointToIntPoint(client_point),
      webkit_glue::WebPointToIntPoint(screen_point),
      static_cast<WebCore::DragOperation>(operations_allowed));

  drop_effect_ = DROP_EFFECT_DEFAULT;
  drag_target_dispatch_ = true;
  DragOperation effect = page_->dragController()->dragEntered(&drag_data);
  // Mask the operation against the drag source's allowed operations.
  if ((effect & drag_data.draggingSourceOperationMask()) != effect) {
    effect = DragOperationNone;
  }
  drag_target_dispatch_ = false;

  if (drop_effect_ != DROP_EFFECT_DEFAULT)
    drag_operation_ = (drop_effect_ != DROP_EFFECT_NONE) ?
        WebDragOperationCopy : WebDragOperationNone;
  else
    drag_operation_ = static_cast<WebDragOperation>(effect);
  return drag_operation_;
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& client_point,
    const WebPoint& screen_point,
    WebDragOperationsMask operations_allowed) {
  ASSERT(current_drag_data_.get());

  operations_allowed_ = operations_allowed;
  DragData drag_data(
      current_drag_data_.get(),
      webkit_glue::WebPointToIntPoint(client_point),
      webkit_glue::WebPointToIntPoint(screen_point),
      static_cast<WebCore::DragOperation>(operations_allowed));

  drop_effect_ = DROP_EFFECT_DEFAULT;
  drag_target_dispatch_ = true;
  DragOperation effect = page_->dragController()->dragUpdated(&drag_data);
  // Mask the operation against the drag source's allowed operations.
  if ((effect & drag_data.draggingSourceOperationMask()) != effect) {
    effect = DragOperationNone;
  }
  drag_target_dispatch_ = false;

  if (drop_effect_ != DROP_EFFECT_DEFAULT)
    drag_operation_ = (drop_effect_ != DROP_EFFECT_NONE) ?
        WebDragOperationCopy : WebDragOperationNone;
  else
    drag_operation_ = static_cast<WebDragOperation>(effect);
  return drag_operation_;
}

void WebViewImpl::dragTargetDragLeave() {
  ASSERT(current_drag_data_.get());

  DragData drag_data(
      current_drag_data_.get(),
      IntPoint(),
      IntPoint(),
      static_cast<WebCore::DragOperation>(operations_allowed_));

  drag_target_dispatch_ = true;
  page_->dragController()->dragExited(&drag_data);
  drag_target_dispatch_ = false;

  current_drag_data_ = NULL;
  drop_effect_ = DROP_EFFECT_DEFAULT;
  drag_operation_ = WebDragOperationNone;
  drag_identity_ = 0;
}

void WebViewImpl::dragTargetDrop(const WebPoint& client_point,
                                 const WebPoint& screen_point) {
  ASSERT(current_drag_data_.get());

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview drag_operation_ state is not DragOperationNone.

  if (drag_operation_ == WebDragOperationNone) {  // IPC RACE CONDITION: do not allow this drop.
    dragTargetDragLeave();
    return;
  }

  DragData drag_data(
      current_drag_data_.get(),
      webkit_glue::WebPointToIntPoint(client_point),
      webkit_glue::WebPointToIntPoint(screen_point),
      static_cast<WebCore::DragOperation>(operations_allowed_));

  drag_target_dispatch_ = true;
  page_->dragController()->performDrag(&drag_data);
  drag_target_dispatch_ = false;

  current_drag_data_ = NULL;
  drop_effect_ = DROP_EFFECT_DEFAULT;
  drag_operation_ = WebDragOperationNone;
  drag_identity_ = 0;
}

int WebViewImpl::dragIdentity() {
  if (drag_target_dispatch_)
    return drag_identity_;
  return 0;
}

void WebViewImpl::inspectElementAt(const WebPoint& point) {
  if (!page_.get())
    return;

  if (point.x == -1 || point.y == -1) {
    page_->inspectorController()->inspect(NULL);
  } else {
    HitTestResult result =
        HitTestResultForWindowPos(webkit_glue::WebPointToIntPoint(point));

    if (!result.innerNonSharedNode())
      return;

    page_->inspectorController()->inspect(result.innerNonSharedNode());
  }
}

WebString WebViewImpl::inspectorSettings() const {
  return inspector_settings_;
}

void WebViewImpl::setInspectorSettings(const WebString& settings) {
  inspector_settings_ = settings;
}

WebDevToolsAgent* WebViewImpl::devToolsAgent() {
  return devtools_agent_.get();
}

WebAccessibilityObject WebViewImpl::accessibilityObject() {
  if (!main_frame())
    return WebAccessibilityObject();

  WebCore::Document* document = main_frame()->frame()->document();

  return AccessibilityObjectToWebAccessibilityObject(
      document->axObjectCache()->getOrCreate(document->renderer()));
}

void WebViewImpl::applyAutofillSuggestions(
    const WebNode& node,
    const WebVector<WebString>& suggestions,
    int default_suggestion_index) {
  if (!page_.get() || suggestions.isEmpty()) {
    HideAutoCompletePopup();
    return;
  }

  ASSERT(default_suggestion_index < static_cast<int>(suggestions.size()));

  if (RefPtr<Frame> focused = page_->focusController()->focusedFrame()) {
    RefPtr<Document> document = focused->document();
    if (!document.get()) {
      HideAutoCompletePopup();
      return;
    }

    RefPtr<Node> focused_node = document->focusedNode();
    // If the node for which we queried the autofill suggestions is not the
    // focused node, then we have nothing to do.
    // TODO(jcampan): also check the carret is at the end and that the text has
    // not changed.
    if (!focused_node.get() ||
        focused_node != webkit_glue::WebNodeToNode(node)) {
      HideAutoCompletePopup();
      return;
    }

    if (!focused_node->hasTagName(WebCore::HTMLNames::inputTag)) {
      ASSERT_NOT_REACHED();
      return;
    }

    WebCore::HTMLInputElement* input_elem =
        static_cast<WebCore::HTMLInputElement*>(focused_node.get());

    // The first time the autocomplete is shown we'll create the client and the
    // popup.
    if (!autocomplete_popup_client_.get())
      autocomplete_popup_client_.set(new AutocompletePopupMenuClient(this));
    autocomplete_popup_client_->Init(input_elem,
                                     suggestions,
                                     default_suggestion_index);
    if (!autocomplete_popup_.get()) {
      autocomplete_popup_ =
          WebCore::PopupContainer::create(autocomplete_popup_client_.get(),
                                          kAutocompletePopupSettings);
    }

    if (autocomplete_popup_showing_) {
      autocomplete_popup_client_->SetSuggestions(suggestions);
      RefreshAutofillPopup();
    } else {
      autocomplete_popup_->show(focused_node->getRect(),
                                focused_node->ownerDocument()->view(), 0);
      autocomplete_popup_showing_ = true;
    }
  }
}

void WebViewImpl::hideAutofillPopup() {
  HideAutoCompletePopup();
}

// WebView --------------------------------------------------------------------

bool WebViewImpl::setDropEffect(bool accept) {
  if (drag_target_dispatch_) {
    drop_effect_ = accept ? DROP_EFFECT_COPY : DROP_EFFECT_NONE;
    return true;
  } else {
    return false;
  }
}

WebDevToolsAgentImpl* WebViewImpl::GetWebDevToolsAgentImpl() {
  return devtools_agent_.get();
}

void WebViewImpl::setIsTransparent(bool is_transparent) {
  // Set any existing frames to be transparent.
  WebCore::Frame* frame = page_->mainFrame();
  while (frame) {
    frame->view()->setTransparent(is_transparent);
    frame = frame->tree()->traverseNext();
  }

  // Future frames check this to know whether to be transparent.
  is_transparent_ = is_transparent;
}

bool WebViewImpl::isTransparent() const {
  return is_transparent_;
}

void WebViewImpl::setIsActive(bool active) {
  if (page() && page()->focusController())
    page()->focusController()->setActive(active);
}

bool WebViewImpl::isActive() const {
  return (page() && page()->focusController())
      ? page()->focusController()->isActive()
      : false;
}

void WebViewImpl::DidCommitLoad(bool* is_new_navigation) {
  if (is_new_navigation)
    *is_new_navigation = observed_new_navigation_;

#ifndef NDEBUG
  ASSERT(!observed_new_navigation_ ||
    page_->mainFrame()->loader()->documentLoader() == new_navigation_loader_);
  new_navigation_loader_ = NULL;
#endif
  observed_new_navigation_ = false;
}

// static
bool WebViewImpl::NavigationPolicyFromMouseEvent(unsigned short button,
                                                 bool ctrl, bool shift,
                                                 bool alt, bool meta,
                                                 WebNavigationPolicy* policy) {
#if PLATFORM(WIN_OS) || PLATFORM(LINUX) || PLATFORM(FREEBSD)
  const bool new_tab_modifier = (button == 1) || ctrl;
#elif PLATFORM(DARWIN)
  const bool new_tab_modifier = (button == 1) || meta;
#endif
  if (!new_tab_modifier && !shift && !alt)
    return false;

  ASSERT(policy);
  if (new_tab_modifier) {
    if (shift) {
      *policy = WebKit::WebNavigationPolicyNewForegroundTab;
    } else {
      *policy = WebKit::WebNavigationPolicyNewBackgroundTab;
    }
  } else {
    if (shift) {
      *policy = WebKit::WebNavigationPolicyNewWindow;
    } else {
      *policy = WebKit::WebNavigationPolicyDownload;
    }
  }
  return true;
}

void WebViewImpl::StartDragging(const WebPoint& event_pos,
                                const WebDragData& drag_data,
                                WebDragOperationsMask mask) {
  if (!client_)
    return;
  ASSERT(!doing_drag_and_drop_);
  doing_drag_and_drop_ = true;
  client_->startDragging(event_pos, drag_data, mask);
}

void WebViewImpl::SetCurrentHistoryItem(WebCore::HistoryItem* item) {
  back_forward_list_client_impl_.SetCurrentHistoryItem(item);
}

WebCore::HistoryItem* WebViewImpl::GetPreviousHistoryItem() {
  return back_forward_list_client_impl_.GetPreviousHistoryItem();
}

void WebViewImpl::ObserveNewNavigation() {
  observed_new_navigation_ = true;
#ifndef NDEBUG
  new_navigation_loader_ = page_->mainFrame()->loader()->documentLoader();
#endif
}

void WebViewImpl::HideAutoCompletePopup() {
  if (autocomplete_popup_showing_) {
    autocomplete_popup_->hidePopup();
    AutoCompletePopupDidHide();
  }
}

void WebViewImpl::AutoCompletePopupDidHide() {
  autocomplete_popup_showing_ = false;
}

void WebViewImpl::SetIgnoreInputEvents(bool new_value) {
  ASSERT(ignore_input_events_ != new_value);
  ignore_input_events_ = new_value;
}

#if ENABLE(NOTIFICATIONS)
WebKit::NotificationPresenterImpl* WebViewImpl::GetNotificationPresenter() {
  if (!notification_presenter_.isInitialized() && client_)
    notification_presenter_.initialize(client_->notificationPresenter());
  return &notification_presenter_;
}
#endif

void WebViewImpl::RefreshAutofillPopup() {
  ASSERT(autocomplete_popup_showing_);

  // Hide the popup if it has become empty.
  if (autocomplete_popup_client_->listSize() == 0) {
    HideAutoCompletePopup();
    return;
  }

  IntRect old_bounds = autocomplete_popup_->boundsRect();
  autocomplete_popup_->refresh();
  IntRect new_bounds = autocomplete_popup_->boundsRect();
  // Let's resize the backing window if necessary.
  if (old_bounds != new_bounds) {
    WebPopupMenuImpl* popup_menu =
        static_cast<WebPopupMenuImpl*>(autocomplete_popup_->client());
    popup_menu->client()->setWindowRect(
        webkit_glue::IntRectToWebRect(new_bounds));
  }
}

Node* WebViewImpl::GetFocusedNode() {
  Frame* frame = page_->focusController()->focusedFrame();
  if (!frame)
    return NULL;

  Document* document = frame->document();
  if (!document)
    return NULL;

  return document->focusedNode();
}

HitTestResult WebViewImpl::HitTestResultForWindowPos(const IntPoint& pos) {
  IntPoint doc_point(
      page_->mainFrame()->view()->windowToContents(pos));
  return page_->mainFrame()->eventHandler()->
      hitTestResultAtPoint(doc_point, false);
}

void WebViewImpl::setTabsToLinks(bool enable) {
  tabs_to_links_ = enable;
}

bool WebViewImpl::tabsToLinks() const {
  return tabs_to_links_;
}
