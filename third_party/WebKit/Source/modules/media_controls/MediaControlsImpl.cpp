/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/media_controls/MediaControlsImpl.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/string_or_trusted_html.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/MutationObserverInit.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/ShadowRoot.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/PointerEvent.h"
#include "core/frame/DOMVisualViewport.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/geometry/DOMRect.h"
#include "core/html/media/AutoplayPolicy.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/media/HTMLMediaElementControlsList.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackContainer.h"
#include "core/html/track/TextTrackList.h"
#include "core/layout/LayoutObject.h"
#include "core/page/SpatialNavigation.h"
#include "core/resize_observer/ResizeObserver.h"
#include "core/resize_observer/ResizeObserverEntry.h"
#include "modules/media_controls/MediaControlsMediaEventListener.h"
#include "modules/media_controls/MediaControlsOrientationLockDelegate.h"
#include "modules/media_controls/MediaControlsResourceLoader.h"
#include "modules/media_controls/MediaControlsRotateToFullscreenDelegate.h"
#include "modules/media_controls/MediaControlsWindowEventListener.h"
#include "modules/media_controls/MediaDownloadInProductHelpManager.h"
#include "modules/media_controls/elements/MediaControlButtonPanelElement.h"
#include "modules/media_controls/elements/MediaControlCastButtonElement.h"
#include "modules/media_controls/elements/MediaControlCurrentTimeDisplayElement.h"
#include "modules/media_controls/elements/MediaControlDownloadButtonElement.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "modules/media_controls/elements/MediaControlFullscreenButtonElement.h"
#include "modules/media_controls/elements/MediaControlLoadingPanelElement.h"
#include "modules/media_controls/elements/MediaControlMuteButtonElement.h"
#include "modules/media_controls/elements/MediaControlOverflowMenuButtonElement.h"
#include "modules/media_controls/elements/MediaControlOverflowMenuListElement.h"
#include "modules/media_controls/elements/MediaControlOverlayEnclosureElement.h"
#include "modules/media_controls/elements/MediaControlOverlayPlayButtonElement.h"
#include "modules/media_controls/elements/MediaControlPanelElement.h"
#include "modules/media_controls/elements/MediaControlPanelEnclosureElement.h"
#include "modules/media_controls/elements/MediaControlPictureInPictureButtonElement.h"
#include "modules/media_controls/elements/MediaControlPlayButtonElement.h"
#include "modules/media_controls/elements/MediaControlRemainingTimeDisplayElement.h"
#include "modules/media_controls/elements/MediaControlScrubbingMessageElement.h"
#include "modules/media_controls/elements/MediaControlTextTrackListElement.h"
#include "modules/media_controls/elements/MediaControlTimelineElement.h"
#include "modules/media_controls/elements/MediaControlToggleClosedCaptionsButtonElement.h"
#include "modules/media_controls/elements/MediaControlVolumeSliderElement.h"
#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"
#include "modules/remoteplayback/RemotePlayback.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/runtime_enabled_features.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebSize.h"

namespace blink {

namespace {

// TODO(steimel): should have better solution than hard-coding pixel values.
// Defined in core/css/mediaControls.css, core/css/mediaControlsAndroid.css,
// and core/paint/MediaControlsPainter.cpp.
constexpr int kOverlayPlayButtonWidth = 48;
constexpr int kOverlayPlayButtonHeight = 48;
constexpr int kOverlayBottomMargin = 10;
constexpr int kAndroidMediaPanelHeight = 48;

constexpr int kMinWidthForOverlayPlayButton = kOverlayPlayButtonWidth;
constexpr int kMinHeightForOverlayPlayButton = kOverlayPlayButtonHeight +
                                               kAndroidMediaPanelHeight +
                                               (2 * kOverlayBottomMargin);

constexpr int kMinScrubbingMessageWidth = 300;

// If you change this value, then also update the corresponding value in
// LayoutTests/media/media-controls.js.
const double kTimeWithoutMouseMovementBeforeHidingMediaControls = 3;

const char* kStateCSSClasses[7] = {
    "phase-pre-ready state-no-source",    // kNoSource
    "phase-pre-ready state-no-metadata",  // kNotLoaded
    "state-loading-metadata",             // kLoadingMetadata
    "phase-ready state-stopped",          // kStopped
    "phase-ready state-playing",          // kPlaying
    "phase-ready state-buffering",        // kBuffering
    "phase-ready state-scrubbing",        // kScrubbing
};

// The padding in pixels inside the button panel.
constexpr int kModernControlsAudioButtonPadding = 20;
constexpr int kModernControlsVideoButtonPadding = 26;

const char kShowDefaultPosterCSSClass[] = "use-default-poster";
const char kActAsAudioControlsCSSClass[] = "audio-only";
const char kScrubbingMessageCSSClass[] = "scrubbing-message";

bool ShouldShowFullscreenButton(const HTMLMediaElement& media_element) {
  // Unconditionally allow the user to exit fullscreen if we are in it
  // now.  Especially on android, when we might not yet know if
  // fullscreen is supported, we sometimes guess incorrectly and show
  // the button earlier, and we don't want to remove it here if the
  // user chose to enter fullscreen.  crbug.com/500732 .
  if (media_element.IsFullscreen())
    return true;

  if (!media_element.IsHTMLVideoElement())
    return false;

  if (!media_element.HasVideo())
    return false;

  if (!Fullscreen::FullscreenEnabled(media_element.GetDocument()))
    return false;

  if (media_element.ControlsListInternal()->ShouldHideFullscreen()) {
    UseCounter::Count(media_element.GetDocument(),
                      WebFeature::kHTMLMediaElementControlsListNoFullscreen);
    return false;
  }

  return true;
}

bool ShouldShowPictureInPictureButton(HTMLMediaElement& media_element) {
  return media_element.SupportsPictureInPicture();
}

bool ShouldShowCastButton(HTMLMediaElement& media_element) {
  if (media_element.FastHasAttribute(HTMLNames::disableremoteplaybackAttr))
    return false;

  // Explicitly do not show cast button when the mediaControlsEnabled setting is
  // false to make sure the overlay does not appear.
  Document& document = media_element.GetDocument();
  if (document.GetSettings() &&
      !document.GetSettings()->GetMediaControlsEnabled()) {
    return false;
  }

  // The page disabled the button via the attribute.
  if (media_element.ControlsListInternal()->ShouldHideRemotePlayback()) {
    UseCounter::Count(
        media_element.GetDocument(),
        WebFeature::kHTMLMediaElementControlsListNoRemotePlayback);
    return false;
  }

  RemotePlayback* remote =
      HTMLMediaElementRemotePlayback::remote(media_element);
  return remote && remote->RemotePlaybackAvailable();
}

bool PreferHiddenVolumeControls(const Document& document) {
  return !document.GetSettings() ||
         document.GetSettings()->GetPreferHiddenVolumeControls();
}

}  // namespace

class MediaControlsImpl::BatchedControlUpdate {
  WTF_MAKE_NONCOPYABLE(BatchedControlUpdate);
  STACK_ALLOCATED();

 public:
  explicit BatchedControlUpdate(MediaControlsImpl* controls)
      : controls_(controls) {
    DCHECK(IsMainThread());
    DCHECK_GE(batch_depth_, 0);
    ++batch_depth_;
  }
  ~BatchedControlUpdate() {
    DCHECK(IsMainThread());
    DCHECK_GT(batch_depth_, 0);
    if (!(--batch_depth_))
      controls_->ComputeWhichControlsFit();
  }

