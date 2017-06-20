/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/html/HTMLVideoElement.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/html/media/MediaCustomControlsFullscreenDetector.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/MediaRemotingInterstitial.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutVideo.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "public/platform/WebCanvas.h"

namespace blink {

using namespace HTMLNames;

namespace {

// This enum is used to record histograms. Do not reorder.
enum VideoPersistenceControlsType {
  kVideoPersistenceControlsTypeNative = 0,
  kVideoPersistenceControlsTypeCustom,
  kVideoPersistenceControlsTypeCount
};

}  // anonymous namespace

inline HTMLVideoElement::HTMLVideoElement(Document& document)
    : HTMLMediaElement(videoTag, document),
      media_remoting_status_(MediaRemotingStatus::kNotStarted),
      remoting_interstitial_(nullptr) {
  if (document.GetSettings()) {
    default_poster_url_ =
        AtomicString(document.GetSettings()->GetDefaultVideoPosterURL());
  }

  if (RuntimeEnabledFeatures::VideoFullscreenDetectionEnabled()) {
    custom_controls_fullscreen_detector_ =
        new MediaCustomControlsFullscreenDetector(*this);
  }
}

HTMLVideoElement* HTMLVideoElement::Create(Document& document) {
  HTMLVideoElement* video = new HTMLVideoElement(document);
  video->EnsureUserAgentShadowRoot();
  video->SuspendIfNeeded();
  return video;
}

DEFINE_TRACE(HTMLVideoElement) {
  visitor->Trace(image_loader_);
  visitor->Trace(custom_controls_fullscreen_detector_);
  visitor->Trace(remoting_interstitial_);
  HTMLMediaElement::Trace(visitor);
}

bool HTMLVideoElement::HasPendingActivity() const {
  return HTMLMediaElement::HasPendingActivity() ||
         (image_loader_ && image_loader_->HasPendingActivity());
}

Node::InsertionNotificationRequest HTMLVideoElement::InsertedInto(
    ContainerNode* insertion_point) {
  if (insertion_point->isConnected() && custom_controls_fullscreen_detector_)
    custom_controls_fullscreen_detector_->Attach();

  return HTMLMediaElement::InsertedInto(insertion_point);
}

void HTMLVideoElement::RemovedFrom(ContainerNode* insertion_point) {
  HTMLMediaElement::RemovedFrom(insertion_point);

  if (custom_controls_fullscreen_detector_)
    custom_controls_fullscreen_detector_->Detach();

  OnBecamePersistentVideo(false);
}

void HTMLVideoElement::ContextDestroyed(ExecutionContext* context) {
  if (custom_controls_fullscreen_detector_)
    custom_controls_fullscreen_detector_->ContextDestroyed();

  HTMLMediaElement::ContextDestroyed(context);
}

bool HTMLVideoElement::LayoutObjectIsNeeded(const ComputedStyle& style) {
  return HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLVideoElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutVideo(this);
}

void HTMLVideoElement::AttachLayoutTree(const AttachContext& context) {
  HTMLMediaElement::AttachLayoutTree(context);

  UpdateDisplayState();
  if (ShouldDisplayPosterImage()) {
    if (!image_loader_)
      image_loader_ = HTMLImageLoader::Create(this);
    image_loader_->UpdateFromElement();
    if (GetLayoutObject())
      ToLayoutImage(GetLayoutObject())
          ->ImageResource()
          ->SetImageResource(image_loader_->GetImage());
  }
}

void HTMLVideoElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == widthAttr)
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  else if (name == heightAttr)
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  else
    HTMLMediaElement::CollectStyleForPresentationAttribute(name, value, style);
}

bool HTMLVideoElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr)
    return true;
  return HTMLMediaElement::IsPresentationAttribute(name);
}

void HTMLVideoElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == posterAttr) {
    // In case the poster attribute is set after playback, don't update the
    // display state, post playback the correct state will be picked up.
    if (GetDisplayMode() < kVideo || !HasAvailableVideoFrame()) {
      // Force a poster recalc by setting m_displayMode to Unknown directly
      // before calling updateDisplayState.
      HTMLMediaElement::SetDisplayMode(kUnknown);
      UpdateDisplayState();
    }
    if (!PosterImageURL().IsEmpty()) {
      if (!image_loader_)
        image_loader_ = HTMLImageLoader::Create(this);
      image_loader_->UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    } else {
      if (GetLayoutObject())
        ToLayoutImage(GetLayoutObject())->ImageResource()->SetImageResource(0);
    }
    // Notify the player when the poster image URL changes.
    if (GetWebMediaPlayer())
      GetWebMediaPlayer()->SetPoster(PosterImageURL());
    // Media remoting doesn't show the original poster image, instead, it shows
    // a grayscaled and blurred copy.
    if (remoting_interstitial_)
      remoting_interstitial_->OnPosterImageChanged();
  } else {
    HTMLMediaElement::ParseAttribute(params);
  }
}

unsigned HTMLVideoElement::videoWidth() const {
  if (!GetWebMediaPlayer())
    return 0;
  return GetWebMediaPlayer()->NaturalSize().width;
}

unsigned HTMLVideoElement::videoHeight() const {
  if (!GetWebMediaPlayer())
    return 0;
  return GetWebMediaPlayer()->NaturalSize().height;
}

bool HTMLVideoElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == posterAttr ||
         HTMLMediaElement::IsURLAttribute(attribute);
}

const AtomicString HTMLVideoElement::ImageSourceURL() const {
  const AtomicString& url = getAttribute(posterAttr);
  if (!StripLeadingAndTrailingHTMLSpaces(url).IsEmpty())
    return url;
  return default_poster_url_;
}

void HTMLVideoElement::SetDisplayMode(DisplayMode mode) {
  DisplayMode old_mode = GetDisplayMode();
  KURL poster = PosterImageURL();

  if (!poster.IsEmpty()) {
    // We have a poster path, but only show it until the user triggers display
    // by playing or seeking and the media engine has something to display.
    // Don't show the poster if there is a seek operation or the video has
    // restarted because of loop attribute
    if (mode == kVideo && old_mode == kPoster && !HasAvailableVideoFrame())
      return;
  }

  HTMLMediaElement::SetDisplayMode(mode);

  if (GetLayoutObject() && GetDisplayMode() != old_mode)
    GetLayoutObject()->UpdateFromElement();
}

// TODO(zqzhang): this callback could be used to hide native controls instead of
// using a settings. See `HTMLMediaElement::onMediaControlsEnabledChange`.
void HTMLVideoElement::OnBecamePersistentVideo(bool value) {
  if (value) {
    // Record the type of video. If it is already fullscreen, it is a video with
    // native controls, otherwise it is assumed to be with custom controls.
    // This is only recorded when entering this mode.
    DEFINE_STATIC_LOCAL(EnumerationHistogram, histogram,
                        ("Media.VideoPersistence.ControlsType",
                         kVideoPersistenceControlsTypeCount));
    if (IsFullscreen())
      histogram.Count(kVideoPersistenceControlsTypeNative);
    else
      histogram.Count(kVideoPersistenceControlsTypeCustom);

    Element* fullscreen_element =
        Fullscreen::FullscreenElementFrom(GetDocument());
    // Only set the video in persistent mode if it is not using native controls
    // and is currently fullscreen.
    if (!fullscreen_element || IsFullscreen())
      return;

    is_persistent_ = true;
    PseudoStateChanged(CSSSelector::kPseudoVideoPersistent);

    // The video is also marked as containing a persistent video to simplify the
    // internal CSS logic.
    for (Element* element = this; element && element != fullscreen_element;
         element = element->ParentOrShadowHostElement()) {
      element->SetContainsPersistentVideo(true);
    }
    fullscreen_element->SetContainsPersistentVideo(true);
  } else {
    if (!is_persistent_)
      return;

    is_persistent_ = false;
    PseudoStateChanged(CSSSelector::kPseudoVideoPersistent);

    Element* fullscreen_element =
        Fullscreen::FullscreenElementFrom(GetDocument());
    // If the page is no longer fullscreen, the full tree will have to be
    // traversed to make sure things are cleaned up.
    for (Element* element = this; element && element != fullscreen_element;
         element = element->ParentOrShadowHostElement()) {
      element->SetContainsPersistentVideo(false);
    }
    if (fullscreen_element)
      fullscreen_element->SetContainsPersistentVideo(false);
  }
}

