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
#include "wtf/Forward.h"

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
  virtual const KURL& url() const = 0;
  virtual bool isSchedulingReload() const = 0;
  virtual bool hasDevicePixelRatioHeaderValue() const = 0;
  virtual float devicePixelRatioHeaderValue() const = 0;
  virtual const ResourceResponse& response() const = 0;
  virtual ResourceStatus getStatus() const = 0;
  virtual bool isPlaceholder() const = 0;
  virtual bool isCacheValidator() const = 0;
  virtual bool schedulingReloadOrShouldReloadBrokenPlaceholder() const = 0;
  enum DoesCurrentFrameHaveSingleSecurityOrigin {
    HasMultipleSecurityOrigin,
    HasSingleSecurityOrigin
  };
  virtual bool isAccessAllowed(
      SecurityOrigin*,
      DoesCurrentFrameHaveSingleSecurityOrigin) const = 0;
  virtual bool hasCacheControlNoStoreHeader() const = 0;
  virtual const ResourceError& resourceError() const = 0;

  // TODO(hiroshige): Remove this.
  virtual void setIsPlaceholder(bool) = 0;

  // TODO(hiroshige): Remove this once MemoryCache becomes further weaker.
  virtual void setDecodedSize(size_t) = 0;

  // TODO(hiroshige): Remove these.
  virtual void willAddClientOrObserver() = 0;
  virtual void didRemoveClientOrObserver() = 0;

  // TODO(hiroshige): Remove this. crbug.com/666214
  virtual void emulateLoadStartedForInspector(
      ResourceFetcher*,
      const KURL&,
      const AtomicString& initiatorName) = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif
