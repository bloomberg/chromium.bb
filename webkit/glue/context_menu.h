// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CONTEXT_MENU_H_
#define WEBKIT_GLUE_CONTEXT_MENU_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webkit_glue_export.h"
#include "webkit/glue/webmenuitem.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"

namespace webkit_glue {

struct CustomContextMenuContext {
  bool is_pepper_menu;
  int request_id;
  // The routing ID of the render widget on which the context menu is shown.
  // It could also be |kCurrentRenderWidget|, which means the render widget that
  // the corresponding ViewHostMsg_ContextMenu is sent to.
  int32 render_widget_id;
  static const int32 kCurrentRenderWidget;

  WEBKIT_GLUE_EXPORT CustomContextMenuContext();
};

}  // namespace webkit_glue

// TODO(viettrungluu): Put this in the webkit_glue namespace.
// Parameters structure for ViewHostMsg_ContextMenu.
// FIXME(beng): This would be more useful in the future and more efficient
//              if the parameters here weren't so literally mapped to what
//              they contain for the ContextMenu task. It might be better
//              to make the string fields more generic so that this object
//              could be used for more contextual actions.
struct WEBKIT_GLUE_EXPORT ContextMenuParams {
  // This is the type of Context Node that the context menu was invoked on.
  WebKit::WebContextMenuData::MediaType media_type;

  // These values represent the coordinates of the mouse when the context menu
  // was invoked.  Coords are relative to the associated RenderView's origin.
  int x;
  int y;

  // This is the URL of the link that encloses the node the context menu was
  // invoked on.
  GURL link_url;

  // The link URL to be used ONLY for "copy link address". We don't validate
  // this field in the frontend process.
  GURL unfiltered_link_url;

  // This is the source URL for the element that the context menu was
  // invoked on.  Example of elements with source URLs are img, audio, and
  // video.
  GURL src_url;

  // This is true if the context menu was invoked on a blocked image.
  bool is_image_blocked;

  // This is the URL of the top level page that the context menu was invoked
  // on.
  GURL page_url;

  // This is the absolute keyword search URL including the %s search tag when
  // the "Add as search engine..." option is clicked (left empty if not used).
  GURL keyword_url;

  // This is the URL of the subframe that the context menu was invoked on.
  GURL frame_url;

  // This is the ID of the subframe that the context menu was invoked on.
  int64 frame_id;

  // This is the history item state of the subframe that the context menu was
  // invoked on.
  std::string frame_content_state;

  // These are the parameters for the media element that the context menu
  // was invoked on.
  int media_flags;

  // This is the text of the selection that the context menu was invoked on.
  string16 selection_text;

  // The misspelled word under the cursor, if any. Used to generate the
  // |dictionary_suggestions| list.
  string16 misspelled_word;

  // Suggested replacements for a misspelled word under the cursor.
  // This vector gets populated in the render process host
  // by intercepting ViewHostMsg_ContextMenu in ResourceMessageFilter
  // and populating dictionary_suggestions if the type is EDITABLE
  // and the misspelled_word is not empty.
  std::vector<string16> dictionary_suggestions;

  // If editable, flag for whether node is speech-input enabled.
  bool speech_input_enabled;

  // If editable, flag for whether spell check is enabled or not.
  bool spellcheck_enabled;

  // Whether context is editable.
  bool is_editable;

#if defined(OS_MACOSX)
  // Writing direction menu items.
  // Currently only used on OS X.
  int writing_direction_default;
  int writing_direction_left_to_right;
  int writing_direction_right_to_left;
#endif  // OS_MACOSX

  // These flags indicate to the browser whether the renderer believes it is
  // able to perform the corresponding action.
  int edit_flags;

  // The security info for the resource we are showing the menu on.
  std::string security_info;

  // The character encoding of the frame on which the menu is invoked.
  std::string frame_charset;

  webkit_glue::CustomContextMenuContext custom_context;
  std::vector<WebMenuItem> custom_items;

  ContextMenuParams();
  ContextMenuParams(const WebKit::WebContextMenuData& data);
  ~ContextMenuParams();
};

#endif  // WEBKIT_GLUE_CONTEXT_MENU_H_
