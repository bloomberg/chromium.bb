/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tests/common/win/testing_common.h"
#include "import/cross/camera_info.h"
#include "core/cross/class_manager.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/service_dependency.h"
#include "utils/cross/math_gtest.h"

namespace o3d {

namespace {

// A class to test CameraInfo.
class TestCameraInfo : public CameraInfo {
 public:
  typedef SmartPointer<TestCameraInfo> Ref;

  static ObjectBase::Ref Create(ServiceLocator* service_locator);

 private:
  explicit TestCameraInfo(ServiceLocator* service_locator)
      : CameraInfo(service_locator) {
  }

  O3D_OBJECT_BASE_DECL_CLASS(TestCameraInfo, CameraInfo);
  DISALLOW_COPY_AND_ASSIGN(TestCameraInfo);
};

O3D_OBJECT_BASE_DEFN_CLASS(
    "o3djs.TestCameraInfo", TestCameraInfo, CameraInfo);

ObjectBase::Ref TestCameraInfo::Create(
    ServiceLocator* service_locator) {
  return ObjectBase::Ref(new TestCameraInfo(service_locator));
}

}  // anonymous namespace

class TestCameraInfoTest : public testing::Test {
 protected:
  TestCameraInfoTest()
      : class_manager_(g_service_locator),
        object_manager_(g_service_locator),
        class_register_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ClassManager> class_manager_;
  ServiceDependency<ObjectManager> object_manager_;
  ClassManager::Register<TestCameraInfo> class_register_;
  Pack* pack_;
};

void TestCameraInfoTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void TestCameraInfoTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Creates a TestCameraInfo, tests basic properties.
TEST_F(TestCameraInfoTest, TestTestCameraInfo) {
  TestCameraInfo *camera_info = pack()->Create<TestCameraInfo>();
  EXPECT_TRUE(camera_info->IsA(TestCameraInfo::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(CameraInfo::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(JSONObject::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(ParamObject::GetApparentClass()));

  EXPECT_EQ(camera_info->aspect_ratio(), 1.0f);
  EXPECT_EQ(camera_info->near_z(), 0.01f);
  EXPECT_EQ(camera_info->far_z(), 10000.0f);
  EXPECT_TRUE(camera_info->transform() == NULL);

  Transform* transform = pack()->Create<Transform>();
  ASSERT_TRUE(transform != NULL);

  camera_info->set_aspect_ratio(2.0f);
  camera_info->set_near_z(0.02f);
  camera_info->set_far_z(20000.0f);
  camera_info->set_transform(transform);
  camera_info->set_eye(Float3(10.0f, 20.0f, 30.0f));
  camera_info->set_target(Float3(11.0f, 22.0f, 33.0f));
  camera_info->set_up(Float3(12.0f, 23.0f, 34.0f));

  EXPECT_EQ(camera_info->aspect_ratio(), 2.0f);
  EXPECT_EQ(camera_info->near_z(), 0.02f);
  EXPECT_EQ(camera_info->far_z(), 20000.0f);
  EXPECT_TRUE(camera_info->transform() == transform);
  EXPECT_EQ(camera_info->eye(), Float3(10.0f, 20.0f, 30.0f));
  EXPECT_EQ(camera_info->target(), Float3(11.0f, 22.0f, 33.0f));
  EXPECT_EQ(camera_info->up(), Float3(12.0f, 23.0f, 34.0f));
}

class PerspectiveCameraInfoTest : public testing::Test {
 protected:
  PerspectiveCameraInfoTest()
      : class_manager_(g_service_locator),
        object_manager_(g_service_locator),
        class_register_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ClassManager> class_manager_;
  ServiceDependency<ObjectManager> object_manager_;
  ClassManager::Register<PerspectiveCameraInfo> class_register_;
  Pack* pack_;
};

void PerspectiveCameraInfoTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void PerspectiveCameraInfoTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Creates a PerspectiveCameraInfo, tests basic properties.
TEST_F(PerspectiveCameraInfoTest, TestPerspectiveCameraInfo) {
  PerspectiveCameraInfo *camera_info = pack()->Create<PerspectiveCameraInfo>();
  EXPECT_TRUE(camera_info->IsA(PerspectiveCameraInfo::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(CameraInfo::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(JSONObject::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(ParamObject::GetApparentClass()));

  EXPECT_EQ(camera_info->field_of_view_y(), 30.0f * 3.14159f / 180.0f);

  camera_info->set_field_of_view_y(60.0f * 3.14159f / 180.0f);

  EXPECT_EQ(camera_info->field_of_view_y(), 60.0f * 3.14159f / 180.0f);
}

class OrthographicCameraInfoTest : public testing::Test {
 protected:
  OrthographicCameraInfoTest()
      : class_manager_(g_service_locator),
        object_manager_(g_service_locator),
        class_register_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ClassManager> class_manager_;
  ServiceDependency<ObjectManager> object_manager_;
  ClassManager::Register<OrthographicCameraInfo> class_register_;
  Pack* pack_;
};

void OrthographicCameraInfoTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void OrthographicCameraInfoTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Creates a OrthographicCameraInfo, tests basic properties.
TEST_F(OrthographicCameraInfoTest, TestOrthographicCameraInfo) {
  OrthographicCameraInfo *camera_info =
      pack()->Create<OrthographicCameraInfo>();
  EXPECT_TRUE(camera_info->IsA(OrthographicCameraInfo::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(CameraInfo::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(JSONObject::GetApparentClass()));
  EXPECT_TRUE(camera_info->IsA(ParamObject::GetApparentClass()));

  EXPECT_EQ(camera_info->mag_x(), 1.0f);
  EXPECT_EQ(camera_info->mag_y(), 1.0f);

  camera_info->set_mag_x(2.0f);
  camera_info->set_mag_y(3.0f);

  EXPECT_EQ(camera_info->mag_x(), 2.0f);
  EXPECT_EQ(camera_info->mag_y(), 3.0f);
}

}  // namespace o3d

