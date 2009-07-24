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
#include "import/cross/json_object.h"
#include "core/cross/class_manager.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/service_dependency.h"
#include "utils/cross/string_writer.h"
#include "utils/cross/json_writer.h"
#include "utils/cross/math_gtest.h"

namespace o3d {

namespace {

// A class to test JSONObject
class TestJSONObject : public JSONObject {
 public:
  typedef SmartPointer<TestJSONObject> Ref;

  static const char* kFloatValueName;
  static const char* kFloat2ValueName;
  static const char* kFloat3ValueName;
  static const char* kFloat4ValueName;
  static const char* kMatrix4ValueName;
  static const char* kIntegerValueName;
  static const char* kBooleanValueName;
  static const char* kStringValueName;
  static const char* kTransformValueName;
  static const char* kMaterialValueName;
  static const char* kOptionalFloatValueName;
  static const char* kOptionalFloat2ValueName;
  static const char* kOptionalFloat3ValueName;
  static const char* kOptionalFloat4ValueName;
  static const char* kOptionalMatrix4ValueName;
  static const char* kOptionalIntegerValueName;
  static const char* kOptionalBooleanValueName;
  static const char* kOptionalStringValueName;

  float float_value() const {
    return float_value_->value();
  }

  void set_float_value(float value) {
    float_value_->set_value(value);
  }

  Float2 float2_value() const {
    return float2_value_->value();
  }

  void set_float2_value(const Float2& value) {
    float2_value_->set_value(value);
  }

  Float3 float3_value() const {
    return float3_value_->value();
  }

  void set_float3_value(const Float3& value) {
    float3_value_->set_value(value);
  }

  Float4 float4_value() const {
    return float4_value_->value();
  }

  void set_float4_value(const Float4& value) {
    float4_value_->set_value(value);
  }

  Matrix4 matrix4_value() const {
    return matrix4_value_->value();
  }

  void set_matrix4_value(const Matrix4& value) {
    matrix4_value_->set_value(value);
  }

  int integer_value() const {
    return integer_value_->value();
  }

  void set_integer_value(int value) {
    integer_value_->set_value(value);
  }

  bool boolean_value() const {
    return boolean_value_->value();
  }

  void set_boolean_value(bool value) {
    boolean_value_->set_value(value);
  }

  String string_value() const {
    return string_value_->value();
  }

  void set_string_value(const String& value) {
    string_value_->set_value(value);
  }

  Transform* transform_value() const {
    return transform_value_->value();
  }

  void set_transform_value(Transform* value) {
    transform_value_->set_value(value);
  }

  Material* material_value() const {
    return material_value_->value();
  }

  void set_material_value(Material* value) {
    material_value_->set_value(value);
  }

  float optional_float_value() const {
    return optional_float_value_->value();
  }

  void set_optional_float_value(float value) {
    optional_float_value_->set_value(value);
  }

  Float2 optional_float2_value() const {
    return optional_float2_value_->value();
  }

  void set_optional_float2_value(const Float2& value) {
    optional_float2_value_->set_value(value);
  }

  Float3 optional_float3_value() const {
    return optional_float3_value_->value();
  }

  void set_optional_float3_value(const Float3& value) {
    optional_float3_value_->set_value(value);
  }

  Float4 optional_float4_value() const {
    return optional_float4_value_->value();
  }

  void set_optional_float4_value(const Float4& value) {
    optional_float4_value_->set_value(value);
  }

  Matrix4 optional_matrix4_value() const {
    return optional_matrix4_value_->value();
  }

  void set_optional_matrix4_value(const Matrix4& value) {
    optional_matrix4_value_->set_value(value);
  }

  int optional_integer_value() const {
    return optional_integer_value_->value();
  }

  void set_optional_integer_value(int value) {
    optional_integer_value_->set_value(value);
  }

  bool optional_boolean_value() const {
    return optional_boolean_value_->value();
  }

  void set_optional_boolean_value(bool value) {
    optional_boolean_value_->set_value(value);
  }

  String optional_string_value() const {
    return optional_string_value_->value();
  }

  void set_optional_string_value(const String& value) {
    optional_string_value_->set_value(value);
  }

 private:
  explicit TestJSONObject(ServiceLocator* service_locator)
      : JSONObject(service_locator) {
    RegisterJSONValue(kFloatValueName, &float_value_);
    RegisterJSONValue(kFloat2ValueName, &float2_value_);
    RegisterJSONValue(kFloat3ValueName, &float3_value_);
    RegisterJSONValue(kFloat4ValueName, &float4_value_);
    RegisterJSONValue(kMatrix4ValueName, &matrix4_value_);
    RegisterJSONValue(kIntegerValueName, &integer_value_);
    RegisterJSONValue(kBooleanValueName, &boolean_value_);
    RegisterJSONValue(kStringValueName, &string_value_);
    RegisterJSONValue(kTransformValueName, &transform_value_);
    RegisterJSONValue(kMaterialValueName, &material_value_);
    RegisterJSONValue(kOptionalFloatValueName, &optional_float_value_);
    RegisterJSONValue(kOptionalFloat2ValueName, &optional_float2_value_);
    RegisterJSONValue(kOptionalFloat3ValueName, &optional_float3_value_);
    RegisterJSONValue(kOptionalFloat4ValueName, &optional_float4_value_);
    RegisterJSONValue(kOptionalMatrix4ValueName, &optional_matrix4_value_);
    RegisterJSONValue(kOptionalIntegerValueName, &optional_integer_value_);
    RegisterJSONValue(kOptionalBooleanValueName, &optional_boolean_value_);
    RegisterJSONValue(kOptionalStringValueName, &optional_string_value_);
  }

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator) {
    return ObjectBase::Ref(new TestJSONObject(service_locator));
  }

  // One of each type of JSONValue
  JSONFloat::Ref float_value_;
  JSONFloat2::Ref float2_value_;
  JSONFloat3::Ref float3_value_;
  JSONFloat4::Ref float4_value_;
  JSONMatrix4::Ref matrix4_value_;
  JSONInteger::Ref integer_value_;
  JSONBoolean::Ref boolean_value_;
  JSONString::Ref string_value_;
  JSONTransform::Ref transform_value_;
  JSONMaterial::Ref material_value_;
  JSONOptionalFloat::Ref optional_float_value_;
  JSONOptionalFloat2::Ref optional_float2_value_;
  JSONOptionalFloat3::Ref optional_float3_value_;
  JSONOptionalFloat4::Ref optional_float4_value_;
  JSONOptionalMatrix4::Ref optional_matrix4_value_;
  JSONOptionalInteger::Ref optional_integer_value_;
  JSONOptionalBoolean::Ref optional_boolean_value_;
  JSONOptionalString::Ref optional_string_value_;

  O3D_OBJECT_BASE_DECL_CLASS(TestJSONObject, JSONObject);
  DISALLOW_COPY_AND_ASSIGN(TestJSONObject);
};

O3D_OBJECT_BASE_DEFN_CLASS("TestJSONObject", TestJSONObject, JSONObject);

const char* TestJSONObject::kFloatValueName = "floatValue";
const char* TestJSONObject::kFloat2ValueName = "float2Value";
const char* TestJSONObject::kFloat3ValueName = "float3Value";
const char* TestJSONObject::kFloat4ValueName = "float4Value";
const char* TestJSONObject::kMatrix4ValueName = "matrix4Value";
const char* TestJSONObject::kIntegerValueName = "integerValue";
const char* TestJSONObject::kBooleanValueName = "booleanValue";
const char* TestJSONObject::kStringValueName = "stringValue";
const char* TestJSONObject::kTransformValueName = "transformValue";
const char* TestJSONObject::kMaterialValueName = "materialValue";
const char* TestJSONObject::kOptionalFloatValueName = "optionalFloatValue";
const char* TestJSONObject::kOptionalFloat2ValueName = "optionalFloat2Value";
const char* TestJSONObject::kOptionalFloat3ValueName = "optionalFloat3Value";
const char* TestJSONObject::kOptionalFloat4ValueName = "optionalFloat4Value";
const char* TestJSONObject::kOptionalMatrix4ValueName = "optionalMatrix4Value";
const char* TestJSONObject::kOptionalIntegerValueName = "optionalIntegerValue";
const char* TestJSONObject::kOptionalStringValueName = "optionalStringValue";
const char* TestJSONObject::kOptionalBooleanValueName = "optionalBooleanValue";

}  // anonymous namespace

