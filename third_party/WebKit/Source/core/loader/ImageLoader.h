/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2009 Apple Inc. All rights reserved.
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

#ifndef ImageLoader_h
#define ImageLoader_h

#include <memory>
#include "core/CoreExport.h"
#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/loader/resource/ImageResourceObserver.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/WeakPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class IncrementLoadEventDelayCount;
class Element;
class ImageLoader;
class LayoutImageResource;

template <typename T>
class EventSender;
using ImageEventSender = EventSender<ImageLoader>;

class CORE_EXPORT ImageLoader : public GarbageCollectedFinalized<ImageLoader>,
                                public ImageResourceObserver {
  USING_PRE_FINALIZER(ImageLoader, Dispose);

 public:
  explicit ImageLoader(Element*);
  ~ImageLoader() override;

  DECLARE_TRACE();

  enum UpdateFromElementBehavior {
    // This should be the update behavior when the element is attached to a
    // document, or when DOM mutations trigger a new load. Starts loading if a
    // load hasn't already been started.
    kUpdateNormal,
    // This should be the update behavior when the resource was changed (via
    // 'src', 'srcset' or 'sizes'). Starts a new load even if a previous load of
    // the same resource have failed, to match Firefox's behavior.
    // FIXME - Verify that this is the right behavior according to the spec.
    kUpdateIgnorePreviousError,
    // This forces the image to update its intrinsic size, even if the image
    // source has not changed.
    kUpdateSizeChanged,
    // This force the image to refetch and reload the image source, even if it
    // has not changed.
    kUpdateForcedReload
  };

  enum BypassMainWorldBehavior {
    kBypassMainWorldCSP,
    kDoNotBypassMainWorldCSP
  };

  void UpdateFromElement(UpdateFromElementBehavior = kUpdateNormal,
                         ReferrerPolicy = kReferrerPolicyDefault);

  void ElementDidMoveToNewDocument();

  Element* GetElement() const { return element_; }
  bool ImageComplete() const { return image_complete_ && !pending_task_; }

  ImageResourceContent* GetImage() const { return image_.Get(); }
  ImageResource* ImageResourceForImageDocument() const {
    return image_resource_for_image_document_;
  }

  // Cancels pending load events, and doesn't dispatch new ones.
  // Note: ClearImage/SetImage.*() are not a simple setter.
  // Check the implementation to see what they do.
  // TODO(hiroshige): Cleanup these methods.
  void ClearImage();
  void SetImageForTest(ImageResourceContent*);

  bool IsLoadingImageDocument() { return loading_image_document_; }
  void SetLoadingImageDocument() { loading_image_document_ = true; }

  bool HasPendingActivity() const {
    return has_pending_load_event_ || has_pending_error_event_ || pending_task_;
  }

  bool HasPendingError() const { return has_pending_error_event_; }

  bool HadError() const { return !failed_load_url_.IsEmpty(); }

  void DispatchPendingEvent(ImageEventSender*);

  static void DispatchPendingLoadEvents();
  static void DispatchPendingErrorEvents();

  bool GetImageAnimationPolicy(ImageAnimationPolicy&) final;

 protected:
  void ImageNotifyFinished(ImageResourceContent*) override;

 private:
  class Task;

  // Called from the task or from updateFromElement to initiate the load.
  void DoUpdateFromElement(BypassMainWorldBehavior,
                           UpdateFromElementBehavior,
                           const KURL&,
                           ReferrerPolicy = kReferrerPolicyDefault);

  virtual void DispatchLoadEvent() = 0;
  virtual void NoImageResourceToLoad() {}

  void UpdatedHasPendingEvent();

  void DispatchPendingLoadEvent();
  void DispatchPendingErrorEvent();

  LayoutImageResource* GetLayoutImageResource();
  void UpdateLayoutObject();

  // Note: SetImage.*() are not a simple setter.
  // Check the implementation to see what they do.
  // TODO(hiroshige): Cleanup these methods.
  void SetImageForImageDocument(ImageResource*);
  void SetImageWithoutConsideringPendingLoadEvent(ImageResourceContent*);
  void UpdateImageState(ImageResourceContent*);

  void ClearFailedLoadURL();
  void DispatchErrorEvent();
  void CrossSiteOrCSPViolationOccurred(AtomicString);
  void EnqueueImageLoadingMicroTask(UpdateFromElementBehavior, ReferrerPolicy);

  void TimerFired(TimerBase*);

  KURL ImageSourceToKURL(AtomicString) const;

  // Used to determine whether to immediately initiate the load or to schedule a
  // microtask.
  bool ShouldLoadImmediately(const KURL&) const;

  // For Oilpan, we must run dispose() as a prefinalizer and call
  // m_image->removeClient(this) (and more.) Otherwise, the ImageResource can
  // invoke didAddClient() for the ImageLoader that is about to die in the
  // current lazy sweeping, and the didAddClient() can access on-heap objects
  // that have already been finalized in the current lazy sweeping.
  void Dispose();

  Member<Element> element_;
  Member<ImageResourceContent> image_;
  Member<ImageResource> image_resource_for_image_document_;
  // FIXME: Oilpan: We might be able to remove this Persistent hack when
  // ImageResourceClient is traceable.
  GC_PLUGIN_IGNORE("http://crbug.com/383741")
  Persistent<Element> keep_alive_;

  Timer<ImageLoader> deref_element_timer_;
  AtomicString failed_load_url_;
  WeakPtr<Task> pending_task_;  // owned by Microtask
  std::unique_ptr<IncrementLoadEventDelayCount> load_delay_counter_;
  bool has_pending_load_event_ : 1;
  bool has_pending_error_event_ : 1;
  bool image_complete_ : 1;
  bool loading_image_document_ : 1;
  bool element_is_protected_ : 1;
  bool suppress_error_events_ : 1;
};

}  // namespace blink

#endif
