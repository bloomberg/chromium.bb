// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <algorithm>
#include <iostream>
#include <ostream>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"
#include "tools/ipc_fuzzer/mutate/rand_util.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

// First include of message files to provide basic type.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

#if defined(COMPILER_GCC)
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(COMPILER_MSVC)
#define PRETTY_FUNCTION __FUNCSIG__
#else
#define PRETTY_FUNCTION __FUNCTION__
#endif

namespace IPC {
class Message;
}  // namespace IPC

namespace {
// For breaking deep recursion.
int g_depth = 0;
}  // namespace

namespace ipc_fuzzer {

// Interface implemented by those who generate basic types.  The types all
// correspond to the types which a pickle from base/pickle.h can pickle,
// plus the floating point types.
class Generator {
 public:
  virtual void GenerateBool(bool* value) = 0;
  virtual void GenerateInt(int* value) = 0;
  virtual void GenerateLong(long* value) = 0;
  virtual void GenerateSize(size_t* value) = 0;
  virtual void GenerateUChar(unsigned char *value) = 0;
  virtual void GenerateWChar(wchar_t *value) = 0;
  virtual void GenerateUInt16(uint16* value) = 0;
  virtual void GenerateUInt32(uint32* value) = 0;
  virtual void GenerateInt64(int64* value) = 0;
  virtual void GenerateUInt64(uint64* value) = 0;
  virtual void GenerateFloat(float *value) = 0;
  virtual void GenerateDouble(double *value) = 0;
  virtual void GenerateString(std::string* value) = 0;
  virtual void GenerateString16(base::string16* value) = 0;
  virtual void GenerateData(char* data, int length) = 0;
  virtual void GenerateBytes(void* data, int data_len) = 0;
};

typedef IPC::Message* (*GeneratorFunction)(Generator*);
typedef std::vector<GeneratorFunction> GeneratorFunctionVector;
GeneratorFunctionVector g_function_vector;

template <typename T>
void GenerateIntegralType(T* value) {
  switch (RandInRange(16)) {
    case 0:
      *value = static_cast<T>(0);
      break;
    case 1:
      *value = static_cast<T>(1);
      break;
    case 2:
      *value = static_cast<T>(-1);
      break;
    case 3:
      *value = static_cast<T>(2);
      break;
    default:
      *value = static_cast<T>(RandU64());
      break;
  }
}

template <typename T>
void GenerateFloatingType(T* value) {
  *value = RandDouble();
}

template <typename T>
void GenerateStringType(T* value) {
  T temp_string;
  size_t length = RandInRange(300);
  for (size_t i = 0; i < length; ++i)
    temp_string += RandInRange(256);
  *value = temp_string;
}

class GeneratorImpl : public Generator {
 public:
  GeneratorImpl() {}
  virtual ~GeneratorImpl() {}

  void GenerateBool(bool* value) override {
    *value = RandInRange(2) ? true: false;
  }

  void GenerateInt(int* value) override {
    GenerateIntegralType<int>(value);
  }

  void GenerateLong(long* value) override {
    GenerateIntegralType<long>(value);
  }

  void GenerateSize(size_t* value) override {
    GenerateIntegralType<size_t>(value);
  }

  void GenerateUChar(unsigned char* value) override {
    GenerateIntegralType<unsigned char>(value);
  }

  void GenerateWChar(wchar_t* value) override {
    GenerateIntegralType<wchar_t>(value);
  }

  void GenerateUInt16(uint16* value) override {
    GenerateIntegralType<uint16>(value);
  }

  void GenerateUInt32(uint32* value) override {
    GenerateIntegralType<uint32>(value);
  }

  void GenerateInt64(int64* value) override {
    GenerateIntegralType<int64>(value);
  }

  void GenerateUInt64(uint64* value) override {
    GenerateIntegralType<uint64>(value);
  }

  void GenerateFloat(float* value) override {
    GenerateFloatingType<float>(value);
  }

  void GenerateDouble(double* value) override {
    GenerateFloatingType<double>(value);
  }

  void GenerateString(std::string* value) override {
    GenerateStringType<std::string>(value);
  }

  void GenerateString16(base::string16* value) override {
    GenerateStringType<base::string16>(value);
  }

  void GenerateData(char* data, int length) override {
    for (int i = 0; i < length; ++i) {
      GenerateIntegralType<char>(&data[i]);
    }
  }

