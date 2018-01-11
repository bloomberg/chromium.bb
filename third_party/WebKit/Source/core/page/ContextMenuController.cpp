/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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

#include "core/page/ContextMenuController.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/editing/EditingTriState.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/ime/InputMethodController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/MouseEvent.h"
#include "core/exported/WebPluginContainerImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/input/ContextMenuAllowedScope.h"
#include "core/input/EventHandler.h"
#include "core/input_type_names.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebMenuSourceType.h"
#include "public/web/WebContextMenuData.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebTextCheckClient.h"

namespace blink {

ContextMenuController::ContextMenuController(Page* page) : page_(page) {}

ContextMenuController::~ContextMenuController() = default;

ContextMenuController* ContextMenuController::Create(Page* page) {
  return new ContextMenuController(page);
}

void ContextMenuController::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(menu_provider_);
  visitor->Trace(hit_test_result_);
}

void ContextMenuController::ClearContextMenu() {
  context_menu_.reset();
  if (menu_provider_)
    menu_provider_->ContextMenuCleared();
  menu_provider_ = nullptr;
  hit_test_result_ = HitTestResult();
}

void ContextMenuController::DocumentDetached(Document* document) {
  if (Node* inner_node = hit_test_result_.InnerNode()) {
    // Invalidate the context menu info if its target document is detached.
    if (inner_node->GetDocument() == document)
      ClearContextMenu();
  }
}

void ContextMenuController::HandleContextMenuEvent(MouseEvent* mouse_event) {
  context_menu_ = CreateContextMenu(mouse_event);
  if (!context_menu_)
    return;

  ShowContextMenu(mouse_event);
}

void ContextMenuController::ShowContextMenuAtPoint(
    LocalFrame* frame,
    float x,
    float y,
    ContextMenuProvider* menu_provider) {
  menu_provider_ = menu_provider;

  LayoutPoint location(x, y);
  context_menu_ = CreateContextMenu(frame, location);
  if (!context_menu_) {
    ClearContextMenu();
    return;
  }

  menu_provider_->PopulateContextMenu(context_menu_.get());
  ShowContextMenu(nullptr);
}

std::unique_ptr<ContextMenu> ContextMenuController::CreateContextMenu(
    MouseEvent* mouse_event) {
  DCHECK(mouse_event);

  return CreateContextMenu(
      mouse_event->target()->ToNode()->GetDocument().GetFrame(),
      LayoutPoint(mouse_event->AbsoluteLocation()));
}

std::unique_ptr<ContextMenu> ContextMenuController::CreateContextMenu(
    LocalFrame* frame,
    const LayoutPoint& location) {
  HitTestRequest::HitTestRequestType type =
      HitTestRequest::kReadOnly | HitTestRequest::kActive;
  HitTestResult result(type, location);

  if (frame)
    result = frame->GetEventHandler().HitTestResultAtPoint(location, type);

  if (!result.InnerNodeOrImageMapImage())
    return nullptr;

  hit_test_result_ = result;

  return WTF::WrapUnique(new ContextMenu);
}

void ContextMenuController::ShowContextMenu(MouseEvent* mouse_event) {
  WebMenuSourceType source_type = kMenuSourceNone;
  if (mouse_event) {
    DCHECK(mouse_event->type() == EventTypeNames::contextmenu);
    source_type = mouse_event->GetMenuSourceType();
  }

  if (ShowContextMenu(context_menu_.get(), source_type) && mouse_event)
    mouse_event->SetDefaultHandled();
}

void ContextMenuController::ContextMenuItemSelected(
    const ContextMenuItem* item) {
  DCHECK(item->GetType() == kActionType ||
         item->GetType() == kCheckableActionType);

  if (item->Action() < kContextMenuItemBaseCustomTag ||
      item->Action() > kContextMenuItemLastCustomTag)
    return;

  DCHECK(menu_provider_);
  menu_provider_->ContextMenuItemSelected(item);
}

Node* ContextMenuController::ContextMenuNodeForFrame(LocalFrame* frame) {
  return hit_test_result_.InnerNodeFrame() == frame
             ? hit_test_result_.InnerNodeOrImageMapImage()
             : nullptr;
}

// Figure out the URL of a page or subframe. Returns |page_type| as the type,
// which indicates page or subframe, or ContextNodeType::kNone if the URL could
// not be determined for some reason.
static KURL UrlFromFrame(LocalFrame* frame) {
  if (frame) {
    DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();
    if (document_loader) {
      return document_loader->UnreachableURL().IsEmpty()
                 ? document_loader->GetRequest().Url()
                 : document_loader->UnreachableURL();
    }
  }
  return KURL();
}

static int ComputeEditFlags(Document& selected_document, Editor& editor) {
  int edit_flags = WebContextMenuData::kCanDoNone;
  if (!selected_document.IsHTMLDocument() &&
      !selected_document.IsXHTMLDocument())
    return edit_flags;

  edit_flags |= WebContextMenuData::kCanTranslate;
  if (editor.CanUndo())
    edit_flags |= WebContextMenuData::kCanUndo;
  if (editor.CanRedo())
    edit_flags |= WebContextMenuData::kCanRedo;
  if (editor.CanCut())
    edit_flags |= WebContextMenuData::kCanCut;
  if (editor.CanCopy())
    edit_flags |= WebContextMenuData::kCanCopy;
  if (editor.CanPaste())
    edit_flags |= WebContextMenuData::kCanPaste;
  if (editor.CanDelete())
    edit_flags |= WebContextMenuData::kCanDelete;
  if (editor.CanEditRichly())
    edit_flags |= WebContextMenuData::kCanEditRichly;
  if (selected_document.queryCommandEnabled("selectAll", ASSERT_NO_EXCEPTION))
    edit_flags |= WebContextMenuData::kCanSelectAll;
  return edit_flags;
}

bool ContextMenuController::ShouldShowContextMenuFromTouch(
    const WebContextMenuData& data) {
  return page_->GetSettings().GetAlwaysShowContextMenuOnTouch() ||
         !data.link_url.IsEmpty() ||
         data.media_type == WebContextMenuData::kMediaTypeImage ||
         data.media_type == WebContextMenuData::kMediaTypeVideo ||
         data.is_editable || !data.selected_text.IsEmpty();
}

static void PopulateSubMenuItems(const Vector<ContextMenuItem>& input_menu,
                                 WebVector<WebMenuItemInfo>& sub_menu_items) {
  Vector<WebMenuItemInfo> sub_items;
  for (size_t i = 0; i < input_menu.size(); ++i) {
    const ContextMenuItem* input_item = &input_menu.at(i);
    if (input_item->Action() < kContextMenuItemBaseCustomTag ||
        input_item->Action() > kContextMenuItemLastCustomTag)
      continue;

    WebMenuItemInfo output_item;
    output_item.label = input_item->Title();
    output_item.enabled = input_item->Enabled();
    output_item.checked = input_item->Checked();
    output_item.action = static_cast<unsigned>(input_item->Action() -
                                               kContextMenuItemBaseCustomTag);
    switch (input_item->GetType()) {
      case kActionType:
        output_item.type = WebMenuItemInfo::kOption;
        break;
      case kCheckableActionType:
        output_item.type = WebMenuItemInfo::kCheckableOption;
        break;
      case kSeparatorType:
        output_item.type = WebMenuItemInfo::kSeparator;
        break;
      case kSubmenuType:
        output_item.type = WebMenuItemInfo::kSubMenu;
        PopulateSubMenuItems(input_item->SubMenuItems(),
                             output_item.sub_menu_items);
        break;
    }
    sub_items.push_back(output_item);
  }

  WebVector<WebMenuItemInfo> output_items(sub_items.size());
  for (size_t i = 0; i < sub_items.size(); ++i)
    output_items[i] = sub_items[i];
  sub_menu_items.Swap(output_items);
}

