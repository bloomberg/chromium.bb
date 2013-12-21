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
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

// First include of message files to provide basic type.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

namespace IPC {
class Message;
}  // namespace IPC

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

namespace {

template <typename T>
void GenerateIntegralType(T* value) {
  *value = static_cast<T>(base::RandUint64());
}

template <typename T>
void GenerateFloatingType(T* value) {
  *value = 1000000.0 * (base::RandDouble() - 0.5);
}

template <typename T>
void GenerateStringType(T* value) {
  T temp_string;
  size_t length = base::RandInt(0, 299);
  for (size_t i = 0; i < length; ++i)
    temp_string += base::RandInt(0, 255);
  *value = temp_string;
}

}  // namespace

class GeneratorImpl : public Generator {
 public:
  GeneratorImpl() {}
  virtual ~GeneratorImpl() {}

  virtual void GenerateBool(bool* value) OVERRIDE {
    *value = base::RandInt(0, 1);
  }

  virtual void GenerateInt(int* value) OVERRIDE {
    GenerateIntegralType<int>(value);
  }

  virtual void GenerateLong(long* value) OVERRIDE {
    GenerateIntegralType<long>(value);
  }

  virtual void GenerateSize(size_t* value) OVERRIDE {
    GenerateIntegralType<size_t>(value);
  }

  virtual void GenerateUChar(unsigned char* value) OVERRIDE {
    GenerateIntegralType<unsigned char>(value);
  }

  virtual void GenerateUInt16(uint16* value) OVERRIDE {
    GenerateIntegralType<uint16>(value);
  }

  virtual void GenerateUInt32(uint32* value) OVERRIDE {
    GenerateIntegralType<uint32>(value);
  }

  virtual void GenerateInt64(int64* value) OVERRIDE {
    GenerateIntegralType<int64>(value);
  }

  virtual void GenerateUInt64(uint64* value) OVERRIDE {
    GenerateIntegralType<uint64>(value);
  }

  virtual void GenerateFloat(float* value) OVERRIDE {
    GenerateFloatingType<float>(value);
  }

  virtual void GenerateDouble(double* value) OVERRIDE {
    GenerateFloatingType<double>(value);
  }

  virtual void GenerateString(std::string* value) OVERRIDE {
    GenerateStringType<std::string>(value);
  }

  virtual void GenerateString16(base::string16* value) OVERRIDE {
    GenerateStringType<base::string16>(value);
  }

  virtual void GenerateData(char* data, int length) OVERRIDE {
    for (int i = 0; i < length; ++i) {
      GenerateIntegralType<char>(&data[i]);
    }
  }

  virtual void GenerateBytes(void* data, int data_len) OVERRIDE {
    GenerateData(static_cast<char*>(data), data_len);
  }
};

// Partially-specialized class that knows how to generate a given type.
template <class P>
struct GenerateTraits {
  static bool Generate(P* p, Generator *generator) {
    // This is the catch-all for types we don't have enough information
    // to generate. Sadly, we must reject this message.
    std::cerr << "Cant handle " << __PRETTY_FUNCTION__ << "\n";
    return false;
  }
};

// Template function to invoke partially-specialized class method.
template <class P>
static bool GenerateParam(P* p, Generator* generator) {
  return GenerateTraits<P>::Generate(p, generator);
}

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
struct GenerateTraits<Tuple0> {
  static bool Generate(Tuple0* p, Generator* generator) {
    return true;
  }
};

template <class A>
struct GenerateTraits<Tuple1<A> > {
  static bool Generate(Tuple1<A>* p, Generator* generator) {
    return GenerateParam(&p->a, generator);
  }
};

template <class A, class B>
struct GenerateTraits<Tuple2<A, B> > {
  static bool Generate(Tuple2<A, B>* p, Generator* generator) {
    return
        GenerateParam(&p->a, generator) &&
        GenerateParam(&p->b, generator);
  }
};