bool HTMLVideoElement::IsPersistent() const {
  return is_persistent_;
}

void HTMLVideoElement::UpdateDisplayState() {
  if (PosterImageURL().IsEmpty())
    SetDisplayMode(kVideo);
  else if (GetDisplayMode() < kPoster)
    SetDisplayMode(kPoster);
}

void HTMLVideoElement::PaintCurrentFrame(PaintCanvas* canvas,
                                         const IntRect& dest_rect,
                                         const PaintFlags* flags) const {
  if (!GetWebMediaPlayer())
    return;

  PaintFlags media_flags;
  if (flags) {
    media_flags = *flags;
  } else {
    media_flags.setAlpha(0xFF);
    media_flags.setFilterQuality(kLow_SkFilterQuality);
  }

  GetWebMediaPlayer()->Paint(canvas, dest_rect, media_flags);
}

bool HTMLVideoElement::CopyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    GLenum target,
    GLuint texture,
    GLenum internal_format,
    GLenum format,
    GLenum type,
    GLint level,
    bool premultiply_alpha,
    bool flip_y) {
  if (!GetWebMediaPlayer())
    return false;

  return GetWebMediaPlayer()->CopyVideoTextureToPlatformTexture(
      gl, target, texture, internal_format, format, type, level,
      premultiply_alpha, flip_y);
}

bool HTMLVideoElement::TexImageImpl(
    WebMediaPlayer::TexImageFunctionID function_id,
    GLenum target,
    gpu::gles2::GLES2Interface* gl,
    GLuint texture,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    bool flip_y,
    bool premultiply_alpha) {
  if (!GetWebMediaPlayer())
    return false;
  return GetWebMediaPlayer()->TexImageImpl(
      function_id, target, gl, texture, level, internalformat, format, type,
      xoffset, yoffset, zoffset, flip_y, premultiply_alpha);
}

bool HTMLVideoElement::HasAvailableVideoFrame() const {
  if (!GetWebMediaPlayer())
    return false;

  return GetWebMediaPlayer()->HasVideo() &&
         GetWebMediaPlayer()->GetReadyState() >=
             WebMediaPlayer::kReadyStateHaveCurrentData;
}

void HTMLVideoElement::webkitEnterFullscreen() {
  if (!IsFullscreen())
    Fullscreen::RequestFullscreen(*this, Fullscreen::RequestType::kPrefixed);
}

void HTMLVideoElement::webkitExitFullscreen() {
  if (IsFullscreen())
    Fullscreen::ExitFullscreen(GetDocument());
}

bool HTMLVideoElement::webkitSupportsFullscreen() {
  return Fullscreen::FullscreenEnabled(GetDocument());
}

bool HTMLVideoElement::webkitDisplayingFullscreen() {
  return IsFullscreen();
}

bool HTMLVideoElement::UsesOverlayFullscreenVideo() const {
  if (RuntimeEnabledFeatures::ForceOverlayFullscreenVideoEnabled())
    return true;

  return GetWebMediaPlayer() &&
         GetWebMediaPlayer()->SupportsOverlayFullscreenVideo();
}

void HTMLVideoElement::DidMoveToNewDocument(Document& old_document) {
  if (image_loader_)
    image_loader_->ElementDidMoveToNewDocument();

  HTMLMediaElement::DidMoveToNewDocument(old_document);
}

unsigned HTMLVideoElement::webkitDecodedFrameCount() const {
  if (!GetWebMediaPlayer())
    return 0;

  return GetWebMediaPlayer()->DecodedFrameCount();
}

unsigned HTMLVideoElement::webkitDroppedFrameCount() const {
  if (!GetWebMediaPlayer())
    return 0;

  return GetWebMediaPlayer()->DroppedFrameCount();
}

KURL HTMLVideoElement::PosterImageURL() const {
  String url = StripLeadingAndTrailingHTMLSpaces(ImageSourceURL());
  if (url.IsEmpty())
    return KURL();
  return GetDocument().CompleteURL(url);
}

PassRefPtr<Image> HTMLVideoElement::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize&) {
  if (!HasAvailableVideoFrame()) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  IntSize intrinsic_size(videoWidth(), videoHeight());
  // FIXME: Not sure if we dhould we be doing anything with the AccelerationHint
  // argument here?
  std::unique_ptr<ImageBuffer> image_buffer =
      ImageBuffer::Create(intrinsic_size);
  if (!image_buffer) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  PaintCurrentFrame(image_buffer->Canvas(),
                    IntRect(IntPoint(0, 0), intrinsic_size), nullptr);
  RefPtr<Image> snapshot = image_buffer->NewImageSnapshot();
  if (!snapshot) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  *status = kNormalSourceImageStatus;
  return snapshot.Release();
}

bool HTMLVideoElement::WouldTaintOrigin(
    SecurityOrigin* destination_security_origin) const {
  return !IsMediaDataCORSSameOrigin(destination_security_origin);
}

FloatSize HTMLVideoElement::ElementSize(const FloatSize&) const {
  return FloatSize(videoWidth(), videoHeight());
}

IntSize HTMLVideoElement::BitmapSourceSize() const {
  return IntSize(videoWidth(), videoHeight());
}

ScriptPromise HTMLVideoElement::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options,
    ExceptionState& exception_state) {
  DCHECK(event_target.ToLocalDOMWindow());
  if (getNetworkState() == HTMLMediaElement::kNetworkEmpty) {
    exception_state.ThrowDOMException(
        kInvalidStateError, "The provided element has not retrieved data.");
    return ScriptPromise();
  }
  if (getReadyState() <= HTMLMediaElement::kHaveMetadata) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "The provided element's player has no current data.");
    return ScriptPromise();
  }
  if ((crop_rect &&
       !ImageBitmap::IsSourceSizeValid(crop_rect->Width(), crop_rect->Height(),
                                       exception_state)) ||
      !ImageBitmap::IsSourceSizeValid(BitmapSourceSize().Width(),
                                      BitmapSourceSize().Height(),
                                      exception_state))
    return ScriptPromise();
  if (!ImageBitmap::IsResizeOptionValid(options, exception_state))
    return ScriptPromise();
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, ImageBitmap::Create(
                        this, crop_rect,
                        event_target.ToLocalDOMWindow()->document(), options));
}

void HTMLVideoElement::MediaRemotingStarted() {
  DCHECK(media_remoting_status_ == MediaRemotingStatus::kNotStarted);
  media_remoting_status_ = MediaRemotingStatus::kStarted;
  if (!remoting_interstitial_) {
    remoting_interstitial_ = new MediaRemotingInterstitial(*this);
    ShadowRoot& shadow_root = EnsureUserAgentShadowRoot();
    shadow_root.InsertBefore(remoting_interstitial_, shadow_root.firstChild());
    HTMLMediaElement::AssertShadowRootChildren(shadow_root);
  }
  remoting_interstitial_->Show();
}

void HTMLVideoElement::MediaRemotingStopped() {
  DCHECK(media_remoting_status_ == MediaRemotingStatus::kDisabled ||
         media_remoting_status_ == MediaRemotingStatus::kStarted);
  if (media_remoting_status_ != MediaRemotingStatus::kDisabled)
    media_remoting_status_ = MediaRemotingStatus::kNotStarted;
  DCHECK(remoting_interstitial_);
  remoting_interstitial_->Hide();
}

void HTMLVideoElement::DisableMediaRemoting() {
  media_remoting_status_ = MediaRemotingStatus::kDisabled;
  if (GetWebMediaPlayer())
    GetWebMediaPlayer()->RequestRemotePlaybackDisabled(true);
}

}  // namespace blink
