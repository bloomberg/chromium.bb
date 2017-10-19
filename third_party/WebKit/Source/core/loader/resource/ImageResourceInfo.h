// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageResourceInfo_h
#define ImageResourceInfo_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/loader/fetch/ResourceStatus.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ResourceError;
class ResourceFetcher;
class ResourceResponse;
class SecurityOrigin;

// Delegate class of ImageResource that encapsulates the interface and data
// visible to ImageResourceContent.
// Do not add new members or new call sites unless really needed.
// TODO(hiroshige): reduce the members of this class to further decouple
// ImageResource and ImageResourceContent.
class CORE_EXPORT ImageResourceInfo : public GarbageCollectedMixin {
 public:
  ~ImageResourceInfo() {}
  virtual const KURL& Url() const = 0;
  virtual bool IsSchedulingReload() const = 0;
  virtual bool HasDevicePixelRatioHeaderValue() const = 0;
  virtual float DevicePixelRatioHeaderValue() const = 0;
  virtual const ResourceResponse& GetResponse() const = 0;
  virtual bool ShouldShowPlaceholder() const = 0;
  virtual bool IsCacheValidator() const = 0;
  virtual bool SchedulingReloadOrShouldReloadBrokenPlaceholder() const = 0;
  enum DoesCurrentFrameHaveSingleSecurityOrigin {
    kHasMultipleSecurityOrigin,
    kHasSingleSecurityOrigin
  };
  virtual bool IsAccessAllowed(
      SecurityOrigin*,
      DoesCurrentFrameHaveSingleSecurityOrigin) const = 0;
  virtual bool HasCacheControlNoStoreHeader() const = 0;
  virtual const ResourceError& GetResourceError() const = 0;

  // TODO(hiroshige): Remove this once MemoryCache becomes further weaker.
  virtual void SetDecodedSize(size_t) = 0;

  // TODO(hiroshige): Remove these.
  virtual void WillAddClientOrObserver() = 0;
  virtual void DidRemoveClientOrObserver() = 0;

  // TODO(hiroshige): Remove this. crbug.com/666214
  virtual void EmulateLoadStartedForInspector(
      ResourceFetcher*,
      const KURL&,
      const AtomicString& initiator_name) = 0;

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif
