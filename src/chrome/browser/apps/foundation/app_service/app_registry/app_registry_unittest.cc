// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/foundation/app_service/app_registry/app_registry.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestAppId1[] = "test.app.id.1";
const char kTestAppId2[] = "test.app.id.2";
}  // namespace

namespace apps {

class AppRegistryTest : public testing::Test {
 protected:
  std::unique_ptr<AppRegistry> CreateAppRegistry() {
    auto pref_service = std::make_unique<TestingPrefServiceSimple>();
    AppRegistry::RegisterPrefs(pref_service->registry());
    return std::make_unique<AppRegistry>(std::move(pref_service));
  }

  void GetAppInfos(AppRegistry* app_registry) {
    base::RunLoop run_loop;
    app_registry->GetApps(base::BindOnce(&AppRegistryTest::GetAppsCallback,
                                         base::Unretained(this),
                                         run_loop.QuitClosure()));
    run_loop.Run();
  }

  void GetAppsCallback(base::OnceClosure quit_closure,
                       std::vector<apps::mojom::AppInfoPtr> app_infos) {
    app_infos_ = std::move(app_infos);
    std::move(quit_closure).Run();
  }

  const std::vector<apps::mojom::AppInfoPtr>& app_infos() const {
    return app_infos_;
  }

  const apps::mojom::AppInfoPtr& FindWithId(const std::string& id) const {
    return *(std::find_if(
        app_infos_.begin(), app_infos_.end(),
        [&id](const apps::mojom::AppInfoPtr& info) { return info->id == id; }));
  }

 private:
  // Required for RunLoop waiting.
  base::MessageLoop message_loop_;

  std::vector<apps::mojom::AppInfoPtr> app_infos_;
};

TEST_F(AppRegistryTest, SetAppPreferred) {
  auto registry = CreateAppRegistry();

  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId1, apps::mojom::AppPreferred::kPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId1,
                            apps::mojom::AppPreferred::kNotPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId2, apps::mojom::AppPreferred::kPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId2,
                            apps::mojom::AppPreferred::kNotPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId2));
}

TEST_F(AppRegistryTest, GetPreferredApps) {
  auto app_registry = CreateAppRegistry();
  GetAppInfos(app_registry.get());
  EXPECT_EQ(std::vector<apps::mojom::AppInfoPtr>{}, app_infos());

  app_registry->SetAppPreferred(kTestAppId1,
                                apps::mojom::AppPreferred::kPreferred);
  GetAppInfos(app_registry.get());
  EXPECT_EQ(1u, app_infos().size());
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred, app_infos()[0]->preferred);

  app_registry->SetAppPreferred(kTestAppId2,
                                apps::mojom::AppPreferred::kPreferred);
  GetAppInfos(app_registry.get());
  EXPECT_EQ(2u, app_infos().size());
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred,
            FindWithId(kTestAppId1)->preferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred,
            FindWithId(kTestAppId2)->preferred);

  app_registry->SetAppPreferred(kTestAppId1,
                                apps::mojom::AppPreferred::kNotPreferred);
  GetAppInfos(app_registry.get());
  EXPECT_EQ(2u, app_infos().size());
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            FindWithId(kTestAppId1)->preferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred,
            FindWithId(kTestAppId2)->preferred);

  app_registry->SetAppPreferred(kTestAppId2,
                                apps::mojom::AppPreferred::kNotPreferred);
  GetAppInfos(app_registry.get());
  EXPECT_EQ(2u, app_infos().size());
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            FindWithId(kTestAppId1)->preferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            FindWithId(kTestAppId2)->preferred);
}

}  // namespace apps
