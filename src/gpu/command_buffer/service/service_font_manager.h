// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SERVICE_FONT_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SERVICE_FONT_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"

namespace gpu {
class Buffer;

class GPU_GLES2_EXPORT ServiceFontManager
    : public base::RefCountedThreadSafe<ServiceFontManager> {
 public:
  class GPU_GLES2_EXPORT Client {
   public:
    virtual ~Client() {}
    virtual scoped_refptr<Buffer> GetShmBuffer(uint32_t shm_id) = 0;
    virtual void ReportProgress() = 0;
  };

  ServiceFontManager(Client* client, bool disable_oopr_debug_crash_dump);
  void Destroy();

  bool Deserialize(const volatile char* memory,
                   uint32_t memory_size,
                   std::vector<SkDiscardableHandleId>* locked_handles);
  bool Unlock(const std::vector<SkDiscardableHandleId>& handles);
  SkStrikeClient* strike_client() { return strike_client_.get(); }
  bool disable_oopr_debug_crash_dump() const {
    return disable_oopr_debug_crash_dump_;
  }
  bool is_destroyed() const { return destroyed_; }

 private:
  friend class base::RefCountedThreadSafe<ServiceFontManager>;
  class SkiaDiscardableManager;

  ~ServiceFontManager();

  bool AddHandle(SkDiscardableHandleId handle_id,
                 ServiceDiscardableHandle handle);
  bool DeleteHandle(SkDiscardableHandleId handle_id);

  base::Lock lock_;

  raw_ptr<Client> client_;
  const base::PlatformThreadId client_thread_id_;
  std::unique_ptr<SkStrikeClient> strike_client_;
  base::flat_map<SkDiscardableHandleId, ServiceDiscardableHandle>
      discardable_handle_map_;
  bool destroyed_ = false;
  const bool disable_oopr_debug_crash_dump_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_FONT_MANAGER_H_
