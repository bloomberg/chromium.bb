// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/context_menu.h"
#include "webkit/glue/glue_serialize.h"

namespace webkit_glue {

const int32 CustomContextMenuContext::kCurrentRenderWidget = kint32max;

CustomContextMenuContext::CustomContextMenuContext()
    : is_pepper_menu(false),
      request_id(0),
      render_widget_id(kCurrentRenderWidget) {
}

}  // namespace webkit_glue

ContextMenuParams::ContextMenuParams()
    : media_type(WebKit::WebContextMenuData::MediaTypeNone),
      x(0),
      y(0),
      is_image_blocked(false),
      frame_id(0),
      media_flags(0),
      spellcheck_enabled(false),
      is_editable(false),
      edit_flags(0),
      referrer_policy(WebKit::WebReferrerPolicyDefault) {
}

ContextMenuParams::ContextMenuParams(const WebKit::WebContextMenuData& data)
    : media_type(data.mediaType),
      x(data.mousePosition.x),
      y(data.mousePosition.y),
      link_url(data.linkURL),
      unfiltered_link_url(data.linkURL),
      src_url(data.srcURL),
      is_image_blocked(data.isImageBlocked),
      page_url(data.pageURL),
      keyword_url(data.keywordURL),
      frame_url(data.frameURL),
      frame_id(0),
      media_flags(data.mediaFlags),
      selection_text(data.selectedText),
      misspelled_word(data.misspelledWord),
      speech_input_enabled(data.isSpeechInputEnabled),
      spellcheck_enabled(data.isSpellCheckingEnabled),
      is_editable(data.isEditable),
#if defined(OS_MACOSX)
      writing_direction_default(data.writingDirectionDefault),
      writing_direction_left_to_right(data.writingDirectionLeftToRight),
      writing_direction_right_to_left(data.writingDirectionRightToLeft),
#endif  // OS_MACOSX
      edit_flags(data.editFlags),
      security_info(data.securityInfo),
      frame_charset(data.frameEncoding.utf8()),
      referrer_policy(data.referrerPolicy) {
  for (size_t i = 0; i < data.dictionarySuggestions.size(); ++i)
    dictionary_suggestions.push_back(data.dictionarySuggestions[i]);

  custom_context.is_pepper_menu = false;
  for (size_t i = 0; i < data.customItems.size(); ++i)
    custom_items.push_back(WebMenuItem(data.customItems[i]));

  if (!data.frameHistoryItem.isNull()) {
    frame_content_state =
        webkit_glue::HistoryItemToString(data.frameHistoryItem);
  }
}

ContextMenuParams::~ContextMenuParams() {
}
