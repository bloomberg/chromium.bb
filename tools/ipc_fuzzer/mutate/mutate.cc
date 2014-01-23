// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <algorithm>
#include <iostream>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "tools/ipc_fuzzer/message_lib/message_cracker.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"
#include "tools/ipc_fuzzer/mutate/rand_util.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace IPC {
class Message;
}  // namespace IPC

namespace ipc_fuzzer {

// Interface implemented by those who fuzz basic types.  The types all
// correspond to the types which a pickle from base/pickle.h can pickle,
// plus the floating point types.
class Fuzzer {
 public:
  // Tweak individual values within a message.
  virtual void FuzzBool(bool* value) = 0;
  virtual void FuzzInt(int* value) = 0;
  virtual void FuzzLong(long* value) = 0;
  virtual void FuzzSize(size_t* value) = 0;
  virtual void FuzzUChar(unsigned char *value) = 0;
  virtual void FuzzUInt16(uint16* value) = 0;
  virtual void FuzzUInt32(uint32* value) = 0;
  virtual void FuzzInt64(int64* value) = 0;
  virtual void FuzzUInt64(uint64* value) = 0;
  virtual void FuzzFloat(float *value) = 0;
  virtual void FuzzDouble(double *value) = 0;
  virtual void FuzzString(std::string* value) = 0;
  virtual void FuzzString16(base::string16* value) = 0;
  virtual void FuzzData(char* data, int length) = 0;
  virtual void FuzzBytes(void* data, int data_len) = 0;
};

template <typename T>
void FuzzIntegralType(T* value, unsigned int frequency) {
  if (RandEvent(frequency)) {
    switch (RandInRange(4)) {
      case 0: (*value) = 0; break;
      case 1: (*value)--; break;
      case 2: (*value)++; break;
      case 3: (*value) = RandU64(); break;
    }
  }
}

template <typename T>
void FuzzStringType(T* value, unsigned int frequency,
                    const T& literal1, const T& literal2) {
  if (RandEvent(frequency)) {
    switch (RandInRange(5)) {
      case 4: (*value) = (*value) + (*value);   // FALLTHROUGH
      case 3: (*value) = (*value) + (*value);   // FALLTHROUGH
      case 2: (*value) = (*value) + (*value); break;
      case 1: (*value) += literal1; break;
      case 0: (*value) = literal2; break;
    }
  }
}

// One such fuzzer implementation.
class DefaultFuzzer : public Fuzzer {
 public:
  DefaultFuzzer(int frequency) : frequency_(frequency) {
  }

  virtual ~DefaultFuzzer() {}

  virtual void FuzzBool(bool* value) OVERRIDE {
    if (RandEvent(frequency_))
      (*value) = !(*value);
  }

  virtual void FuzzInt(int* value) OVERRIDE {
    FuzzIntegralType<int>(value, frequency_);
  }

  virtual void FuzzLong(long* value) OVERRIDE {
    FuzzIntegralType<long>(value, frequency_);
  }

  virtual void FuzzSize(size_t* value) OVERRIDE {
    FuzzIntegralType<size_t>(value, frequency_);
  }

  virtual void FuzzUChar(unsigned char* value) OVERRIDE {
    FuzzIntegralType<unsigned char>(value, frequency_);
  }

  virtual void FuzzUInt16(uint16* value) OVERRIDE {
    FuzzIntegralType<uint16>(value, frequency_);
  }

  virtual void FuzzUInt32(uint32* value) OVERRIDE {
    FuzzIntegralType<uint32>(value, frequency_);
  }

  virtual void FuzzInt64(int64* value) OVERRIDE {
    FuzzIntegralType<int64>(value, frequency_);
  }

  virtual void FuzzUInt64(uint64* value) OVERRIDE {
    FuzzIntegralType<uint64>(value, frequency_);
  }

  virtual void FuzzFloat(float* value) OVERRIDE {
    if (RandEvent(frequency_))
      *value = RandDouble();
  }

  virtual void FuzzDouble(double* value) OVERRIDE {
    if (RandEvent(frequency_))
      *value = RandDouble();
  }

  virtual void FuzzString(std::string* value) OVERRIDE {
    FuzzStringType<std::string>(value, frequency_, "BORKED", std::string());
  }

  virtual void FuzzString16(base::string16* value) OVERRIDE {
    FuzzStringType<base::string16>(value, frequency_,
                                   base::WideToUTF16(L"BORKED"),
                                   base::WideToUTF16(L""));
  }

  virtual void FuzzData(char* data, int length) OVERRIDE {
    if (RandEvent(frequency_)) {
      for (int i = 0; i < length; ++i) {
        FuzzIntegralType<char>(&data[i], frequency_);
      }
    }
  }

  virtual void FuzzBytes(void* data, int data_len) OVERRIDE {
    FuzzData(static_cast<char*>(data), data_len);
  }

 private:
  unsigned int frequency_;
};


// No-op fuzzer.  Rewrites each message unchanged to check if the message
// re-assembly is legit.
class NoOpFuzzer : public Fuzzer {
 public:
  NoOpFuzzer() {}
  virtual ~NoOpFuzzer() {}

  virtual void FuzzBool(bool* value) OVERRIDE {}
  virtual void FuzzInt(int* value) OVERRIDE {}
  virtual void FuzzLong(long* value) OVERRIDE {}
  virtual void FuzzSize(size_t* value) OVERRIDE {}
  virtual void FuzzUChar(unsigned char* value) OVERRIDE {}
  virtual void FuzzUInt16(uint16* value) OVERRIDE {}
  virtual void FuzzUInt32(uint32* value) OVERRIDE {}
  virtual void FuzzInt64(int64* value) OVERRIDE {}
  virtual void FuzzUInt64(uint64* value) OVERRIDE {}
  virtual void FuzzFloat(float* value) OVERRIDE {}
  virtual void FuzzDouble(double* value) OVERRIDE {}
  virtual void FuzzString(std::string* value) OVERRIDE {}
  virtual void FuzzString16(base::string16* value) OVERRIDE {}
  virtual void FuzzData(char* data, int length) OVERRIDE {}
  virtual void FuzzBytes(void* data, int data_len) OVERRIDE {}
};

class FuzzerFactory {
 public:
  static Fuzzer *Create(const std::string& name, int frequency) {
    if (name == "no-op")
      return new NoOpFuzzer();

    if (name == "default")
      return new DefaultFuzzer(frequency);

    std::cerr << "No such fuzzer: " << name << "\n";
    return 0;
  }
};

// Partially-specialized class that knows how to fuzz a given type.
template <class P>
struct FuzzTraits {
  static void Fuzz(P* p, Fuzzer *fuzzer) {
    // This is the catch-all for types we don't have enough information
    // to fuzz. It simply does nothing to the type. We might want to
    // change it to randomly flip a bit in the range (p, p+sizeof(P)).
  }
};

// Template function to invoke partially-specialized class method.
template <class P>
static void FuzzParam(P* p, Fuzzer* fuzzer) {
  FuzzTraits<P>::Fuzz(p, fuzzer);
}

// Specializations to fuzz primitive types.
template <>
struct FuzzTraits<bool> {
  static void Fuzz(bool* p, Fuzzer* fuzzer) {
    fuzzer->FuzzBool(p);
  }
};

template <>
struct FuzzTraits<int> {
  static void Fuzz(int* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt(p);
  }
};

template <>
struct FuzzTraits<unsigned int> {
  static void Fuzz(unsigned int* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt(reinterpret_cast<int*>(p));
  }
};

template <>
struct FuzzTraits<long> {
  static void Fuzz(long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzLong(p);
  }
};

template <>
struct FuzzTraits<unsigned long> {
  static void Fuzz(unsigned long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzLong(reinterpret_cast<long*>(p));
  }
};

template <>
struct FuzzTraits<long long> {
  static void Fuzz(long long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64*>(p));
  }
};

template <>
struct FuzzTraits<unsigned long long> {
  static void Fuzz(unsigned long long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64*>(p));
  }
};

template <>
struct FuzzTraits<short> {
  static void Fuzz(short* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16*>(p));
  }
};

template <>
struct FuzzTraits<unsigned short> {
  static void Fuzz(unsigned short* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16*>(p));
  }
};

template <>
struct FuzzTraits<char> {
  static void Fuzz(char* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(reinterpret_cast<unsigned char*>(p));
  }
};

template <>
struct FuzzTraits<unsigned char> {
  static void Fuzz(unsigned char* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(p);
  }
};

template <>
struct FuzzTraits<float> {
  static void Fuzz(float* p, Fuzzer* fuzzer) {
    fuzzer->FuzzFloat(p);
  }
};

template <>
struct FuzzTraits<double> {
  static void Fuzz(double* p, Fuzzer* fuzzer) {
    fuzzer->FuzzDouble(p);
  }
};

template <>
struct FuzzTraits<std::string> {
  static void Fuzz(std::string* p, Fuzzer* fuzzer) {
    fuzzer->FuzzString(p);
  }
};

template <>
struct FuzzTraits<base::string16> {
  static void Fuzz(base::string16* p, Fuzzer* fuzzer) {
    fuzzer->FuzzString16(p);
  }
};

// Specializations to fuzz tuples.
template <class A>
struct FuzzTraits<Tuple1<A> > {
  static void Fuzz(Tuple1<A>* p, Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
  }
};

template <class A, class B>
struct FuzzTraits<Tuple2<A, B> > {
  static void Fuzz(Tuple2<A, B>* p, Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
  }
};

template <class A, class B, class C>
struct FuzzTraits<Tuple3<A, B, C> > {
  static void Fuzz(Tuple3<A, B, C>* p, Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
    FuzzParam(&p->c, fuzzer);
  }
};

template <class A, class B, class C, class D>
struct FuzzTraits<Tuple4<A, B, C, D> > {
  static void Fuzz(Tuple4<A, B, C, D>* p, Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
    FuzzParam(&p->c, fuzzer);
    FuzzParam(&p->d, fuzzer);
  }
};

template <class A, class B, class C, class D, class E>
struct FuzzTraits<Tuple5<A, B, C, D, E> > {
  static void Fuzz(Tuple5<A, B, C, D, E>* p, Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
    FuzzParam(&p->c, fuzzer);
    FuzzParam(&p->d, fuzzer);
    FuzzParam(&p->e, fuzzer);
  }
};

// Specializations to fuzz containers.
template <class A>
struct FuzzTraits<std::vector<A> > {
  static void Fuzz(std::vector<A>* p, Fuzzer* fuzzer) {
    for (size_t i = 0; i < p->size(); ++i) {
      FuzzParam(&p->at(i), fuzzer);
    }
  }
};

template <class A, class B>
struct FuzzTraits<std::map<A, B> > {
  static void Fuzz(std::map<A, B>* p, Fuzzer* fuzzer) {
    typename std::map<A, B>::iterator it;
    for (it = p->begin(); it != p->end(); ++it) {
      FuzzParam(&it->second, fuzzer);
    }
  }
};

template <class A, class B>
struct FuzzTraits<std::pair<A, B> > {
  static void Fuzz(std::pair<A, B>* p, Fuzzer* fuzzer) {
    FuzzParam(&p->second, fuzzer);
  }
};

// Specializations to fuzz hand-coded tyoes
template <>
struct FuzzTraits<base::FileDescriptor> {
  static void Fuzz(base::FileDescriptor* p, Fuzzer* fuzzer) {
    FuzzParam(&p->fd, fuzzer);
  }
};

template <>
struct FuzzTraits<GURL> {
  static void Fuzz(GURL *p, Fuzzer* fuzzer) {
    FuzzParam(&p->possibly_invalid_spec(), fuzzer);
  }
};

template <>
struct FuzzTraits<gfx::Point> {
  static void Fuzz(gfx::Point *p, Fuzzer* fuzzer) {
    int x = p->x();
    int y = p->y();
    FuzzParam(&x, fuzzer);
    FuzzParam(&y, fuzzer);
    p->SetPoint(x, y);
  }
};

template <>
struct FuzzTraits<gfx::Size> {
  static void Fuzz(gfx::Size *p, Fuzzer* fuzzer) {
    int w = p->width();
    int h = p->height();
    FuzzParam(&w, fuzzer);
    FuzzParam(&h, fuzzer);
    p->SetSize(w, h);
  }
};

template <>
struct FuzzTraits<gfx::Rect> {
  static void Fuzz(gfx::Rect *p, Fuzzer* fuzzer) {
    gfx::Point origin = p->origin();
    gfx::Size  size = p->size();
    FuzzParam(&origin, fuzzer);
    FuzzParam(&size, fuzzer);
    p->set_origin(origin);
    p->set_size(size);
  }
};

// Redefine macros to generate fuzzing from traits declarations.
// Null out all the macros that need nulling.
#include "ipc/ipc_message_null_macros.h"

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

