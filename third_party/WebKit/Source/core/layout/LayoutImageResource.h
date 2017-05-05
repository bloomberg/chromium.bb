/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutImageResource_h
#define LayoutImageResource_h

#include "core/loader/resource/ImageResourceContent.h"
#include "core/style/StyleImage.h"

namespace blink {

class LayoutObject;

class LayoutImageResource
    : public GarbageCollectedFinalized<LayoutImageResource> {
  WTF_MAKE_NONCOPYABLE(LayoutImageResource);

 public:
  virtual ~LayoutImageResource();

  static LayoutImageResource* Create() { return new LayoutImageResource; }

  virtual void Initialize(LayoutObject*);
  virtual void Shutdown();

  void SetImageResource(ImageResourceContent*);
  ImageResourceContent* CachedImage() const { return cached_image_.Get(); }
  virtual bool HasImage() const { return cached_image_; }

  void ResetAnimation();
  bool MaybeAnimated() const;

  virtual PassRefPtr<Image> GetImage(const IntSize&) const;
  virtual bool ErrorOccurred() const {
    return cached_image_ && cached_image_->ErrorOccurred();
  }

  virtual bool ImageHasRelativeSize() const {
    return cached_image_ ? cached_image_->ImageHasRelativeSize() : false;
  }

  virtual LayoutSize ImageSize(float multiplier) const;

  virtual WrappedImagePtr ImagePtr() const { return cached_image_.Get(); }

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->Trace(cached_image_); }

 protected:
  LayoutImageResource();
  LayoutObject* layout_object_;
  Member<ImageResourceContent> cached_image_;
};

}  // namespace blink

#endif  // LayoutImage_h
