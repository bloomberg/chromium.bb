// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationImageLoader_h
#define NotificationImageLoader_h

#include <memory>
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/ModulesExport.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/RefPtr.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class ExecutionContext;
class KURL;
class ResourceError;

// Asynchronously downloads an image when given a url, decodes the loaded data,
// and passes the bitmap to the given callback.
class MODULES_EXPORT NotificationImageLoader final
    : public GarbageCollectedFinalized<NotificationImageLoader>,
      public ThreadableLoaderClient {
 public:
  // Type names are used in UMAs, so do not rename.
  enum class Type { kImage, kIcon, kBadge, kActionIcon };

  // The bitmap may be empty if the request failed or the image data could not
  // be decoded.
  using ImageCallback = Function<void(const SkBitmap&)>;

  explicit NotificationImageLoader(Type);
  ~NotificationImageLoader() override;

  // Scales down |image| according to its type and returns result. If it is
  // already small enough, |image| is returned unchanged.
  static SkBitmap ScaleDownIfNeeded(const SkBitmap& image, Type);

  // Asynchronously downloads an image from the given url, decodes the loaded
  // data, and passes the bitmap to the callback. Times out if the load takes
  // too long and ImageCallback is invoked with an empty bitmap.
  void Start(ExecutionContext*, const KURL&, ImageCallback);

  // Cancels the pending load, if there is one. The |m_imageCallback| will not
  // be run.
  void Stop();

  // ThreadableLoaderClient interface.
  void DidReceiveData(const char* data, unsigned length) override;
  void DidFinishLoading(unsigned long resource_identifier,
                        double finish_time) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  void Trace(blink::Visitor* visitor) { visitor->Trace(threadable_loader_); }

 private:
  void RunCallbackWithEmptyBitmap();

  Type type_;
  bool stopped_;
  double start_time_;
  RefPtr<SharedBuffer> data_;
  ImageCallback image_callback_;
  Member<ThreadableLoader> threadable_loader_;
};

}  // namespace blink

#endif  // NotificationImageLoader_h
