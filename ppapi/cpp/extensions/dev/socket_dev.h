// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_DEV_SOCKET_DEV_H_
#define PPAPI_CPP_EXTENSIONS_DEV_SOCKET_DEV_H_

#include <string>
#include <vector>

#include "ppapi/c/extensions/dev/ppb_ext_socket_dev.h"
#include "ppapi/cpp/dev/var_dictionary_dev.h"
#include "ppapi/cpp/extensions/dict_field.h"
#include "ppapi/cpp/extensions/ext_output_traits.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"

namespace pp {

template <class T>
class CompletionCallbackWithOutput;

namespace ext {

template <class T>
class Optional;

namespace socket {

// Data types ------------------------------------------------------------------
class SocketType_Dev {
 public:
  enum ValueType {
    NONE,
    TCP,
    UDP
  };

  SocketType_Dev();
  SocketType_Dev(ValueType in_value);
  ~SocketType_Dev();

  bool Populate(const PP_Var& var_value);

  Var CreateVar() const;

  ValueType value;

  static const char* const kTcp;
  static const char* const kUdp;
};

typedef VarDictionary_Dev CreateOptions_Dev;

class CreateInfo_Dev : public internal::OutputObjectBase {
 public:
  CreateInfo_Dev();
  ~CreateInfo_Dev();

  bool Populate(const PP_Ext_Socket_CreateInfo_Dev& value);

  Var CreateVar() const;

  static const char* const kSocketId;

  DictField<int32_t> socket_id;
};

class AcceptInfo_Dev : public internal::OutputObjectBase {
 public:
  AcceptInfo_Dev();
  ~AcceptInfo_Dev();

  bool Populate(const PP_Ext_Socket_AcceptInfo_Dev& value);

  Var CreateVar() const;

  static const char* const kResultCode;
  static const char* const kSocketId;

  DictField<int32_t> result_code;
  OptionalDictField<int32_t> socket_id;
};

class ReadInfo_Dev : public internal::OutputObjectBase {
 public:
  ReadInfo_Dev();
  ~ReadInfo_Dev();

  bool Populate(const PP_Ext_Socket_ReadInfo_Dev& value);

  Var CreateVar() const;

  static const char* const kResultCode;
  static const char* const kData;

  DictField<int32_t> result_code;
  DictField<VarArrayBuffer> data;
};

class WriteInfo_Dev : public internal::OutputObjectBase {
 public:
  WriteInfo_Dev();
  ~WriteInfo_Dev();

  bool Populate(const PP_Ext_Socket_WriteInfo_Dev& value);

  Var CreateVar() const;

  static const char* const kBytesWritten;

  DictField<int32_t> bytes_written;
};

class RecvFromInfo_Dev : public internal::OutputObjectBase {
 public:
  RecvFromInfo_Dev();
  ~RecvFromInfo_Dev();

  bool Populate(const PP_Ext_Socket_RecvFromInfo_Dev& value);

  Var CreateVar() const;

  static const char* const kResultCode;
  static const char* const kData;
  static const char* const kAddress;
  static const char* const kPort;

  DictField<int32_t> result_code;
  DictField<VarArrayBuffer> data;
  DictField<std::string> address;
  DictField<int32_t> port;
};

class SocketInfo_Dev : public internal::OutputObjectBase {
 public:
  SocketInfo_Dev();
  ~SocketInfo_Dev();

  bool Populate(const PP_Ext_Socket_SocketInfo_Dev& value);

  Var CreateVar() const;

  static const char* const kSocketType;
  static const char* const kConnected;
  static const char* const kPeerAddress;
  static const char* const kPeerPort;
  static const char* const kLocalAddress;
  static const char* const kLocalPort;

  DictField<SocketType_Dev> socket_type;
  DictField<bool> connected;
  OptionalDictField<std::string> peer_address;
  OptionalDictField<int32_t> peer_port;
  OptionalDictField<std::string> local_address;
  OptionalDictField<int32_t> local_port;
};

class NetworkInterface_Dev : public internal::OutputObjectBase {
 public:
  NetworkInterface_Dev();
  ~NetworkInterface_Dev();

  bool Populate(const PP_Ext_Socket_NetworkInterface_Dev& value);

  Var CreateVar() const;

  static const char* const kName;
  static const char* const kAddress;

  DictField<std::string> name;
  DictField<std::string> address;
};

// Functions -------------------------------------------------------------------
class Socket_Dev {
 public:
  explicit Socket_Dev(const InstanceHandle& instance);
  ~Socket_Dev();

  int32_t Create(const SocketType_Dev& type,
                 const Optional<CreateOptions_Dev>& options,
                 const CompletionCallbackWithOutput<CreateInfo_Dev>& callback);
  void Destroy(int32_t socket_id);
  // TODO(yzshen): Support more powerful traits.
  //int32_t Connect(int32_t socket_id,
  //                const std::string& hostname,
  //                int32_t port,
  //                const CompletionCallbackWithOutput<int32_t>& callback);
  //int32_t Bind(int32_t socket_id,
  //             const std::string& address,
  //             int32_t port,
  //             const CompletionCallbackWithOutput<int32_t>& callback);
  void Disconnect(int32_t socket_id);
  int32_t Read(int32_t socket_id,
               const Optional<int32_t>& buffer_size,
               const CompletionCallbackWithOutput<ReadInfo_Dev>& callback);
  int32_t Write(int32_t socket_id,
                const VarArrayBuffer& data,
                const CompletionCallbackWithOutput<WriteInfo_Dev>& callback);
  int32_t RecvFrom(
      int32_t socket_id,
      const Optional<int32_t>& buffer_size,
      const CompletionCallbackWithOutput<RecvFromInfo_Dev>& callback);
  int32_t SendTo(int32_t socket_id,
                 const VarArrayBuffer& data,
                 const std::string& address,
                 int32_t port,
                 const CompletionCallbackWithOutput<WriteInfo_Dev>& callback);
  // TODO(yzshen): Support more powerful traits.
  //int32_t Listen(int32_t socket_id,
  //               const std::string& address,
  //               int32_t port,
  //               const Optional<int32_t>& backlog,
  //               const CompletionCallbackWithOutput<int32_t>& callback);
  int32_t Accept(int32_t socket_id,
                 const CompletionCallbackWithOutput<AcceptInfo_Dev>& callback);
  // TODO(yzshen): Support more powerful traits.
  //int32_t SetKeepAlive(int32_t socket_id,
  //                     bool enable,
  //                     const Optional<int32_t>& delay,
  //                     const CompletionCallbackWithOutput<bool>& callback);
  //int32_t SetNoDelay(int32_t socket_id,
  //                   bool no_delay,
  //                   const CompletionCallbackWithOutput<bool>& callback);
  int32_t GetInfo(int32_t socket_id,
                  const CompletionCallbackWithOutput<SocketInfo_Dev>& callback);
  int32_t GetNetworkList(
      const CompletionCallbackWithOutput<std::vector<NetworkInterface_Dev> >&
          callback);

 private:
  InstanceHandle instance_;
};

}  // namespace socket
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_DEV_SOCKET_DEV_H_
