// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_TEST_MEDIA_CONTROLS_MENU_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_TEST_MEDIA_CONTROLS_MENU_HOST_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/media_controls/touchless/media_controls.mojom-blink.h"

namespace blink {

struct TestMenuHostArgList {
  WTF::Vector<mojom::MenuItem> menu_items;
  mojom::blink::VideoStatePtr video_state;
  WTF::Vector<mojom::blink::TextTrackMetadataPtr> text_tracks;
};

class TestMediaControlsMenuHost : public mojom::blink::MediaControlsMenuHost {
 public:
  mojom::blink::MediaControlsMenuHostPtr CreateMediaControlsMenuHostPtr();
  void ShowMediaMenu(
      const WTF::Vector<mojom::MenuItem>& menu_items,
      mojom::blink::VideoStatePtr video_state,
      base::Optional<WTF::Vector<mojom::blink::TextTrackMetadataPtr>>
          text_tracks,
      ShowMediaMenuCallback callback) override;

  TestMenuHostArgList& GetMenuHostArgList();
  void SetMenuResponse(mojom::blink::MenuItem menu_item, int track_index);

 private:
  mojo::Binding<mojom::blink::MediaControlsMenuHost> binding_{this};
  TestMenuHostArgList arg_list_;
  mojom::blink::MenuResponsePtr response_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_TEST_MEDIA_CONTROLS_MENU_HOST_H_
