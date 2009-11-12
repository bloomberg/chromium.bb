// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CONTEXT_MENU_H_
#define WEBKIT_GLUE_CONTEXT_MENU_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"

// Parameters structure for ViewHostMsg_ContextMenu.
// FIXME(beng): This would be more useful in the future and more efficient
//              if the parameters here weren't so literally mapped to what
//              they contain for the ContextMenu task. It might be better
//              to make the string fields more generic so that this object
//              could be used for more contextual actions.
struct ContextMenuParams {
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

  // This is the URL of the top level page that the context menu was invoked
  // on.
  GURL page_url;

  // This is the URL of the subframe that the context menu was invoked on.
  GURL frame_url;

  // These are the parameters for the media element that the context menu
  // was invoked on.
  int media_flags;

  // This is the text of the selection that the context menu was invoked on.
  std::wstring selection_text;

  // The misspelled word under the cursor, if any. Used to generate the
  // |dictionary_suggestions| list.
  string16 misspelled_word;

  // Suggested replacements for a misspelled word under the cursor.
  // This vector gets populated in the render process host
  // by intercepting ViewHostMsg_ContextMenu in ResourceMessageFilter
  // and populating dictionary_suggestions if the type is EDITABLE
  // and the misspelled_word is not empty.
  std::vector<string16> dictionary_suggestions;

  // If editable, flag for whether spell check is enabled or not.
  bool spellcheck_enabled;

  // Whether context is editable.
  bool is_editable;

  // These flags indicate to the browser whether the renderer believes it is
  // able to perform the corresponding action.
  int edit_flags;

  // The security info for the resource we are showing the menu on.
  std::string security_info;

  // The character encoding of the frame on which the menu is invoked.
  std::string frame_charset;

  ContextMenuParams() {}

  ContextMenuParams(const WebKit::WebContextMenuData& data)
      : media_type(data.mediaType),
        x(data.mousePosition.x),
        y(data.mousePosition.y),
        link_url(data.linkURL),
        unfiltered_link_url(data.linkURL),
        src_url(data.srcURL),
        page_url(data.pageURL),
        frame_url(data.frameURL),
        media_flags(data.mediaFlags),
        selection_text(UTF16ToWideHack(data.selectedText)),
        misspelled_word(data.misspelledWord),
        spellcheck_enabled(data.isSpellCheckingEnabled),
        is_editable(data.isEditable),
        edit_flags(data.editFlags),
        security_info(data.securityInfo),
        frame_charset(data.frameEncoding.utf8()) {}
};

#endif  // WEBKIT_GLUE_CONTEXT_MENU_H_
