// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ContextMenu.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "MediaError.h"
#include "PlatformString.h"
#include "Widget.h"
#undef LOG

#include "base/i18n/word_iterator.h"
#include "base/string_util.h"
#include "webkit/api/public/WebContextMenuData.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLResponse.h"
#include "webkit/api/public/WebViewClient.h"
#include "webkit/api/src/WebDataSourceImpl.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/context_menu_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webview_impl.h"

using WebKit::WebContextMenuData;
using WebKit::WebDataSource;
using WebKit::WebDataSourceImpl;
using WebKit::WebFrame;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebURL;

namespace {
// Helper function to determine whether text is a single word or a sentence.
bool IsASingleWord(const std::wstring& text) {
  WordIterator iter(text, WordIterator::BREAK_WORD);
  int word_count = 0;
  if (!iter.Init()) return false;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_count++;
      if (word_count > 1) // More than one word.
        return false;
    }
  }

  // Check for 0 words.
  if (!word_count)
    return false;

  // Has a single word.
  return true;
}

// Helper function to get misspelled word on which context menu
// is to be evolked. This function also sets the word on which context menu
// has been evoked to be the selected word, as required. This function changes
// the selection only when there were no selected characters.
std::wstring GetMisspelledWord(const WebCore::ContextMenu* default_menu,
                               WebCore::Frame* selected_frame) {
  std::wstring misspelled_word_string;

  // First select from selectedText to check for multiple word selection.
  misspelled_word_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
      false);

  // If some texts were already selected, we don't change the selection.
  if (!misspelled_word_string.empty()) {
    // Don't provide suggestions for multiple words.
    if (!IsASingleWord(misspelled_word_string))
      return L"";
    return misspelled_word_string;
  }

  WebCore::HitTestResult hit_test_result = selected_frame->eventHandler()->
      hitTestResultAtPoint(default_menu->hitTestResult().point(), true);
  WebCore::Node* inner_node = hit_test_result.innerNode();
  WebCore::VisiblePosition pos(inner_node->renderer()->positionForPoint(
      hit_test_result.localPoint()));

  WebCore::VisibleSelection selection;
  if (pos.isNotNull()) {
    selection = WebCore::VisibleSelection(pos);
    selection.expandUsingGranularity(WebCore::WordGranularity);
  }

  if (selection.isRange()) {
    selected_frame->setSelectionGranularity(WebCore::WordGranularity);
  }

  if (selected_frame->shouldChangeSelection(selection))
    selected_frame->selection()->setSelection(selection);

  misspelled_word_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
      false);

  // If misspelled word is empty, then that portion should not be selected.
  // Set the selection to that position only, and do not expand.
  if (misspelled_word_string.empty()) {
    selection = WebCore::VisibleSelection(pos);
    selected_frame->selection()->setSelection(selection);
  }

  return misspelled_word_string;
}

}  // namespace

ContextMenuClientImpl::~ContextMenuClientImpl() {
}

void ContextMenuClientImpl::contextMenuDestroyed() {
  // Our lifetime is bound to the WebViewImpl.
}

// Figure out the URL of a page or subframe. Returns |page_type| as the type,
// which indicates page or subframe, or ContextNodeType::NONE if the URL could not
// be determined for some reason.
static WebURL GetURLFromFrame(WebCore::Frame* frame) {
  if (frame) {
    WebCore::DocumentLoader* dl = frame->loader()->documentLoader();
    if (dl) {
      WebDataSource* ds = WebDataSourceImpl::fromDocumentLoader(dl);
      if (ds) {
        return ds->hasUnreachableURL() ? ds->unreachableURL()
                                       : ds->request().url();
      }
    }
  }
  return WebURL();
}

