// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frames_manager_impl.h"

#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/web_state/web_frame_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns true if |web_frame| is contained in |frames|.
bool ContainsWebFrame(std::set<web::WebFrame*> frames,
                      web::WebFrame* web_frame) {
  return frames.end() != std::find(frames.begin(), frames.end(), web_frame);
}

}  // namespace

namespace web {

class WebFramesManagerImplTest : public PlatformTest {
 protected:
  WebFramesManagerImplTest() {
    WebFramesManagerImpl::CreateForWebState(&test_web_state_);
    frames_manager_ = WebFramesManagerImpl::FromWebState(&test_web_state_);
  }

  TestWebState test_web_state_;
  WebFramesManagerImpl* frames_manager_ = nullptr;
};

// Tests that the WebFrame for the main frame is returned.
TEST_F(WebFramesManagerImplTest, GetMainWebFrame) {
  GURL security_origin;
  auto web_frame = std::make_unique<WebFrameImpl>(
      "web_frame", /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* web_frame_ptr = web_frame.get();

  frames_manager_->AddFrame(std::move(web_frame));

  EXPECT_EQ(web_frame_ptr, frames_manager_->GetMainWebFrame());
  auto frames = frames_manager_->GetAllWebFrames();
  EXPECT_EQ(1ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, web_frame_ptr));
}

// Tests that the WebFrame returns null if no main frame is known.
TEST_F(WebFramesManagerImplTest, NoMainWebFrame) {
  EXPECT_EQ(nullptr, frames_manager_->GetMainWebFrame());

  GURL security_origin;
  const std::string web_frame_frame_id = "web_frame";
  auto web_frame =
      std::make_unique<WebFrameImpl>(web_frame_frame_id, /*is_main_frame=*/true,
                                     security_origin, &test_web_state_);

  frames_manager_->AddFrame(std::move(web_frame));
  frames_manager_->RemoveFrameWithId(web_frame_frame_id);

  EXPECT_EQ(nullptr, frames_manager_->GetMainWebFrame());
  EXPECT_EQ(0ul, frames_manager_->GetAllWebFrames().size());
}

// Tests that the WebFramesManagerImpl returns a list of all current WebFrame
// objects.
TEST_F(WebFramesManagerImplTest, AddFrames) {
  GURL security_origin;
  auto main_web_frame = std::make_unique<WebFrameImpl>(
      "main_web_frame",
      /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* main_web_frame_ptr = main_web_frame.get();
  frames_manager_->AddFrame(std::move(main_web_frame));

  auto child_web_frame = std::make_unique<WebFrameImpl>(
      "child_web_frame",
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  WebFrameImpl* child_web_frame_ptr = child_web_frame.get();
  frames_manager_->AddFrame(std::move(child_web_frame));

  auto frames = frames_manager_->GetAllWebFrames();
  EXPECT_EQ(2ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, main_web_frame_ptr));
  EXPECT_TRUE(ContainsWebFrame(frames, child_web_frame_ptr));
}

// Tests that the WebFramesManagerImpl correctly removes a WebFrame instance.
TEST_F(WebFramesManagerImplTest, RemoveFrame) {
  GURL security_origin;
  auto main_web_frame = std::make_unique<WebFrameImpl>(
      "main_web_frame",
      /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* main_web_frame_ptr = main_web_frame.get();
  frames_manager_->AddFrame(std::move(main_web_frame));

  const std::string child_web_frame_1_frame_id = "child_web_frame_1_frame_id";
  auto child_web_frame_1 = std::make_unique<WebFrameImpl>(
      child_web_frame_1_frame_id,
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  frames_manager_->AddFrame(std::move(child_web_frame_1));

  auto child_web_frame_2 = std::make_unique<WebFrameImpl>(
      "child_web_frame_2",
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  WebFrameImpl* child_web_frame_2_ptr = child_web_frame_2.get();
  frames_manager_->AddFrame(std::move(child_web_frame_2));

  frames_manager_->RemoveFrameWithId(child_web_frame_1_frame_id);

  auto frames = frames_manager_->GetAllWebFrames();
  EXPECT_EQ(2ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, main_web_frame_ptr));
  EXPECT_TRUE(ContainsWebFrame(frames, child_web_frame_2_ptr));
}

// Tests that all frames are removed after a call to |RemoveAllFrames|.
TEST_F(WebFramesManagerImplTest, RemoveAllFrames) {
  GURL security_origin;
  frames_manager_->AddFrame(std::make_unique<WebFrameImpl>(
      "main_web_frame",
      /*is_main_frame=*/true, security_origin, &test_web_state_));
  frames_manager_->AddFrame(std::make_unique<WebFrameImpl>(
      "web_frame",
      /*is_main_frame=*/false, security_origin, &test_web_state_));

  ASSERT_EQ(2ul, frames_manager_->GetAllWebFrames().size());
  frames_manager_->RemoveAllWebFrames();
  EXPECT_EQ(nullptr, frames_manager_->GetMainWebFrame());
  EXPECT_EQ(0ul, frames_manager_->GetAllWebFrames().size());
}

// Tests that the WebFramesManagerImpl correctly ignores attempted removal of an
// already removed WebFrame.
TEST_F(WebFramesManagerImplTest, RemoveNonexistantFrame) {
  GURL security_origin;
  const std::string main_web_frame_frame_id = "main_web_frame";
  auto main_web_frame = std::make_unique<WebFrameImpl>(
      main_web_frame_frame_id,
      /*is_main_frame=*/true, security_origin, &test_web_state_);
  WebFrameImpl* main_web_frame_ptr = main_web_frame.get();

  frames_manager_->AddFrame(std::move(main_web_frame));
  auto frames = frames_manager_->GetAllWebFrames();
  EXPECT_EQ(1ul, frames.size());
  EXPECT_TRUE(ContainsWebFrame(frames, main_web_frame_ptr));

  frames_manager_->RemoveFrameWithId(main_web_frame_frame_id);
  EXPECT_EQ(0ul, frames_manager_->GetAllWebFrames().size());
  frames_manager_->RemoveFrameWithId(main_web_frame_frame_id);
  EXPECT_EQ(0ul, frames_manager_->GetAllWebFrames().size());
}

// Tests that a WebFrame is correctly returned by its frame id.
TEST_F(WebFramesManagerImplTest, GetFrameWithId) {
  GURL security_origin;

  const std::string web_frame_frame_id = "web_frame_frame_id";
  auto web_frame = std::make_unique<WebFrameImpl>(
      web_frame_frame_id,
      /*is_main_frame=*/false, security_origin, &test_web_state_);
  WebFrameImpl* web_frame_ptr = web_frame.get();
  frames_manager_->AddFrame(std::move(web_frame));

  EXPECT_EQ(web_frame_ptr, frames_manager_->GetFrameWithId(web_frame_frame_id));
  EXPECT_EQ(nullptr, frames_manager_->GetFrameWithId("invalid_id"));
}

}  // namespace web
