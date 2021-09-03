// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_store.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

class ElementStoreTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    element_store_ = std::make_unique<ElementStore>(web_contents_.get());
  }

 protected:
  std::unique_ptr<ElementFinder::Result> CreateElement(
      const std::string& object_id) {
    auto element = std::make_unique<ElementFinder::Result>();
    element->dom_object.object_data.object_id = object_id;
    element->dom_object.object_data.node_frame_id =
        web_contents_->GetMainFrame()->GetDevToolsFrameToken().ToString();
    return element;
  }

  // This consumes the element while adding it to simulate the way of the
  // result going out of life.
  void AddElement(const std::string& client_id,
                  std::unique_ptr<ElementFinder::Result> element) {
    element_store_->AddElement(client_id, element->dom_object);
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ElementStore> element_store_;
};

TEST_F(ElementStoreTest, AddElementToStore) {
  auto element = CreateElement("1");
  AddElement("1", std::move(element));

  EXPECT_TRUE(element_store_->HasElement("1"));
  EXPECT_FALSE(element_store_->HasElement("2"));
}

TEST_F(ElementStoreTest, GetElementFromStore) {
  auto element = CreateElement("1");
  AddElement("1", std::move(element));

  ElementFinder::Result result;
  EXPECT_EQ(ACTION_APPLIED,
            element_store_->GetElement("1", &result).proto_status());
  EXPECT_EQ("1", result.object_id());
}

TEST_F(ElementStoreTest, GetElementFromStoreWithBadFrameHost) {
  auto element = std::make_unique<ElementFinder::Result>();
  element->dom_object.object_data.object_id = "1";
  element->dom_object.object_data.node_frame_id = "unknown";
  AddElement("1", std::move(element));

  ElementFinder::Result result;
  EXPECT_EQ(CLIENT_ID_RESOLUTION_FAILED,
            element_store_->GetElement("1", &result).proto_status());
}

TEST_F(ElementStoreTest, GetElementFromStoreWithNoFrameId) {
  auto element = std::make_unique<ElementFinder::Result>();
  element->dom_object.object_data.object_id = "1";
  AddElement("1", std::move(element));

  ElementFinder::Result result;
  EXPECT_EQ(ACTION_APPLIED,
            element_store_->GetElement("1", &result).proto_status());
  EXPECT_EQ(web_contents_->GetMainFrame(), result.container_frame_host);
}

TEST_F(ElementStoreTest, AddElementToStoreOverwrites) {
  auto element_1 = CreateElement("1");
  auto element_2 = CreateElement("2");

  AddElement("e", std::move(element_1));
  AddElement("e", std::move(element_2));

  ElementFinder::Result result;
  EXPECT_EQ(ACTION_APPLIED,
            element_store_->GetElement("e", &result).proto_status());
  EXPECT_EQ("2", result.object_id());
}

TEST_F(ElementStoreTest, GetUnknownElementFromStore) {
  ElementFinder::Result result;
  EXPECT_EQ(CLIENT_ID_RESOLUTION_FAILED,
            element_store_->GetElement("1", &result).proto_status());
}

TEST_F(ElementStoreTest, RemoveElementFromStore) {
  auto element = CreateElement("1");
  AddElement("1", std::move(element));

  EXPECT_TRUE(element_store_->RemoveElement("1"));
  EXPECT_FALSE(element_store_->RemoveElement("1"));
}

TEST_F(ElementStoreTest, ClearStore) {
  auto element = CreateElement("1");
  AddElement("1", std::move(element));

  element_store_->Clear();

  EXPECT_FALSE(element_store_->HasElement("1"));
}

}  // namespace
}  // namespace autofill_assistant