WebCore::PlatformMenuDescription
    ContextMenuClientImpl::getCustomMenuFromDefaultItems(
        WebCore::ContextMenu* default_menu) {
  // Displaying the context menu in this function is a big hack as we don't
  // have context, i.e. whether this is being invoked via a script or in
  // response to user input (Mouse event WM_RBUTTONDOWN,
  // Keyboard events KeyVK_APPS, Shift+F10). Check if this is being invoked
  // in response to the above input events before popping up the context menu.
  if (!webview_->context_menu_allowed())
    return NULL;

  WebCore::HitTestResult r = default_menu->hitTestResult();
  WebCore::Frame* selected_frame = r.innerNonSharedNode()->document()->frame();

  WebContextMenuData data;
  WebCore::IntPoint mouse_position = selected_frame->view()->contentsToWindow(
      r.point());
  data.mousePosition = webkit_glue::IntPointToWebPoint(mouse_position);

  // Links, Images, Media tags, and Image/Media-Links take preference over
  // all else.
  data.linkURL = webkit_glue::KURLToWebURL(r.absoluteLinkURL());

  data.mediaType = WebContextMenuData::MediaTypeNone;

  if (!r.absoluteImageURL().isEmpty()) {
    data.srcURL = webkit_glue::KURLToWebURL(r.absoluteImageURL());
    data.mediaType = WebContextMenuData::MediaTypeImage;
  } else if (!r.absoluteMediaURL().isEmpty()) {
    data.srcURL = webkit_glue::KURLToWebURL(r.absoluteMediaURL());

    // We know that if absoluteMediaURL() is not empty, then this is a media
    // element.
    WebCore::HTMLMediaElement* media_element =
        static_cast<WebCore::HTMLMediaElement*>(r.innerNonSharedNode());
    if (media_element->hasTagName(WebCore::HTMLNames::videoTag)) {
      data.mediaType = WebContextMenuData::MediaTypeVideo;
    } else if (media_element->hasTagName(WebCore::HTMLNames::audioTag)) {
      data.mediaType = WebContextMenuData::MediaTypeAudio;
    }

    if (media_element->error()) {
      data.mediaFlags |= WebContextMenuData::MediaInError;
    }
    if (media_element->paused()) {
      data.mediaFlags |= WebContextMenuData::MediaPaused;
    }
    if (media_element->muted()) {
      data.mediaFlags |= WebContextMenuData::MediaMuted;
    }
    if (media_element->loop()) {
      data.mediaFlags |= WebContextMenuData::MediaLoop;
    }
    if (media_element->supportsSave()) {
      data.mediaFlags |= WebContextMenuData::MediaCanSave;
    }
    if (media_element->hasAudio()) {
      data.mediaFlags |= WebContextMenuData::MediaHasAudio;
    }
  }
  // If it's not a link, an image, a media element, or an image/media link,
  // show a selection menu or a more generic page menu.
  data.frameEncoding = webkit_glue::StringToWebString(
      selected_frame->loader()->encoding());

  // Send the frame and page URLs in any case.
  data.pageURL = GetURLFromFrame(webview_->main_frame()->frame());
  if (selected_frame != webview_->main_frame()->frame()) {
    data.frameURL = GetURLFromFrame(selected_frame);
  }

  if (r.isSelected()) {
    data.selectedText = WideToUTF16Hack(CollapseWhitespace(
        webkit_glue::StringToStdWString(selected_frame->selectedText()),
        false));
  }

  data.isEditable = false;
  if (r.isContentEditable()) {
    data.isEditable = true;
    if (webview_->GetFocusedWebCoreFrame()->editor()->
        isContinuousSpellCheckingEnabled()) {
      data.isSpellCheckingEnabled = true;
      // TODO: GetMisspelledWord should move downstream to RenderView.
      data.misspelledWord = WideToUTF16Hack(
          GetMisspelledWord(default_menu, selected_frame));
    }
  }

  // Now retrieve the security info.
  WebCore::DocumentLoader* dl = selected_frame->loader()->documentLoader();
  WebDataSource* ds = WebDataSourceImpl::fromDocumentLoader(dl);
  if (ds)
    data.securityInfo = ds->response().securityInfo();

  // Compute edit flags.
  data.editFlags = WebContextMenuData::CanDoNone;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canUndo())
    data.editFlags |= WebContextMenuData::CanUndo;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canRedo())
    data.editFlags |= WebContextMenuData::CanRedo;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canCut())
    data.editFlags |= WebContextMenuData::CanCut;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canCopy())
    data.editFlags |= WebContextMenuData::CanCopy;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canPaste())
    data.editFlags |= WebContextMenuData::CanPaste;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canDelete())
    data.editFlags |= WebContextMenuData::CanDelete;
  // We can always select all...
  data.editFlags |= WebContextMenuData::CanSelectAll;

  WebFrame* selected_web_frame = WebFrameImpl::FromFrame(selected_frame);
  if (webview_->client())
    webview_->client()->showContextMenu(selected_web_frame, data);

  return NULL;
}

void ContextMenuClientImpl::contextMenuItemSelected(
    WebCore::ContextMenuItem*, const WebCore::ContextMenu*) {
}

void ContextMenuClientImpl::downloadURL(const WebCore::KURL&) {
}

void ContextMenuClientImpl::copyImageToClipboard(const WebCore::HitTestResult&) {
}

void ContextMenuClientImpl::searchWithGoogle(const WebCore::Frame*) {
}

void ContextMenuClientImpl::lookUpInDictionary(WebCore::Frame*) {
}

void ContextMenuClientImpl::speak(const WebCore::String&) {
}

bool ContextMenuClientImpl::isSpeaking() {
  return false;
}

void ContextMenuClientImpl::stopSpeaking() {
}

bool ContextMenuClientImpl::shouldIncludeInspectElementItem() {
  return false;
}