 private:
  Member<MediaControlsImpl> controls_;
  static int batch_depth_;
};

// Count of number open batches for controls visibility.
int MediaControlsImpl::BatchedControlUpdate::batch_depth_ = 0;

class MediaControlsImpl::MediaControlsResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit MediaControlsResizeObserverDelegate(MediaControlsImpl* controls)
      : controls_(controls) {
    DCHECK(controls);
  }
  ~MediaControlsResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    DCHECK_EQ(entries[0]->target(), controls_->MediaElement());
    controls_->NotifyElementSizeChanged(entries[0]->contentRect());
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(controls_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<MediaControlsImpl> controls_;
};

// Observes changes to the HTMLMediaElement attributes that affect controls.
class MediaControlsImpl::MediaElementMutationCallback
    : public MutationObserver::Delegate {
 public:
  explicit MediaElementMutationCallback(MediaControlsImpl* controls)
      : controls_(controls), observer_(MutationObserver::Create(this)) {
    MutationObserverInit init;
    init.setAttributeOldValue(true);
    init.setAttributes(true);
    init.setAttributeFilter({HTMLNames::disableremoteplaybackAttr.ToString(),
                             HTMLNames::disablepictureinpictureAttr.ToString(),
                             HTMLNames::posterAttr.ToString()});
    observer_->observe(&controls_->MediaElement(), init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return &controls_->GetDocument();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    for (const auto& record : records) {
      if (record->type() != "attributes")
        continue;

      const Element& element = *ToElement(record->target());
      if (record->oldValue() == element.getAttribute(record->attributeName()))
        continue;

      if (record->attributeName() ==
          HTMLNames::disableremoteplaybackAttr.ToString()) {
        controls_->RefreshCastButtonVisibilityWithoutUpdate();
      }

      if (record->attributeName() ==
              HTMLNames::disablepictureinpictureAttr.ToString() &&
          controls_->picture_in_picture_button_) {
        controls_->picture_in_picture_button_->SetIsWanted(
            ShouldShowPictureInPictureButton(controls_->MediaElement()));
      }

      if (record->attributeName() == HTMLNames::posterAttr.ToString())
        controls_->UpdateCSSClassFromState();

      BatchedControlUpdate batch(controls_);
    }
  }

  void Disconnect() { observer_->disconnect(); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(controls_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<MediaControlsImpl> controls_;
  Member<MutationObserver> observer_;
};

// static
bool MediaControlsImpl::IsModern() {
  return RuntimeEnabledFeatures::ModernMediaControlsEnabled();
}

bool MediaControlsImpl::IsTouchEvent(Event* event) {
  return event->IsTouchEvent() || event->IsGestureEvent() ||
         (event->IsMouseEvent() && ToMouseEvent(event)->FromTouch());
}

MediaControlsImpl::MediaControlsImpl(HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      MediaControls(media_element),
      overlay_enclosure_(nullptr),
      overlay_play_button_(nullptr),
      overlay_cast_button_(nullptr),
      enclosure_(nullptr),
      panel_(nullptr),
      play_button_(nullptr),
      timeline_(nullptr),
      scrubbing_message_(nullptr),
      current_time_display_(nullptr),
      duration_display_(nullptr),
      mute_button_(nullptr),
      volume_slider_(nullptr),
      toggle_closed_captions_button_(nullptr),
      text_track_list_(nullptr),
      overflow_list_(nullptr),
      media_button_panel_(nullptr),
      loading_panel_(nullptr),
      picture_in_picture_button_(nullptr),
      cast_button_(nullptr),
      fullscreen_button_(nullptr),
      download_button_(nullptr),
      media_event_listener_(new MediaControlsMediaEventListener(this)),
      window_event_listener_(MediaControlsWindowEventListener::Create(
          this,
          WTF::BindRepeating(&MediaControlsImpl::HideAllMenus,
                             WrapWeakPersistent(this)))),
      orientation_lock_delegate_(nullptr),
      rotate_to_fullscreen_delegate_(nullptr),
      hide_media_controls_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kUnspecedTimer),
          this,
          &MediaControlsImpl::HideMediaControlsTimerFired),
      hide_timer_behavior_flags_(kIgnoreNone),
      is_mouse_over_controls_(false),
      is_paused_for_scrubbing_(false),
      resize_observer_(ResizeObserver::Create(
          media_element.GetDocument(),
          new MediaControlsResizeObserverDelegate(this))),
      element_size_changed_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kUnspecedTimer),
          this,
          &MediaControlsImpl::ElementSizeChangedTimerFired),
      keep_showing_until_timer_fires_(false) {
  resize_observer_->observe(&media_element);
}

MediaControlsImpl* MediaControlsImpl::Create(HTMLMediaElement& media_element,
                                             ShadowRoot& shadow_root) {
  MediaControlsImpl* controls = new MediaControlsImpl(media_element);
  controls->SetShadowPseudoId(AtomicString("-webkit-media-controls"));
  controls->InitializeControls();
  controls->Reset();

  if (RuntimeEnabledFeatures::VideoFullscreenOrientationLockEnabled() &&
      media_element.IsHTMLVideoElement()) {
    // Initialize the orientation lock when going fullscreen feature.
    controls->orientation_lock_delegate_ =
        new MediaControlsOrientationLockDelegate(
            ToHTMLVideoElement(media_element));
  }
  if (RuntimeEnabledFeatures::VideoRotateToFullscreenEnabled() &&
      media_element.IsHTMLVideoElement()) {
    // Initialize the rotate-to-fullscreen feature.
    controls->rotate_to_fullscreen_delegate_ =
        new MediaControlsRotateToFullscreenDelegate(
            ToHTMLVideoElement(media_element));
  }

  // Initialize download in-product-help for video elements if enabled.
  if (media_element.GetDocument().GetSettings() &&
      media_element.GetDocument()
          .GetSettings()
          ->GetMediaDownloadInProductHelpEnabled() &&
      media_element.IsHTMLVideoElement()) {
    controls->download_iph_manager_ =
        new MediaDownloadInProductHelpManager(*controls);
  }

  MediaControlsResourceLoader::InjectMediaControlsUAStyleSheet();

  shadow_root.AppendChild(controls);
  return controls;
}

// The media controls DOM structure looks like:
//
// MediaControlsImpl
//     (-webkit-media-controls)
// +-MediaControlLoadingPanelElement
// |    (-internal-media-controls-loading-panel)
// |    {if ModernMediaControlsEnabled}
// +-MediaControlOverlayEnclosureElement
// |    (-webkit-media-controls-overlay-enclosure)
// | +-MediaControlOverlayPlayButtonElement
// | |    (-webkit-media-controls-overlay-play-button)
// | | {if mediaControlsOverlayPlayButtonEnabled}
// | \-MediaControlCastButtonElement
// |     (-internal-media-controls-overlay-cast-button)
// \-MediaControlPanelEnclosureElement
//   |    (-webkit-media-controls-enclosure)
//   \-MediaControlPanelElement
//     |    (-webkit-media-controls-panel)
//     |  {if ModernMediaControlsEnabled and is video element and is Android}
//     +-MediaControlScrubbingMessageElement
//     |  (-internal-media-controls-scrubbing-message)
//     |  {if ModernMediaControlsEnabled, otherwise
//     |   contents are directly attached to parent.
//     +-MediaControlOverlayPlayButtonElement
//     |  (-webkit-media-controls-overlay-play-button)
//     |  {if ModernMediaControlsEnabled}
//     +-MediaControlButtonPanelElement
//     |  |  (-internal-media-controls-button-panel)
//     |  |  <video> only, otherwise children are directly attached to parent
//     |  +-MediaControlPlayButtonElement
//     |  |   (-webkit-media-controls-play-button)
//     |  +-MediaControlCurrentTimeDisplayElement
//     |  |    (-webkit-media-controls-current-time-display)
//     |  +-MediaControlRemainingTimeDisplayElement
//     |  |    (-webkit-media-controls-time-remaining-display)
//     |  +-HTMLDivElement
//     |  |    (-internal-media-controls-button-spacer)
//     |  |    {if ModernMediaControls is enabled and is video element}
//     |  +-MediaControlMuteButtonElement
//     |  |    (-webkit-media-controls-mute-button)
//     |  +-MediaControlVolumeSliderElement
//     |  |    (-webkit-media-controls-volume-slider)
//     |  +-MediaControlPictureInPictureButtonElement
//     |  |   (-webkit-media-controls-picture-in-picture-button)
//     |  +-MediaControlFullscreenButtonElement
//     |  |    (-webkit-media-controls-fullscreen-button)
//     |  +-MediaControlDownloadButtonElement
//     |  |    (-internal-media-controls-download-button)
//     |  +-MediaControlToggleClosedCaptionsButtonElement
//     |  |    (-webkit-media-controls-toggle-closed-captions-button)
//     |  +-MediaControlCastButtonElement
//     |    (-internal-media-controls-cast-button)
//     \-MediaControlTimelineElement
//          (-webkit-media-controls-timeline)
// +-MediaControlTextTrackListElement
// |    (-internal-media-controls-text-track-list)
// | {for each renderable text track}
//  \-MediaControlTextTrackListItem
//  |   (-internal-media-controls-text-track-list-item)
//  +-MediaControlTextTrackListItemInput
//  |    (-internal-media-controls-text-track-list-item-input)
//  +-MediaControlTextTrackListItemCaptions
//  |    (-internal-media-controls-text-track-list-kind-captions)
//  +-MediaControlTextTrackListItemSubtitles
//       (-internal-media-controls-text-track-list-kind-subtitles)
void MediaControlsImpl::InitializeControls() {
  if (IsModern() && MediaElement().IsHTMLVideoElement()) {
    loading_panel_ = new MediaControlLoadingPanelElement(*this);
    AppendChild(loading_panel_);
  }

  overlay_enclosure_ = new MediaControlOverlayEnclosureElement(*this);

  if (RuntimeEnabledFeatures::MediaControlsOverlayPlayButtonEnabled() ||
      IsModern()) {
    overlay_play_button_ = new MediaControlOverlayPlayButtonElement(*this);

    if (!IsModern())
      overlay_enclosure_->AppendChild(overlay_play_button_);
  }

  overlay_cast_button_ = new MediaControlCastButtonElement(*this, true);
  overlay_enclosure_->AppendChild(overlay_cast_button_);

  AppendChild(overlay_enclosure_);

  // Create an enclosing element for the panel so we can visually offset the
  // controls correctly.
  enclosure_ = new MediaControlPanelEnclosureElement(*this);

  panel_ = new MediaControlPanelElement(*this);

  // If using the modern media controls, the buttons should belong to a
  // seperate button panel. This is because they are displayed in two lines.
  if (IsModern() && MediaElement().IsHTMLVideoElement()) {
    media_button_panel_ = new MediaControlButtonPanelElement(*this);
    if (RuntimeEnabledFeatures::DoubleTapToJumpOnVideoEnabled()) {
      scrubbing_message_ = new MediaControlScrubbingMessageElement(*this);
    }
  }

  play_button_ = new MediaControlPlayButtonElement(*this);

  current_time_display_ = new MediaControlCurrentTimeDisplayElement(*this);
  current_time_display_->SetIsWanted(true);

  duration_display_ = new MediaControlRemainingTimeDisplayElement(*this);
  timeline_ = new MediaControlTimelineElement(*this);
  mute_button_ = new MediaControlMuteButtonElement(*this);

  volume_slider_ = new MediaControlVolumeSliderElement(*this);
  if (PreferHiddenVolumeControls(GetDocument()))
    volume_slider_->SetIsWanted(false);

  // TODO(apacible): Enable for modern controls when SVG is added.
  if (RuntimeEnabledFeatures::PictureInPictureEnabled() && !IsModern() &&
      MediaElement().IsHTMLVideoElement()) {
    picture_in_picture_button_ =
        new MediaControlPictureInPictureButtonElement(*this);
    picture_in_picture_button_->SetIsWanted(
        ShouldShowPictureInPictureButton(MediaElement()));
  }

  fullscreen_button_ = new MediaControlFullscreenButtonElement(*this);
  download_button_ = new MediaControlDownloadButtonElement(*this);
  cast_button_ = new MediaControlCastButtonElement(*this, false);
  toggle_closed_captions_button_ =
      new MediaControlToggleClosedCaptionsButtonElement(*this);
  overflow_menu_ = new MediaControlOverflowMenuButtonElement(*this);

  PopulatePanel();
  enclosure_->AppendChild(panel_);

  AppendChild(enclosure_);

  text_track_list_ = new MediaControlTextTrackListElement(*this);
  AppendChild(text_track_list_);


  overflow_list_ = new MediaControlOverflowMenuListElement(*this);
  AppendChild(overflow_list_);

  // The order in which we append elements to the overflow list is significant
  // because it determines how the elements show up in the overflow menu
  // relative to each other.  The first item appended appears at the top of the
  // overflow menu.
  overflow_list_->AppendChild(play_button_->CreateOverflowElement(
      new MediaControlPlayButtonElement(*this)));
  overflow_list_->AppendChild(fullscreen_button_->CreateOverflowElement(
      new MediaControlFullscreenButtonElement(*this)));
  overflow_list_->AppendChild(download_button_->CreateOverflowElement(
      new MediaControlDownloadButtonElement(*this)));
  overflow_list_->AppendChild(mute_button_->CreateOverflowElement(
      new MediaControlMuteButtonElement(*this)));
  overflow_list_->AppendChild(cast_button_->CreateOverflowElement(
      new MediaControlCastButtonElement(*this, false)));
  overflow_list_->AppendChild(
      toggle_closed_captions_button_->CreateOverflowElement(
          new MediaControlToggleClosedCaptionsButtonElement(*this)));
  if (RuntimeEnabledFeatures::PictureInPictureEnabled() && !IsModern() &&
      MediaElement().IsHTMLVideoElement()) {
    overflow_list_->AppendChild(
        picture_in_picture_button_->CreateOverflowElement(
            new MediaControlPictureInPictureButtonElement(*this)));
  }

  // Set the default CSS classes.
  UpdateCSSClassFromState();
}

void MediaControlsImpl::PopulatePanel() {
  // Clear the panels.
  panel_->setInnerHTML(StringOrTrustedHTML::FromString(""));
  if (media_button_panel_)
    media_button_panel_->setInnerHTML(StringOrTrustedHTML::FromString(""));

  Element* button_panel = panel_;
  if (IsModern() && MediaElement().IsHTMLVideoElement() &&
      !is_acting_as_audio_controls_) {
    if (scrubbing_message_)
      panel_->AppendChild(scrubbing_message_);
    panel_->AppendChild(overlay_play_button_);
    panel_->AppendChild(media_button_panel_);
    button_panel = media_button_panel_;
  }

  button_panel->AppendChild(play_button_);
  button_panel->AppendChild(current_time_display_);
  button_panel->AppendChild(duration_display_);

  if (IsModern() && MediaElement().IsHTMLVideoElement()) {
    MediaControlElementsHelper::CreateDiv(
        "-internal-media-controls-button-spacer", button_panel);
  }

  panel_->AppendChild(timeline_);

  button_panel->AppendChild(mute_button_);
  button_panel->AppendChild(volume_slider_);

  if (picture_in_picture_button_)
    button_panel->AppendChild(picture_in_picture_button_);

  button_panel->AppendChild(fullscreen_button_);
  button_panel->AppendChild(download_button_);
  button_panel->AppendChild(cast_button_);
  button_panel->AppendChild(toggle_closed_captions_button_);
  button_panel->AppendChild(overflow_menu_);
}

Node::InsertionNotificationRequest MediaControlsImpl::InsertedInto(
    ContainerNode* root) {
  if (!MediaElement().isConnected())
    return HTMLDivElement::InsertedInto(root);

  // TODO(mlamouri): we should show the controls instead of having
  // HTMLMediaElement do it.

  // m_windowEventListener doesn't need to be re-attached as it's only needed
  // when a menu is visible.
  media_event_listener_->Attach();
  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Attach();
  if (rotate_to_fullscreen_delegate_)
    rotate_to_fullscreen_delegate_->Attach();

  if (!resize_observer_) {
    resize_observer_ =
        ResizeObserver::Create(MediaElement().GetDocument(),
                               new MediaControlsResizeObserverDelegate(this));
    HTMLMediaElement& html_media_element = MediaElement();
    resize_observer_->observe(&html_media_element);
  }

  if (!element_mutation_callback_)
    element_mutation_callback_ = new MediaElementMutationCallback(this);

  return HTMLDivElement::InsertedInto(root);
}

void MediaControlsImpl::UpdateCSSClassFromState() {
  const ControlsState state = State();
  StringBuilder builder;
  builder.Append(kStateCSSClasses[state]);

  if (MediaElement().IsHTMLVideoElement() && !is_acting_as_audio_controls_ &&
      !VideoElement().HasAvailableVideoFrame() &&
      VideoElement().PosterImageURL().IsEmpty() &&
      state <= ControlsState::kLoadingMetadata) {
    builder.Append(" ");
    builder.Append(kShowDefaultPosterCSSClass);
  }

  if (is_acting_as_audio_controls_) {
    builder.Append(" ");
    builder.Append(kActAsAudioControlsCSSClass);
  }

  const AtomicString& classes = builder.ToAtomicString();
  if (getAttribute("class") != classes)
    setAttribute("class", classes);

  if (loading_panel_)
    loading_panel_->UpdateDisplayState();
}

MediaControlsImpl::ControlsState MediaControlsImpl::State() const {
  HTMLMediaElement::NetworkState network_state =
      MediaElement().getNetworkState();
  HTMLMediaElement::ReadyState ready_state = MediaElement().getReadyState();

  if (is_scrubbing_ && ready_state != HTMLMediaElement::kHaveNothing)
    return ControlsState::kScrubbing;

  switch (network_state) {
    case HTMLMediaElement::kNetworkEmpty:
    case HTMLMediaElement::kNetworkNoSource:
      return ControlsState::kNoSource;
    case HTMLMediaElement::kNetworkLoading:
      if (ready_state == HTMLMediaElement::kHaveNothing)
        return ControlsState::kLoadingMetadata;
      if (!MediaElement().paused() &&
          ready_state != HTMLMediaElement::kHaveEnoughData) {
        return ControlsState::kBuffering;
      }
      break;
    case HTMLMediaElement::kNetworkIdle:
      if (ready_state == HTMLMediaElement::kHaveNothing)
        return ControlsState::kNotLoaded;
      break;
  }

  if (!MediaElement().paused())
    return ControlsState::kPlaying;
  return ControlsState::kStopped;
}

void MediaControlsImpl::RemovedFrom(ContainerNode*) {
  DCHECK(!MediaElement().isConnected());

  // TODO(mlamouri): we hide show the controls instead of having
  // HTMLMediaElement do it.

  window_event_listener_->Stop();
  media_event_listener_->Detach();
  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Detach();
  if (rotate_to_fullscreen_delegate_)
    rotate_to_fullscreen_delegate_->Detach();

  if (resize_observer_) {
    resize_observer_->disconnect();
    resize_observer_.Clear();
  }

  if (element_mutation_callback_) {
    element_mutation_callback_->Disconnect();
    element_mutation_callback_.Clear();
  }
}

void MediaControlsImpl::Reset() {
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_events_in_shadow;
  BatchedControlUpdate batch(this);

  OnDurationChange();

  // Show everything that we might hide.
  current_time_display_->SetIsWanted(true);
  timeline_->SetIsWanted(true);

  // If the player has entered an error state, force it into the paused state.
  if (MediaElement().error())
    MediaElement().pause();

  UpdatePlayState();

  UpdateCurrentTimeDisplay();

  timeline_->SetPosition(MediaElement().currentTime());

  OnVolumeChange();
  OnTextTracksAddedOrRemoved();

  if (picture_in_picture_button_) {
    picture_in_picture_button_->SetIsWanted(
        ShouldShowPictureInPictureButton(MediaElement()));
  }

  OnControlsListUpdated();
}

void MediaControlsImpl::OnControlsListUpdated() {
  BatchedControlUpdate batch(this);

  fullscreen_button_->SetIsWanted(ShouldShowFullscreenButton(MediaElement()));

  RefreshCastButtonVisibilityWithoutUpdate();

  download_button_->SetIsWanted(
      download_button_->ShouldDisplayDownloadButton());
}

LayoutObject* MediaControlsImpl::PanelLayoutObject() {
  return panel_->GetLayoutObject();
}

LayoutObject* MediaControlsImpl::ContainerLayoutObject() {
  return GetLayoutObject();
}

void MediaControlsImpl::MaybeShow() {
  panel_->SetIsWanted(true);
  panel_->SetIsDisplayed(true);
  if (overlay_play_button_ && !is_paused_for_scrubbing_)
    overlay_play_button_->UpdateDisplayType();
  // Only make the controls visible if they won't get hidden by OnTimeUpdate.
  if (MediaElement().paused() || !ShouldHideMediaControls())
    MakeOpaque();
  if (download_iph_manager_)
    download_iph_manager_->SetControlsVisibility(true);
  if (loading_panel_)
    loading_panel_->OnControlsShown();

  timeline_->OnControlsShown();
}

void MediaControlsImpl::Hide() {
  panel_->SetIsWanted(false);
  panel_->SetIsDisplayed(false);

  // When we permanently hide the native media controls, we no longer want to
  // hide the cursor, since the video will be using custom controls.
  ShowCursor();

  if (overlay_play_button_)
    overlay_play_button_->SetIsWanted(false);
  if (download_iph_manager_)
    download_iph_manager_->SetControlsVisibility(false);
  if (loading_panel_)
    loading_panel_->OnControlsHidden();

  // Cancel scrubbing if necessary.
  if (is_scrubbing_) {
    is_paused_for_scrubbing_ = false;
    EndScrubbing();
  }
  timeline_->OnControlsHidden();
}

bool MediaControlsImpl::IsVisible() const {
  return panel_->IsOpaque();
}

void MediaControlsImpl::MakeOpaque() {
  ShowCursor();
  panel_->MakeOpaque();
}

void MediaControlsImpl::MakeOpaqueFromPointerEvent() {
  if (IsVisible())
    return;

  MakeOpaque();
  pointer_event_did_show_controls_ = true;
}

void MediaControlsImpl::MakeTransparent() {
  // Only hide the cursor if the controls are enabled.
  if (MediaElement().ShouldShowControls())
    HideCursor();
  panel_->MakeTransparent();
}

bool MediaControlsImpl::ShouldHideMediaControls(unsigned behavior_flags) const {
  // Never hide for a media element without visual representation.
  if (!MediaElement().IsHTMLVideoElement() || !MediaElement().HasVideo() ||
      ToHTMLVideoElement(MediaElement()).IsRemotingInterstitialVisible()) {
    return false;
  }

  RemotePlayback* remote =
      HTMLMediaElementRemotePlayback::remote(MediaElement());
  if (remote && remote->GetState() != WebRemotePlaybackState::kDisconnected)
    return false;

  // Keep the controls visible as long as the timer is running.
  const bool ignore_wait_for_timer = behavior_flags & kIgnoreWaitForTimer;
  if (!ignore_wait_for_timer && keep_showing_until_timer_fires_)
    return false;

  // Don't hide if the mouse is over the controls.
  const bool ignore_controls_hover = behavior_flags & kIgnoreControlsHover;
  if (!ignore_controls_hover && AreVideoControlsHovered())
    return false;

  // Don't hide if the mouse is over the video area.
  const bool ignore_video_hover = behavior_flags & kIgnoreVideoHover;
  if (!ignore_video_hover && is_mouse_over_controls_)
    return false;

  // Don't hide if focus is on the HTMLMediaElement or within the
  // controls/shadow tree. (Perform the checks separately to avoid going
  // through all the potential ancestor hosts for the focused element.)
  const bool ignore_focus = behavior_flags & kIgnoreFocus;
  if (!ignore_focus && (MediaElement().IsFocused() ||
                        contains(GetDocument().FocusedElement()))) {
    return false;
  }

  // Don't hide the media controls when a panel is showing.
  if (text_track_list_->IsWanted() || overflow_list_->IsWanted())
    return false;

  // Don't hide the media controls while the in product help is showing.
  if (download_iph_manager_ && download_iph_manager_->IsShowingInProductHelp())
    return false;

  return true;
}

bool MediaControlsImpl::AreVideoControlsHovered() const {
  DCHECK(MediaElement().IsHTMLVideoElement());

  if (IsModern())
    return media_button_panel_->IsHovered() || timeline_->IsHovered();

  return panel_->IsHovered();
}

void MediaControlsImpl::UpdatePlayState() {
  if (is_paused_for_scrubbing_)
    return;

  if (overlay_play_button_)
    overlay_play_button_->UpdateDisplayType();
  play_button_->UpdateDisplayType();
}

HTMLDivElement* MediaControlsImpl::PanelElement() {
  return panel_;
}

void MediaControlsImpl::BeginScrubbing() {
  if (!MediaElement().paused()) {
    is_paused_for_scrubbing_ = true;
    MediaElement().pause();
  }

  if (scrubbing_message_) {
    scrubbing_message_->SetIsWanted(true);
    if (scrubbing_message_->DoesFit())
      panel_->setAttribute("class", kScrubbingMessageCSSClass);
  }

  is_scrubbing_ = true;
  UpdateCSSClassFromState();
}

void MediaControlsImpl::EndScrubbing() {
  if (is_paused_for_scrubbing_) {
    is_paused_for_scrubbing_ = false;
    if (MediaElement().paused())
      MediaElement().Play();
  }

  if (scrubbing_message_) {
    scrubbing_message_->SetIsWanted(false);
    panel_->removeAttribute("class");
  }

  is_scrubbing_ = false;
  UpdateCSSClassFromState();
}

void MediaControlsImpl::UpdateCurrentTimeDisplay() {
  current_time_display_->SetCurrentValue(MediaElement().currentTime());
}

void MediaControlsImpl::ToggleTextTrackList() {
  if (!MediaElement().HasClosedCaptions()) {
    text_track_list_->SetVisible(false);
    return;
  }

  if (!text_track_list_->IsWanted())
    window_event_listener_->Start();

  PositionPopupMenu(text_track_list_);

  text_track_list_->SetVisible(!text_track_list_->IsWanted());
}

void MediaControlsImpl::ShowTextTrackAtIndex(unsigned index_to_enable) {
  TextTrackList* track_list = MediaElement().textTracks();
  if (index_to_enable >= track_list->length())
    return;
  TextTrack* track = track_list->AnonymousIndexedGetter(index_to_enable);
  if (track && track->CanBeRendered())
    track->setMode(TextTrack::ShowingKeyword());
}

void MediaControlsImpl::DisableShowingTextTracks() {
  TextTrackList* track_list = MediaElement().textTracks();
  for (unsigned i = 0; i < track_list->length(); ++i) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (track->mode() == TextTrack::ShowingKeyword())
      track->setMode(TextTrack::DisabledKeyword());
  }
}