template <class A, class B, class C>
struct GenerateTraits<Tuple3<A, B, C> > {
  static bool Generate(Tuple3<A, B, C>* p, Generator* generator) {
    return
        GenerateParam(&p->a, generator) &&
        GenerateParam(&p->b, generator) &&
        GenerateParam(&p->c, generator);
  }
};

template <class A, class B, class C, class D>
struct GenerateTraits<Tuple4<A, B, C, D> > {
  static bool Generate(Tuple4<A, B, C, D>* p, Generator* generator) {
    return
        GenerateParam(&p->a, generator) &&
        GenerateParam(&p->b, generator) &&
        GenerateParam(&p->c, generator) &&
        GenerateParam(&p->d, generator);
  }
};

template <class A, class B, class C, class D, class E>
struct GenerateTraits<Tuple5<A, B, C, D, E> > {
  static bool Generate(Tuple5<A, B, C, D, E>* p, Generator* generator) {
    return
        GenerateParam(&p->a, generator) &&
        GenerateParam(&p->b, generator) &&
        GenerateParam(&p->c, generator) &&
        GenerateParam(&p->d, generator) &&
        GenerateParam(&p->e, generator);
  }
};

// Specializations to generate containers.
template <class A>
struct GenerateTraits<std::vector<A> > {
  static bool Generate(std::vector<A>* p, Generator* generator) {
    static int depth = 0;
    size_t count = ++depth > 3 ? 0 : base::RandInt(0, 19);
    p->resize(count);
    for (size_t i = 0; i < count; ++i) {
      if (!GenerateParam(&p->at(i), generator)) {
        --depth;
        return false;
      }
    }
    --depth;
    return true;
  }
};

template <class A>
struct GenerateTraits<std::set<A> > {
  static bool Generate(std::set<A>* p, Generator* generator) {
    static int depth = 0;
    size_t count = ++depth > 3 ? 0 : base::RandInt(0, 19);
    A a;
    for (size_t i = 0; i < count; ++i) {
      if (!GenerateParam(&a, generator)) {
        --depth;
        return false;
      }
      p->insert(a);
    }
    --depth;
    return true;
  }
};


