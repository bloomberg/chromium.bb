// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlDownloadButtonElement.h"

#include "core/dom/events/Event.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/media/HTMLMediaElementControlsList.h"
#include "core/html/media/HTMLMediaSource.h"
#include "core/input_type_names.h"
#include "core/page/Page.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/MediaDownloadInProductHelpManager.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlDownloadButtonElement::MediaControlDownloadButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaDownloadButton) {
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-internal-media-controls-download-button"));
  SetIsWanted(false);
}

bool MediaControlDownloadButtonElement::ShouldDisplayDownloadButton() {
  if (!MediaElement().SupportsSave())
    return false;

  // The attribute disables the download button.
  // This is run after `SupportSave()` to guarantee that it is recorded only if
  // it blocks the download button from showing up.
  if (MediaElement().ControlsListInternal()->ShouldHideDownload()) {
    UseCounter::Count(MediaElement().GetDocument(),
                      WebFeature::kHTMLMediaElementControlsListNoDownload);
    return false;
  }

  return true;
}

WebLocalizedString::Name
MediaControlDownloadButtonElement::GetOverflowStringName() const {
  return WebLocalizedString::kOverflowMenuDownload;
}

bool MediaControlDownloadButtonElement::HasOverflowButton() const {
  return true;
}

void MediaControlDownloadButtonElement::Trace(blink::Visitor* visitor) {
  MediaControlInputElement::Trace(visitor);
}

const char* MediaControlDownloadButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "DownloadOverflowButton" : "DownloadButton";
}

void MediaControlDownloadButtonElement::UpdateShownState() {
  MediaControlInputElement::UpdateShownState();

  if (GetMediaControls().DownloadInProductHelp()) {
    GetMediaControls().DownloadInProductHelp()->SetDownloadButtonVisibility(
        IsWanted() && DoesFit());
  }
}

void MediaControlDownloadButtonElement::DefaultEventHandler(Event* event) {
  const KURL& url = MediaElement().currentSrc();
  if (event->type() == EventTypeNames::click &&
      !(url.IsNull() || url.IsEmpty())) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.Download"));
    ResourceRequest request(url);
    request.SetUIStartTime(
        (event->PlatformTimeStamp() - TimeTicks()).InSecondsF());
    request.SetInputPerfMetricReportPolicy(
        InputToLoadPerfMetricReportPolicy::kReportLink);
    request.SetSuggestedFilename(MediaElement().title());
    request.SetRequestContext(WebURLRequest::kRequestContextDownload);
    request.SetRequestorOrigin(SecurityOrigin::Create(GetDocument().Url()));
    GetDocument().GetFrame()->Client()->DownloadURL(request);
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