String MediaControlsImpl::GetTextTrackLabel(TextTrack* track) const {
  if (!track) {
    return MediaElement().GetLocale().QueryString(
        WebLocalizedString::kTextTracksOff);
  }

  String track_label = track->label();

  if (track_label.IsEmpty())
    track_label = track->language();

  if (track_label.IsEmpty()) {
    track_label = String(MediaElement().GetLocale().QueryString(
        WebLocalizedString::kTextTracksNoLabel,
        String::Number(track->TrackIndex() + 1)));
  }

  return track_label;
}

void MediaControlsImpl::RefreshCastButtonVisibility() {
  RefreshCastButtonVisibilityWithoutUpdate();
  BatchedControlUpdate batch(this);
}

void MediaControlsImpl::RefreshCastButtonVisibilityWithoutUpdate() {
  if (!ShouldShowCastButton(MediaElement())) {
    cast_button_->SetIsWanted(false);
    overlay_cast_button_->SetIsWanted(false);
    return;
  }

  // The reason for the autoplay muted test is that some pages (e.g. vimeo.com)
  // have an autoplay background video which has to be muted on Android to play.
  // In such cases we don't want to automatically show the cast button, since
  // it looks strange and is unlikely to correspond with anything the user wants
  // to do.  If a user does want to cast a muted autoplay video then they can
  // still do so by touching or clicking on the video, which will cause the cast
  // button to appear. Note that this concerns various animated images websites
  // too.
  if (!MediaElement().ShouldShowControls() &&
      !MediaElement().GetAutoplayPolicy().IsOrWillBeAutoplayingMuted()) {
    // Note that this is a case where we add the overlay cast button
    // without wanting the panel cast button.  We depend on the fact
    // that computeWhichControlsFit() won't change overlay cast button
    // visibility in the case where the cast button isn't wanted.
    // We don't call compute...() here, but it will be called as
    // non-cast changes (e.g., resize) occur.  If the panel button
    // is shown, however, compute...() will take control of the
    // overlay cast button if it needs to hide it from the panel.
    if (RuntimeEnabledFeatures::MediaCastOverlayButtonEnabled())
      overlay_cast_button_->TryShowOverlay();
    cast_button_->SetIsWanted(false);
  } else if (MediaElement().ShouldShowControls()) {
    overlay_cast_button_->SetIsWanted(false);
    cast_button_->SetIsWanted(true);
  }
}

