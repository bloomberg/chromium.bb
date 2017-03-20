// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchRegistration_h
#define BackgroundFetchRegistration_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"
#include "wtf/text/WTFString.h"

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
  BackgroundFetchRegistration(ServiceWorkerRegistration*,
                              String tag,
                              HeapVector<IconDefinition> icons,
                              long long totalDownloadSize,
                              String title);
  ~BackgroundFetchRegistration();

  String tag() const;
  HeapVector<IconDefinition> icons() const;
  long long totalDownloadSize() const;
  String title() const;

  ScriptPromise abort(ScriptState*);

  DECLARE_TRACE();

 private:
  void didAbort(ScriptPromiseResolver*, mojom::blink::BackgroundFetchError);

  Member<ServiceWorkerRegistration> m_registration;

  String m_tag;
  HeapVector<IconDefinition> m_icons;
  long long m_totalDownloadSize;
  String m_title;
};

}  // namespace blink

#endif  // BackgroundFetchRegistration_h