class JSONObjectTest : public testing::Test {
 protected:
  JSONObjectTest()
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
  ClassManager::Register<TestJSONObject> class_register_;
  Pack* pack_;
};

void JSONObjectTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void JSONObjectTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Creates a JSONObject, tests basic properties.
TEST_F(JSONObjectTest, BasicTest) {
  TestJSONObject* object = pack()->Create<TestJSONObject>();
  ASSERT_TRUE(object != NULL);
  EXPECT_TRUE(object->IsA(TestJSONObject::GetApparentClass()));
  EXPECT_TRUE(object->IsA(JSONObject::GetApparentClass()));
  EXPECT_TRUE(object->IsA(ParamObject::GetApparentClass()));

  // Test everything has its default value.
  EXPECT_EQ(object->float_value(), 0.0f);
  EXPECT_EQ(object->float2_value(), Float2(0.0f, 0.0f));
  EXPECT_EQ(object->float2_value(), Float2(0.0f, 0.0f));
  EXPECT_EQ(object->float3_value(), Float3(0.0f, 0.0f, 0.0f));
  EXPECT_EQ(object->float4_value(), Float4(0.0f, 0.0f, 0.0f, 0.0f));
  EXPECT_EQ(object->matrix4_value(), Matrix4::identity());
  EXPECT_EQ(object->integer_value(), 0);
  EXPECT_FALSE(object->boolean_value());
  EXPECT_EQ(object->string_value(), "");
  EXPECT_TRUE(object->transform_value() == NULL);
  EXPECT_TRUE(object->material_value() == NULL);

  // Test that all the values got created by serializing it.
  {
    StringWriter output(StringWriter::LF);
    JsonWriter json_writer(&output, 2);
    object->Serialize(&json_writer);
    json_writer.Close();
    // There is an assumption here that objects will be serialized in this
    // order.
    static const char *kExpected =
        "\"object\": {\n"
        "  \"booleanValue\": false,\n"
        "  \"float2Value\": [0,0],\n"
        "  \"float3Value\": [0,0,0],\n"
        "  \"float4Value\": [0,0,0,0],\n"
        "  \"floatValue\": 0,\n"
        "  \"integerValue\": 0,\n"
        "  \"materialValue\": null,\n"
        "  \"matrix4Value\": [[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],\n"
        "  \"stringValue\": \"\",\n"
        "  \"transformValue\": null\n"
        "}\n";
    EXPECT_EQ(kExpected, output.ToString());
  }
}