bool ContextMenuController::ShowContextMenu(const ContextMenu* default_menu,
                                            WebMenuSourceType source_type) {
  // Displaying the context menu in this function is a big hack as we don't
  // have context, i.e. whether this is being invoked via a script or in
  // response to user input (Mouse event WM_RBUTTONDOWN,
  // Keyboard events KeyVK_APPS, Shift+F10). Check if this is being invoked
  // in response to the above input events before popping up the context menu.
  if (!ContextMenuAllowedScope::IsContextMenuAllowed())
    return false;

  HitTestResult r = hit_test_result_;
  r.SetToShadowHostIfInRestrictedShadowRoot();

  LocalFrame* selected_frame = r.InnerNodeFrame();

  WebContextMenuData data;
  data.mouse_position = selected_frame->View()->ContentsToViewport(
      r.RoundedPointInInnerNodeFrame());

  data.edit_flags = ComputeEditFlags(
      *selected_frame->GetDocument(),
      ToLocalFrame(page_->GetFocusController().FocusedOrMainFrame())
          ->GetEditor());

  // Links, Images, Media tags, and Image/Media-Links take preference over
  // all else.
  data.link_url = r.AbsoluteLinkURL();

  if (r.InnerNode()->IsHTMLElement()) {
    HTMLElement* html_element = ToHTMLElement(r.InnerNode());
    if (!html_element->title().IsEmpty()) {
      data.title_text = html_element->title();
    } else {
      data.title_text = html_element->AltText();
    }
  }

  if (IsHTMLCanvasElement(r.InnerNode())) {
    data.media_type = WebContextMenuData::kMediaTypeCanvas;
    data.has_image_contents = true;
  } else if (!r.AbsoluteImageURL().IsEmpty()) {
    data.src_url = r.AbsoluteImageURL();
    data.media_type = WebContextMenuData::kMediaTypeImage;
    data.media_flags |= WebContextMenuData::kMediaCanPrint;

    // An image can be null for many reasons, like being blocked, no image
    // data received from server yet.
    data.has_image_contents = r.GetImage() && !r.GetImage()->IsNull();
    data.is_placeholder_image =
        r.GetImage() && r.GetImage()->IsPlaceholderImage();
    if (data.has_image_contents &&
        IsHTMLImageElement(r.InnerNodeOrImageMapImage())) {
      HTMLImageElement* image_element =
          ToHTMLImageElement(r.InnerNodeOrImageMapImage());
      if (image_element && image_element->CachedImage()) {
        data.image_response = WrappedResourceResponse(
            image_element->CachedImage()->GetResponse());
      }
    }
  } else if (!r.AbsoluteMediaURL().IsEmpty()) {
    data.src_url = r.AbsoluteMediaURL();

    // We know that if absoluteMediaURL() is not empty, then this
    // is a media element.
    HTMLMediaElement* media_element = ToHTMLMediaElement(r.InnerNode());
    if (IsHTMLVideoElement(*media_element))
      data.media_type = WebContextMenuData::kMediaTypeVideo;
    else if (IsHTMLAudioElement(*media_element))
      data.media_type = WebContextMenuData::kMediaTypeAudio;

    if (media_element->error())
      data.media_flags |= WebContextMenuData::kMediaInError;
    if (media_element->paused())
      data.media_flags |= WebContextMenuData::kMediaPaused;
    if (media_element->muted())
      data.media_flags |= WebContextMenuData::kMediaMuted;
    if (media_element->Loop())
      data.media_flags |= WebContextMenuData::kMediaLoop;
    if (media_element->SupportsSave())
      data.media_flags |= WebContextMenuData::kMediaCanSave;
    if (media_element->HasAudio())
      data.media_flags |= WebContextMenuData::kMediaHasAudio;
    // Media controls can be toggled only for video player. If we toggle
    // controls for audio then the player disappears, and there is no way to
    // return it back. Don't set this bit for fullscreen video, since
    // toggling is ignored in that case.
    if (media_element->IsHTMLVideoElement() && media_element->HasVideo() &&
        !media_element->IsFullscreen())
      data.media_flags |= WebContextMenuData::kMediaCanToggleControls;
    if (media_element->ShouldShowControls())
      data.media_flags |= WebContextMenuData::kMediaControls;
  } else if (IsHTMLObjectElement(*r.InnerNode()) ||
             IsHTMLEmbedElement(*r.InnerNode())) {
    LayoutObject* object = r.InnerNode()->GetLayoutObject();
    if (object && object->IsLayoutEmbeddedContent()) {
      WebPluginContainerImpl* plugin_view =
          ToLayoutEmbeddedContent(object)->Plugin();
      if (plugin_view) {
        data.media_type = WebContextMenuData::kMediaTypePlugin;

        WebPlugin* plugin = plugin_view->Plugin();
        data.link_url = plugin->LinkAtPosition(data.mouse_position);

        HTMLPlugInElement* plugin_element = ToHTMLPlugInElement(r.InnerNode());
        data.src_url =
            plugin_element->GetDocument().CompleteURL(plugin_element->Url());

        // Figure out the text selection and text edit flags.
        WebString text = plugin->SelectionAsText();
        if (!text.IsEmpty()) {
          data.selected_text = text;
          data.edit_flags |= WebContextMenuData::kCanCopy;
        }
        bool plugin_can_edit_text = plugin->CanEditText();
        if (plugin_can_edit_text) {
          data.is_editable = true;
          if (!!(data.edit_flags & WebContextMenuData::kCanCopy))
            data.edit_flags |= WebContextMenuData::kCanCut;
          data.edit_flags |= WebContextMenuData::kCanPaste;
          // TODO(bug 753216): Implement "SelectAll" command and enable when
          // focus is within an editable text area.
          data.edit_flags &= ~WebContextMenuData::kCanSelectAll;
        }
        // Disable translation for plugins.
        data.edit_flags &= ~WebContextMenuData::kCanTranslate;

        // Figure out the media flags.
        data.media_flags |= WebContextMenuData::kMediaCanSave;
        if (plugin->SupportsPaginatedPrint())
          data.media_flags |= WebContextMenuData::kMediaCanPrint;

        // Add context menu commands that are supported by the plugin.
        // Only show rotate view options if focus is not in an editable text
        // area.
        if (!plugin_can_edit_text && plugin->CanRotateView())
          data.media_flags |= WebContextMenuData::kMediaCanRotate;
      }
    }
  }

  // If it's not a link, an image, a media element, or an image/media link,
  // show a selection menu or a more generic page menu.
  if (selected_frame->GetDocument()->Loader())
    data.frame_encoding = selected_frame->GetDocument()->EncodingName();

  // Send the frame and page URLs in any case.
  if (!page_->MainFrame()->IsLocalFrame()) {
    // TODO(kenrb): This works around the problem of URLs not being
    // available for top-level frames that are in a different process.
    // It mostly works to convert the security origin to a URL, but
    // extensions accessing that property will not get the correct value
    // in that case. See https://crbug.com/534561
    const SecurityOrigin* origin =
        page_->MainFrame()->GetSecurityContext()->GetSecurityOrigin();
    if (origin)
      data.page_url = KURL(origin->ToString());
  } else {
    data.page_url = WebURL(UrlFromFrame(ToLocalFrame(page_->MainFrame())));
  }

  if (selected_frame != page_->MainFrame())
    data.frame_url = WebURL(UrlFromFrame(selected_frame));

  data.selection_start_offset = 0;
  // HitTestResult::isSelected() ensures clean layout by performing a hit test.
  // If source_type is |kMenuSourceAdjustSelectionReset| we know the original
  // HitTestResult in SelectionController passed the inside check already, so
  // let it pass.
  if (r.IsSelected() || source_type == kMenuSourceAdjustSelectionReset) {
    data.selected_text = selected_frame->SelectedText();
    WebRange range =
        selected_frame->GetInputMethodController().GetSelectionOffsets();
    data.selection_start_offset = range.StartOffset();
  }

  if (r.IsContentEditable()) {
    data.is_editable = true;
    SpellChecker& spell_checker = selected_frame->GetSpellChecker();

    // Spellchecker adds spelling markers to misspelled words and attaches
    // suggestions to these markers in the background. Therefore, when a
    // user right-clicks a mouse on a word, Chrome just needs to find a
    // spelling marker on the word instead of spellchecking it.
    std::pair<String, String> misspelled_word_and_description =
        spell_checker.SelectMisspellingAsync();
    data.misspelled_word = misspelled_word_and_description.first;
    const String& description = misspelled_word_and_description.second;
    if (description.length()) {
      Vector<String> suggestions;
      description.Split('\n', suggestions);
      data.dictionary_suggestions = suggestions;
    } else if (spell_checker.GetTextCheckerClient()) {
      int misspelled_offset, misspelled_length;
      spell_checker.GetTextCheckerClient()->CheckSpelling(
          data.misspelled_word, misspelled_offset, misspelled_length,
          &data.dictionary_suggestions);
    }
  }

  if (selected_frame->GetEditor().SelectionHasStyle(
          CSSPropertyDirection, "ltr") != EditingTriState::kFalse) {
    data.writing_direction_left_to_right |=
        WebContextMenuData::kCheckableMenuItemChecked;
  }
  if (selected_frame->GetEditor().SelectionHasStyle(
          CSSPropertyDirection, "rtl") != EditingTriState::kFalse) {
    data.writing_direction_right_to_left |=
        WebContextMenuData::kCheckableMenuItemChecked;
  }

  data.referrer_policy = static_cast<WebReferrerPolicy>(
      selected_frame->GetDocument()->GetReferrerPolicy());

  // Filter out custom menu elements and add them into the data.
  PopulateSubMenuItems(default_menu->Items(), data.custom_items);

  if (auto* anchor = ToHTMLAnchorElementOrNull(r.URLElement())) {
    // Extract suggested filename for saving file.
    data.suggested_filename = anchor->FastGetAttribute(HTMLNames::downloadAttr);

    // If the anchor wants to suppress the referrer, update the referrerPolicy
    // accordingly.
    if (anchor->HasRel(kRelationNoReferrer))
      data.referrer_policy = kWebReferrerPolicyNever;

    data.link_text = anchor->innerText();
  }

  // Find the input field type.
  if (auto* input = ToHTMLInputElementOrNull(r.InnerNode())) {
    if (input->type() == InputTypeNames::password)
      data.input_field_type = WebContextMenuData::kInputFieldTypePassword;
    else if (input->IsTextField())
      data.input_field_type = WebContextMenuData::kInputFieldTypePlainText;
    else
      data.input_field_type = WebContextMenuData::kInputFieldTypeOther;
  } else {
    data.input_field_type = WebContextMenuData::kInputFieldTypeNone;
  }

  IntRect anchor;
  IntRect focus;
  selected_frame->Selection().ComputeAbsoluteBounds(anchor, focus);
  anchor = selected_frame->View()->ContentsToViewport(anchor);
  focus = selected_frame->View()->ContentsToViewport(focus);
  int left = std::min(focus.X(), anchor.X());
  int top = std::min(focus.Y(), anchor.Y());
  int right = std::max(focus.X() + focus.Width(), anchor.X() + anchor.Width());
  int bottom =
      std::max(focus.Y() + focus.Height(), anchor.Y() + anchor.Height());
  data.selection_rect = WebRect(left, top, right - left, bottom - top);

  data.source_type = source_type;

  const bool from_touch = source_type == kMenuSourceTouch ||
                          source_type == kMenuSourceLongPress ||
                          source_type == kMenuSourceLongTap;
  if (from_touch && !ShouldShowContextMenuFromTouch(data))
    return false;

  WebLocalFrameImpl* selected_web_frame =
      WebLocalFrameImpl::FromFrame(selected_frame);
  if (!selected_web_frame || !selected_web_frame->Client())
    return false;

  selected_web_frame->Client()->ShowContextMenu(data);
  return true;
}

}  // namespace blink
