/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#ifndef WebContextMenuData_h
#define WebContextMenuData_h

#include "WebHistoryItem.h"
#include "WebMenuItemInfo.h"
#include "public/platform/WebMenuSourceType.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebReferrerPolicy.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebVector.h"

#define WEBCONTEXT_MEDIATYPEFILE_DEFINED

namespace blink {

// This struct is passed to WebViewClient::ShowContextMenu.
struct WebContextMenuData {
  enum MediaType {
    // No special node is in context.
    kMediaTypeNone,
    // An image node is selected.
    kMediaTypeImage,
    // A video node is selected.
    kMediaTypeVideo,
    // An audio node is selected.
    kMediaTypeAudio,
    // A canvas node is selected.
    kMediaTypeCanvas,
    // A file node is selected.
    kMediaTypeFile,
    // A plugin node is selected.
    kMediaTypePlugin,
    kMediaTypeLast = kMediaTypePlugin
  };
  // The type of media the context menu is being invoked on.
  MediaType media_type;

  // The x and y position of the mouse pointer (relative to the webview).
  WebPoint mouse_position;

  // The absolute URL of the link that is in context.
  WebURL link_url;

  // The absolute URL of the image/video/audio that is in context.
  WebURL src_url;

  // Whether the image in context is a null.
  bool has_image_contents;

  // If |media_type| is MediaTypeImage and |has_image_contents| is true, then
  // this contains the image's WebURLResponse.
  WebURLResponse image_response;

  // The absolute URL of the page in context.
  WebURL page_url;

  // The absolute keyword search URL including the %s search tag when the
  // "Add as search engine..." option is clicked (left empty if not used).
  WebURL keyword_url;

  // The absolute URL of the subframe in context.
  WebURL frame_url;

  // The encoding for the frame in context.
  WebString frame_encoding;

  // History state of the subframe in context.
  WebHistoryItem frame_history_item;

  enum MediaFlags {
    kMediaNone = 0x0,
    kMediaInError = 0x1,
    kMediaPaused = 0x2,
    kMediaMuted = 0x4,
    kMediaLoop = 0x8,
    kMediaCanSave = 0x10,
    kMediaHasAudio = 0x20,
    kMediaCanToggleControls = 0x40,
    kMediaControls = 0x80,
    kMediaCanPrint = 0x100,
    kMediaCanRotate = 0x200,
  };

  // Extra attributes describing media elements.
  int media_flags;

  // The text of the link that is in the context.
  WebString link_text;

  // The raw text of the selection in context.
  WebString selected_text;

  // Title attribute or alt attribute (if title is not available) of the
  // selection in context.
  WebString title_text;

  // Whether spell checking is enabled.
  bool is_spell_checking_enabled;

  // Suggested filename for saving file.
  WebString suggested_filename;

  // The editable (possibily) misspelled word.
  WebString misspelled_word;

  // If misspelledWord is not empty, holds suggestions from the dictionary.
  WebVector<WebString> dictionary_suggestions;

  // Whether context is editable.
  bool is_editable;

  enum InputFieldType {
    // Not an input field.
    kInputFieldTypeNone,
    // type = text, tel, search, number, email, url
    kInputFieldTypePlainText,
    // type = password
    kInputFieldTypePassword,
    // type = <etc.>
    kInputFieldTypeOther,
    kInputFieldTypeLast = kInputFieldTypeOther
  };

  // If this node is an input field, the type of that field.
  InputFieldType input_field_type;

  enum CheckableMenuItemFlags {
    kCheckableMenuItemDisabled = 0x0,
    kCheckableMenuItemEnabled = 0x1,
    kCheckableMenuItemChecked = 0x2,
  };

  // Writing direction menu items - values are unions of
  // CheckableMenuItemFlags.
  int writing_direction_default;
  int writing_direction_left_to_right;
  int writing_direction_right_to_left;

  enum EditFlags {
    kCanDoNone = 0x0,
    kCanUndo = 0x1,
    kCanRedo = 0x2,
    kCanCut = 0x4,
    kCanCopy = 0x8,
    kCanPaste = 0x10,
    kCanDelete = 0x20,
    kCanSelectAll = 0x40,
    kCanTranslate = 0x80,
    kCanEditRichly = 0x100,
  };

  // Which edit operations are available in the context.
  int edit_flags;

  // The referrer policy applicable to this context.
  WebReferrerPolicy referrer_policy;

  // Custom context menu items provided by the WebCore internals.
  WebVector<WebMenuItemInfo> custom_items;

  // Selection in viewport coordinates.
  WebRect selection_rect;

  WebMenuSourceType source_type;

  WebContextMenuData()
      : media_type(kMediaTypeNone),
        has_image_contents(false),
        media_flags(kMediaNone),
        is_spell_checking_enabled(false),
        is_editable(false),
        writing_direction_default(kCheckableMenuItemDisabled),
        writing_direction_left_to_right(kCheckableMenuItemEnabled),
        writing_direction_right_to_left(kCheckableMenuItemEnabled),
        edit_flags(0) {}
};

}  // namespace blink

#endif
