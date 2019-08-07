// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_FETCHER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_FETCHER_PROPERTIES_H_

#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_status.h"

namespace blink {

class FetchClientSettingsObject;

// ResourceFetcherProperties consists of properties of the global context (e.g.,
// Frame, Worker) necessary to fetch resources. FetchClientSettingsObject
// implementing https://html.spec.whatwg.org/C/webappapis.html#settings-object
// is one such example.
//
// This class consists of pure virtual getters. Do not put operations. Do not
// put getters for a specific request such as
// GetCachePolicy(const ResourceRequest&, ResourceType). Do not put a function
// with default implementation.
//
// The distinction between FetchClientSettingsObject and
// ResourceFetcherProperties is sometimes ambiguous. Put a property in
// FetchClientSettingsObject when the property is clearly defined in the spec.
// Otherwise, put it to this class.
class PLATFORM_EXPORT ResourceFetcherProperties
    : public GarbageCollectedFinalized<ResourceFetcherProperties> {
 public:
  using ControllerServiceWorkerMode = mojom::ControllerServiceWorkerMode;

  ResourceFetcherProperties() = default;
  virtual ~ResourceFetcherProperties() = default;
  virtual void Trace(Visitor*) {}

  // Returns the client settings object bound to this global context.
  virtual const FetchClientSettingsObject& GetFetchClientSettingsObject()
      const = 0;

  // Returns whether this global context is a top-level frame.
  virtual bool IsMainFrame() const = 0;

  // Returns whether a controller service worker exists and if it has a fetch
  // handler.
  virtual ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const = 0;

  // Returns an identifier for the service worker controlling this global
  // context. This function cannot be called when
  // GetControllerServiceWorkerMode returns kNoController.
  virtual int64_t ServiceWorkerId() const = 0;

  // Returns whether this global context is suspended, which means we should
  // defer making a new request.
  // https://html.spec.whatwg.org/C/webappapis.html#pause
  virtual bool IsPaused() const = 0;

  // Returns whether this global context is detached. Note that in some cases
  // the loading pipeline continues working after detached (e.g., for fetch()
  // operations with "keepalive" specified).
  virtual bool IsDetached() const = 0;

  // Returns whether the main resource for this global context is loaded.
  virtual bool IsLoadComplete() const = 0;

  // Returns whether we should disallow a sub resource loading.
  virtual bool ShouldBlockLoadingSubResource() const = 0;

  // Returns the scheduling status of the associated frame. Returns |kNone|
  // if there is no such a frame.
  virtual scheduler::FrameStatus GetFrameStatus() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_FETCHER_PROPERTIES_H_
