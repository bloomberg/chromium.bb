// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchRegistration_h
#define BackgroundFetchRegistration_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"

namespace blink {

class IconDefinition;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

// Represents an individual Background Fetch registration. Gives developers
// access to its properties, options, and enables them to abort the fetch.
class BackgroundFetchRegistration final
    : public GarbageCollectedFinalized<BackgroundFetchRegistration>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BackgroundFetchRegistration(String id,
                              HeapVector<IconDefinition> icons,
                              long long total_download_size,
                              String title);
  ~BackgroundFetchRegistration();

  // Sets the ServiceWorkerRegistration that this BackgroundFetchRegistration
  // has been associated with.
  void SetServiceWorkerRegistration(ServiceWorkerRegistration*);

  String id() const;
  HeapVector<IconDefinition> icons() const;
  long long totalDownloadSize() const;
  String title() const;

  ScriptPromise abort(ScriptState*);

  DECLARE_TRACE();

 private:
  void DidAbort(ScriptPromiseResolver*, mojom::blink::BackgroundFetchError);

  Member<ServiceWorkerRegistration> registration_;

  String id_;
  HeapVector<IconDefinition> icons_;
  long long total_download_size_;
  String title_;
};

}  // namespace blink

#endif  // BackgroundFetchRegistration_h
