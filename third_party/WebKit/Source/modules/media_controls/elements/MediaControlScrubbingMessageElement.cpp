// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlScrubbingMessageElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ShadowRoot.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/media/HTMLMediaElement.h"
#include "modules/media_controls/MediaControlsResourceLoader.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

MediaControlScrubbingMessageElement::MediaControlScrubbingMessageElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaScrubbingMessage) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-scrubbing-message"));
  CreateShadowRootInternal();

  ShadowRoot* shadow_root = GetShadowRoot();

  // This stylesheet element will contain rules that are specific to the
  // scrubbing message. The shadow DOM protects these rules from bleeding
  // across to the parent DOM.
  HTMLStyleElement* style =
      HTMLStyleElement::Create(GetDocument(), CreateElementFlags());
  style->setTextContent(
      MediaControlsResourceLoader::GetScrubbingMessageStyleSheet());
  shadow_root->AppendChild(style);

  HTMLDivElement* arrow_left_div1 =
      MediaControlElementsHelper::CreateDivWithId("arrow-left1", shadow_root);
  HTMLDivElement* arrow_left_div2 =
      MediaControlElementsHelper::CreateDivWithId("arrow-left2", shadow_root);
  HTMLDivElement* message_div =
      MediaControlElementsHelper::CreateDivWithId("message", shadow_root);
  HTMLDivElement* arrow_right_div1 =
      MediaControlElementsHelper::CreateDivWithId("arrow-right1", shadow_root);
  HTMLDivElement* arrow_right_div2 =
      MediaControlElementsHelper::CreateDivWithId("arrow-right2", shadow_root);

  arrow_left_div1->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowLeftSVGImage());
  arrow_left_div2->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowLeftSVGImage());
  message_div->setInnerText(MediaElement().GetLocale().QueryString(
                                WebLocalizedString::kMediaScrubbingMessageText),
                            ASSERT_NO_EXCEPTION);
  arrow_right_div1->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowRightSVGImage());
  arrow_right_div2->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowRightSVGImage());

  SetIsWanted(false);
}

}  // namespace blink