template <class A, class B>
struct GenerateTraits<std::map<A, B> > {
  static bool Generate(std::map<A, B>* p, Generator* generator) {
    static int depth = 0;
    size_t count = ++depth > 3 ? 0 : base::RandInt(0, 19);
    std::pair<A, B> place_holder;
    for (size_t i = 0; i < count; ++i) {
      if (!GenerateParam(&place_holder, generator)) {
        --depth;
        return false;
      }
      p->insert(place_holder);
    }
    --depth;
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

// Specializations to generate hand-coded tyoes
template <>
struct GenerateTraits<base::NullableString16> {
  static bool Generate(base::NullableString16* p, Generator* generator) {
    *p = base::NullableString16();
    return true;
  }
};

template <>
struct GenerateTraits<base::FileDescriptor> {
  static bool Generate(base::FileDescriptor* p, Generator* generator) {
    // I don't think we can generate real ones due to check on construct.
    p->fd = -1;
    return true;
  }
};

template <>
struct GenerateTraits<base::FilePath> {
  static bool Generate(base::FilePath* p, Generator* generator) {
    const char path_chars[] = "ACz0/.~:";
    size_t count = base::RandInt(0, 59);
    std::string random_path;
    for (size_t i = 0; i < count; ++i)
      random_path += path_chars[base::RandInt(0, sizeof(path_chars) - 2)];
    *p = base::FilePath(random_path);
    return true;
  }
};

template <>
struct GenerateTraits<base::Time> {
  static bool Generate(base::Time* p, Generator* generator) {
    *p = base::Time::FromInternalValue(base::RandUint64());
    return true;
  }
};

template <>
struct GenerateTraits<base::TimeDelta> {
  static bool Generate(base::TimeDelta* p, Generator* generator) {
    *p = base::TimeDelta::FromInternalValue(base::RandUint64());
    return true;
  }
};

template <>
struct GenerateTraits<base::TimeTicks> {
  static bool Generate(base::TimeTicks* p, Generator* generator) {
    *p = base::TimeTicks::FromInternalValue(base::RandUint64());
    return true;
  }
};

template <>
struct GenerateTraits<GURL> {
  static bool Generate(GURL *p, Generator* generator) {
    const char url_chars[] = "Ahtp0:/.?+\%&#";
    size_t count = base::RandInt(0, 99);
    std::string random_url;
    for (size_t i = 0; i < count; ++i)
      random_url += url_chars[base::RandInt(0, sizeof(url_chars) - 2)];
    int selector = base::RandInt(0, 9);
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
struct GenerateTraits<gfx::Size> {
  static bool Generate(gfx::Size *p, Generator* generator) {
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
  static bool Generate(gfx::SizeF *p, Generator* generator) {
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
          int value = base::RandInt(0, 1 << shift);                \
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
    return new name(base::RandInt(0, MAX_FAKE_ROUTING_ID - 1));         \
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
      return new name(base::RandInt(0, MAX_FAKE_ROUTING_ID - 1)         \
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
      return new name(base::RandInt(0, MAX_FAKE_ROUTING_ID - 1)         \
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
#define IPC_MEMBERS_IN_1(p) p.a
#define IPC_MEMBERS_IN_2(p) p.a, p.b
#define IPC_MEMBERS_IN_3(p) p.a, p.b, p.c
#define IPC_MEMBERS_IN_4(p) p.a, p.b, p.c, p.d
#define IPC_MEMBERS_IN_5(p) p.a, p.b, p.c, p.d, p.e

#define IPC_MEMBERS_OUT_0()
#define IPC_MEMBERS_OUT_1() NULL
#define IPC_MEMBERS_OUT_2() NULL, NULL
#define IPC_MEMBERS_OUT_3() NULL, NULL, NULL
#define IPC_MEMBERS_OUT_4() NULL, NULL, NULL, NULL
#define IPC_MEMBERS_OUT_5() NULL, NULL, NULL, NULL, NULL

#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

typedef IPC::Message* (*GeneratorFunction)(Generator*);
typedef std::vector<GeneratorFunction> GeneratorFunctionVector;

void PopulateGeneratorFunctionVector(
    GeneratorFunctionVector *function_vector) {
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  function_vector->push_back(generator_for_##name);
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

static const char kCountSwitch[] = "count";
static const char kHelpSwitch[] = "help";

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  CommandLine::StringVector args = cmd->GetArgs();

  if (args.size() != 1 || cmd->HasSwitch(kHelpSwitch)) {
    std::cerr << "Usage: ipc_fuzzer_generate [--help] [--count=n] outfile\n";
    return EXIT_FAILURE;
  }
  std::string output_file_name = args[0];

  int message_count = 10000;
  if (cmd->HasSwitch(kCountSwitch))
    message_count = atoi(cmd->GetSwitchValueASCII(kCountSwitch).c_str());

  GeneratorFunctionVector function_vector;
  PopulateGeneratorFunctionVector(&function_vector);
  std::cerr << "Counted " << function_vector.size()
            << " distinct messages present in chrome.\n";

  Generator* generator = new GeneratorImpl();
  ipc_fuzzer::MessageVector message_vector;

  int bad_count = 0;
  if (message_count < 0) {
    // Enumerate them all.
    for (size_t i = 0; i < function_vector.size(); ++i) {
      if (IPC::Message* new_message = (*function_vector[i])(generator))
        message_vector.push_back(new_message);
      else
        bad_count += 1;
    }
  } else {
    // Generate a random batch.
    for (int i = 0; i < message_count; ++i) {
      size_t index = base::RandInt(0, function_vector.size() - 1);
      if (IPC::Message* new_message = (*function_vector[index])(generator))
        message_vector.push_back(new_message);
      else
        bad_count += 1;
    }
  }

  std::cerr << "Failed to generate " << bad_count << " messages.\n";

  if (!ipc_fuzzer::MessageFile::Write(
          base::FilePath(output_file_name), message_vector)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