// Set up so next include will generate fuzz trait classes.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  template <> \
  struct FuzzTraits<struct_name> { \
    static void Fuzz(struct_name *p, Fuzzer* fuzzer) {

#define IPC_STRUCT_TRAITS_MEMBER(name) \
      FuzzParam(&p->name, fuzzer);

#define IPC_STRUCT_TRAITS_PARENT(type) \
      FuzzParam(static_cast<type*>(p), fuzzer);

#define IPC_STRUCT_TRAITS_END() \
    } \
  };

// TODO(tsepez): Make sure to end up with an enum that meets |condition|.
#undef IPC_ENUM_TRAITS_VALIDATE
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, conditon) \
  template <> \
  struct FuzzTraits<enum_name> { \
    static void Fuzz(enum_name* p, Fuzzer* fuzzer) { \
      FuzzParam(reinterpret_cast<int*>(p), fuzzer); \
    } \
  };

// Bring them into existence.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"

// Redefine macros to generate fuzzing funtions
#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist)           \
  IPC_##kind##_##type##_FUZZ(name, in, out, ilist, olist)

#define IPC_EMPTY_CONTROL_FUZZ(name, in, out, ilist, olist)                 \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, Fuzzer* fuzzer) {      \
    return NULL;                                                            \
  }

#define IPC_EMPTY_ROUTED_FUZZ(name, in, out, ilist, olist)                  \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, Fuzzer* fuzzer) {      \
    return NULL;                                                            \
  }

#define IPC_ASYNC_CONTROL_FUZZ(name, in, out, ilist, olist)                 \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, Fuzzer* fuzzer) {      \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::Read(real_msg, &p);                                               \
    FuzzParam(&p, fuzzer);                                                  \
    return new name(IPC_MEMBERS_IN_##in(p));                                \
  }

#define IPC_ASYNC_ROUTED_FUZZ(name, in, out, ilist, olist)                  \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, Fuzzer* fuzzer) {      \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::Read(real_msg, &p);                                               \
    FuzzParam(&p, fuzzer);                                                  \
    return new name(msg->routing_id()                                       \
                    IPC_COMMA_##in                                          \
                    IPC_MEMBERS_IN_##in(p));                                \
  }

#define IPC_SYNC_CONTROL_FUZZ(name, in, out, ilist, olist)                  \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, Fuzzer* fuzzer) {      \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::ReadSendParam(real_msg, &p);                                      \
    FuzzParam(&p, fuzzer);                                                  \
    name* new_msg = new name(IPC_MEMBERS_IN_##in(p)                         \
                             IPC_COMMA_AND_##out(IPC_COMMA_##in)            \
                             IPC_MEMBERS_OUT_##out());                      \
    MessageCracker::CopyMessageID(new_msg, real_msg);                       \
    return new_msg;                                                         \
  }


#define IPC_SYNC_ROUTED_FUZZ(name, in, out, ilist, olist)                   \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, Fuzzer* fuzzer) {      \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::ReadSendParam(real_msg, &p);                                      \
    FuzzParam(&p, fuzzer);                                                  \
    name* new_msg = new name(msg->routing_id()                              \
                             IPC_COMMA_OR_##out(IPC_COMMA_##in)             \
                             IPC_MEMBERS_IN_##in(p)                         \
                             IPC_COMMA_AND_##out(IPC_COMMA_##in)            \
                             IPC_MEMBERS_OUT_##out());                      \
    MessageCracker::CopyMessageID(new_msg, real_msg);                       \
    return new_msg;                                                         \
  }

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

typedef IPC::Message* (*FuzzFunction)(IPC::Message*, Fuzzer*);
typedef base::hash_map<uint32, FuzzFunction> FuzzFunctionMap;

// Redefine macros to register fuzzing functions into map.
#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  (*map)[static_cast<uint32>(name::ID)] = fuzzer_for_##name;

