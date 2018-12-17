// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/manifest.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

const char kTestServiceName[] = "test_service";

const Manifest& GetPackagedService1Manifest() {
  static base::NoDestructor<Manifest> manifest{ManifestBuilder()
                                                   .WithServiceName("service_1")
                                                   .WithDisplayName("Service 1")
                                                   .Build()};
  return *manifest;
}

const Manifest& GetPackagedService2Manifest() {
  static base::NoDestructor<Manifest> manifest{ManifestBuilder()
                                                   .WithServiceName("service_2")
                                                   .WithDisplayName("Service 2")
                                                   .Build()};
  return *manifest;
}

const Manifest& GetManifest() {
  static base::NoDestructor<Manifest> manifest{
      ManifestBuilder()
          .WithServiceName(kTestServiceName)
          .WithDisplayName("The Test Service, Obviously")
          .WithOptions(
              ManifestOptionsBuilder()
                  .WithSandboxType("none")
                  .WithInstanceSharingPolicy(
                      Manifest::InstanceSharingPolicy::kSharedAcrossGroups)
                  .CanConnectToInstancesWithAnyId(true)
                  .CanConnectToInstancesInAnyGroup(true)
                  .CanRegisterOtherServiceInstances(false)
                  .Build())
          .ExposeCapability(
              "capability_1",
              Manifest::InterfaceList<mojom::Connector, mojom::PIDReceiver>())
          .ExposeCapability("capability_2",
                            Manifest::InterfaceList<mojom::Connector>())
          .RequireCapability("service_42", "computation")
          .RequireCapability("frobinator", "frobination")
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "filter_capability_1",
              Manifest::InterfaceList<mojom::Connector>())
          .RequireInterfaceFilterCapability_Deprecated(
              "browser", "navigation:frame", "some_filter_capability")
          .RequireInterfaceFilterCapability_Deprecated(
              "browser", "navigation:frame", "another_filter_capability")
          .PackageService(GetPackagedService1Manifest())
          .PackageService(GetPackagedService2Manifest())
          .PreloadFile("file1_key",
                       base::FilePath(FILE_PATH_LITERAL("AUTOEXEC.BAT")))
          .PreloadFile("file2_key",
                       base::FilePath(FILE_PATH_LITERAL("CONFIG.SYS")))
          .PreloadFile("file3_key", base::FilePath(FILE_PATH_LITERAL(".vimrc")))
          .Build()};
  return *manifest;
}

TEST(ManifestTest, BasicBuilder) {
  const auto& manifest = GetManifest();
  EXPECT_EQ(std::string(kTestServiceName), manifest.service_name);
  EXPECT_EQ(std::string("none"), manifest.options.sandbox_type);
  EXPECT_TRUE(manifest.options.can_connect_to_instances_in_any_group);
  EXPECT_TRUE(manifest.options.can_connect_to_instances_with_any_id);
  EXPECT_FALSE(manifest.options.can_register_other_service_instances);
  EXPECT_EQ(Manifest::InstanceSharingPolicy::kSharedAcrossGroups,
            manifest.options.instance_sharing_policy);
  EXPECT_EQ(2u, manifest.exposed_capabilities.size());
  EXPECT_EQ(2u, manifest.required_capabilities.size());
  EXPECT_EQ(1u, manifest.exposed_interface_filter_capabilities.size());
  EXPECT_EQ(2u, manifest.required_interface_filter_capabilities.size());
  EXPECT_EQ(2u, manifest.packaged_services.size());
  EXPECT_EQ(manifest.packaged_services[0].service_name,
            GetPackagedService1Manifest().service_name);
  EXPECT_EQ(3u, manifest.preloaded_files.size());
}

}  // namespace service_manager