  void GenerateBytes(void* data, int data_len) override {
    GenerateData(static_cast<char*>(data), data_len);
  }
};

// Partially-specialized class that knows how to generate a given type.
template <class P>
struct GenerateTraits {
  static bool Generate(P* p, Generator *generator) {
    // This is the catch-all for types we don't have enough information
    // to generate.
    std::cerr << "Can't handle " << PRETTY_FUNCTION << "\n";
    return false;
  }
};

// Template function to invoke partially-specialized class method.
template <class P>
static bool GenerateParam(P* p, Generator* generator) {
  return GenerateTraits<P>::Generate(p, generator);
};

template <class P>
static bool GenerateParamArray(P* p, size_t length, Generator* generator) {
  for (size_t i = 0; i < length; i++, p++) {
    if (!GenerateTraits<P>::Generate(p, generator))
      return false;
  }
  return true;
};

// Specializations to generate primitive types.
template <>
struct GenerateTraits<bool> {
  static bool Generate(bool* p, Generator* generator) {
    generator->GenerateBool(p);
    return true;
  }
};

template <>
struct GenerateTraits<int> {
  static bool Generate(int* p, Generator* generator) {
    generator->GenerateInt(p);
    return true;
  }
};

template <>
struct GenerateTraits<unsigned int> {
  static bool Generate(unsigned int* p, Generator* generator) {
    generator->GenerateInt(reinterpret_cast<int*>(p));
    return true;
  }
};

template <>
struct GenerateTraits<long> {
  static bool Generate(long* p, Generator* generator) {
    generator->GenerateLong(p);
    return true;
  }
};

template <>
struct GenerateTraits<unsigned long> {
  static bool Generate(unsigned long* p, Generator* generator) {
    generator->GenerateLong(reinterpret_cast<long*>(p));
    return true;
  }
};

template <>
struct GenerateTraits<long long> {
  static bool Generate(long long* p, Generator* generator) {
    generator->GenerateInt64(reinterpret_cast<int64*>(p));
    return true;
  }
};

template <>
struct GenerateTraits<unsigned long long> {
  static bool Generate(unsigned long long* p, Generator* generator) {
    generator->GenerateInt64(reinterpret_cast<int64*>(p));
    return true;
  }
};

template <>
struct GenerateTraits<short> {
  static bool Generate(short* p, Generator* generator) {
    generator->GenerateUInt16(reinterpret_cast<uint16*>(p));
    return true;
  }
};

template <>
struct GenerateTraits<unsigned short> {
  static bool Generate(unsigned short* p, Generator* generator) {
    generator->GenerateUInt16(reinterpret_cast<uint16*>(p));
    return true;
  }
};

template <>
struct GenerateTraits<char> {
  static bool Generate(char* p, Generator* generator) {
    generator->GenerateUChar(reinterpret_cast<unsigned char*>(p));
    return true;
  }
};

template <>
struct GenerateTraits<unsigned char> {
  static bool Generate(unsigned char* p, Generator* generator) {
    generator->GenerateUChar(p);
    return true;
  }
};

template <>
struct GenerateTraits<wchar_t> {
  static bool Generate(wchar_t* p, Generator* generator) {
    generator->GenerateWChar(p);
    return true;
  }
};

template <>
struct GenerateTraits<float> {
  static bool Generate(float* p, Generator* generator) {
    generator->GenerateFloat(p);
    return true;
  }
};

template <>
struct GenerateTraits<double> {
  static bool Generate(double* p, Generator* generator) {
    generator->GenerateDouble(p);
    return true;
  }
};

template <>
struct GenerateTraits<std::string> {
  static bool Generate(std::string* p, Generator* generator) {
    generator->GenerateString(p);
    return true;
  }
};

template <>
struct GenerateTraits<base::string16> {
  static bool Generate(base::string16* p, Generator* generator) {
    generator->GenerateString16(p);
    return true;
  }
};

// Specializations to generate tuples.
template <>
struct GenerateTraits<Tuple<>> {
  static bool Generate(Tuple<>* p, Generator* generator) {
    return true;
  }
};

template <class A>
struct GenerateTraits<Tuple<A>> {
  static bool Generate(Tuple<A>* p, Generator* generator) {
    return GenerateParam(&get<0>(*p), generator);
  }
};

template <class A, class B>
struct GenerateTraits<Tuple<A, B>> {
  static bool Generate(Tuple<A, B>* p, Generator* generator) {
    return
        GenerateParam(&get<0>(*p), generator) &&
        GenerateParam(&get<1>(*p), generator);
  }
};

template <class A, class B, class C>
struct GenerateTraits<Tuple<A, B, C>> {
  static bool Generate(Tuple<A, B, C>* p, Generator* generator) {
    return
        GenerateParam(&get<0>(*p), generator) &&
        GenerateParam(&get<1>(*p), generator) &&
        GenerateParam(&get<2>(*p), generator);
  }
};

template <class A, class B, class C, class D>
struct GenerateTraits<Tuple<A, B, C, D>> {
  static bool Generate(Tuple<A, B, C, D>* p, Generator* generator) {
    return
        GenerateParam(&get<0>(*p), generator) &&
        GenerateParam(&get<1>(*p), generator) &&
        GenerateParam(&get<2>(*p), generator) &&
        GenerateParam(&get<3>(*p), generator);
  }
};

template <class A, class B, class C, class D, class E>
struct GenerateTraits<Tuple<A, B, C, D, E>> {
  static bool Generate(Tuple<A, B, C, D, E>* p, Generator* generator) {
    return
        GenerateParam(&get<0>(*p), generator) &&
        GenerateParam(&get<1>(*p), generator) &&
        GenerateParam(&get<2>(*p), generator) &&
        GenerateParam(&get<3>(*p), generator) &&
        GenerateParam(&get<4>(*p), generator);
  }
};

// Specializations to generate containers.
template <class A>
struct GenerateTraits<std::vector<A> > {
  static bool Generate(std::vector<A>* p, Generator* generator) {
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    p->resize(count);
    for (size_t i = 0; i < count; ++i) {
      if (!GenerateParam(&p->at(i), generator)) {
        --g_depth;
        return false;
      }
    }
    --g_depth;
    return true;
  }
};

template <class A>
struct GenerateTraits<std::set<A> > {
  static bool Generate(std::set<A>* p, Generator* generator) {
    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    A a;
    for (size_t i = 0; i < count; ++i) {
      if (!GenerateParam(&a, generator)) {
        --g_depth;
        return false;
      }
      p->insert(a);
    }
    --g_depth;
    return true;
  }
};


template <class A, class B>
struct GenerateTraits<std::map<A, B> > {
  static bool Generate(std::map<A, B>* p, Generator* generator) {
    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    std::pair<A, B> place_holder;
    for (size_t i = 0; i < count; ++i) {
      if (!GenerateParam(&place_holder, generator)) {
        --g_depth;
        return false;
      }
      p->insert(place_holder);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B, class C, class D>
struct GenerateTraits<std::map<A, B, C, D>> {
  static bool Generate(std::map<A, B, C, D>* p, Generator* generator) {
    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    std::pair<A, B> place_holder;
    for (size_t i = 0; i < count; ++i) {
      if (!GenerateParam(&place_holder, generator)) {
        --g_depth;
        return false;
      }
      p->insert(place_holder);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B>
struct GenerateTraits<std::pair<A, B> > {
  static bool Generate(std::pair<A, B>* p, Generator* generator) {
    return
        GenerateParam(&p->first, generator) &&
        GenerateParam(&p->second, generator);
  }
};

// Specializations to generate hand-coded types.
template <>
struct GenerateTraits<base::NullableString16> {
  static bool Generate(base::NullableString16* p, Generator* generator) {
    *p = base::NullableString16();
    return true;
  }
};

template <>
struct GenerateTraits<base::FilePath> {
  static bool Generate(base::FilePath* p, Generator* generator) {
    const char path_chars[] = "ACz0/.~:";
    size_t count = RandInRange(60);
    base::FilePath::StringType random_path;
    for (size_t i = 0; i < count; ++i)
      random_path += path_chars[RandInRange(sizeof(path_chars) - 1)];
    *p = base::FilePath(random_path);
    return true;
  }
};

template <>
struct GenerateTraits<base::File::Error> {
  static bool Generate(base::File::Error* p, Generator* generator) {
    int temporary;
    if (!GenerateParam(&temporary, generator))
      return false;
    *p = static_cast<base::File::Error>(temporary);
    return true;
  }
};

template <>
struct GenerateTraits<base::File::Info> {
  static bool Generate(base::File::Info* p, Generator* generator) {
    double last_modified;
    double last_accessed;
    double creation_time;
    if (!GenerateParam(&p->size, generator))
      return false;
    if (!GenerateParam(&p->is_directory, generator))
      return false;
    if (!GenerateParam(&last_modified, generator))
      return false;
    if (!GenerateParam(&last_accessed, generator))
      return false;
    if (!GenerateParam(&creation_time, generator))
      return false;
    p->last_modified = base::Time::FromDoubleT(last_modified);
    p->last_accessed = base::Time::FromDoubleT(last_accessed);
    p->creation_time = base::Time::FromDoubleT(creation_time);
    return true;
  }
};

template <>
struct GenerateTraits<base::Time> {
  static bool Generate(base::Time* p, Generator* generator) {
    *p = base::Time::FromInternalValue(RandU64());
    return true;
  }
};

template <>
struct GenerateTraits<base::TimeDelta> {
  static bool Generate(base::TimeDelta* p, Generator* generator) {
    *p = base::TimeDelta::FromInternalValue(RandU64());
    return true;
  }
};

template <>
struct GenerateTraits<base::TimeTicks> {
  static bool Generate(base::TimeTicks* p, Generator* generator) {
    *p = base::TimeTicks::FromInternalValue(RandU64());
    return true;
  }
};

template <>
struct GenerateTraits<base::ListValue> {
  static bool Generate(base::ListValue* p, Generator* generator) {
    ++g_depth;
    size_t list_length = g_depth > 3 ? 0 : RandInRange(8);
    for (size_t index = 0; index < list_length; ++index) {
      switch (RandInRange(8))
      {
        case base::Value::TYPE_BOOLEAN: {
          bool tmp;
          generator->GenerateBool(&tmp);
          p->Set(index, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_INTEGER: {
          int tmp;
          generator->GenerateInt(&tmp);
          p->Set(index, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_DOUBLE: {
          double tmp;
          generator->GenerateDouble(&tmp);
          p->Set(index, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_STRING: {
          std::string tmp;
          generator->GenerateString(&tmp);
          p->Set(index, new base::StringValue(tmp));
          break;
        }
        case base::Value::TYPE_BINARY: {
          char tmp[200];
          size_t bin_length = RandInRange(sizeof(tmp));
          generator->GenerateData(tmp, bin_length);
          p->Set(index,
                 base::BinaryValue::CreateWithCopiedBuffer(tmp, bin_length));
          break;
        }
        case base::Value::TYPE_DICTIONARY: {
          base::DictionaryValue* tmp = new base::DictionaryValue();
          GenerateParam(tmp, generator);
          p->Set(index, tmp);
          break;
        }
        case base::Value::TYPE_LIST: {
          base::ListValue* tmp = new base::ListValue();
          GenerateParam(tmp, generator);
          p->Set(index, tmp);
          break;
        }
        case base::Value::TYPE_NULL:
        default:
          break;
      }
    }
    --g_depth;
    return true;
  }
};

template <>
struct GenerateTraits<base::DictionaryValue> {
  static bool Generate(base::DictionaryValue* p, Generator* generator) {
    ++g_depth;
    size_t dict_length = g_depth > 3 ? 0 : RandInRange(8);
    for (size_t index = 0; index < dict_length; ++index) {
      std::string property;
      generator->GenerateString(&property);
      switch (RandInRange(8))
      {
        case base::Value::TYPE_BOOLEAN: {
          bool tmp;
          generator->GenerateBool(&tmp);
          p->SetWithoutPathExpansion(property, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_INTEGER: {
          int tmp;
          generator->GenerateInt(&tmp);
          p->SetWithoutPathExpansion(property, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_DOUBLE: {
          double tmp;
          generator->GenerateDouble(&tmp);
          p->SetWithoutPathExpansion(property, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_STRING: {
          std::string tmp;
          generator->GenerateString(&tmp);
          p->SetWithoutPathExpansion(property, new base::StringValue(tmp));
          break;
        }
        case base::Value::TYPE_BINARY: {
          char tmp[200];
          size_t bin_length = RandInRange(sizeof(tmp));
          generator->GenerateData(tmp, bin_length);
          p->SetWithoutPathExpansion(
              property,
              base::BinaryValue::CreateWithCopiedBuffer(tmp, bin_length));
          break;
        }
        case base::Value::TYPE_DICTIONARY: {
          base::DictionaryValue* tmp = new base::DictionaryValue();
          GenerateParam(tmp, generator);
          p->SetWithoutPathExpansion(property, tmp);
          break;
        }
        case base::Value::TYPE_LIST: {
          base::ListValue* tmp = new base::ListValue();
          GenerateParam(tmp, generator);
          p->SetWithoutPathExpansion(property, tmp);
          break;
        }
        case base::Value::TYPE_NULL:
        default:
          break;
      }
    }
    --g_depth;
    return true;
  }
};

template <>
struct GenerateTraits<blink::WebGamepad> {
  static bool Generate(blink::WebGamepad* p, Generator* generator) {
    if (!GenerateParam(&p->connected, generator))
      return false;
    if (!GenerateParam(&p->timestamp, generator))
      return false;
    unsigned idLength = static_cast<unsigned>(
        RandInRange(blink::WebGamepad::idLengthCap + 1));
    if (!GenerateParamArray(&p->id[0], idLength, generator))
      return false;
    p->axesLength = static_cast<unsigned>(
        RandInRange(blink::WebGamepad::axesLengthCap + 1));
    if (!GenerateParamArray(&p->axes[0], p->axesLength, generator))
      return false;
    p->buttonsLength = static_cast<unsigned>(
        RandInRange(blink::WebGamepad::buttonsLengthCap + 1));
    if (!GenerateParamArray(&p->buttons[0], p->buttonsLength, generator))
      return false;
    unsigned mappingsLength = static_cast<unsigned>(
      RandInRange(blink::WebGamepad::mappingLengthCap + 1));
    if (!GenerateParamArray(&p->mapping[0], mappingsLength, generator))
      return false;
    return true;
  }
};

template <>
struct GenerateTraits<blink::WebGamepadButton> {
  static bool Generate(blink::WebGamepadButton* p, Generator* generator) {
    bool pressed;
    double value;
    if (!GenerateParam(&pressed, generator))
      return false;
    if (!GenerateParam(&value, generator))
      return false;
    *p = blink::WebGamepadButton(pressed, value);
    return true;
  }
};

template <>
struct GenerateTraits<cc::CompositorFrame> {
  static bool Generate(cc::CompositorFrame* p, Generator* generator) {
    if (!GenerateParam(&p->metadata, generator))
      return false;
    switch (RandInRange(4)) {
      case 0: {
        p->delegated_frame_data.reset(new cc::DelegatedFrameData());
        if (!GenerateParam(p->delegated_frame_data.get(), generator))
          return false;
        return true;
      }
      case 1: {
        p->gl_frame_data.reset(new cc::GLFrameData());
        if (!GenerateParam(p->gl_frame_data.get(), generator))
          return false;
        return true;
      }
      case 2: {
        p->software_frame_data.reset(new cc::SoftwareFrameData());
        if (!GenerateParam(p->software_frame_data.get(), generator))
          return false;
        return true;
      }
      default:
        // Generate nothing to handle the no frame case.
        return true;
    }
  }
};

template <>
struct GenerateTraits<cc::CompositorFrameAck> {
  static bool Generate(cc::CompositorFrameAck* p, Generator* generator) {
    if (!GenerateParam(&p->resources, generator))
      return false;
    if (!GenerateParam(&p->last_software_frame_id, generator))
      return false;
    p->gl_frame_data.reset(new cc::GLFrameData);
    if (!GenerateParam(p->gl_frame_data.get(), generator))
      return false;
    return true;
  }
};

template <>
struct GenerateTraits<cc::DelegatedFrameData> {
  static bool Generate(cc::DelegatedFrameData* p, Generator* generator) {
    if (!GenerateParam(&p->device_scale_factor, generator))
      return false;
    if (!GenerateParam(&p->resource_list, generator))
      return false;
    if (!GenerateParam(&p->render_pass_list, generator))
      return false;
    return true;
  }
};

template <class A>
struct GenerateTraits<cc::ListContainer<A>> {
  static bool Generate(cc::ListContainer<A>* p, Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    return true;
  }
};

template <>
struct GenerateTraits<cc::QuadList> {
  static bool Generate(cc::QuadList* p, Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    return true;
  }
};

template <>
struct GenerateTraits<cc::RenderPass> {
  static bool Generate(cc::RenderPass* p, Generator* generator) {
    if (!GenerateParam(&p->id, generator))
      return false;
    if (!GenerateParam(&p->output_rect, generator))
      return false;
    if (!GenerateParam(&p->damage_rect, generator))
      return false;
    if (!GenerateParam(&p->transform_to_root_target, generator))
      return false;
    if (!GenerateParam(&p->has_transparent_background, generator))
      return false;
    if (!GenerateParam(&p->quad_list, generator))
      return false;
    if (!GenerateParam(&p->shared_quad_state_list, generator))
      return false;
    // Omitting |copy_requests| as it is not sent over IPC.
    return true;
  }
};

template <>
struct GenerateTraits<cc::RenderPassList> {
  static bool Generate(cc::RenderPassList* p, Generator* generator) {
    size_t count = RandElementCount();
    for (size_t i = 0; i < count; ++i) {
      scoped_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
      if (!GenerateParam(render_pass.get(), generator))
        return false;
      p->push_back(render_pass.Pass());
    }
    return true;
  }
};

template <>
struct GenerateTraits<cc::SoftwareFrameData> {
  static bool Generate(cc::SoftwareFrameData* p, Generator* generator) {
    if (!GenerateParam(&p->id, generator))
      return false;
    if (!GenerateParam(&p->size, generator))
      return false;
    if (!GenerateParam(&p->damage_rect, generator))
      return false;
    if (!GenerateParam(&p->bitmap_id, generator))
      return false;
    return true;
  }
};

template <>
struct GenerateTraits<content::IndexedDBKey> {
  static bool Generate(content::IndexedDBKey* p, Generator* generator) {
    ++g_depth;
    blink::WebIDBKeyType web_type =
        static_cast<blink::WebIDBKeyType>(RandInRange(7));
    switch (web_type)
    {
      case blink::WebIDBKeyTypeArray: {
        size_t length = g_depth > 3 ? 0 : RandInRange(4);
        std::vector<content::IndexedDBKey> array;
        array.resize(length);
        for (size_t i = 0; i < length; ++i) {
            if (!GenerateParam(&array[i], generator)) {
              --g_depth;
              return false;
            }
        }
        *p = content::IndexedDBKey(array);
        return true;
      }
      case blink::WebIDBKeyTypeBinary: {
        std::string binary;
        if (!GenerateParam(&binary, generator)) {
            --g_depth;
            return false;
        }
        *p = content::IndexedDBKey(binary);
        return true;
      }
      case blink::WebIDBKeyTypeString: {
        base::string16 string;
        if (!GenerateParam(&string, generator))
          return false;
        *p = content::IndexedDBKey(string);
        return true;
      }
      case blink::WebIDBKeyTypeDate:
      case blink::WebIDBKeyTypeNumber: {
        double number;
        if (!GenerateParam(&number, generator)) {
            --g_depth;
            return false;
        }
        *p = content::IndexedDBKey(number, web_type);
        return true;
      }
      case blink::WebIDBKeyTypeInvalid:
      case blink::WebIDBKeyTypeNull: {
        *p = content::IndexedDBKey(web_type);
        return true;
      }
      default: {
          NOTREACHED();
          --g_depth;
          return false;
      }
    }
  }
};

template <>
struct GenerateTraits<content::IndexedDBKeyRange> {
  static bool Generate(content::IndexedDBKeyRange *p, Generator* generator) {
    content::IndexedDBKey lower;
    content::IndexedDBKey upper;
    bool lower_open;
    bool upper_open;
    if (!GenerateParam(&lower, generator))
      return false;
    if (!GenerateParam(&upper, generator))
      return false;
    if (!GenerateParam(&lower_open, generator))
      return false;
    if (!GenerateParam(&upper_open, generator))
      return false;
    *p = content::IndexedDBKeyRange(lower, upper, lower_open, upper_open);
    return true;
  }
};

template <>
struct GenerateTraits<content::IndexedDBKeyPath> {
  static bool Generate(content::IndexedDBKeyPath *p, Generator* generator) {
    switch (RandInRange(3)) {
      case 0: {
        std::vector<base::string16> array;
        if (!GenerateParam(&array, generator))
          return false;
        *p = content::IndexedDBKeyPath(array);
        break;
      }
      case 1: {
        base::string16 string;
        if (!GenerateParam(&string, generator))
          return false;
        *p = content::IndexedDBKeyPath(string);
        break;
      }
      case 2: {
        *p = content::IndexedDBKeyPath();
        break;
      }
    }
    return true;
  }
};

template <>
struct GenerateTraits<content::NPIdentifier_Param> {
  static bool Generate(content::NPIdentifier_Param* p, Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = content::NPIdentifier_Param();
    return true;
  }
};

template <>
struct GenerateTraits<content::NPVariant_Param> {
  static bool Generate(content::NPVariant_Param* p, Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = content::NPVariant_Param();
    return true;
  }
};

template <>
struct GenerateTraits<content::PageState> {
  static bool Generate(content::PageState* p, Generator* generator) {
    std::string junk;
    if (!GenerateParam(&junk, generator))
      return false;
    *p = content::PageState::CreateFromEncodedData(junk);
    return true;
  }
};

template <>
struct GenerateTraits<content::SyntheticGesturePacket> {
  static bool Generate(content::SyntheticGesturePacket* p,
                       Generator* generator) {
    scoped_ptr<content::SyntheticGestureParams> gesture_params;
    switch (RandInRange(
        content::SyntheticGestureParams::SYNTHETIC_GESTURE_TYPE_MAX + 1)) {
      case content::SyntheticGestureParams::GestureType::
          SMOOTH_SCROLL_GESTURE: {
        content::SyntheticSmoothScrollGestureParams* params =
            new content::SyntheticSmoothScrollGestureParams();
        if (!GenerateParam(&params->anchor, generator))
          return false;
        if (!GenerateParam(&params->distances, generator))
          return false;
        if (!GenerateParam(&params->prevent_fling, generator))
          return false;
        if (!GenerateParam(&params->speed_in_pixels_s, generator))
          return false;
        gesture_params.reset(params);
        break;
      }
      case content::SyntheticGestureParams::GestureType::
          SMOOTH_MOUSE_DRAG_GESTURE: {
        content::SyntheticSmoothMouseDragGestureParams* params =
            new content::SyntheticSmoothMouseDragGestureParams();
        if (!GenerateParam(&params->start_point, generator))
          return false;
        if (!GenerateParam(&params->distances, generator))
          return false;
        if (!GenerateParam(&params->speed_in_pixels_s, generator))
          return false;
        gesture_params.reset(params);
        break;
      }
      case content::SyntheticGestureParams::GestureType::PINCH_GESTURE: {
        content::SyntheticPinchGestureParams* params =
            new content::SyntheticPinchGestureParams();
        if (!GenerateParam(&params->scale_factor, generator))
          return false;
        if (!GenerateParam(&params->anchor, generator))
          return false;
        if (!GenerateParam(&params->relative_pointer_speed_in_pixels_s,
                           generator))
          return false;
        gesture_params.reset(params);
        break;
      }
      case content::SyntheticGestureParams::GestureType::TAP_GESTURE: {
        content::SyntheticTapGestureParams* params =
            new content::SyntheticTapGestureParams();
        if (!GenerateParam(&params->position, generator))
          return false;
        if (!GenerateParam(&params->duration_ms, generator))
          return false;
        gesture_params.reset(params);
        break;
      }
    }
    p->set_gesture_params(gesture_params.Pass());
    return true;
  }
};

template <>
struct GenerateTraits<content::WebCursor> {
  static bool Generate(content::WebCursor* p, Generator* generator) {
    content::WebCursor::CursorInfo info;

    // |type| enum is not validated on de-serialization, so pick random value.
    if (!GenerateParam(reinterpret_cast<int *>(&info.type), generator))
      return false;
    if (!GenerateParam(&info.hotspot, generator))
      return false;
    if (!GenerateParam(&info.image_scale_factor, generator))
      return false;
    if (!GenerateParam(&info.custom_image, generator))
      return false;
    // Omitting |externalHandle| since it is not serialized.

    // Scale factor is expected to be greater than 0, otherwise we hit
    // a check failure.
    info.image_scale_factor = fabs(info.image_scale_factor) + 0.001;

    *p = content::WebCursor(info);
    return true;
  }
};

template <>
struct GenerateTraits<ContentSettingsPattern> {
  static bool Generate(ContentSettingsPattern* p, Generator* generator) {
    // TODO(mbarbella): This can crash if a pattern is generated from a random
    // string. We could carefully generate a pattern or fix pattern generation.
    *p = ContentSettingsPattern();
    return true;
  }
};

template <>
struct GenerateTraits<ExtensionMsg_PermissionSetStruct> {
  static bool Generate(ExtensionMsg_PermissionSetStruct* p,
                       Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = ExtensionMsg_PermissionSetStruct();
    return true;
  }
};

template <>
struct GenerateTraits<extensions::URLPatternSet> {
  static bool Generate(extensions::URLPatternSet* p, Generator* generator) {
    std::set<URLPattern> patterns;
    if (!GenerateParam(&patterns, generator))
      return false;
    *p = extensions::URLPatternSet(patterns);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::Point> {
  static bool Generate(gfx::Point *p, Generator* generator) {
    int x;
    int y;
    if (!GenerateParam(&x, generator))
      return false;
    if (!GenerateParam(&y, generator))
      return false;
    p->SetPoint(x, y);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::PointF> {
  static bool Generate(gfx::PointF *p, Generator* generator) {
    float x;
    float y;
    if (!GenerateParam(&x, generator))
      return false;
    if (!GenerateParam(&y, generator))
      return false;
    p->SetPoint(x, y);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::Rect> {
  static bool Generate(gfx::Rect *p, Generator* generator) {
    gfx::Point origin;
    gfx::Size  size;
    if (!GenerateParam(&origin, generator))
      return false;
    if (!GenerateParam(&size, generator))
      return false;
    p->set_origin(origin);
    p->set_size(size);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::RectF> {
  static bool Generate(gfx::RectF *p, Generator* generator) {
    gfx::PointF origin;
    gfx::SizeF  size;
    if (!GenerateParam(&origin, generator))
      return false;
    if (!GenerateParam(&size, generator))
      return false;
    p->set_origin(origin);
    p->set_size(size);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::Range> {
  static bool Generate(gfx::Range *p, Generator* generator) {
    size_t start;
    size_t end;
    if (!GenerateParam(&start, generator))
      return false;
    if (!GenerateParam(&end, generator))
      return false;
    *p = gfx::Range(start, end);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::Size> {
  static bool Generate(gfx::Size* p, Generator* generator) {
    int w;
    int h;
    if (!GenerateParam(&w, generator))
      return false;
    if (!GenerateParam(&h, generator))
      return false;
    p->SetSize(w, h);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::SizeF> {
  static bool Generate(gfx::SizeF* p, Generator* generator) {
    float w;
    float h;
    if (!GenerateParam(&w, generator))
      return false;
    if (!GenerateParam(&h, generator))
      return false;
    p->SetSize(w, h);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::Transform> {
  static bool Generate(gfx::Transform* p, Generator* generator) {
    SkMScalar matrix[16];
    if (!GenerateParamArray(&matrix[0], arraysize(matrix), generator))
      return false;
    *p = gfx::Transform(matrix[0], matrix[1], matrix[2], matrix[3], matrix[4],
                        matrix[5], matrix[6], matrix[7], matrix[8], matrix[9],
                        matrix[10], matrix[11], matrix[12], matrix[13],
                        matrix[14], matrix[15]);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::Vector2d> {
  static bool Generate(gfx::Vector2d *p, Generator* generator) {
    int x;
    int y;
    if (!GenerateParam(&x, generator))
      return false;
    if (!GenerateParam(&y, generator))
      return false;
    *p = gfx::Vector2d(x, y);
    return true;
  }
};

template <>
struct GenerateTraits<gfx::Vector2dF> {
  static bool Generate(gfx::Vector2dF *p, Generator* generator) {
    float x;
    float y;
    if (!GenerateParam(&x, generator))
      return false;
    if (!GenerateParam(&y, generator))
      return false;
    *p = gfx::Vector2dF(x, y);
    return true;
  }
};

template <>
struct GenerateTraits<gpu::Mailbox> {
  static bool Generate(gpu::Mailbox* p, Generator* generator) {
    generator->GenerateBytes(p->name, sizeof(p->name));
    return true;
  }
};

template <>
struct GenerateTraits<gpu::MailboxHolder> {
  static bool Generate(gpu::MailboxHolder* p, Generator* generator) {
    gpu::Mailbox mailbox;
    uint32_t texture_target;
    uint32_t sync_point;
    if (!GenerateParam(&mailbox, generator))
      return false;
    if (!GenerateParam(&texture_target, generator))
      return false;
    if (!GenerateParam(&sync_point, generator))
      return false;
    *p = gpu::MailboxHolder(mailbox, texture_target, sync_point);
    return true;
  }
};

template <>
struct GenerateTraits<gpu::ValueState> {
  static bool Generate(gpu::ValueState* p, Generator* generator) {
    gpu::ValueState state;
    if (!GenerateParamArray(&state.float_value[0], 4, generator))
      return false;
    if (!GenerateParamArray(&state.int_value[0], 4, generator))
      return false;
    *p = state;
    return true;
  }
};

template <>
struct GenerateTraits<GURL> {
  static bool Generate(GURL* p, Generator* generator) {
    const char url_chars[] = "Ahtp0:/.?+\\%&#";
    size_t count = RandInRange(100);
    std::string random_url;
    for (size_t i = 0; i < count; ++i)
      random_url += url_chars[RandInRange(sizeof(url_chars) - 1)];
    int selector = RandInRange(10);
    if (selector == 0)
      random_url = std::string("http://") + random_url;
    else if (selector == 1)
      random_url = std::string("file://") + random_url;
    else if (selector == 2)
      random_url = std::string("javascript:") + random_url;
    else if (selector == 2)
      random_url = std::string("data:") + random_url;
    *p = GURL(random_url);
    return true;
  }
};

#if defined(OS_WIN)
template <>
struct GenerateTraits<HWND> {
  static bool Generate(HWND* p, Generator* generator) {
    // TODO(aarya): This should actually generate something.
    return true;
  }
};
#endif

template <>
struct GenerateTraits<IPC::Message> {
  static bool Generate(IPC::Message *p, Generator* generator) {
    if (g_function_vector.empty())
      return false;
    size_t index = RandInRange(g_function_vector.size());
    IPC::Message* ipc_message = (*g_function_vector[index])(generator);
    if (!ipc_message)
      return false;
    p = ipc_message;
    return true;
  }
};

template <>
struct GenerateTraits<IPC::PlatformFileForTransit> {
  static bool Generate(IPC::PlatformFileForTransit* p, Generator* generator) {
    // TODO(inferno): I don't think we can generate real ones due to check on
    // construct.
    *p = IPC::InvalidPlatformFileForTransit();
    return true;
  }
};

template <>
struct GenerateTraits<IPC::ChannelHandle> {
  static bool Generate(IPC::ChannelHandle* p, Generator* generator) {
    // TODO(inferno): Add way to generate real channel handles.
#if defined(OS_WIN)
    HANDLE fake_handle = (HANDLE)(RandU64());
    p->pipe = IPC::ChannelHandle::PipeHandle(fake_handle);
    return true;
#elif defined(OS_POSIX)
    return
      GenerateParam(&p->name, generator) &&
      GenerateParam(&p->socket, generator);
#endif
  }
};

#if defined(OS_WIN)
template <>
struct GenerateTraits<LOGFONT> {
  static bool Generate(LOGFONT* p, Generator* generator) {
    // TODO(aarya): This should actually generate something.
    return true;
  }
};
#endif

template <>
struct GenerateTraits<media::AudioParameters> {
  static bool Generate(media::AudioParameters* p, Generator* generator) {
    int format;
    int channel_layout;
    int sample_rate;
    int bits_per_sample;
    int frames_per_buffer;
    int channels;
    int effects;
    if (!GenerateParam(&format, generator))
      return false;
    if (!GenerateParam(&channel_layout, generator))
      return false;
    if (!GenerateParam(&sample_rate, generator))
      return false;
    if (!GenerateParam(&bits_per_sample, generator))
      return false;
    if (!GenerateParam(&frames_per_buffer, generator))
      return false;
    if (!GenerateParam(&channels, generator))
      return false;
    if (!GenerateParam(&effects, generator))
      return false;
    media::AudioParameters params(
        static_cast<media::AudioParameters::Format>(format),
        static_cast<media::ChannelLayout>(channel_layout), channels,
        sample_rate, bits_per_sample, frames_per_buffer, effects);
    *p = params;
    return true;
  }
};

template <>
struct GenerateTraits<media::VideoCaptureFormat> {
  static bool Generate(media::VideoCaptureFormat* p, Generator* generator) {
    int frame_size_width;
    int frame_size_height;
    int pixel_format;
    if (!GenerateParam(&frame_size_height, generator))
      return false;
    if (!GenerateParam(&frame_size_width, generator))
      return false;
    if (!GenerateParam(&pixel_format, generator))
      return false;
    if (!GenerateParam(&p->frame_rate, generator))
      return false;
    p->frame_size.SetSize(frame_size_width, frame_size_height);
    p->pixel_format = static_cast<media::VideoPixelFormat>(pixel_format);
    return true;
  }
};

template <>
struct GenerateTraits<net::LoadTimingInfo> {
  static bool Generate(net::LoadTimingInfo* p, Generator* generator) {
    return GenerateParam(&p->socket_log_id, generator) &&
           GenerateParam(&p->socket_reused, generator) &&
           GenerateParam(&p->request_start_time, generator) &&
           GenerateParam(&p->request_start, generator) &&
           GenerateParam(&p->proxy_resolve_start, generator) &&
           GenerateParam(&p->proxy_resolve_end, generator) &&
           GenerateParam(&p->connect_timing.dns_start, generator) &&
           GenerateParam(&p->connect_timing.dns_end, generator) &&
           GenerateParam(&p->connect_timing.connect_start, generator) &&
           GenerateParam(&p->connect_timing.connect_end, generator) &&
           GenerateParam(&p->connect_timing.ssl_start, generator) &&
           GenerateParam(&p->connect_timing.ssl_end, generator) &&
           GenerateParam(&p->send_start, generator) &&
           GenerateParam(&p->send_end, generator) &&
           GenerateParam(&p->receive_headers_end, generator);
  }
};

template <>
struct GenerateTraits<net::HostPortPair> {
  static bool Generate(net::HostPortPair* p, Generator* generator) {
    std::string host;
    uint16 port;
    if (!GenerateParam(&host, generator))
      return false;
    if (!GenerateParam(&port, generator))
      return false;
    p->set_host(host);
    p->set_port(port);
    return true;
  }
};

template <>
struct GenerateTraits<net::IPEndPoint> {
  static bool Generate(net::IPEndPoint* p, Generator* generator) {
    net::IPAddressNumber address;
    int port;
    if (!GenerateParam(&address, generator))
      return false;
    if (!GenerateParam(&port, generator))
      return false;
    net::IPEndPoint ip_endpoint(address, port);
    *p = ip_endpoint;
    return true;
  }
};

template <>
struct GenerateTraits<network_hints::LookupRequest> {
  static bool Generate(network_hints::LookupRequest* p, Generator* generator) {
    network_hints::LookupRequest request;
    if (!GenerateParam(&request.hostname_list, generator))
      return false;
    *p = request;
    return true;
  }
};

// PP_ traits.
template <>
struct GenerateTraits<PP_Bool> {
  static bool Generate(PP_Bool *p, Generator* generator) {
    bool tmp;
    if (!GenerateParam(&tmp, generator))
      return false;
    *p = PP_FromBool(tmp);
    return true;
  }
};

template <>
struct GenerateTraits<PP_KeyInformation> {
  static bool Generate(PP_KeyInformation* p, Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = PP_KeyInformation();
    return true;
  }
};

template <>
struct GenerateTraits<PP_NetAddress_Private> {
  static bool Generate(PP_NetAddress_Private *p, Generator* generator) {
    p->size = RandInRange(sizeof(p->data) + 1);
    generator->GenerateBytes(&p->data, p->size);
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::PPB_X509Certificate_Fields> {
  static bool Generate(ppapi::PPB_X509Certificate_Fields* p,
                       Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::proxy::PPBFlash_DrawGlyphs_Params> {
  static bool Generate(ppapi::proxy::PPBFlash_DrawGlyphs_Params* p,
                       Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = ppapi::proxy::PPBFlash_DrawGlyphs_Params();
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::proxy::ResourceMessageCallParams> {
  static bool Generate(
      ppapi::proxy::ResourceMessageCallParams *p, Generator* generator) {
    PP_Resource resource;
    int32_t sequence;
    bool has_callback;
    if (!GenerateParam(&resource, generator))
      return false;
    if (!GenerateParam(&sequence, generator))
      return false;
    if (!GenerateParam(&has_callback, generator))
      return false;
    *p = ppapi::proxy::ResourceMessageCallParams(resource, sequence);
    if (has_callback)
      p->set_has_callback();
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::proxy::ResourceMessageReplyParams> {
  static bool Generate(
      ppapi::proxy::ResourceMessageReplyParams *p, Generator* generator) {
    PP_Resource resource;
    int32_t sequence;
    int32_t result;
    if (!GenerateParam(&resource, generator))
      return false;
    if (!GenerateParam(&sequence, generator))
      return false;
    if (!GenerateParam(&result, generator))
      return false;
    *p = ppapi::proxy::ResourceMessageReplyParams(resource, sequence);
    p->set_result(result);
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::proxy::SerializedHandle> {
  static bool Generate(ppapi::proxy::SerializedHandle* p,
                       Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = ppapi::proxy::SerializedHandle();
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::proxy::SerializedFontDescription> {
  static bool Generate(ppapi::proxy::SerializedFontDescription* p,
                       Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = ppapi::proxy::SerializedFontDescription();
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::proxy::SerializedTrueTypeFontDesc> {
  static bool Generate(ppapi::proxy::SerializedTrueTypeFontDesc* p,
                       Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = ppapi::proxy::SerializedTrueTypeFontDesc();
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::proxy::SerializedVar> {
  static bool Generate(ppapi::proxy::SerializedVar* p, Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = ppapi::proxy::SerializedVar();
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::HostResource> {
  static bool Generate(ppapi::HostResource *p, Generator* generator) {
    PP_Instance instance;
    PP_Resource resource;
    if (!GenerateParam(&instance, generator))
      return false;
    if (!GenerateParam(&resource, generator))
      return false;
    p->SetHostResource(instance, resource);
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::PepperFilePath> {
  static bool Generate(ppapi::PepperFilePath *p, Generator* generator) {
    unsigned domain = RandInRange(ppapi::PepperFilePath::DOMAIN_MAX_VALID+1);
    base::FilePath path;
    if (!GenerateParam(&path, generator))
      return false;
    *p = ppapi::PepperFilePath(
        static_cast<ppapi::PepperFilePath::Domain>(domain), path);
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::PpapiPermissions> {
  static bool Generate(ppapi::PpapiPermissions *p, Generator* generator) {
    uint32_t bits;
    if (!GenerateParam(&bits, generator))
      return false;
    *p = ppapi::PpapiPermissions(bits);
    return true;
  }
};

template <>
struct GenerateTraits<ppapi::SocketOptionData> {
  static bool Generate(ppapi::SocketOptionData *p, Generator* generator) {
    // FIXME: we can do better here.
    int32 temp;
    if (!GenerateParam(&temp, generator))
      return false;
    p->SetInt32(temp);
    return true;
  }
};

template <>
struct GenerateTraits<printing::PdfRenderSettings> {
  static bool Generate(printing::PdfRenderSettings *p, Generator* generator) {
    gfx::Rect area;
    int dpi;
    bool autorotate;
    if (!GenerateParam(&area, generator))
      return false;
    if (!GenerateParam(&dpi, generator))
      return false;
    if (!GenerateParam(&autorotate, generator))
      return false;
    *p = printing::PdfRenderSettings(area, dpi, autorotate);
    return true;
  }
};

template <>
struct GenerateTraits<remoting::ScreenResolution> {
  static bool Generate(remoting::ScreenResolution* p, Generator* generator) {
    webrtc::DesktopSize size;
    webrtc::DesktopVector vector;
    if (!GenerateParam(&size, generator))
      return false;
    if (!GenerateParam(&vector, generator))
      return false;
    *p = remoting::ScreenResolution(size, vector);
    return true;
  }
};

template <>
struct GenerateTraits<SkBitmap> {
  static bool Generate(SkBitmap* p, Generator* generator) {
    // TODO(mbarbella): This should actually generate something.
    *p = SkBitmap();
    return true;
  }
};

template <>
struct GenerateTraits<storage::DataElement> {
  static bool Generate(storage::DataElement* p, Generator* generator) {
    switch (RandInRange(4)) {
      case storage::DataElement::Type::TYPE_BYTES: {
        if (RandEvent(2)) {
          p->SetToEmptyBytes();
        } else {
          // TODO(mbarbella): Occasionally send more data here.
          char data[256];
          int data_len = RandInRange(sizeof(data));
          generator->GenerateBytes(&data[0], data_len);
          p->SetToBytes(&data[0], data_len);
        }
        return true;
      }
      case storage::DataElement::Type::TYPE_FILE: {
        base::FilePath path;
        uint64 offset;
        uint64 length;
        base::Time modification_time;
        if (!GenerateParam(&path, generator))
          return false;
        if (!GenerateParam(&offset, generator))
          return false;
        if (!GenerateParam(&length, generator))
          return false;
        if (!GenerateParam(&modification_time, generator))
          return false;
        p->SetToFilePathRange(path, offset, length, modification_time);
        return true;
      }
      case storage::DataElement::Type::TYPE_BLOB: {
        std::string uuid;
        uint64 offset;
        uint64 length;
        if (!GenerateParam(&uuid, generator))
          return false;
        if (!GenerateParam(&offset, generator))
          return false;
        if (!GenerateParam(&length, generator))
          return false;
        p->SetToBlobRange(uuid, offset, length);
        return true;
      }
      case storage::DataElement::Type::TYPE_FILE_FILESYSTEM: {
        GURL url;
        uint64 offset;
        uint64 length;
        base::Time modification_time;
        if (!GenerateParam(&url, generator))
          return false;
        if (!GenerateParam(&offset, generator))
          return false;
        if (!GenerateParam(&length, generator))
          return false;
        if (!GenerateParam(&modification_time, generator))
          return false;
        p->SetToFileSystemUrlRange(url, offset, length, modification_time);
        return true;
      }
      default: {
        NOTREACHED();
        return false;
      }
    }
  }
};

template <>
struct GenerateTraits<ui::LatencyInfo> {
  static bool Generate(ui::LatencyInfo* p, Generator* generator) {
    // TODO(inferno): Add param traits for |latency_components|.
    p->input_coordinates_size = static_cast<uint32>(
        RandInRange(ui::LatencyInfo::kMaxInputCoordinates + 1));
    if (!GenerateParamArray(
        &p->input_coordinates[0], p->input_coordinates_size, generator))
      return false;
    if (!GenerateParam(&p->trace_id, generator))
      return false;
    if (!GenerateParam(&p->terminated, generator))
      return false;
    return true;
  }
};

template <>
struct GenerateTraits<ui::LatencyInfo::InputCoordinate> {
  static bool Generate(
      ui::LatencyInfo::InputCoordinate* p, Generator* generator) {
    float x;
    float y;
    if (!GenerateParam(&x, generator))
      return false;
    if (!GenerateParam(&y, generator))
      return false;
    *p = ui::LatencyInfo::InputCoordinate(x, y);
    return true;
  }
};

template <>
struct GenerateTraits<url::Origin> {
  static bool Generate(url::Origin* p, Generator* generator) {
    std::string origin;
    if (!GenerateParam(&origin, generator))
        return false;
    *p = url::Origin(origin);
    return true;
  }
};

template <>
struct GenerateTraits<URLPattern> {
  static bool Generate(URLPattern* p, Generator* generator) {
    int valid_schemes;
    std::string host;
    std::string port;
    std::string path;
    if (!GenerateParam(&valid_schemes, generator))
      return false;
    if (!GenerateParam(&host, generator))
      return false;
    if (!GenerateParam(&port, generator))
      return false;
    if (!GenerateParam(&path, generator))
      return false;
    *p = URLPattern(valid_schemes);
    p->SetHost(host);
    p->SetPort(port);
    p->SetPath(path);
    return true;
  }
};

template <>
struct GenerateTraits<webrtc::DesktopSize> {
  static bool Generate(webrtc::DesktopSize* p, Generator* generator) {
    int32_t width;
    int32_t height;
    if (!GenerateParam(&width, generator))
      return false;
    if (!GenerateParam(&height, generator))
      return false;
    *p = webrtc::DesktopSize(width, height);
    return true;
  }
};

template <>
struct GenerateTraits<webrtc::DesktopVector> {
  static bool Generate(webrtc::DesktopVector* p, Generator* generator) {
    int32_t x;
    int32_t y;
    if (!GenerateParam(&x, generator))
      return false;
    if (!GenerateParam(&y, generator))
      return false;
    p->set(x, y);
    return true;
  }
};

template <>
struct GenerateTraits<webrtc::DesktopRect> {
  static bool Generate(webrtc::DesktopRect* p, Generator* generator) {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    if (!GenerateParam(&left, generator))
      return false;
    if (!GenerateParam(&top, generator))
      return false;
    if (!GenerateParam(&right, generator))
      return false;
    if (!GenerateParam(&bottom, generator))
      return false;
    *p = webrtc::DesktopRect::MakeLTRB(left, top, right, bottom);
    return true;
  }
};

template <>
struct GenerateTraits<webrtc::MouseCursor> {
  static bool Generate(webrtc::MouseCursor* p, Generator* generator) {
    webrtc::DesktopVector hotspot;
    if (!GenerateParam(&hotspot, generator))
      return false;
    // Using a small size here to avoid OOM or overflow on image allocation.
    webrtc::DesktopSize size(RandInRange(100), RandInRange(100));
    p->set_image(new webrtc::BasicDesktopFrame(size));
    p->set_hotspot(hotspot);
    return true;
  }
};

// Redefine macros to generate generating from traits declarations.
// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_BEGIN_WITH_PARENT
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent) \
  IPC_STRUCT_BEGIN(struct_name)
#define IPC_STRUCT_BEGIN(struct_name) IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name, ...) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate generate trait classes.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  template <> \
  struct GenerateTraits<struct_name> { \
    static bool Generate(struct_name *p, Generator* generator) {

#define IPC_STRUCT_TRAITS_MEMBER(name) \
      if (!GenerateParam(&p->name, generator)) \
        return false;

#define IPC_STRUCT_TRAITS_PARENT(type) \
      if (!GenerateParam(static_cast<type*>(p), generator)) \
        return false;

#define IPC_STRUCT_TRAITS_END() \
      return true; \
    } \
  };

// If |condition| isn't met, the messsge will fail to serialize. Try
// increasingly smaller ranges until we find one that happens to meet
// the condition, or fail trying.
#undef IPC_ENUM_TRAITS_VALIDATE
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, condition)             \
  template <>                                                      \
  struct GenerateTraits<enum_name> {                               \
    static bool Generate(enum_name* p, Generator* generator) {     \
      for (int shift = 30; shift; --shift) {                       \
        for (int tries = 0; tries < 2; ++tries) {                  \
          int value = RandInRange(1 << shift);                     \
          if (condition) {                                         \
            *reinterpret_cast<int*>(p) = value;                    \
            return true;                                           \
          }                                                        \
        }                                                          \
      }                                                            \
      std::cerr << "failed to satisfy " << #condition << "\n";     \
      return false;                                                \
    }                                                              \
  };

// Bring them into existence.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

// Redefine macros to generate generating funtions
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist)       \
  IPC_##kind##_##type##_GENERATE(name, in, out, ilist, olist)

#define IPC_EMPTY_CONTROL_GENERATE(name, in, out, ilist, olist)         \
  IPC::Message* generator_for_##name(Generator* generator) {            \
    return new name();                                                  \
  }

#define IPC_EMPTY_ROUTED_GENERATE(name, in, out, ilist, olist)          \
  IPC::Message* generator_for_##name(Generator* generator) {            \
    return new name(RandInRange(MAX_FAKE_ROUTING_ID));                  \
  }

#define IPC_ASYNC_CONTROL_GENERATE(name, in, out, ilist, olist)         \
  IPC::Message* generator_for_##name(Generator* generator) {            \
    IPC_TUPLE_IN_##in ilist p;                                          \
    if (GenerateParam(&p, generator)) {                                 \
      return new name(IPC_MEMBERS_IN_##in(p));                          \
    }                                                                   \
    std::cerr << "Don't know how to generate " << #name << "\n";        \
    return 0;                                                           \
  }

#define IPC_ASYNC_ROUTED_GENERATE(name, in, out, ilist, olist)          \
  IPC::Message* generator_for_##name(Generator* generator) {            \
    IPC_TUPLE_IN_##in ilist p;                                          \
    if (GenerateParam(&p, generator)) {                                 \
      return new name(RandInRange(MAX_FAKE_ROUTING_ID)                  \
                      IPC_COMMA_##in                                    \
                      IPC_MEMBERS_IN_##in(p));                          \
    }                                                                   \
    std::cerr << "Don't know how to generate " << #name << "\n";        \
    return 0;                                                           \
  }

#define IPC_SYNC_CONTROL_GENERATE(name, in, out, ilist, olist)          \
  IPC::Message* generator_for_##name(Generator* generator) {            \
    IPC_TUPLE_IN_##in ilist p;                                          \
    if (GenerateParam(&p, generator)) {                                 \
      return new name(IPC_MEMBERS_IN_##in(p)                            \
                      IPC_COMMA_AND_##out(IPC_COMMA_##in)               \
                      IPC_MEMBERS_OUT_##out());                         \
    }                                                                   \
    std::cerr << "Don't know how to generate " << #name << "\n";        \
    return 0;                                                           \
  }

#define IPC_SYNC_ROUTED_GENERATE(name, in, out, ilist, olist)           \
  IPC::Message* generator_for_##name(Generator* generator) {            \
    IPC_TUPLE_IN_##in ilist p;                                          \
    if (GenerateParam(&p, generator)) {                                 \
      return new name(RandInRange(MAX_FAKE_ROUTING_ID)                  \
                      IPC_COMMA_OR_##out(IPC_COMMA_##in)                \
                      IPC_MEMBERS_IN_##in(p)                            \
                      IPC_COMMA_AND_##out(IPC_COMMA_##in)               \
                      IPC_MEMBERS_OUT_##out());                         \
    }                                                                   \
    std::cerr << "Don't know how to generate " << #name << "\n";        \
    return 0;                                                           \
  }

#define MAX_FAKE_ROUTING_ID 15

#define IPC_MEMBERS_IN_0(p)
#define IPC_MEMBERS_IN_1(p) get<0>(p)
#define IPC_MEMBERS_IN_2(p) get<0>(p), get<1>(p)
#define IPC_MEMBERS_IN_3(p) get<0>(p), get<1>(p), get<2>(p)
#define IPC_MEMBERS_IN_4(p) get<0>(p), get<1>(p), get<2>(p), get<3>(p)
#define IPC_MEMBERS_IN_5(p) get<0>(p), get<1>(p), get<2>(p), get<3>(p), \
                            get<4>(p)

#define IPC_MEMBERS_OUT_0()
#define IPC_MEMBERS_OUT_1() NULL
#define IPC_MEMBERS_OUT_2() NULL, NULL
#define IPC_MEMBERS_OUT_3() NULL, NULL, NULL
#define IPC_MEMBERS_OUT_4() NULL, NULL, NULL, NULL
#define IPC_MEMBERS_OUT_5() NULL, NULL, NULL, NULL, NULL

#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

void PopulateGeneratorFunctionVector(
    GeneratorFunctionVector *function_vector) {
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  function_vector->push_back(generator_for_##name);
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

static const char kCountSwitch[] = "count";
static const char kHelpSwitch[] = "help";

int GenerateMain(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = cmd->GetArgs();

  if (args.size() != 1 || cmd->HasSwitch(kHelpSwitch)) {
    std::cerr << "Usage: ipc_fuzzer_generate [--help] [--count=n] outfile\n";
    return EXIT_FAILURE;
  }
  base::FilePath::StringType output_file_name = args[0];

  int message_count = 1000;
  if (cmd->HasSwitch(kCountSwitch))
    message_count = atoi(cmd->GetSwitchValueASCII(kCountSwitch).c_str());

  InitRand();

  PopulateGeneratorFunctionVector(&g_function_vector);
  std::cerr << "Counted " << g_function_vector.size()
            << " distinct messages present in chrome.\n";

  Generator* generator = new GeneratorImpl();
  MessageVector message_vector;

  int bad_count = 0;
  if (message_count < 0) {
    // Enumerate them all.
    for (size_t i = 0; i < g_function_vector.size(); ++i) {
      if (IPC::Message* new_message = (*g_function_vector[i])(generator))
        message_vector.push_back(new_message);
      else
        bad_count += 1;
    }
  } else {
    // Generate a random batch.
    for (int i = 0; i < message_count; ++i) {
      size_t index = RandInRange(g_function_vector.size());
      if (IPC::Message* new_message = (*g_function_vector[index])(generator))
        message_vector.push_back(new_message);
      else
        bad_count += 1;
    }
  }

  std::cerr << "Failed to generate " << bad_count << " messages.\n";

  if (!MessageFile::Write(base::FilePath(output_file_name), message_vector))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

}  // namespace ipc_fuzzer

int main(int argc, char** argv) {
  return ipc_fuzzer::GenerateMain(argc, argv);
}
