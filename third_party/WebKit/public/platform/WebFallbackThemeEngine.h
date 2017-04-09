/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebFallbackThemeEngine_h
#define WebFallbackThemeEngine_h

#include "WebCanvas.h"
#include "WebColor.h"
#include "WebSize.h"

namespace blink {

struct WebRect;

class WebFallbackThemeEngine {
 public:
  // The UI part which is being accessed.
  enum Part {
    // ScrollbarTheme parts
    kPartScrollbarDownArrow,
    kPartScrollbarLeftArrow,
    kPartScrollbarRightArrow,
    kPartScrollbarUpArrow,
    kPartScrollbarHorizontalThumb,
    kPartScrollbarVerticalThumb,
    kPartScrollbarHorizontalTrack,
    kPartScrollbarVerticalTrack,
    kPartScrollbarCorner,

    // LayoutTheme parts
    kPartCheckbox,
    kPartRadio,
    kPartButton,
    kPartTextField,
    kPartMenuList,
    kPartSliderTrack,
    kPartSliderThumb,
    kPartInnerSpinButton,
    kPartProgressBar
  };

  // The current state of the associated Part.
  enum State {
    kStateDisabled,
    kStateHover,
    kStateNormal,
    kStatePressed,
  };

  // Extra parameters for drawing the PartScrollbarHorizontalTrack and
  // PartScrollbarVerticalTrack.
  struct ScrollbarTrackExtraParams {
    // The bounds of the entire track, as opposed to the part being painted.
    int track_x;
    int track_y;
    int track_width;
    int track_height;
  };

  // Extra parameters for PartCheckbox, PartPushButton and PartRadio.
  struct ButtonExtraParams {
    bool checked;
    bool indeterminate;  // Whether the button state is indeterminate.
    bool is_default;     // Whether the button is default button.
    bool has_border;
    WebColor background_color;
  };

  // Extra parameters for PartTextField
  struct TextFieldExtraParams {
    bool is_text_area;
    bool is_listbox;
    WebColor background_color;
  };

  // Extra parameters for PartMenuList
  struct MenuListExtraParams {
    bool has_border;
    bool has_border_radius;
    int arrow_x;
    int arrow_y;
    int arrow_size;
    WebColor arrow_color;
    WebColor background_color;
  };

  // Extra parameters for PartSliderTrack and PartSliderThumb
  struct SliderExtraParams {
    bool vertical;
    bool in_drag;
  };

  // Extra parameters for PartInnerSpinButton
  struct InnerSpinButtonExtraParams {
    bool spin_up;
    bool read_only;
  };

  // Extra parameters for PartProgressBar
  struct ProgressBarExtraParams {
    bool determinate;
    int value_rect_x;
    int value_rect_y;
    int value_rect_width;
    int value_rect_height;
  };

  union ExtraParams {
    ScrollbarTrackExtraParams scrollbar_track;
    ButtonExtraParams button;
    TextFieldExtraParams text_field;
    MenuListExtraParams menu_list;
    SliderExtraParams slider;
    InnerSpinButtonExtraParams inner_spin;
    ProgressBarExtraParams progress_bar;
  };

  // Gets the size of the given theme part. For variable sized items
  // like vertical scrollbar thumbs, the width will be the required width of
  // the track while the height will be the minimum height.
  virtual WebSize GetSize(Part) { return WebSize(); }
  // Paint the given the given theme part.
  virtual void Paint(WebCanvas*,
                     Part,
                     State,
                     const WebRect&,
                     const ExtraParams*) {}
};

}  // namespace blink

#endif
