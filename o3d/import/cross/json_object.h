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

// This file declares the JSONObject class.

#ifndef O3D_IMPORT_CROSS_JSON_OBJECT_H_
#define O3D_IMPORT_CROSS_JSON_OBJECT_H_

#include <map>
#include "core/cross/param_object.h"
#include "core/cross/material.h"
#include "core/cross/transform.h"
#include "serializer/cross/serializer.h"

namespace o3d {

class StructuredWriter;

// A JSONValue is a base class for all JSON values stored in a JSONObject.
class JSONValue : public RefCounted {
 public:
  typedef SmartPointer<JSONValue> Ref;

  virtual ~JSONValue() {
  }

  // Whether or not this value exists.
  bool exists() const {
    return exists_;
  }

  // Sets the existence of this value.
  void set_exists(bool exists) {
    exists_ = exists;
  }

  // Function to serialize the value of this JSON value.
  virtual void Serialize(StructuredWriter* writer) const = 0;

 protected:
  // optional means the value is optional. If true the value defaults to
  // not existing. If false the value defaults to existing.
  explicit JSONValue(bool optional)
      : optional_(optional), exists_(!optional) {
  }

 private:
  bool optional_;
  bool exists_;  // used for optional values.
};

// A Template for optional typed non-ref JSON values.
template<class T, bool optional>
class TypedJSONValue : public JSONValue {
 public:
  typedef T DataType;
  TypedJSONValue()
      : JSONValue(optional) {
  }
  virtual ~TypedJSONValue() {}

  // Sets the value stored in the JSONValue.
  void set_value(const DataType& value) {
    set_exists(true);
    value_ = value;
  }

  // Returns the current value stored in the JSONValue.
  DataType value() const {
    DCHECK(exists());
    return value_;
  }

  // Overridden from JSONValue.
  virtual void Serialize(StructuredWriter* writer) const {
    DCHECK(exists());
    o3d::Serialize(writer, value());
  }

 private:
  // The value stored in the JSONValue.
  DataType value_;

  DISALLOW_COPY_AND_ASSIGN(TypedJSONValue);
};

// A Template for typed ref JSON values.
template<typename T>
class TypedRefJSONValue : public JSONValue {
 public:
  typedef T* Pointer;
  typedef T DataType;

  TypedRefJSONValue()
      : JSONValue(false) {
  }

  virtual ~TypedRefJSONValue() {
  }

  // Set the value stored in the.
  void set_value(Pointer const value) {
    value_ = typename T::Ref(value);
  }

  // Returns the current value stored in the Param.
  Pointer value() const {
    return value_.Get();
  }

  // Overridden from JSONValue.
  virtual void Serialize(StructuredWriter* writer) const {
    o3d::Serialize(writer, static_cast<ObjectBase*>(value()));
  }

 protected:
  typename T::Ref value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedRefJSONValue);
};

// Classes for various types of JSON Data.
class JSONFloat : public TypedJSONValue<float, false> {
 public:
  typedef SmartPointer<JSONFloat> Ref;
  JSONFloat() {
    set_value(0.0f);
  }
};
class JSONFloat2 : public TypedJSONValue<Float2, false> {
 public:
  typedef SmartPointer<JSONFloat2> Ref;
  JSONFloat2() {
    set_value(Float2(0.0f, 0.0f));
  }
};
class JSONFloat3 : public TypedJSONValue<Float3, false> {
 public:
  typedef SmartPointer<JSONFloat3> Ref;
  JSONFloat3() {
    set_value(Float3(0.0f, 0.0f, 0.0f));
  }
};
class JSONFloat4 : public TypedJSONValue<Float4, false> {
 public:
  typedef SmartPointer<JSONFloat4> Ref;
  JSONFloat4() {
    set_value(Float4(0.0f, 0.0f, 0.0f, 0.0f));
  }
};
class JSONMatrix4 : public TypedJSONValue<Matrix4, false> {
 public:
  typedef SmartPointer<JSONMatrix4> Ref;
  JSONMatrix4() {
    set_value(Matrix4::identity());
  }
};
class JSONInteger : public TypedJSONValue<int, false> {
 public:
  typedef SmartPointer<JSONInteger> Ref;
  JSONInteger() {
    set_value(0);
  }
};
class JSONBoolean : public TypedJSONValue<bool, false> {
 public:
  typedef SmartPointer<JSONBoolean> Ref;
  JSONBoolean() {
    set_value(false);
  }
};
class JSONString : public TypedJSONValue<String, false> {
 public:
  typedef SmartPointer<JSONString> Ref;
};
class JSONTransform : public TypedRefJSONValue<Transform> {
 public:
  typedef SmartPointer<JSONTransform> Ref;
};
class JSONMaterial : public TypedRefJSONValue<Material> {
 public:
  typedef SmartPointer<JSONMaterial> Ref;
};
class JSONOptionalFloat : public TypedJSONValue<float, true> {
 public:
  typedef SmartPointer<JSONOptionalFloat> Ref;
};
class JSONOptionalFloat2 : public TypedJSONValue<Float2, true> {
 public:
  typedef SmartPointer<JSONOptionalFloat2> Ref;
};
class JSONOptionalFloat3 : public TypedJSONValue<Float3, true> {
 public:
  typedef SmartPointer<JSONOptionalFloat3> Ref;
};
class JSONOptionalFloat4 : public TypedJSONValue<Float4, true> {
 public:
  typedef SmartPointer<JSONOptionalFloat4> Ref;
};
class JSONOptionalMatrix4 : public TypedJSONValue<Matrix4, true> {
 public:
  typedef SmartPointer<JSONOptionalMatrix4> Ref;
};
class JSONOptionalInteger : public TypedJSONValue<int, true> {
 public:
  typedef SmartPointer<JSONOptionalInteger> Ref;
};
class JSONOptionalBoolean : public TypedJSONValue<bool, true> {
 public:
  typedef SmartPointer<JSONOptionalBoolean> Ref;
};
class JSONOptionalString : public TypedJSONValue<String, true> {
 public:
  typedef SmartPointer<JSONOptionalString> Ref;
};

// A JSONObject is a used for serialization only and is not part of the normal
// O3D plugin. It is used to easily add name/value pairs to be serialized by the
// serializer as JSON objects.  Being that it's ParamObject you have 2 choices
// for getting data serialized. As O3D Params or as JSONValues. The reason to
// use one over the other... Params can be animated but are relatively heavy.
// JSONValues become plane JavaScript when deserialized.
// To add a Param use RegisterParamRef, to add a JSONValue use
// RegisterJSONValue.
class JSONObject : public ParamObject {
 public:
  typedef SmartPointer<JSONObject> Ref;
  typedef std::map<String, JSONValue::Ref> NameValueMap;

  // Seralizes the JSONObject.
  void Serialize(StructuredWriter* writer) const;

 protected:
  explicit JSONObject(ServiceLocator* service_locator);

  // Registers a pointer to a reference to a JSONValue.
  // Parameters:
  //   name: name of JSONValue
  //   ref_pointer: Pointer to typed reference to JSONValue.
  template<typename T>
  void RegisterJSONValue(const String& name, T* ref_pointer) {
    *ref_pointer = T(new typename T::ClassType());
    AddProperty(name, ref_pointer->Get());
  }

 private:
  // Adds a property to this JSON Object.
  void AddProperty(const String& name, JSONValue* value);

  NameValueMap properties_;

  O3D_OBJECT_BASE_DECL_CLASS(JSONObject, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(JSONObject);
};

}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_JSON_OBJECT_H_


