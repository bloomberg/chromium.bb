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
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/CoreExport.h"
#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/loader/resource/ImageResourceObserver.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/WeakPtr.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/TaskType.h"

namespace blink {

class IncrementLoadEventDelayCount;
class Element;
class LayoutImageResource;
class ExceptionState;
class ScriptState;

class CORE_EXPORT ImageLoader : public GarbageCollectedFinalized<ImageLoader>,
                                public ImageResourceObserver {
  USING_PRE_FINALIZER(ImageLoader, Dispose);

 public:
  explicit ImageLoader(Element*);
  ~ImageLoader() override;

  void Trace(blink::Visitor*);

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

  ImageResourceContent* GetContent() const { return image_content_.Get(); }

  // Cancels pending load events, and doesn't dispatch new ones.
  // Note: ClearImage/SetImage.*() are not a simple setter.
  // Check the implementation to see what they do.
  // TODO(hiroshige): Cleanup these methods.
  void ClearImage();
  void SetImageForTest(ImageResourceContent*);

  // Image document loading:
  // When |loading_image_document_| is true:
  //   Loading via ImageDocument.
  //   |image_resource_for_image_document_| points to a ImageResource that is
  //   not associated with a ResourceLoader.
  //   The corresponding ImageDocument is responsible for supplying the response
  //   and data to |image_resource_for_image_document_| and thus
  //   |image_content_|.
  // Otherwise:
  //   Normal loading via ResourceFetcher/ResourceLoader.
  //   |image_resource_for_image_document_| is null.
  bool IsLoadingImageDocument() { return loading_image_document_; }
  void SetLoadingImageDocument() { loading_image_document_ = true; }
  ImageResource* ImageResourceForImageDocument() const {
    return image_resource_for_image_document_;
  }

  bool HasPendingActivity() const { return HasPendingEvent() || pending_task_; }

  bool HasPendingError() const { return pending_error_event_.IsActive(); }

  bool HadError() const { return !failed_load_url_.IsEmpty(); }

  bool GetImageAnimationPolicy(ImageAnimationPolicy&) final;

  ScriptPromise Decode(ScriptState*, ExceptionState&);

 protected:
  void ImageChanged(ImageResourceContent*,
                    CanDeferInvalidation,
                    const IntRect*) override;
  void ImageNotifyFinished(ImageResourceContent*) override;

 private:
  class Task;

  enum class UpdateType { kAsync, kSync };

  // Called from the task or from updateFromElement to initiate the load.
  void DoUpdateFromElement(BypassMainWorldBehavior,
                           UpdateFromElementBehavior,
                           const KURL&,
                           ReferrerPolicy = kReferrerPolicyDefault,
                           UpdateType = UpdateType::kAsync);

  virtual void DispatchLoadEvent() = 0;
  virtual void NoImageResourceToLoad() {}

  bool HasPendingEvent() const;

  void DispatchPendingLoadEvent(std::unique_ptr<IncrementLoadEventDelayCount>);
  void DispatchPendingErrorEvent(std::unique_ptr<IncrementLoadEventDelayCount>);

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

  void DispatchDecodeRequestsIfComplete();
  void RejectPendingDecodes(UpdateType = UpdateType::kAsync);
  void DecodeRequestFinished(uint64_t request_id, bool success);

  Member<Element> element_;
  Member<ImageResourceContent> image_content_;
  Member<ImageResource> image_resource_for_image_document_;

  AtomicString failed_load_url_;
  WeakPtr<Task> pending_task_;  // owned by Microtask
  std::unique_ptr<IncrementLoadEventDelayCount>
      delay_until_do_update_from_element_;

  // Delaying load event: the timeline should be:
  // (0) ImageResource::Fetch() is called.
  // (1) ResourceFetcher::StartLoad(): Resource loading is actually started.
  // (2) ResourceLoader::DidFinishLoading() etc:
  //         Resource loading is finished, but SVG document load might be
  //         incomplete because of asynchronously loaded subresources.
  // (3) ImageNotifyFinished(): Image is completely loaded.
  // and we delay Document load event from (1) to (3):
  // - |ResourceFetcher::loaders_| delays Document load event from (1) to (2).
  // - |delay_until_image_notify_finished_| delays Document load event from
  //   the first ImageChanged() (at some time between (1) and (2)) until (3).
  // Ideally, we might want to delay Document load event from (1) to (3),
  // but currently we piggyback on ImageChanged() because adding a callback
  // hook at (1) might complicate the code.
  std::unique_ptr<IncrementLoadEventDelayCount>
      delay_until_image_notify_finished_;

  TaskHandle pending_load_event_;
  TaskHandle pending_error_event_;

  bool image_complete_ : 1;
  bool loading_image_document_ : 1;
  bool suppress_error_events_ : 1;

  // DecodeRequest represents a single request to the Decode() function. The
  // decode requests have one of the following states:
  //
  // - kPendingMicrotask: This is the initial state. The caller is responsible
  // for scheduling a microtask that would advance the state to the next value.
  // Images invalidated by the pending mutations microtask (|pending_task_|) do
  // not invalidate decode requests in this state. The exception is synchronous
  // updates that do not go through |pending_task_|.
  //
  // - kPendingLoad: Once the microtask runs, it advances the state to
  // kPendingLoad which waits for the image to be complete. If |pending_task_|
  // runs and modifies the image, it invalidates any DecodeRequests in this
  // state.
  //
  // - kDispatched: Once the image is loaded and the request to decode it is
  // dispatched on behalf of this DecodeRequest, the state changes to
  // kDispatched. If |pending_task_| runs and modifies the image, it invalidates
  // any DecodeRequests in this state.
  class DecodeRequest : public GarbageCollected<DecodeRequest> {
   public:
    enum State { kPendingMicrotask, kPendingLoad, kDispatched };

    DecodeRequest(ImageLoader*, ScriptPromiseResolver*);
    DecodeRequest(DecodeRequest&&) = default;
    ~DecodeRequest() = default;

    void Trace(blink::Visitor*);

    DecodeRequest& operator=(DecodeRequest&&) = default;

    uint64_t request_id() const { return request_id_; }
    State state() const { return state_; }
    ScriptPromise promise() { return resolver_->Promise(); }

    void Resolve();
    void Reject();

    void ProcessForTask();
    void NotifyDecodeDispatched();

   private:
    static uint64_t s_next_request_id_;

    uint64_t request_id_ = 0;
    State state_ = kPendingMicrotask;

    Member<ScriptPromiseResolver> resolver_;
    Member<ImageLoader> loader_;
  };

  HeapVector<Member<DecodeRequest>> decode_requests_;
};

}  // namespace blink

#endif
