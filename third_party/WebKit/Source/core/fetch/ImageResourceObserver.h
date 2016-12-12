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

#ifndef ImageResourceObserver_h
#define ImageResourceObserver_h

#include "core/CoreExport.h"
#include "platform/graphics/ImageAnimationPolicy.h"
#include "platform/network/ResourceLoadPriority.h"
#include "wtf/Forward.h"

namespace blink {

class ImageResourceContent;
class IntRect;

class CORE_EXPORT ImageResourceObserver {
 public:
  virtual ~ImageResourceObserver() {}

  // Called whenever a frame of an image changes, either because we got more
  // data from the network or because we are animating. If not null, the IntRect
  // is the changed rect of the image.
  virtual void imageChanged(ImageResourceContent*, const IntRect* = 0) {}

  // Called just after imageChanged() if all image data is received or errored.
  // TODO(hiroshige): Merge imageNotifyFinished() into imageChanged().
  virtual void imageNotifyFinished(ImageResourceContent*) {}

  // Called to find out if this client wants to actually display the image. Used
  // to tell when we can halt animation. Content nodes that hold image refs for
  // example would not render the image, but LayoutImages would (assuming they
  // have visibility: visible and their layout tree isn't hidden e.g., in the
  // b/f cache or in a background tab).
  //
  // An implementation of this method is not allowed to add or remove
  // ImageResource observers.
  virtual bool willRenderImage() { return false; }

  // Called to get imageAnimation policy from settings. An implementation of
  // this method is not allowed to add or remove ImageResource observers.
  virtual bool getImageAnimationPolicy(ImageAnimationPolicy&) { return false; }

  // Return the observer's requested resource priority. An implementation of
  // this method is not allowed to add or remove ImageResource observers.
  virtual ResourcePriority computeResourcePriority() const {
    return ResourcePriority();
  }

  // Name for debugging, e.g. shown in memory-infra.
  virtual String debugName() const = 0;

  static bool isExpectedType(ImageResourceObserver*) { return true; }
};

}  // namespace blink

#endif