TEST_F(JSONObjectTest, SetTest) {
  TestJSONObject* object = pack()->Create<TestJSONObject>();
  ASSERT_TRUE(object != NULL);
  // Test setting the values.
  Matrix4 test_matrix(Vector4(1.0f, 2.0f, 3.0f, 4.0f),
                      Vector4(2.0f, 2.0f, 8.0f, 9.0f),
                      Vector4(3.0f, 5.0f, 3.0f, 10.0f),
                      Vector4(4.0f, 6.0f, 7.0f, 4.0f));
  Matrix4 test_matrix2(Vector4(11.0f, 12.0f, 13.0f, 14.0f),
                       Vector4(12.0f, 12.0f, 18.0f, 19.0f),
                       Vector4(13.0f, 15.0f, 13.0f, 110.0f),
                       Vector4(14.0f, 16.0f, 17.0f, 14.0f));

  object->set_float_value(1.2f);
  object->set_float2_value(Float2(3.4f, 5.6f));
  object->set_float3_value(Float3(7.8f, 9.0f, 10.0f));
  object->set_float4_value(Float4(11.0f, 12.0f, 13.0f, 14.0f));
  object->set_matrix4_value(test_matrix);
  object->set_integer_value(123);
  object->set_boolean_value(true);
  object->set_string_value("test");
  object->set_optional_float_value(15.0f);
  object->set_optional_float2_value(Float2(14.0f, 15.0f));
  object->set_optional_float3_value(Float3(13.0f, 14.0f, 15.0f));
  object->set_optional_float4_value(Float4(12.0f, 13.0f, 14.0f, 15.0f));
  object->set_optional_matrix4_value(test_matrix2);
  object->set_optional_integer_value(456);
  object->set_optional_string_value("hello");
  object->set_optional_boolean_value(false);

  Transform* transform = pack()->Create<Transform>();
  Material* material = pack()->Create<Material>();
  ASSERT_TRUE(transform != NULL);
  ASSERT_TRUE(material != NULL);

  object->set_transform_value(transform);
  object->set_material_value(material);

  // Check the new values
  EXPECT_EQ(object->float_value(), 1.2f);
  EXPECT_EQ(object->float2_value(), Float2(3.4f, 5.6f));
  EXPECT_EQ(object->float3_value(), Float3(7.8f, 9.0f, 10.0f));
  EXPECT_EQ(object->float4_value(), Float4(11.0f, 12.0f, 13.0f, 14.0f));
  EXPECT_EQ(object->matrix4_value(), test_matrix);
  EXPECT_EQ(object->integer_value(), 123);
  EXPECT_TRUE(object->boolean_value());
  EXPECT_EQ(object->string_value(), "test");
  EXPECT_TRUE(object->transform_value() == transform);
  EXPECT_TRUE(object->material_value() == material);

  // See that they got set by serializing them.
  {
    StringWriter output(StringWriter::LF);
    JsonWriter json_writer(&output, 2);
    object->Serialize(&json_writer);
    json_writer.Close();

    char buffer[1000];
    static const char* kExpected =
      "\"object\": {\n"
      "  \"booleanValue\": true,\n"
      "  \"float2Value\": [3.4,5.6],\n"
      "  \"float3Value\": [7.8,9,10],\n"
      "  \"float4Value\": [11,12,13,14],\n"
      "  \"floatValue\": 1.2,\n"
      "  \"integerValue\": 123,\n"
      "  \"materialValue\": {\"ref\":%d},\n"
      "  \"matrix4Value\": [[1,2,3,4],[2,2,8,9],[3,5,3,10],[4,6,7,4]],\n"
      "  \"optionalBooleanValue\": false,\n"
      "  \"optionalFloat2Value\": [14,15],\n"
      "  \"optionalFloat3Value\": [13,14,15],\n"
      "  \"optionalFloat4Value\": [12,13,14,15],\n"
      "  \"optionalFloatValue\": 15,\n"
      "  \"optionalIntegerValue\": 456,\n"
      "  \"optionalMatrix4Value\": "
      "[[11,12,13,14],[12,12,18,19],[13,15,13,110],[14,16,17,14]],\n"
      "  \"optionalStringValue\": \"hello\",\n"
      "  \"stringValue\": \"test\",\n"
      "  \"transformValue\": {\"ref\":%d}\n"
      "}\n";
    sprintf(buffer, kExpected, material->id(), transform->id());  // NOLINT
    EXPECT_EQ(static_cast<const char*>(buffer), output.ToString());
  }
}

}  // namespace o3d