void MediaControlsImpl::ShowOverlayCastButtonIfNeeded() {
  if (MediaElement().ShouldShowControls() ||
      !ShouldShowCastButton(MediaElement()) ||
      !RuntimeEnabledFeatures::MediaCastOverlayButtonEnabled()) {
    return;
  }

  overlay_cast_button_->TryShowOverlay();
  ResetHideMediaControlsTimer();
}

void MediaControlsImpl::EnterFullscreen() {
  Fullscreen::RequestFullscreen(MediaElement());
}

void MediaControlsImpl::ExitFullscreen() {
  Fullscreen::ExitFullscreen(GetDocument());
}

void MediaControlsImpl::RemotePlaybackStateChanged() {
  cast_button_->UpdateDisplayType();
  overlay_cast_button_->UpdateDisplayType();
}

void MediaControlsImpl::UpdateOverflowMenuWanted() const {
  // If the bool is true then the element is "sticky" this means that we will
  // always try and show it unless there is not room for it.
  std::pair<MediaControlElementBase*, bool> row_elements[] = {
      std::make_pair(play_button_.Get(), true),
      std::make_pair(mute_button_.Get(), true),
      std::make_pair(fullscreen_button_.Get(), true),
      std::make_pair(current_time_display_.Get(), true),
      std::make_pair(duration_display_.Get(), true),
      picture_in_picture_button_.Get()
          ? std::make_pair(picture_in_picture_button_.Get(), false)
          : std::make_pair(nullptr, false),
      std::make_pair(cast_button_.Get(), false),
      std::make_pair(download_button_.Get(), false),
      std::make_pair(toggle_closed_captions_button_.Get(), false),
  };

  // These are the elements in order of priority that take up vertical room.
  MediaControlElementBase* column_elements[] = {
      overlay_play_button_.Get(), media_button_panel_.Get(), timeline_.Get(),
  };

  // Current size of the media controls.
  WebSize controls_size = size_;

  // The video controls are more than one row so we need to allocate vertical
  // room and hide the overlay play button if there is not enough room.
  if (MediaElement().IsHTMLVideoElement() && !is_acting_as_audio_controls_) {
    controls_size.width -= kModernControlsVideoButtonPadding;

    // Allocate vertical room for the column elements.
    for (MediaControlElementBase* element : column_elements) {
      WebSize element_size = element->GetSizeOrDefault();
      if (controls_size.height - element_size.height >= 0) {
        element->SetDoesFit(true);
        controls_size.height -= element_size.height;
      } else {
        element->SetDoesFit(false);
      }
    }

    // If we cannot show the overlay play button, show the normal one.
    play_button_->SetIsWanted(!overlay_play_button_->DoesFit());
  } else {
    controls_size.width -= kModernControlsAudioButtonPadding;

    // Undo any IsWanted/DoesFit changes made in the above block if we're
    // switching to act as audio controls.
    if (is_acting_as_audio_controls_) {
      play_button_->SetIsWanted(true);

      for (MediaControlElementBase* element : column_elements)
        element->SetDoesFit(true);
    }
  }

  // Go through the elements and if they are sticky allocate them to the panel
  // if we have enough room. If not (or they are not sticky) then add them to
  // the overflow menu. Once we have run out of room add_elements will be
  // made false and no more elements will be added.
  MediaControlElementBase* last_element = nullptr;
  bool add_elements = true;
  bool overflow_wanted = false;
  for (std::pair<MediaControlElementBase*, bool> pair : row_elements) {
    MediaControlElementBase* element = pair.first;
    if (!element)
      continue;

    // If the element is wanted then it should take up space, otherwise skip it.
    element->SetOverflowElementIsWanted(false);
    if (!element->IsWanted())
      continue;

    // Get the size of the element and see if we should allocate space to it.
    WebSize element_size = element->GetSizeOrDefault();
    bool does_fit = add_elements && pair.second &&
                    ((controls_size.width - element_size.width) >= 0);
    element->SetDoesFit(does_fit);

    // The element does fit and is sticky so we should allocate space for it. If
    // we cannot fit this element we should stop allocating space for other
    // elements.
    if (does_fit) {
      controls_size.width -= element_size.width;
      last_element = element;
    } else {
      add_elements = false;
      if (element->HasOverflowButton()) {
        overflow_wanted = true;
        element->SetOverflowElementIsWanted(true);
      }
    }
  }

  overflow_menu_->SetDoesFit(overflow_wanted);
  overflow_menu_->SetIsWanted(overflow_wanted);

  // If we want to show the overflow button and we do not have any space to show
  // it then we should hide the last shown element.
  int overflow_icon_width = overflow_menu_->GetSizeOrDefault().width;
  if (overflow_wanted && last_element &&
      controls_size.width < overflow_icon_width) {
    last_element->SetDoesFit(false);
    last_element->SetOverflowElementIsWanted(true);
  }

  MaybeRecordElementsDisplayed();

  if (download_iph_manager_)
    download_iph_manager_->UpdateInProductHelp();
}