void PopulateFuzzFunctionMap(FuzzFunctionMap *map) {
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

static IPC::Message* RewriteMessage(
    IPC::Message* message,
    Fuzzer* fuzzer,
    FuzzFunctionMap* map) {
  FuzzFunctionMap::iterator it = map->find(message->type());
  if (it == map->end()) {
    // This usually indicates a missing message file in all_messages.h, or
    // that the message dump file is taken from a different revision of
    // chromium from this executable.
    std::cerr << "Unknown message type: ["
              << IPC_MESSAGE_ID_CLASS(message->type()) << ", "
              << IPC_MESSAGE_ID_LINE(message->type()) << "].\n";
    return 0;
  }

  return (*it->second)(message, fuzzer);
}

namespace {

const char kHelpSwitch[] = "help";
const char kHelpSwitchHelp[] =
    "show this message";

const char kFrequencySwitch[] = "frequency";
const char kFrequencySwitchHelp[] =
    "probability of mutation; tweak every 1/|q| times.";

const char kFuzzerNameSwitch[] = "fuzzer-name";
const char kFuzzerNameSwitchHelp[] =
    "select default or no-op fuzzer.";

const char kPermuteSwitch[] = "permute";
const char kPermuteSwitchHelp[] =
    "Randomly shuffle the order of all messages.";

const char kTypeListSwitch[] = "type-list";
const char kTypeListSwitchHelp[] =
    "explicit list of the only message-ids to mutate.";

void usage() {
  std::cerr << "Mutate messages from an exiting message file.\n";

  std::cerr << "Usage:\n"
            << "  ipc_fuzzer_mutate"
            << " [--" << kHelpSwitch << "]"
            << " [--" << kFuzzerNameSwitch << "=f]"
            << " [--" << kFrequencySwitch << "=q]"
            << " [--" << kTypeListSwitch << "=x,y,z...]"
            << " [--" << kPermuteSwitch << "]"
            << " infile outfile\n";

  std::cerr
      << " --" << kHelpSwitch << "         - " << kHelpSwitchHelp << "\n"
      << " --" << kFuzzerNameSwitch <<  "  - " << kFuzzerNameSwitchHelp << "\n"
      << " --" << kFrequencySwitch << "    - " << kFrequencySwitchHelp << "\n"
      << " --" << kTypeListSwitch <<  "    - " << kTypeListSwitchHelp << "\n"
      << " --" << kPermuteSwitch << "      - " << kPermuteSwitchHelp << "\n";
}

}  // namespace

int MutateMain(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  CommandLine::StringVector args = cmd->GetArgs();

  if (args.size() != 2 || cmd->HasSwitch(kHelpSwitch)) {
    usage();
    return EXIT_FAILURE;
  }

  std::string input_file_name = args[0];
  std::string output_file_name = args[1];

  bool permute = cmd->HasSwitch(kPermuteSwitch);

  std::string fuzzer_name = "default";
  if (cmd->HasSwitch(kFuzzerNameSwitch))
    fuzzer_name = cmd->GetSwitchValueASCII(kFuzzerNameSwitch);

  int frequency = 23;
  if (cmd->HasSwitch(kFrequencySwitch))
    frequency = atoi(cmd->GetSwitchValueASCII(kFrequencySwitch).c_str());

  std::string type_string_list = cmd->GetSwitchValueASCII(kTypeListSwitch);
  std::vector<std::string> type_string_vector;
  base::SplitString(type_string_list, ',', &type_string_vector);
  std::set<int> type_set;
  for (size_t i = 0; i < type_string_vector.size(); ++i) {
    type_set.insert(atoi(type_string_vector[i].c_str()));
  }

  InitRand();

  Fuzzer* fuzzer = FuzzerFactory::Create(fuzzer_name, frequency);
  if (!fuzzer)
    return EXIT_FAILURE;

  FuzzFunctionMap fuzz_function_map;
  PopulateFuzzFunctionMap(&fuzz_function_map);

  MessageVector message_vector;
  if (!MessageFile::Read(base::FilePath(input_file_name), &message_vector))
    return EXIT_FAILURE;

  for (size_t i = 0; i < message_vector.size(); ++i) {
    IPC::Message* msg = message_vector[i];
    if (!type_set.empty() && type_set.end() == std::find(
            type_set.begin(), type_set.end(), msg->type())) {
      continue;
    }
    IPC::Message* new_message = RewriteMessage(msg, fuzzer, &fuzz_function_map);
    if (new_message) {
      delete message_vector[i];
      message_vector[i] = new_message;
    }
  }

  if (permute) {
    std::random_shuffle(message_vector.begin(), message_vector.end(),
                        RandInRange);
  }

  if (!MessageFile::Write(base::FilePath(output_file_name), message_vector))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

}  // namespace ipc_fuzzer

int main(int argc, char** argv) {
  return ipc_fuzzer::MutateMain(argc, argv);
}
