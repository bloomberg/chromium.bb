// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include "base/run_loop.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SerialChooserContextTest : public testing::Test {
 public:
  SerialChooserContextTest() = default;
  ~SerialChooserContextTest() override = default;

  Profile* profile() { return &profile_; }

  SerialChooserContext* GetContext(Profile* profile) {
    return SerialChooserContextFactory::GetForProfile(profile);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

}  // namespace

TEST_F(SerialChooserContextTest, GrantAndRevokeEphemeralPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  SerialChooserContext* context = GetContext(profile());
  EXPECT_FALSE(context->HasPortPermission(origin, origin, *port));
  context->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<ChooserContextBase::Object>> origin_objects =
      context->GetGrantedObjects(origin.GetURL(), origin.GetURL());
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->requesting_origin);
  EXPECT_EQ(origin.GetURL(), objects[0]->embedding_origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  context->RevokeObjectPermission(origin.GetURL(), origin.GetURL(),
                                  objects[0]->value);
  EXPECT_FALSE(context->HasPortPermission(origin, origin, *port));
  origin_objects = context->GetGrantedObjects(origin.GetURL(), origin.GetURL());
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}
