// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {
namespace gles2 {
namespace {
static const uint32_t kNewServiceId = 431;

class TestSharedImageBackingPassthrough : public SharedImageBacking {
 public:
  class TestSharedImageRepresentationPassthrough
      : public SharedImageRepresentationGLTexturePassthrough {
   public:
    TestSharedImageRepresentationPassthrough(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<TexturePassthrough>& texture_passthrough)
        : SharedImageRepresentationGLTexturePassthrough(manager,
                                                        backing,
                                                        tracker),
          texture_passthrough_(texture_passthrough) {}

    const scoped_refptr<TexturePassthrough>& GetTexturePassthrough() override {
      return texture_passthrough_;
    }

    void set_can_access(bool can_access) { can_access_ = can_access; }
    bool BeginAccess(GLenum mode) override { return can_access_; }

   private:
    const scoped_refptr<TexturePassthrough>& texture_passthrough_;
    bool can_access_ = true;
  };

  TestSharedImageBackingPassthrough(const Mailbox& mailbox,
                                    viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage,
                                    GLuint texture_id)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           0 /* estimated_size */) {
    texture_passthrough_ =
        base::MakeRefCounted<TexturePassthrough>(texture_id, GL_TEXTURE_2D);
  }

  bool IsCleared() const override { return false; }

  void SetCleared() override {}

  void Update() override {}

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    return false;
  }

  void Destroy() override { texture_passthrough_.reset(); }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {}

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    return std::make_unique<TestSharedImageRepresentationPassthrough>(
        manager, this, tracker, texture_passthrough_);
  }

 private:
  scoped_refptr<TexturePassthrough> texture_passthrough_;
};

}  // namespace

using namespace cmds;

TEST_F(GLES2DecoderPassthroughTest, CreateAndTexStorage2DSharedImageCHROMIUM) {
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBackingPassthrough>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, kNewServiceId),
          &memory_tracker);

  CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
      *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name, GL_NONE);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(mailbox.name) + sizeof(GLenum)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Make sure the new client ID is associated with the provided service ID.
  uint32_t found_service_id = 0;
  EXPECT_TRUE(GetPassthroughResources()->texture_id_map.GetServiceID(
      kNewClientId, &found_service_id));
  EXPECT_EQ(found_service_id, kNewServiceId);
  scoped_refptr<TexturePassthrough> found_texture_passthrough;
  EXPECT_TRUE(GetPassthroughResources()->texture_object_map.GetServiceID(
      kNewClientId, &found_texture_passthrough));
  EXPECT_EQ(found_texture_passthrough->service_id(), kNewServiceId);
  found_texture_passthrough.reset();
  EXPECT_EQ(1u, GetPassthroughResources()->texture_shared_image_map.count(
                    kNewClientId));

  // Delete the texture and make sure it is no longer accessible.
  DoDeleteTexture(kNewClientId);
  EXPECT_FALSE(GetPassthroughResources()->texture_id_map.GetServiceID(
      kNewClientId, &found_service_id));
  EXPECT_FALSE(GetPassthroughResources()->texture_object_map.GetServiceID(
      kNewClientId, &found_texture_passthrough));
  EXPECT_EQ(0u, GetPassthroughResources()->texture_shared_image_map.count(
                    kNewClientId));

  shared_image.reset();
}

TEST_F(GLES2DecoderPassthroughTest,
       CreateAndTexStorage2DSharedImageCHROMIUMInvalidMailbox) {
  // Attempt to use an invalid mailbox.
  Mailbox mailbox;
  CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
      *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name, GL_NONE);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(mailbox.name) + sizeof(GLenum)));

  // CreateAndTexStorage2DSharedImage should fail if the mailbox is invalid.
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Make sure the new client_id is associated with a texture id, even though
  // the command failed.
  uint32_t found_service_id = 0;
  EXPECT_TRUE(GetPassthroughResources()->texture_id_map.GetServiceID(
      kNewClientId, &found_service_id));
  EXPECT_NE(0u, found_service_id);
}

TEST_F(GLES2DecoderPassthroughTest,
       CreateAndTexStorage2DSharedImageCHROMIUMPreexistingTexture) {
  MemoryTypeTracker memory_tracker(nullptr);
  // Create a texture with kNewClientId.
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBackingPassthrough>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, kNewServiceId),
          &memory_tracker);

  {
    CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
        *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    cmd.Init(kNewClientId, mailbox.name, GL_NONE);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(mailbox.name) + sizeof(GLenum)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // Try to import the SharedImage a second time at the same client ID. We
  // should get a GL failure.
  {
    CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
        *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    cmd.Init(kNewClientId, mailbox.name, GL_NONE);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(mailbox.name) + sizeof(GLenum)));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }

  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

TEST_F(GLES2DecoderPassthroughTest, BeginEndSharedImageAccessCRHOMIUM) {
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBackingPassthrough>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, kNewServiceId),
          &memory_tracker);

  CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
      *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name, GL_NONE);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(mailbox.name) + sizeof(GLenum)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Begin/end read access for the created image.
  BeginSharedImageAccessDirectCHROMIUM read_access_cmd;
  read_access_cmd.Init(kNewClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_access_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndSharedImageAccessDirectCHROMIUM read_end_cmd;
  read_end_cmd.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Begin/end read/write access for the created image.
  BeginSharedImageAccessDirectCHROMIUM readwrite_access_cmd;
  readwrite_access_cmd.Init(kNewClientId,
                            GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(readwrite_access_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndSharedImageAccessDirectCHROMIUM readwrite_end_cmd;
  readwrite_end_cmd.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(readwrite_end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Cleanup
  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

TEST_F(GLES2DecoderPassthroughTest,
       BeginSharedImageAccessDirectCHROMIUMInvalidMode) {
  // Try to begin access with an invalid mode.
  BeginSharedImageAccessDirectCHROMIUM bad_mode_access_cmd;
  bad_mode_access_cmd.Init(kClientTextureId, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bad_mode_access_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest,
       BeginSharedImageAccessDirectCHROMIUMNotSharedImage) {
  // Try to begin access with a texture that is not a shared image.
  BeginSharedImageAccessDirectCHROMIUM not_shared_image_access_cmd;
  not_shared_image_access_cmd.Init(
      kClientTextureId, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(not_shared_image_access_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest,
       BeginSharedImageAccessDirectCHROMIUMCantBeginAccess) {
  // Create a shared image.
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBackingPassthrough>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, kNewServiceId),
          &memory_tracker);

  CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
      *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name, GL_NONE);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(mailbox.name) + sizeof(GLenum)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try to begin access with a shared image representation that fails
  // BeginAccess.
  auto found =
      GetPassthroughResources()->texture_shared_image_map.find(kNewClientId);
  ASSERT_TRUE(found !=
              GetPassthroughResources()->texture_shared_image_map.end());
  static_cast<TestSharedImageBackingPassthrough::
                  TestSharedImageRepresentationPassthrough*>(
      found->second.get())
      ->set_can_access(false);
  BeginSharedImageAccessDirectCHROMIUM read_access_cmd;
  read_access_cmd.Init(kNewClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_access_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Cleanup
  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

TEST_F(GLES2DecoderPassthroughTest,
       EndSharedImageAccessDirectCHROMIUMNotSharedImage) {
  // Try to end access with a texture that is not a shared image.
  EndSharedImageAccessDirectCHROMIUM not_shared_image_end_cmd;
  not_shared_image_end_cmd.Init(kClientTextureId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(not_shared_image_end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

}  // namespace gles2
}  // namespace gpu
