// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlDownloadButtonElement.h"

#include "core/dom/events/Event.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAnchorElement.h"
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
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-internal-media-controls-download-button"));
  SetIsWanted(false);
}

bool MediaControlDownloadButtonElement::ShouldDisplayDownloadButton() {
  const KURL& url = MediaElement().currentSrc();

  // Check page settings to see if download is disabled.
  if (GetDocument().GetPage() &&
      GetDocument().GetPage()->GetSettings().GetHideDownloadUI())
    return false;

  // URLs that lead to nowhere are ignored.
  if (url.IsNull() || url.IsEmpty())
    return false;

  // If we have no source, we can't download.
  if (MediaElement().getNetworkState() == HTMLMediaElement::kNetworkEmpty ||
      MediaElement().getNetworkState() == HTMLMediaElement::kNetworkNoSource) {
    return false;
  }

  // Local files and blobs (including MSE) should not have a download button.
  if (url.IsLocalFile() || url.ProtocolIs("blob"))
    return false;

  // MediaStream can't be downloaded.
  if (HTMLMediaElement::IsMediaStreamURL(url.GetString()))
    return false;

  // MediaSource can't be downloaded.
  if (HTMLMediaSource::Lookup(url))
    return false;

  // HLS stream shouldn't have a download button.
  if (HTMLMediaElement::IsHLSURL(url))
    return false;

  // Infinite streams don't have a clear end at which to finish the download
  // (would require adding UI to prompt for the duration to download).
  if (MediaElement().duration() == std::numeric_limits<double>::infinity())
    return false;

  // The attribute disables the download button.
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
  visitor->Trace(anchor_);
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
    if (!anchor_) {
      HTMLAnchorElement* anchor = HTMLAnchorElement::Create(GetDocument());
      anchor->setAttribute(HTMLNames::downloadAttr, "");
      anchor_ = anchor;
    }
    anchor_->SetURL(url);
    anchor_->DispatchSimulatedClick(event);
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
