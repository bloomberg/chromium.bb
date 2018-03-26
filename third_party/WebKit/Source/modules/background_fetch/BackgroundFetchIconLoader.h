// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchIconLoader_h
#define BackgroundFetchIconLoader_h

#include <memory>

#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/ModulesExport.h"
#include "modules/background_fetch/BackgroundFetchTypeConverters.h"
#include "platform/SharedBuffer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class IconDefinition;

class MODULES_EXPORT BackgroundFetchIconLoader final
    : public GarbageCollectedFinalized<BackgroundFetchIconLoader>,
      public ThreadableLoaderClient {
 public:
  // The bitmap may be empty if the request failed or the image data
  // could not be decoded.
  using IconCallback = base::OnceCallback<void(const SkBitmap&)>;

  BackgroundFetchIconLoader();
  ~BackgroundFetchIconLoader() override;

  // Scales down |icon| and returns result. If it is already small enough,
  // |icon| is returned unchanged.
  static SkBitmap ScaleDownIfNeeded(const SkBitmap& icon);

  // Asynchronously download an icon from the given url, decodes the loaded
  // data, and passes the bitmap to the given callback.
  void Start(ExecutionContext*, HeapVector<IconDefinition>, IconCallback);

  // Cancels the pending load, if there is one. The |icon_callback_| will not
  // be run.
  void Stop();

  // ThreadableLoaderClient interface.
  void DidReceiveData(const char* data, unsigned length) override;
  void DidFinishLoading(unsigned long resource_identifier,
                        double finish_time) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(threadable_loader_);
    visitor->Trace(icons_);
  }

 private:
  void RunCallbackWithEmptyBitmap();

  bool stopped_ = false;
  scoped_refptr<SharedBuffer> data_;
  IconCallback icon_callback_;
  HeapVector<IconDefinition> icons_;
  Member<ThreadableLoader> threadable_loader_;
};

}  // namespace blink

#endif  // BackgroundFetchIconLoader_h
