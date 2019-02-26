// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_

#include "base/containers/flat_map.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
class MemoryAllocatorDump;
}  // namespace trace_event
}  // namespace base

namespace gpu {
class MailboxManager;
class SharedImageManager;
class SharedImageRepresentation;
class SharedImageRepresentationGLTexture;
class SharedImageRepresentationGLTexturePassthrough;
class SharedImageRepresentationSkia;
class MemoryTypeTracker;

// Represents the actual storage (GL texture, VkImage, GMB) for a SharedImage.
// Should not be accessed direclty, instead is accessed through a
// SharedImageRepresentation.
class GPU_GLES2_EXPORT SharedImageBacking {
 public:
  SharedImageBacking(const Mailbox& mailbox,
                     viz::ResourceFormat format,
                     const gfx::Size& size,
                     const gfx::ColorSpace& color_space,
                     uint32_t usage,
                     size_t estimated_size);

  virtual ~SharedImageBacking();

  viz::ResourceFormat format() const { return format_; }
  const gfx::Size& size() const { return size_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  uint32_t usage() const { return usage_; }
  const Mailbox& mailbox() const { return mailbox_; }
  size_t estimated_size() const { return estimated_size_; }
  void OnContextLost() { have_context_ = false; }

  // Concrete functions to manage a ref count.
  void AddRef(SharedImageRepresentation* representation);
  void ReleaseRef(SharedImageRepresentation* representation);
  bool HasAnyRefs() const { return !refs_.empty(); }

  // Tracks whether the backing has ever been cleared, or whether it may contain
  // uninitialized pixels.
  virtual bool IsCleared() const = 0;

  // Marks the backing as cleared, after which point it is assumed to contain no
  // unintiailized pixels.
  virtual void SetCleared() = 0;

  virtual void Update() = 0;

  // Destroys the underlying backing. Must be called before destruction.
  virtual void Destroy() = 0;

  // Allows the backing to attach additional data to the dump or dump
  // additional sub paths.
  virtual void OnMemoryDump(const std::string& dump_name,
                            base::trace_event::MemoryAllocatorDump* dump,
                            base::trace_event::ProcessMemoryDump* pmd,
                            uint64_t client_tracing_id) {}

  // Prepares the backing for use with the legacy mailbox system.
  // TODO(ericrk): Remove this once the new codepath is complete.
  virtual bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) = 0;

 protected:
  // Used by SharedImageManager.
  friend class SharedImageManager;
  virtual std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);
  virtual std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker);
  virtual std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);

  // Used by subclasses in Destroy.
  bool have_context() const { return have_context_; }

 private:
  const Mailbox mailbox_;
  const viz::ResourceFormat format_;
  const gfx::Size size_;
  const gfx::ColorSpace color_space_;
  const uint32_t usage_;
  const size_t estimated_size_;

  bool have_context_ = true;
  // A vector of SharedImageRepresentations which hold references to this
  // backing. The first reference is considered the owner, and the vector is
  // ordered by the order in which references were taken.
  std::vector<SharedImageRepresentation*> refs_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