void MediaControlsImpl::UpdateScrubbingMessageFits() const {
  if (scrubbing_message_)
    scrubbing_message_->SetDoesFit(size_.Width() >= kMinScrubbingMessageWidth);
}

void MediaControlsImpl::MaybeToggleControlsFromTap() {
  if (MediaElement().paused())
    return;

  // If the controls are visible we should try to hide them unless they should
  // be kept around for another reason. If the controls are not visible then
  // show them and start the timer to automatically hide them. If a pointer
  // event showed the controls in this batch of events then we should not hiden
  // the controls.
  if (IsVisible() && !pointer_event_did_show_controls_) {
    MakeTransparent();
  } else {
    MakeOpaque();
    if (ShouldHideMediaControls(kIgnoreWaitForTimer)) {
      keep_showing_until_timer_fires_ = true;
      StartHideMediaControlsTimer();
    }

    pointer_event_did_show_controls_ = false;
  }
}

void MediaControlsImpl::DefaultEventHandler(Event* event) {
  HTMLDivElement::DefaultEventHandler(event);

  // Do not handle events to not interfere with the rest of the page if no
  // controls should be visible.
  if (!MediaElement().ShouldShowControls())
    return;

  // Add IgnoreControlsHover to m_hideTimerBehaviorFlags when we see a touch
  // event, to allow the hide-timer to do the right thing when it fires.
  // FIXME: Preferably we would only do this when we're actually handling the
  // event here ourselves.
  bool is_touch_event = IsTouchEvent(event);
  hide_timer_behavior_flags_ |=
      is_touch_event ? kIgnoreControlsHover : kIgnoreNone;

  // Touch events are treated differently to avoid fake mouse events to trigger
  // random behavior. The expect behaviour for touch is that a tap will show the
  // controls and they will hide when the timer to hide fires.
  if (is_touch_event) {
    if (event->type() != EventTypeNames::gesturetap)
      return;

    if (!ContainsRelatedTarget(event)) {
      if (!MediaElement().paused()) {
        if (!IsVisible()) {
          MakeOpaque();
          // When the panel switches from invisible to visible, we need to mark
          // the event handled to avoid buttons below the tap to be activated.
          event->SetDefaultHandled();
        }
        if (ShouldHideMediaControls(kIgnoreWaitForTimer)) {
          keep_showing_until_timer_fires_ = true;
          StartHideMediaControlsTimer();
        }
      }
    }

    return;
  }

  if (event->type() == EventTypeNames::pointerover) {
    if (!ContainsRelatedTarget(event)) {
      is_mouse_over_controls_ = true;
      if (!MediaElement().paused()) {
        MakeOpaqueFromPointerEvent();
        StartHideMediaControlsIfNecessary();
      }
    }
    return;
  }

  if (event->type() == EventTypeNames::pointerout) {
    if (!ContainsRelatedTarget(event)) {
      is_mouse_over_controls_ = false;
      StopHideMediaControlsTimer();
    }
    return;
  }

  // The pointer event has finished so we should clear the bit.
  if (event->type() == EventTypeNames::mouseout) {
    pointer_event_did_show_controls_ = false;
    return;
  }

  if (event->type() == EventTypeNames::pointermove) {
    // When we get a mouse move, show the media controls, and start a timer
    // that will hide the media controls after a 3 seconds without a mouse move.
    MakeOpaqueFromPointerEvent();
    if (ShouldHideMediaControls(kIgnoreVideoHover))
      StartHideMediaControlsTimer();
    return;
  }

  // If the user is interacting with the controls via the keyboard, don't hide
  // the controls. This will fire when the user tabs between controls (focusin)
  // or when they seek either the timeline or volume sliders (input).
  if (event->type() == EventTypeNames::focusin ||
      event->type() == EventTypeNames::input)
    ResetHideMediaControlsTimer();

  if (event->IsKeyboardEvent() &&
      !IsSpatialNavigationEnabled(GetDocument().GetFrame())) {
    const String& key = ToKeyboardEvent(event)->key();
    if (key == "Enter" || ToKeyboardEvent(event)->keyCode() == ' ') {
      if (IsModern()) {
        overlay_play_button_->OnMediaKeyboardEvent(event);
      } else {
        play_button_->OnMediaKeyboardEvent(event);
      }
      return;
    }
    if (key == "ArrowLeft" || key == "ArrowRight" || key == "Home" ||
        key == "End") {
      timeline_->OnMediaKeyboardEvent(event);
      return;
    }
    // We don't allow the user to change the volume on modern media controls.
    if (!IsModern() && (key == "ArrowDown" || key == "ArrowUp")) {
      for (int i = 0; i < 5; i++)
        volume_slider_->OnMediaKeyboardEvent(event);
      return;
    }
  }
}

void MediaControlsImpl::HideMediaControlsTimerFired(TimerBase*) {
  unsigned behavior_flags =
      hide_timer_behavior_flags_ | kIgnoreFocus | kIgnoreVideoHover;
  hide_timer_behavior_flags_ = kIgnoreNone;
  keep_showing_until_timer_fires_ = false;

  if (MediaElement().paused())
    return;

  if (!ShouldHideMediaControls(behavior_flags))
    return;

  MakeTransparent();
  overlay_cast_button_->SetIsWanted(false);
}

void MediaControlsImpl::StartHideMediaControlsTimer() {
  hide_media_controls_timer_.StartOneShot(
      kTimeWithoutMouseMovementBeforeHidingMediaControls, FROM_HERE);
}

void MediaControlsImpl::StopHideMediaControlsTimer() {
  keep_showing_until_timer_fires_ = false;
  hide_media_controls_timer_.Stop();
}

void MediaControlsImpl::ResetHideMediaControlsTimer() {
  StopHideMediaControlsTimer();
  if (!MediaElement().paused())
    StartHideMediaControlsTimer();
}

void MediaControlsImpl::HideCursor() {
  SetInlineStyleProperty(CSSPropertyCursor, "none", false);
}

void MediaControlsImpl::ShowCursor() {
  RemoveInlineStyleProperty(CSSPropertyCursor);
}

bool MediaControlsImpl::ContainsRelatedTarget(Event* event) {
  if (!event->IsPointerEvent())
    return false;
  EventTarget* related_target = ToPointerEvent(event)->relatedTarget();
  if (!related_target)
    return false;
  return contains(related_target->ToNode());
}

void MediaControlsImpl::OnVolumeChange() {
  mute_button_->UpdateDisplayType();
  volume_slider_->SetVolume(MediaElement().muted() ? 0
                                                   : MediaElement().volume());

  // Update visibility of volume controls.
  // TODO(mlamouri): it should not be part of the volumechange handling because
  // it is using audio availability as input.
  BatchedControlUpdate batch(this);
  volume_slider_->SetIsWanted(MediaElement().HasAudio() &&
                              !PreferHiddenVolumeControls(GetDocument()));
  mute_button_->SetIsWanted(MediaElement().HasAudio());
}

void MediaControlsImpl::OnFocusIn() {
  if (!MediaElement().ShouldShowControls())
    return;

  ResetHideMediaControlsTimer();
  MaybeShow();
}

void MediaControlsImpl::OnTimeUpdate() {
  timeline_->SetPosition(MediaElement().currentTime());
  UpdateCurrentTimeDisplay();

  // 'timeupdate' might be called in a paused state. The controls should not
  // become transparent in that case.
  if (MediaElement().paused()) {
    MakeOpaque();
    return;
  }

  if (IsVisible() && ShouldHideMediaControls())
    MakeTransparent();

  if (download_iph_manager_)
    download_iph_manager_->UpdateInProductHelp();
}

void MediaControlsImpl::OnDurationChange() {
  BatchedControlUpdate batch(this);

  const double duration = MediaElement().duration();
  bool was_finite_duration = std::isfinite(duration_display_->CurrentValue());

  // Update the displayed current time/duration.
  duration_display_->SetCurrentValue(duration);
  duration_display_->SetIsWanted(std::isfinite(duration));
  // TODO(crbug.com/756698): Determine if this is still needed since the format
  // of the current time no longer depends on the duration.
  UpdateCurrentTimeDisplay();

  // Update the timeline (the UI with the seek marker).
  timeline_->SetDuration(duration);
  if (!was_finite_duration && std::isfinite(duration)) {
    download_button_->SetIsWanted(
        download_button_->ShouldDisplayDownloadButton());
  }
}

