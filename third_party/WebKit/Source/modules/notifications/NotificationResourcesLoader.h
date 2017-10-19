// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationResourcesLoader_h
#define NotificationResourcesLoader_h

#include <memory>
#include "modules/ModulesExport.h"
#include "modules/notifications/NotificationImageLoader.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/ThreadState.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Vector.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class ExecutionContext;
struct WebNotificationData;
struct WebNotificationResources;

// Fetches the resources specified in a given WebNotificationData. Uses a
// callback to notify the caller when all fetches have finished.
class MODULES_EXPORT NotificationResourcesLoader final
    : public GarbageCollectedFinalized<NotificationResourcesLoader> {
  USING_PRE_FINALIZER(NotificationResourcesLoader, Stop);

 public:
  // Called when all fetches have finished. Passes a pointer to the
  // NotificationResourcesLoader so callers that use multiple loaders can use
  // the same function to handle the callbacks.
  using CompletionCallback = Function<void(NotificationResourcesLoader*)>;

  explicit NotificationResourcesLoader(CompletionCallback);
  ~NotificationResourcesLoader();

  // Starts fetching the resources specified in the given WebNotificationData.
  // If all the urls for the resources are empty or invalid,
  // |m_completionCallback| will be run synchronously, otherwise it will be
  // run asynchronously when all fetches have finished. Should not be called
  // more than once.
  void Start(ExecutionContext*, const WebNotificationData&);

  // Returns a new WebNotificationResources populated with the resources that
  // have been fetched.
  std::unique_ptr<WebNotificationResources> GetResources() const;

  // Stops every loader in |m_imageLoaders|. This is also used as the
  // pre-finalizer.
  void Stop();

  virtual void Trace(blink::Visitor*);

 private:
  void LoadImage(ExecutionContext*,
                 NotificationImageLoader::Type,
                 const KURL&,
                 NotificationImageLoader::ImageCallback);
  void DidLoadImage(const SkBitmap& image);
  void DidLoadIcon(const SkBitmap& image);
  void DidLoadBadge(const SkBitmap& image);
  void DidLoadActionIcon(size_t action_index, const SkBitmap& image);

  // Decrements |m_pendingRequestCount| and runs |m_completionCallback| if
  // there are no more pending requests.
  void DidFinishRequest();

  bool started_;
  CompletionCallback completion_callback_;
  int pending_request_count_;
  HeapVector<Member<NotificationImageLoader>> image_loaders_;
  SkBitmap image_;
  SkBitmap icon_;
  SkBitmap badge_;
  Vector<SkBitmap> action_icons_;
};

}  // namespace blink

#endif  // NotificationResourcesLoader_h
