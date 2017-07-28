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

#ifndef HTMLVideoElement_h
#define HTMLVideoElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {
class ExceptionState;
class ImageBitmapOptions;
class MediaCustomControlsFullscreenDetector;
class MediaRemotingInterstitial;

class CORE_EXPORT HTMLVideoElement final : public HTMLMediaElement,
                                           public CanvasImageSource,
                                           public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HTMLVideoElement* Create(Document&);
  DECLARE_VIRTUAL_TRACE();

  bool HasPendingActivity() const final;

  enum class MediaRemotingStatus { kNotStarted, kStarted, kDisabled };

  // Node override.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  unsigned videoWidth() const;
  unsigned videoHeight() const;

  // Fullscreen
  void webkitEnterFullscreen();
  void webkitExitFullscreen();
  bool webkitSupportsFullscreen();
  bool webkitDisplayingFullscreen();
  bool UsesOverlayFullscreenVideo() const override;

  // Statistics
  unsigned webkitDecodedFrameCount() const;
  unsigned webkitDroppedFrameCount() const;

  // Used by canvas to gain raw pixel access
  void PaintCurrentFrame(PaintCanvas*, const IntRect&, const PaintFlags*) const;

  // Used by WebGL to do GPU-GPU textures copy if possible.
  bool CopyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface*,
                                         GLenum target,
                                         GLuint texture,
                                         GLenum internal_format,
                                         GLenum format,
                                         GLenum type,
                                         GLint level,
                                         bool premultiply_alpha,
                                         bool flip_y);

  // Used by WebGL to do CPU-GPU texture upload if possible.
  bool TexImageImpl(WebMediaPlayer::TexImageFunctionID,
                    GLenum target,
                    gpu::gles2::GLES2Interface*,
                    GLuint texture,
                    GLint level,
                    GLint internalformat,
                    GLenum format,
                    GLenum type,
                    GLint xoffset,
                    GLint yoffset,
                    GLint zoffset,
                    bool flip_y,
                    bool premultiply_alpha);

  bool ShouldDisplayPosterImage() const { return GetDisplayMode() == kPoster; }

  bool HasAvailableVideoFrame() const;

  KURL PosterImageURL() const override;

  // CanvasImageSource implementation
  RefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                        AccelerationHint,
                                        SnapshotReason,
                                        const FloatSize&) override;
  bool IsVideoElement() const override { return true; }
  bool WouldTaintOrigin(SecurityOrigin*) const override;
  FloatSize ElementSize(const FloatSize&) const override;
  const KURL& SourceURL() const override { return currentSrc(); }
  bool IsHTMLVideoElement() const override { return true; }
  int SourceWidth() override { return videoWidth(); }
  int SourceHeight() override { return videoHeight(); }
  // Video elements currently always go through RAM when used as a canvas image
  // source.
  bool IsAccelerated() const override { return false; }

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) override;

  // WebMediaPlayerClient implementation.
  void OnBecamePersistentVideo(bool) final;

  bool IsPersistent() const;

  MediaRemotingStatus GetMediaRemotingStatus() const {
    return media_remoting_status_;
  }
  void DisableMediaRemoting();

  void MediaRemotingStarted() final;
  void MediaRemotingStopped() final;
  WebMediaPlayer::DisplayType DisplayType() const final;

 private:
  friend class MediaCustomControlsFullscreenDetectorTest;
  friend class HTMLMediaElementEventListenersTest;
  friend class HTMLVideoElementPersistentTest;

  HTMLVideoElement(Document&);

  // SuspendableObject functions.
  void ContextDestroyed(ExecutionContext*) final;

  bool LayoutObjectIsNeeded(const ComputedStyle&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;
  void AttachLayoutTree(AttachContext&) override;
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;
  bool IsURLAttribute(const Attribute&) const override;
  const AtomicString ImageSourceURL() const override;

  void UpdateDisplayState() override;
  void DidMoveToNewDocument(Document& old_document) override;
  void SetDisplayMode(DisplayMode) override;

  Member<HTMLImageLoader> image_loader_;
  Member<MediaCustomControlsFullscreenDetector>
      custom_controls_fullscreen_detector_;

  MediaRemotingStatus media_remoting_status_;

  Member<MediaRemotingInterstitial> remoting_interstitial_;

  AtomicString default_poster_url_;

  // TODO(mlamouri): merge these later. At the moment, the former is used for
  // CSS rules used to hide the custom controls and the latter is used to report
  // the display type. It's unclear whether using the CSS rules also when native
  // controls are used would or would not have side effects.
  bool is_persistent_ = false;
  bool is_picture_in_picture_ = false;
};

}  // namespace blink

#endif  // HTMLVideoElement_h
