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

template <typename T>
void GenerateIntegralType(T* value) {
  switch (RandInRange(16)) {
    case 0:
      *value = 0;
      break;
    case 1:
      *value = 1;
      break;
    case 2:
      *value = -1;
      break;
    case 3:
      *value = 2;
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

  virtual void GenerateBool(bool* value) OVERRIDE {
    *value = RandInRange(2);
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
    // to generate.
    std::cerr << "Can't handle " << __PRETTY_FUNCTION__ << "\n";
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
    size_t count = ++g_depth > 3 ? 0 : RandInRange(20);
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
    size_t count = ++g_depth > 3 ? 0 : RandInRange(20);
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
    size_t count = ++g_depth > 3 ? 0 : RandInRange(20);
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
    size_t count = RandInRange(60);
    std::string random_path;
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
    if (GenerateParam(&last_accessed, generator))
      return false;
    if (GenerateParam(&creation_time, generator))
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
struct GenerateTraits<GURL> {
  static bool Generate(GURL *p, Generator* generator) {
    const char url_chars[] = "Ahtp0:/.?+\%&#";
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

// FIXME: Actually generate something.
template <>
struct GenerateTraits<SkBitmap> {
  static bool Generate(SkBitmap* p, Generator* generator) {
    *p = SkBitmap();
    return true;
  }
};

template <>
struct GenerateTraits<IPC::ChannelHandle> {
  static bool Generate(IPC::ChannelHandle* p, Generator* generator) {
    return
        GenerateParam(&p->name, generator) &&
        GenerateParam(&p->socket, generator);
  }
};

template <>
struct GenerateTraits<cc::CompositorFrame> {
  // FIXME: this should actually generate something
  static bool Generate(cc::CompositorFrame* p, Generator* generator) {
    return true;
  }
};

template <>
struct GenerateTraits<cc::CompositorFrameAck> {
  // FIXME: this should actually generate something
  static bool Generate(cc::CompositorFrameAck* p, Generator* generator) {
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
          if (!GenerateParam(&array[i], generator))
            return false;
        }
        *p = content::IndexedDBKey(array);
        return true;
      }
      case blink::WebIDBKeyTypeBinary: {
        std::string binary;
        if (!GenerateParam(&binary, generator))
          return false;
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
        if (!GenerateParam(&number, generator))
          return false;
        *p = content::IndexedDBKey(number, web_type);
        return true;
      }
      case blink::WebIDBKeyTypeInvalid:
      case blink::WebIDBKeyTypeNull: {
        *p = content::IndexedDBKey(web_type);
        return true;
      }
      default:
        NOTREACHED();
        return false;
    }
    --g_depth;
    return true;
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
struct GenerateTraits<content::PageState> {
  static bool Generate(content::PageState *p, Generator* generator) {
    std::string junk;
    if (!GenerateParam(&junk, generator))
      return false;
    *p = content::PageState::CreateFromEncodedData(junk);
    return true;
  }
};

template <>
struct GenerateTraits<gpu::Mailbox> {
  static bool Generate(gpu::Mailbox *p, Generator* generator) {
    generator->GenerateBytes(p->name, sizeof(p->name));
    return true;
  }
};

template <>
struct GenerateTraits<media::AudioParameters> {
  static bool Generate(media::AudioParameters *p, Generator* generator) {
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
        static_cast<media::ChannelLayout>(channel_layout),
        channels,
        sample_rate,
        bits_per_sample,
        frames_per_buffer,
        effects);
    *p = params;
    return true;
  }
};

template <>
struct GenerateTraits<media::VideoCaptureFormat> {
  static bool Generate(media::VideoCaptureFormat *p, Generator* generator) {
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
  static bool Generate(net::LoadTimingInfo *p, Generator* generator) {
    return
        GenerateParam(&p->socket_log_id, generator) &&
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
  static bool Generate(net::HostPortPair *p, Generator* generator) {
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
  static bool Generate(net::IPEndPoint *p, Generator* generator) {
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
struct GenerateTraits<PP_NetAddress_Private> {
  static bool Generate(PP_NetAddress_Private *p, Generator* generator) {
    p->size = RandInRange(sizeof(p->data) + 1);
    generator->GenerateBytes(&p->data, p->size);
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

int GenerateMain(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  CommandLine::StringVector args = cmd->GetArgs();

  if (args.size() != 1 || cmd->HasSwitch(kHelpSwitch)) {
    std::cerr << "Usage: ipc_fuzzer_generate [--help] [--count=n] outfile\n";
    return EXIT_FAILURE;
  }
  std::string output_file_name = args[0];

  int message_count = 1000;
  if (cmd->HasSwitch(kCountSwitch))
    message_count = atoi(cmd->GetSwitchValueASCII(kCountSwitch).c_str());

  InitRand();

  GeneratorFunctionVector function_vector;
  PopulateGeneratorFunctionVector(&function_vector);
  std::cerr << "Counted " << function_vector.size()
            << " distinct messages present in chrome.\n";

  Generator* generator = new GeneratorImpl();
  MessageVector message_vector;

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
      size_t index = RandInRange(function_vector.size());
      if (IPC::Message* new_message = (*function_vector[index])(generator))
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
