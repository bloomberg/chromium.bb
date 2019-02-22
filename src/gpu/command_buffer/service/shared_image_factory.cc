// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_factory.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/trace_util.h"

namespace gpu {

SharedImageFactory::SharedImageFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    MailboxManager* mailbox_manager,
    SharedImageManager* shared_image_manager,
    ImageFactory* image_factory,
    gles2::MemoryTracker* tracker)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      mailbox_manager_(mailbox_manager),
      shared_image_manager_(shared_image_manager),
      backing_factory_(
          std::make_unique<SharedImageBackingFactoryGLTexture>(gpu_preferences,
                                                               workarounds,
                                                               gpu_feature_info,
                                                               image_factory,
                                                               tracker)) {}

SharedImageFactory::~SharedImageFactory() {
  DCHECK(mailboxes_.empty());
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::ResourceFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           uint32_t usage) {
  if (mailboxes_.find(mailbox) != mailboxes_.end()) {
    LOG(ERROR) << "CreateSharedImage: mailbox already exists";
    return false;
  }

  auto backing =
      backing_factory_->CreateSharedImage(format, size, color_space, usage);
  if (!backing) {
    LOG(ERROR) << "CreateSharedImage: could not create backing.";
    return false;
  }

  // TODO(ericrk): Handle the non-legacy case.
  if (!backing->ProduceLegacyMailbox(mailbox, mailbox_manager_)) {
    LOG(ERROR)
        << "CreateSharedImage: could not convert backing to legacy mailbox.";
    backing->Destroy(true /* have_context */);
    return false;
  }

  if (!shared_image_manager_->Register(mailbox, std::move(backing))) {
    LOG(ERROR) << "CreateSharedImage: Could not register backing with "
                  "SharedImageManager.";
    backing->Destroy(true /* have_context */);
    return false;
  }

  mailboxes_.emplace(mailbox);
  return true;
}

bool SharedImageFactory::DestroySharedImage(const Mailbox& mailbox) {
  auto it = mailboxes_.find(mailbox);
  if (it == mailboxes_.end()) {
    LOG(ERROR) << "Could not find shared image mailbox";
    return false;
  }
  shared_image_manager_->Unregister(mailbox, true /* have_context */);
  mailboxes_.erase(it);
  return true;
}

void SharedImageFactory::DestroyAllSharedImages(bool have_context) {
    for (const auto& mailbox : mailboxes_) {
      shared_image_manager_->Unregister(mailbox, have_context);
    }
    mailboxes_.clear();
}

bool SharedImageFactory::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd,
    int client_id,
    uint64_t client_tracing_id) {
  if (use_passthrough_)
    return true;

  // TODO(ericrk): Move some of this to SharedImageBacking.
  for (const auto& mailbox : mailboxes_) {
    auto* texture =
        static_cast<gles2::Texture*>(mailbox_manager_->ConsumeTexture(mailbox));
    DCHECK(texture);

    // Unique name in the process.
    std::string dump_name =
        base::StringPrintf("gpu/shared-images/client_0x%" PRIX32 "/mailbox_%s",
                           client_id, mailbox.ToDebugString().c_str());

    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    static_cast<uint64_t>(texture->estimated_size()));
    // Add a mailbox guid which expresses shared ownership with the client
    // process.
    // This must match the client-side.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox);
    pmd->CreateSharedGlobalAllocatorDump(client_guid);
    pmd->AddOwnershipEdge(dump->guid(), client_guid);
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto service_guid =
        gl::GetGLTextureServiceGUIDForTracing(texture->service_id());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    // TODO(piman): coalesce constant with TextureManager::DumpTextureRef.
    int importance = 2;  // This client always owns the ref.

    pmd->AddOwnershipEdge(client_guid, service_guid, importance);

    // Dump all sub-levels held by the texture. They will appear below the
    // main gl/textures/client_X/mailbox_Y dump.
    texture->DumpLevelMemory(pmd, client_tracing_id, dump_name);
  }

  return true;
}

}  // namespace gpu
