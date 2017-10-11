/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef ImageResource_h
#define ImageResource_h

#include "core/CoreExport.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/loader/resource/ImageResourceInfo.h"
#include "core/loader/resource/MultipartImageResourceParser.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"
#include <memory>

namespace blink {

class FetchParameters;
class ImageResourceContent;
class ResourceClient;
class ResourceFetcher;
class SecurityOrigin;

// ImageResource implements blink::Resource interface and image-specific logic
// for loading images.
// Image-related things (blink::Image and ImageResourceObserver) are handled by
// ImageResourceContent.
// Most users should use ImageResourceContent instead of ImageResource.
// https://docs.google.com/document/d/1O-fB83mrE0B_V8gzXNqHgmRLCvstTB4MMi3RnVLr8bE/edit?usp=sharing
//
// As for the lifetimes of ImageResourceContent::m_image and m_data, see this
// document:
// https://docs.google.com/document/d/1v0yTAZ6wkqX2U_M6BNIGUJpM1s0TIw1VsqpxoL7aciY/edit?usp=sharing
class CORE_EXPORT ImageResource final
    : public Resource,
      public MultipartImageResourceParser::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ImageResource);

 public:
  using ClientType = ResourceClient;

  // Use ImageResourceContent::Fetch() unless ImageResource is required.
  // TODO(hiroshige): Make Fetch() private.
  static ImageResource* Fetch(FetchParameters&, ResourceFetcher*);

  // TODO(hiroshige): Make Create() test-only by refactoring ImageDocument.
  static ImageResource* Create(const ResourceRequest&);
  static ImageResource* CreateForTest(const KURL&);

  ~ImageResource() override;

  ImageResourceContent* GetContent();
  const ImageResourceContent* GetContent() const;

  void ReloadIfLoFiOrPlaceholderImage(ResourceFetcher*,
                                      ReloadLoFiOrPlaceholderPolicy);

  void DidAddClient(ResourceClient*) override;

  ResourcePriority PriorityFromObservers() override;

  void AllClientsAndObserversRemoved() override;

  bool CanReuse(const FetchParameters&) const override;
  bool CanUseCacheValidator() const override;

  RefPtr<const SharedBuffer> ResourceBuffer() const override;
  void NotifyStartLoad() override;
  void ResponseReceived(const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override;
  void AppendData(const char*, size_t) override;
  void Finish(double finish_time, WebTaskRunner*) override;
  void FinishAsError(const ResourceError&, WebTaskRunner*) override;

  // For compatibility, images keep loading even if there are HTTP errors.
  bool ShouldIgnoreHTTPStatusCodeErrors() const override { return true; }

  // MultipartImageResourceParser::Client
  void OnePartInMultipartReceived(const ResourceResponse&) final;
  void MultipartDataReceived(const char*, size_t) final;

  bool ShouldShowPlaceholder() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  enum class MultipartParsingState : uint8_t {
    kWaitingForFirstPart,
    kParsingFirstPart,
    kFinishedParsingFirstPart,
  };

  class ImageResourceInfoImpl;
  class ImageResourceFactory;

  ImageResource(const ResourceRequest&,
                const ResourceLoaderOptions&,
                ImageResourceContent*,
                bool is_placeholder);

  // Only for ImageResourceInfoImpl.
  void DecodeError(bool all_data_received);
  bool IsAccessAllowed(
      SecurityOrigin*,
      ImageResourceInfo::DoesCurrentFrameHaveSingleSecurityOrigin) const;

  bool HasClientsOrObservers() const override;

  void UpdateImageAndClearBuffer();
  void UpdateImage(RefPtr<SharedBuffer>,
                   ImageResourceContent::UpdateImageOption,
                   bool all_data_received);

  void NotifyFinished() override;

  void DestroyDecodedDataIfPossible() override;
  void DestroyDecodedDataForFailedRevalidation() override;

  void FlushImageIfNeeded(TimerBase*);

  bool ShouldReloadBrokenPlaceholder() const;

  Member<ImageResourceContent> content_;

  // TODO(hiroshige): move |m_devicePixelRatioHeaderValue| and
  // |m_hasDevicePixelRatioHeaderValue| to ImageResourceContent and update
  // it via ImageResourceContent::updateImage().
  float device_pixel_ratio_header_value_;

  Member<MultipartImageResourceParser> multipart_parser_;
  MultipartParsingState multipart_parsing_state_ =
      MultipartParsingState::kWaitingForFirstPart;
  bool has_device_pixel_ratio_header_value_;

  // Indicates if the ImageResource is currently scheduling a reload, e.g.
  // because reloadIfLoFi() was called.
  bool is_scheduling_reload_;

  // Indicates if this ImageResource is either attempting to load a placeholder
  // image, or is a (possibly broken) placeholder image.
  enum class PlaceholderOption {
    // Do not show or reload placeholder.
    kDoNotReloadPlaceholder,

    // Show placeholder, and do not reload. The original image will still be
    // loaded and shown if the image is explicitly reloaded, e.g. when
    // ReloadIfLoFiOrPlaceholderImage is called with kReloadAlways.
    kShowAndDoNotReloadPlaceholder,

    // Do not show placeholder, reload only when decode error occurs.
    kReloadPlaceholderOnDecodeError,

    // Show placeholder and reload.
    kShowAndReloadPlaceholderAlways,
  };
  PlaceholderOption placeholder_option_;

  Timer<ImageResource> flush_timer_;
  double last_flush_time_ = 0.;

  bool is_during_finish_as_error_ = false;
};

DEFINE_RESOURCE_TYPE_CASTS(Image);

}  // namespace blink

#endif