void MediaControlsImpl::OnPlay() {
  UpdatePlayState();
  timeline_->SetPosition(MediaElement().currentTime());
  UpdateCurrentTimeDisplay();
  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnPlaying() {
  timeline_->OnPlaying();
  if (download_iph_manager_)
    download_iph_manager_->SetIsPlaying(true);

  StartHideMediaControlsTimer();
  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnPause() {
  UpdatePlayState();
  timeline_->SetPosition(MediaElement().currentTime());
  UpdateCurrentTimeDisplay();
  MakeOpaque();

  StopHideMediaControlsTimer();

  if (download_iph_manager_)
    download_iph_manager_->SetIsPlaying(false);

  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnTextTracksAddedOrRemoved() {
  toggle_closed_captions_button_->UpdateDisplayType();
  toggle_closed_captions_button_->SetIsWanted(
      MediaElement().HasClosedCaptions());
  BatchedControlUpdate batch(this);
}

void MediaControlsImpl::OnTextTracksChanged() {
  toggle_closed_captions_button_->UpdateDisplayType();
}

void MediaControlsImpl::OnError() {
  // TODO(mlamouri): we should only change the aspects of the control that need
  // to be changed.
  Reset();
  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnLoadedMetadata() {
  // TODO(mlamouri): we should only change the aspects of the control that need
  // to be changed.
  Reset();
  UpdateCSSClassFromState();

  if (ShouldActAsAudioControls() != is_acting_as_audio_controls_) {
    if (is_acting_as_audio_controls_)
      StopActingAsAudioControls();
    else
      StartActingAsAudioControls();
  }
}

void MediaControlsImpl::OnEnteredFullscreen() {
  fullscreen_button_->SetIsFullscreen(true);
  StopHideMediaControlsTimer();
  StartHideMediaControlsTimer();
}

void MediaControlsImpl::OnExitedFullscreen() {
  fullscreen_button_->SetIsFullscreen(false);
  StopHideMediaControlsTimer();
  StartHideMediaControlsTimer();
}

void MediaControlsImpl::OnPanelKeypress() {
  // If the user is interacting with the controls via the keyboard, don't hide
  // the controls. This is called when the user mutes/unmutes, turns CC on/off,
  // etc.
  ResetHideMediaControlsTimer();
}

void MediaControlsImpl::NotifyElementSizeChanged(DOMRectReadOnly* new_size) {
  // Note that this code permits a bad frame on resize, since it is
  // run after the relayout / paint happens.  It would be great to improve
  // this, but it would be even greater to move this code entirely to
  // JS and fix it there.

  IntSize old_size = size_;
  size_.SetWidth(new_size->width());
  size_.SetHeight(new_size->height());

  // Don't bother to do any work if this matches the most recent size.
  if (old_size != size_)
    element_size_changed_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void MediaControlsImpl::ElementSizeChangedTimerFired(TimerBase*) {
  ComputeWhichControlsFit();
}

void MediaControlsImpl::OnLoadingProgress() {
  timeline_->RenderBarSegments();
}

void MediaControlsImpl::ComputeWhichControlsFit() {
  // Hide all controls that don't fit, and show the ones that do.
  // This might be better suited for a layout, but since JS media controls
  // won't benefit from that anwyay, we just do it here like JS will.
  if (IsModern()) {
    UpdateOverflowMenuWanted();
    UpdateScrubbingMessageFits();
    return;
  }

  // Controls that we'll hide / show, in order of decreasing priority.
  MediaControlElementBase* elements[] = {
      // Exclude m_overflowMenu; we handle it specially.
      play_button_.Get(),
      fullscreen_button_.Get(),
      download_button_.Get(),
      timeline_.Get(),
      mute_button_.Get(),
      volume_slider_.Get(),
      toggle_closed_captions_button_.Get(),
      picture_in_picture_button_.Get() ? picture_in_picture_button_.Get()
                                       : nullptr,
      cast_button_.Get(),
      current_time_display_.Get(),
      duration_display_.Get(),
  };

  // TODO(mlamouri): we need a more dynamic way to find out the width of an
  // element.
  const int kSliderMargin = 36;  // Sliders have 18px margin on each side.

  if (!size_.Width()) {
    // No layout yet -- hide everything, then make them show up later.
    // This prevents the wrong controls from being shown briefly
    // immediately after the first layout and paint, but before we have
    // a chance to revise them.
    for (MediaControlElementBase* element : elements) {
      if (element)
        element->SetDoesFit(false);
    }
    return;
  }

  // Assume that all controls require 48px, unless we can get the computed
  // style for a button. The minimumWidth is recorded and re-use for future
  // MediaControls instances and future calls to this method given that at the
  // moment the controls button width is per plataform.
  // TODO(mlamouri): improve the mechanism without bandaid.
  static int minimum_width = 48;
  if (play_button_->GetLayoutObject() &&
      play_button_->GetLayoutObject()->Style()) {
    const ComputedStyle* style = play_button_->GetLayoutObject()->Style();
    minimum_width = ceil(style->Width().Pixels() / style->EffectiveZoom());
  } else if (overflow_menu_->GetLayoutObject() &&
             overflow_menu_->GetLayoutObject()->Style()) {
    const ComputedStyle* style = overflow_menu_->GetLayoutObject()->Style();
    minimum_width = ceil(style->Width().Pixels() / style->EffectiveZoom());
  }

  // Insert an overflow menu. However, if we see that the overflow menu
  // doesn't end up containing at least two elements, we will not display it
  // but instead make place for the first element that was dropped.
  overflow_menu_->SetDoesFit(true);
  overflow_menu_->SetIsWanted(true);
  int used_width = minimum_width;

  std::list<MediaControlElementBase*> overflow_elements;
  MediaControlElementBase* first_displaced_element = nullptr;
  // For each control that fits, enable it in order of decreasing priority.
  for (MediaControlElementBase* element : elements) {
    if (!element)
      continue;

    int width = minimum_width;
    if ((element == timeline_.Get()) || (element == volume_slider_.Get()))
      width += kSliderMargin;

    element->SetOverflowElementIsWanted(false);
    if (!element->IsWanted())
      continue;

    if (used_width + width <= size_.Width()) {
      element->SetDoesFit(true);
      used_width += width;
    } else {
      element->SetDoesFit(false);
      element->SetOverflowElementIsWanted(true);
      if (element->HasOverflowButton())
        overflow_elements.push_front(element);
      // We want a way to access the first media element that was
      // removed. If we don't end up needing an overflow menu, we can
      // use the space the overflow menu would have taken up to
      // instead display that media element.
      if (!element->HasOverflowButton() && !first_displaced_element)
        first_displaced_element = element;
    }
  }

  // If we don't have at least two overflow elements, we will not show the
  // overflow menu.
  if (overflow_elements.empty()) {
    overflow_menu_->SetIsWanted(false);
    used_width -= minimum_width;
    if (first_displaced_element) {
      int width = minimum_width;
      if ((first_displaced_element == timeline_.Get()) ||
          (first_displaced_element == volume_slider_.Get()))
        width += kSliderMargin;
      if (used_width + width <= size_.Width())
        first_displaced_element->SetDoesFit(true);
    }
  } else if (overflow_elements.size() == 1) {
    overflow_menu_->SetIsWanted(false);
    overflow_elements.front()->SetDoesFit(true);
  }

  // Decide if the overlay play button fits.
  if (overlay_play_button_) {
    bool does_fit = size_.Width() >= kMinWidthForOverlayPlayButton &&
                    size_.Height() >= kMinHeightForOverlayPlayButton;
    overlay_play_button_->SetDoesFit(does_fit);
  }

  if (download_iph_manager_)
    download_iph_manager_->UpdateInProductHelp();

  MaybeRecordElementsDisplayed();
}

void MediaControlsImpl::MaybeRecordElementsDisplayed() const {
  // Record the display state when needed. It is only recorded when the media
  // element is in a state that allows it in order to reduce noise in the
  // metrics.
  if (!MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()))
    return;

  MediaControlElementBase* elements[] = {
      play_button_.Get(),
      fullscreen_button_.Get(),
      download_button_.Get(),
      timeline_.Get(),
      mute_button_.Get(),
      volume_slider_.Get(),
      toggle_closed_captions_button_.Get(),
      picture_in_picture_button_.Get() ? picture_in_picture_button_.Get()
                                       : nullptr,
      cast_button_.Get(),
      current_time_display_.Get(),
      duration_display_.Get(),
      overlay_play_button_.Get(),
      overlay_cast_button_.Get(),
  };

  // Record which controls are used.
  for (const auto& element : elements) {
    if (element)
      element->MaybeRecordDisplayed();
  }
  overflow_menu_->MaybeRecordDisplayed();
}

void MediaControlsImpl::PositionPopupMenu(Element* popup_menu) {
  // The popup is positioned slightly on the inside of the bottom right corner.
  static constexpr int kPopupMenuMarginPx = 4;
  static const char kImportant[] = "important";
  static const char kPx[] = "px";

  DCHECK(MediaElement().getBoundingClientRect());
  DCHECK(GetDocument().domWindow());
  DCHECK(GetDocument().domWindow()->visualViewport());

  // The legacy text tracks have their own button so they should position
  // themselves based on that button.
  DOMRect* bounding_client_rect =
      (popup_menu == text_track_list_ && !IsModern())
          ? toggle_closed_captions_button_->getBoundingClientRect()
          : overflow_menu_->getBoundingClientRect();
  DOMVisualViewport* viewport = GetDocument().domWindow()->visualViewport();

  WTF::String bottom_str_value = WTF::String::Number(
      viewport->height() - bounding_client_rect->bottom() + kPopupMenuMarginPx);
  WTF::String right_str_value = WTF::String::Number(
      viewport->width() - bounding_client_rect->right() + kPopupMenuMarginPx);

  bottom_str_value.append(kPx);
  right_str_value.append(kPx);

  popup_menu->style()->setProperty(&GetDocument(), "bottom", bottom_str_value,
                                   kImportant, ASSERT_NO_EXCEPTION);
  popup_menu->style()->setProperty(&GetDocument(), "right", right_str_value,
                                   kImportant, ASSERT_NO_EXCEPTION);
}

bool MediaControlsImpl::ShouldActAsAudioControls() const {
  // A video element should act like an audio element when it has an audio track
  // but no video track.
  return IsModern() && MediaElement().IsHTMLVideoElement() &&
         MediaElement().HasAudio() && !MediaElement().HasVideo();
}

void MediaControlsImpl::StartActingAsAudioControls() {
  DCHECK(ShouldActAsAudioControls());
  DCHECK(!is_acting_as_audio_controls_);

  is_acting_as_audio_controls_ = true;
  PopulatePanel();
  UpdateCSSClassFromState();
  UpdateOverflowMenuWanted();
}

void MediaControlsImpl::StopActingAsAudioControls() {
  DCHECK(!ShouldActAsAudioControls());
  DCHECK(is_acting_as_audio_controls_);

  is_acting_as_audio_controls_ = false;
  PopulatePanel();
  UpdateCSSClassFromState();
  UpdateOverflowMenuWanted();
}

void MediaControlsImpl::Invalidate(Element* element) {
  if (!element)
    return;

  if (LayoutObject* layout_object = element->GetLayoutObject()) {
    layout_object
        ->SetShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
  }
}

void MediaControlsImpl::NetworkStateChanged() {
  Invalidate(play_button_);
  Invalidate(overlay_play_button_);
  Invalidate(mute_button_);
  Invalidate(fullscreen_button_);
  Invalidate(download_button_);
  Invalidate(timeline_);
  Invalidate(volume_slider_);

  // Update the display state of the download button in case we now have a
  // source or no longer have a source.
  download_button_->SetIsWanted(
      download_button_->ShouldDisplayDownloadButton());

  UpdateCSSClassFromState();
}

bool MediaControlsImpl::OverflowMenuVisible() {
  return overflow_list_ ? overflow_list_->IsWanted() : false;
}

void MediaControlsImpl::ToggleOverflowMenu() {
  DCHECK(overflow_list_);

  if (!overflow_list_->IsWanted())
    window_event_listener_->Start();

  PositionPopupMenu(overflow_list_);

  overflow_list_->SetIsWanted(!overflow_list_->IsWanted());
}

void MediaControlsImpl::HideAllMenus() {
  window_event_listener_->Stop();

  if (overflow_list_->IsWanted())
    overflow_list_->SetIsWanted(false);
  if (text_track_list_->IsWanted())
    text_track_list_->SetVisible(false);
}

void MediaControlsImpl::StartHideMediaControlsIfNecessary() {
  if (ShouldHideMediaControls())
    StartHideMediaControlsTimer();
}

const MediaControlDownloadButtonElement& MediaControlsImpl::DownloadButton()
    const {
  return *download_button_;
}

void MediaControlsImpl::DidDismissDownloadInProductHelp() {
  StartHideMediaControlsIfNecessary();
}

MediaDownloadInProductHelpManager* MediaControlsImpl::DownloadInProductHelp() {
  return download_iph_manager_;
}

void MediaControlsImpl::OnWaiting() {
  UpdateCSSClassFromState();
}

void MediaControlsImpl::MaybeRecordOverflowTimeToAction() {
  overflow_list_->MaybeRecordTimeTaken(
      MediaControlOverflowMenuListElement::kTimeToAction);
}

void MediaControlsImpl::OnLoadedData() {
  UpdateCSSClassFromState();
}

HTMLVideoElement& MediaControlsImpl::VideoElement() {
  DCHECK(MediaElement().IsHTMLVideoElement());
  return *ToHTMLVideoElement(&MediaElement());
}

void MediaControlsImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_mutation_callback_);
  visitor->Trace(resize_observer_);
  visitor->Trace(panel_);
  visitor->Trace(overlay_play_button_);
  visitor->Trace(overlay_enclosure_);
  visitor->Trace(play_button_);
  visitor->Trace(current_time_display_);
  visitor->Trace(timeline_);
  visitor->Trace(scrubbing_message_);
  visitor->Trace(mute_button_);
  visitor->Trace(volume_slider_);
  visitor->Trace(picture_in_picture_button_);
  visitor->Trace(toggle_closed_captions_button_);
  visitor->Trace(fullscreen_button_);
  visitor->Trace(download_button_);
  visitor->Trace(duration_display_);
  visitor->Trace(enclosure_);
  visitor->Trace(text_track_list_);
  visitor->Trace(overflow_menu_);
  visitor->Trace(overflow_list_);
  visitor->Trace(cast_button_);
  visitor->Trace(overlay_cast_button_);
  visitor->Trace(media_event_listener_);
  visitor->Trace(window_event_listener_);
  visitor->Trace(orientation_lock_delegate_);
  visitor->Trace(rotate_to_fullscreen_delegate_);
  visitor->Trace(download_iph_manager_);
  visitor->Trace(media_button_panel_);
  visitor->Trace(loading_panel_);
  MediaControls::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
