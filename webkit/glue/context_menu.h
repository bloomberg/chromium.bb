// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CONTEXT_MENU_H_
#define WEBKIT_GLUE_CONTEXT_MENU_H_

#include <vector>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

// The type of node that the user may perform a contextual action on
// in the WebView.
struct ContextNodeType {
  enum TypeBit {
    // No node is selected
    NONE = 0x0,

    // The top page is selected
    PAGE = 0x1,

    // A subframe page is selected
    FRAME = 0x2,

    // A link is selected
    LINK = 0x4,

    // An image is selected
    IMAGE = 0x8,

    // There is a textual or mixed selection that is selected
    SELECTION = 0x10,

    // An editable element is selected
    EDITABLE = 0x20,

    // A misspelled word is selected
    MISSPELLED_WORD = 0x40,

    // A video node is selected
    VIDEO = 0x80,

    // A video node is selected
    AUDIO = 0x100,
  };

  enum Capability {
    CAN_DO_NONE = 0x0,
    CAN_UNDO = 0x1,
    CAN_REDO = 0x2,
    CAN_CUT = 0x4,
    CAN_COPY = 0x8,
    CAN_PASTE = 0x10,
    CAN_DELETE = 0x20,
    CAN_SELECT_ALL = 0x40,
  };

  int32 type;
  ContextNodeType() : type(NONE) {}
  explicit ContextNodeType(int32 t) : type(t) {}
};

// Parameters structure used in ContextMenuParams with attributes needed to
// render the context menu for media elements.
//
// TODO(ajwong): Add support for multiple audio tracks and subtitles.
struct ContextMenuMediaParams {
  // Values for the bitfield representing the state of the media player.
  // If the state is IN_ERROR, most media controls should disable
  // themselves.
  enum PlayerStateBit {
    NO_STATE = 0x0,
    IN_ERROR = 0x1,
    PAUSED = 0x2,
    MUTED = 0x4,
    LOOP = 0x8,
    CAN_SAVE = 0x10,
  };

  // A bitfield representing the current state of the player, such as
  // playing, muted, etc.
  int32 player_state;

  // Whether a playable audio track is present.
  bool has_audio;

  ContextMenuMediaParams()
      : player_state(NO_STATE), has_audio(false) {
  }
};

// Parameters structure for ViewHostMsg_ContextMenu.
// FIXME(beng): This would be more useful in the future and more efficient
//              if the parameters here weren't so literally mapped to what
//              they contain for the ContextMenu task. It might be better
//              to make the string fields more generic so that this object
//              could be used for more contextual actions.
struct ContextMenuParams {
  // This is the type of Context Node that the context menu was invoked on.
  ContextNodeType node_type;

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
  ContextMenuMediaParams media_params;

  // This is the text of the selection that the context menu was invoked on.
  std::wstring selection_text;

  // The misspelled word under the cursor, if any. Used to generate the
  // |dictionary_suggestions| list.
  std::wstring misspelled_word;

  // Suggested replacements for a misspelled word under the cursor.
  // This vector gets populated in the render process host
  // by intercepting ViewHostMsg_ContextMenu in ResourceMessageFilter
  // and populating dictionary_suggestions if the type is EDITABLE
  // and the misspelled_word is not empty.
  std::vector<std::wstring> dictionary_suggestions;

  // If editable, flag for whether spell check is enabled or not.
  bool spellcheck_enabled;

  // These flags indicate to the browser whether the renderer believes it is
  // able to perform the corresponding action.
  int edit_flags;

  // The security info for the resource we are showing the menu on.
  std::string security_info;

  // The character encoding of the frame on which the menu is invoked.
  std::string frame_charset;
};

#endif  // WEBKIT_GLUE_CONTEXT_MENU_H_
