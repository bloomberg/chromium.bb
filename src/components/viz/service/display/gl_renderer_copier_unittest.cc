// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer_copier.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {

namespace {

class CopierTestGLES2Interface : public TestGLES2Interface {
 public:
  // Sets how GL will respond to queries regarding the implementation's internal
  // read-back format.
  void SetOptimalReadbackFormat(GLenum format, GLenum type) {
    format_ = format;
    type_ = type;
  }

  // GLES2Interface override.
  void GetIntegerv(GLenum pname, GLint* params) override {
    switch (pname) {
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        ASSERT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
                  CheckFramebufferStatus(GL_FRAMEBUFFER));
        params[0] = format_;
        break;
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        ASSERT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
                  CheckFramebufferStatus(GL_FRAMEBUFFER));
        params[0] = type_;
        break;
      default:
        TestGLES2Interface::GetIntegerv(pname, params);
        break;
    }
  }

 private:
  GLenum format_ = 0;
  GLenum type_ = 0;
};

}  // namespace

class GLRendererCopierTest : public testing::Test {
 public:
  using ReusableThings = GLRendererCopier::ReusableThings;

  void SetUp() override {
    auto context_provider = TestContextProvider::Create(
        std::make_unique<CopierTestGLES2Interface>());
    context_provider->BindToCurrentThread();
    copier_ = std::make_unique<GLRendererCopier>(std::move(context_provider),
                                                 nullptr);
  }

  void TearDown() override { copier_.reset(); }

  GLRendererCopier* copier() const { return copier_.get(); }

  CopierTestGLES2Interface* test_gl() const {
    return static_cast<CopierTestGLES2Interface*>(
        copier_->context_provider_->ContextGL());
  }

  // These simply forward method calls to GLRendererCopier.
  std::unique_ptr<ReusableThings> TakeReusableThingsOrCreate(
      const base::UnguessableToken& requester) {
    return copier_->TakeReusableThingsOrCreate(requester);
  }
  void StashReusableThingsOrDelete(const base::UnguessableToken& requester,
                                   std::unique_ptr<ReusableThings> things) {
    return copier_->StashReusableThingsOrDelete(requester, std::move(things));
  }
  ReusableThings* PeekReusableThings(const base::UnguessableToken& requester) {
    const auto it = copier_->cache_.find(requester);
    if (it == copier_->cache_.end())
      return nullptr;
    return it->second.get();
  }
  size_t GetCopierCacheSize() { return copier_->cache_.size(); }
  void FreeUnusedCachedResources() { copier_->FreeUnusedCachedResources(); }
  GLenum GetOptimalReadbackFormat() const {
    return copier_->GetOptimalReadbackFormat();
  }

  static constexpr int kKeepalivePeriod = GLRendererCopier::kKeepalivePeriod;

 private:
  std::unique_ptr<GLRendererCopier> copier_;
};

// Tests that named objects, such as textures or framebuffers, are only cached
// when the CopyOutputRequest has specified a "source" of requests.
TEST_F(GLRendererCopierTest, ReusesThingsFromSameSource) {
  // With no source set in a copy request, expect to never re-use any textures
  // or framebuffers.
  const base::UnguessableToken no_source;
  EXPECT_EQ(0u, GetCopierCacheSize());
  auto things = TakeReusableThingsOrCreate(no_source);
  EXPECT_TRUE(!!things);
  StashReusableThingsOrDelete(no_source, std::move(things));
  EXPECT_EQ(nullptr, PeekReusableThings(no_source));
  EXPECT_EQ(0u, GetCopierCacheSize());

  // With a source set in the request, objects should now be cached and re-used.
  const auto source = base::UnguessableToken::Create();
  things = TakeReusableThingsOrCreate(source);
  ReusableThings* things_raw_ptr = things.get();
  EXPECT_TRUE(!!things_raw_ptr);
  StashReusableThingsOrDelete(source, std::move(things));
  EXPECT_EQ(things_raw_ptr, PeekReusableThings(source));
  EXPECT_EQ(1u, GetCopierCacheSize());

  // A second, separate source gets its own cache entry.
  const auto source2 = base::UnguessableToken::Create();
  things = TakeReusableThingsOrCreate(source2);
  things_raw_ptr = things.get();
  EXPECT_TRUE(!!things_raw_ptr);
  EXPECT_EQ(1u, GetCopierCacheSize());
  StashReusableThingsOrDelete(source2, std::move(things));
  EXPECT_EQ(things_raw_ptr, PeekReusableThings(source2));
  EXPECT_EQ(2u, GetCopierCacheSize());
}

// Tests that cached resources are freed if unused for a while.
TEST_F(GLRendererCopierTest, FreesUnusedResources) {
  // Take and then stash a ReusableThings instance for a valid source.
  const base::UnguessableToken source = base::UnguessableToken::Create();
  EXPECT_EQ(0u, GetCopierCacheSize());
  StashReusableThingsOrDelete(source, TakeReusableThingsOrCreate(source));
  EXPECT_TRUE(!!PeekReusableThings(source));
  EXPECT_EQ(1u, GetCopierCacheSize());

  // Call FreesUnusedCachedResources() the maximum number of times before the
  // cache entry would be considered for freeing.
  for (int i = 0; i < kKeepalivePeriod - 1; ++i) {
    FreeUnusedCachedResources();
    EXPECT_TRUE(!!PeekReusableThings(source));
    EXPECT_EQ(1u, GetCopierCacheSize());
    if (HasFailure())
      break;
  }

  // Calling FreeUnusedCachedResources() just one more time should cause the
  // cache entry to be freed.
  FreeUnusedCachedResources();
  EXPECT_FALSE(!!PeekReusableThings(source));
  EXPECT_EQ(0u, GetCopierCacheSize());
}

TEST_F(GLRendererCopierTest, DetectsBGRAForReadbackFormat) {
  test_gl()->SetOptimalReadbackFormat(GL_BGRA_EXT, GL_UNSIGNED_BYTE);
  EXPECT_EQ(static_cast<GLenum>(GL_BGRA_EXT), GetOptimalReadbackFormat());
}

TEST_F(GLRendererCopierTest, DetectsRGBAForReadbackFormat) {
  test_gl()->SetOptimalReadbackFormat(GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), GetOptimalReadbackFormat());
}

TEST_F(GLRendererCopierTest, FallsBackOnRGBAForReadbackFormat_BadFormat) {
  test_gl()->SetOptimalReadbackFormat(GL_RGB, GL_UNSIGNED_BYTE);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), GetOptimalReadbackFormat());
}

TEST_F(GLRendererCopierTest, FallsBackOnRGBAForReadbackFormat_BadType) {
  test_gl()->SetOptimalReadbackFormat(GL_BGRA_EXT, GL_UNSIGNED_SHORT);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), GetOptimalReadbackFormat());
}

}  // namespace viz
