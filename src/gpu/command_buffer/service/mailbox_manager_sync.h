// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_SYNC_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_SYNC_H_

#include <map>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_definition.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

// Manages resources scoped beyond the context or context group level
// and across threads and driver level share groups by synchronizing
// texture state.
class GPU_GLES2_EXPORT MailboxManagerSync : public MailboxManager {
 public:
  MailboxManagerSync();
  ~MailboxManagerSync() override;

  // MailboxManager implementation:
  Texture* ConsumeTexture(const Mailbox& mailbox) override;
  void ProduceTexture(const Mailbox& mailbox, TextureBase* texture) override;
  bool UsesSync() override;
  void PushTextureUpdates(const SyncToken& token) override;
  void PullTextureUpdates(const SyncToken& token) override;
  void TextureDeleted(TextureBase* texture) override;

 private:
  static bool SkipTextureWorkarounds(const Texture* texture);

  class TextureGroup : public base::RefCounted<TextureGroup> {
   public:
    explicit TextureGroup(const TextureDefinition& definition);
    static TextureGroup* FromName(const Mailbox& name);

    void AddName(const Mailbox& name);
    void RemoveName(const Mailbox& name);

    void AddTexture(MailboxManagerSync* manager, Texture* texture);
    // Returns true if there are other textures left in the group after removal.
    bool RemoveTexture(MailboxManagerSync* manager, Texture* texture);
    Texture* FindTexture(MailboxManagerSync* manager);

    const TextureDefinition& GetDefinition() { return definition_; }
    void SetDefinition(TextureDefinition definition) {
      definition_ = definition;
    }

   private:
    friend class base::RefCounted<TextureGroup>;
    ~TextureGroup();

    typedef std::vector<std::pair<MailboxManagerSync*, Texture*>> TextureList;
    std::vector<Mailbox> names_;
    TextureList textures_;
    TextureDefinition definition_;

    typedef std::map<Mailbox, scoped_refptr<TextureGroup>>
        MailboxToGroupMap;
    static base::LazyInstance<MailboxToGroupMap>::DestructorAtExit
        mailbox_to_group_;
  };

  struct TextureGroupRef {
    TextureGroupRef(unsigned version, TextureGroup* group);
    TextureGroupRef(const TextureGroupRef& other);
    ~TextureGroupRef();
    unsigned version;
    scoped_refptr<TextureGroup> group;
  };
  static void UpdateDefinitionLocked(TextureBase* texture,
                                     TextureGroupRef* group_ref);

  typedef std::map<Texture*, TextureGroupRef> TextureToGroupMap;
  TextureToGroupMap texture_to_group_;

  DISALLOW_COPY_AND_ASSIGN(MailboxManagerSync);
};

}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_SYNC_H_
