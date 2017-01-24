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

class FetchRequest;
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

  // Use ImageResourceContent::fetch() unless ImageResource is required.
  // TODO(hiroshige): Make fetch() private.
  static ImageResource* fetch(FetchRequest&, ResourceFetcher*);

  // TODO(hiroshige): Make create() test-only by refactoring ImageDocument.
  static ImageResource* create(const ResourceRequest&);

  ~ImageResource() override;

  ImageResourceContent* getContent();
  const ImageResourceContent* getContent() const;

  void reloadIfLoFiOrPlaceholderImage(ResourceFetcher*,
                                      ReloadLoFiOrPlaceholderPolicy);

  void didAddClient(ResourceClient*) override;

  ResourcePriority priorityFromObservers() override;

  void allClientsAndObserversRemoved() override;

  PassRefPtr<const SharedBuffer> resourceBuffer() const override;
  void appendData(const char*, size_t) override;
  void error(const ResourceError&) override;
  void responseReceived(const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override;
  void finish(double finishTime = 0.0) override;

  // For compatibility, images keep loading even if there are HTTP errors.
  bool shouldIgnoreHTTPStatusCodeErrors() const override { return true; }

  bool isImage() const override { return true; }

  // MultipartImageResourceParser::Client
  void onePartInMultipartReceived(const ResourceResponse&) final;
  void multipartDataReceived(const char*, size_t) final;

  // Used by tests.
  bool isPlaceholder() const { return m_isPlaceholder; }

  DECLARE_VIRTUAL_TRACE();

 private:
  enum class MultipartParsingState : uint8_t {
    WaitingForFirstPart,
    ParsingFirstPart,
    FinishedParsingFirstPart,
  };

  class ImageResourceInfoImpl;
  class ImageResourceFactory;

  ImageResource(const ResourceRequest&,
                const ResourceLoaderOptions&,
                ImageResourceContent*,
                bool isPlaceholder);

  // Only for ImageResourceInfoImpl.
  void decodeError(bool allDataReceived);
  bool isAccessAllowed(
      SecurityOrigin*,
      ImageResourceInfo::DoesCurrentFrameHaveSingleSecurityOrigin) const;

  bool hasClientsOrObservers() const override;

  void updateImageAndClearBuffer();
  void updateImage(PassRefPtr<SharedBuffer>,
                   ImageResourceContent::UpdateImageOption,
                   bool allDataReceived);

  void checkNotify() override;

  void destroyDecodedDataIfPossible() override;
  void destroyDecodedDataForFailedRevalidation() override;

  void flushImageIfNeeded(TimerBase*);

  bool shouldReloadBrokenPlaceholder() const {
    return m_isPlaceholder && willPaintBrokenImage();
  }

  bool willPaintBrokenImage() const;

  Member<ImageResourceContent> m_content;

  // TODO(hiroshige): move |m_devicePixelRatioHeaderValue| and
  // |m_hasDevicePixelRatioHeaderValue| to ImageResourceContent and update
  // it via ImageResourceContent::updateImage().
  float m_devicePixelRatioHeaderValue;

  Member<MultipartImageResourceParser> m_multipartParser;
  MultipartParsingState m_multipartParsingState =
      MultipartParsingState::WaitingForFirstPart;
  bool m_hasDevicePixelRatioHeaderValue;

  // Indicates if the ImageResource is currently scheduling a reload, e.g.
  // because reloadIfLoFi() was called.
  bool m_isSchedulingReload;

  // Indicates if this ImageResource is either attempting to load a placeholder
  // image, or is a (possibly broken) placeholder image.
  bool m_isPlaceholder;

  Timer<ImageResource> m_flushTimer;
  double m_lastFlushTime = 0.;
};

DEFINE_RESOURCE_TYPE_CASTS(Image);

}  // namespace blink

#endif
