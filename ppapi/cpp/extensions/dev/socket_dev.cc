// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/extensions/dev/socket_dev.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/extensions/optional.h"
#include "ppapi/cpp/extensions/to_var_converter.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Ext_Socket_Dev_0_1>() {
  return PPB_EXT_SOCKET_DEV_INTERFACE_0_1;
}

}  // namespace

namespace ext {
namespace socket {

const char* const SocketType_Dev::kTcp = "tcp";
const char* const SocketType_Dev::kUdp = "udp";

SocketType_Dev::SocketType_Dev() : value(NONE) {
}

SocketType_Dev::SocketType_Dev(ValueType in_value) : value(in_value) {
}

SocketType_Dev::~SocketType_Dev() {
}

bool SocketType_Dev::Populate(const PP_Var& var_value) {
  if (var_value.type != PP_VARTYPE_STRING)
    return false;

  std::string string_value = Var(var_value).AsString();
  if (string_value == kTcp) {
    value = TCP;
  } else if (string_value == kUdp) {
    value = UDP;
  } else {
    value = NONE;
    return false;
  }
  return true;
}

Var SocketType_Dev::CreateVar() const {
  switch (value) {
    case TCP:
      return Var(kTcp);
    case UDP:
      return Var(kUdp);
    default:
      PP_NOTREACHED();
      return Var(std::string());
  }
}

const char* const CreateInfo_Dev::kSocketId = "socketId";

CreateInfo_Dev::CreateInfo_Dev()
    : socket_id(kSocketId) {
}

CreateInfo_Dev::~CreateInfo_Dev() {
}

bool CreateInfo_Dev::Populate(const PP_Ext_Socket_CreateInfo_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = socket_id.Populate(dict);

  return result;
}

Var CreateInfo_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = socket_id.AddTo(&dict);
  // Suppress unused variable warnings.
  static_cast<void>(result);
  PP_DCHECK(result);

  return dict;
}

const char* const AcceptInfo_Dev::kResultCode = "resultCode";
const char* const AcceptInfo_Dev::kSocketId = "socketId";

AcceptInfo_Dev::AcceptInfo_Dev()
    : result_code(kResultCode),
      socket_id(kSocketId) {
}

AcceptInfo_Dev::~AcceptInfo_Dev() {
}

bool AcceptInfo_Dev::Populate(const PP_Ext_Socket_AcceptInfo_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = result_code.Populate(dict);
  result = socket_id.Populate(dict) && result;

  return result;
}

Var AcceptInfo_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = result_code.AddTo(&dict);
  result = socket_id.MayAddTo(&dict) && result;
  PP_DCHECK(result);

  return dict;
}

const char* const ReadInfo_Dev::kResultCode = "resultCode";
const char* const ReadInfo_Dev::kData = "data";

ReadInfo_Dev::ReadInfo_Dev()
    : result_code(kResultCode),
      data(kData) {
}

ReadInfo_Dev::~ReadInfo_Dev() {
}

bool ReadInfo_Dev::Populate(const PP_Ext_Socket_ReadInfo_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = result_code.Populate(dict);
  result = data.Populate(dict) && result;

  return result;
}

Var ReadInfo_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = result_code.AddTo(&dict);
  result = data.AddTo(&dict) && result;
  PP_DCHECK(result);

  return dict;
}

const char* const WriteInfo_Dev::kBytesWritten = "bytesWritten";

WriteInfo_Dev::WriteInfo_Dev()
    : bytes_written(kBytesWritten) {
}

WriteInfo_Dev::~WriteInfo_Dev() {
}

bool WriteInfo_Dev::Populate(const PP_Ext_Socket_WriteInfo_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = bytes_written.Populate(dict);

  return result;
}

Var WriteInfo_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = bytes_written.AddTo(&dict);
  // Suppress unused variable warnings.
  static_cast<void>(result);
  PP_DCHECK(result);

  return dict;
}

const char* const RecvFromInfo_Dev::kResultCode = "resultCode";
const char* const RecvFromInfo_Dev::kData = "data";
const char* const RecvFromInfo_Dev::kAddress = "address";
const char* const RecvFromInfo_Dev::kPort = "port";

RecvFromInfo_Dev::RecvFromInfo_Dev()
    : result_code(kResultCode),
      data(kData),
      address(kAddress),
      port(kPort) {
}

RecvFromInfo_Dev::~RecvFromInfo_Dev() {
}

bool RecvFromInfo_Dev::Populate(const PP_Ext_Socket_RecvFromInfo_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = result_code.Populate(dict);
  result = data.Populate(dict) && result;
  result = address.Populate(dict) && result;
  result = port.Populate(dict) && result;

  return result;
}

Var RecvFromInfo_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = result_code.AddTo(&dict);
  result = data.AddTo(&dict) && result;
  result = address.AddTo(&dict) && result;
  result = port.AddTo(&dict) && result;
  PP_DCHECK(result);

  return dict;
}

const char* const SocketInfo_Dev::kSocketType = "socketType";
const char* const SocketInfo_Dev::kConnected = "connected";
const char* const SocketInfo_Dev::kPeerAddress = "peerAddress";
const char* const SocketInfo_Dev::kPeerPort = "peerPort";
const char* const SocketInfo_Dev::kLocalAddress = "localAddress";
const char* const SocketInfo_Dev::kLocalPort = "localPort";

SocketInfo_Dev::SocketInfo_Dev()
    : socket_type(kSocketType),
      connected(kConnected),
      peer_address(kPeerAddress),
      peer_port(kPeerPort),
      local_address(kLocalAddress),
      local_port(kLocalPort) {
}

SocketInfo_Dev::~SocketInfo_Dev() {
}

bool SocketInfo_Dev::Populate(const PP_Ext_Socket_SocketInfo_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = socket_type.Populate(dict);
  result = connected.Populate(dict) && result;
  result = peer_address.Populate(dict) && result;
  result = peer_port.Populate(dict) && result;
  result = local_address.Populate(dict) && result;
  result = local_port.Populate(dict) && result;

  return result;
}

Var SocketInfo_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = socket_type.AddTo(&dict);
  result = connected.AddTo(&dict) && result;
  result = peer_address.MayAddTo(&dict) && result;
  result = peer_port.MayAddTo(&dict) && result;
  result = local_address.MayAddTo(&dict) && result;
  result = local_port.MayAddTo(&dict) && result;
  PP_DCHECK(result);

  return dict;
}

const char* const NetworkInterface_Dev::kName = "name";
const char* const NetworkInterface_Dev::kAddress = "address";

NetworkInterface_Dev::NetworkInterface_Dev()
    : name(kName),
      address(kAddress) {
}

NetworkInterface_Dev::~NetworkInterface_Dev() {
}

bool NetworkInterface_Dev::Populate(
    const PP_Ext_Socket_NetworkInterface_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = name.Populate(dict);
  result = address.Populate(dict) && result;

  return result;
}

Var NetworkInterface_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = name.AddTo(&dict);
  result = address.AddTo(&dict) && result;
  PP_DCHECK(result);

  return dict;
}

Socket_Dev::Socket_Dev(const InstanceHandle& instance) : instance_(instance) {
}

Socket_Dev::~Socket_Dev() {
}

int32_t Socket_Dev::Create(const SocketType_Dev& type,
                           const Optional<CreateOptions_Dev>& options,
                           const CreateCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<SocketType_Dev> type_var(type);
  internal::ToVarConverter<Optional<CreateOptions_Dev> > options_var(options);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Create(
      instance_.pp_instance(),
      type_var.pp_var(),
      options_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

void Socket_Dev::Destroy(int32_t socket_id) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return;

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Destroy(
      instance_.pp_instance(),
      socket_id_var.pp_var());
}

int32_t Socket_Dev::Connect(int32_t socket_id,
                            const std::string& hostname,
                            int32_t port,
                            const ConnectCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<std::string> hostname_var(hostname);
  internal::ToVarConverter<int32_t> port_var(port);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Connect(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      hostname_var.pp_var(),
      port_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::Bind(int32_t socket_id,
                         const std::string& address,
                         int32_t port,
                         const BindCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<std::string> address_var(address);
  internal::ToVarConverter<int32_t> port_var(port);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Bind(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      address_var.pp_var(),
      port_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

void Socket_Dev::Disconnect(int32_t socket_id) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return;

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Disconnect(
      instance_.pp_instance(),
      socket_id_var.pp_var());
}

int32_t Socket_Dev::Read(int32_t socket_id,
                         const Optional<int32_t>& buffer_size,
                         const ReadCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<Optional<int32_t> > buffer_size_var(buffer_size);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Read(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      buffer_size_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::Write(int32_t socket_id,
                          const VarArrayBuffer& data,
                          const WriteCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<Var> data_var(data);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Write(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      data_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::RecvFrom(int32_t socket_id,
                             const Optional<int32_t>& buffer_size,
                             const RecvFromCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<Optional<int32_t> > buffer_size_var(buffer_size);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->RecvFrom(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      buffer_size_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::SendTo(int32_t socket_id,
                           const VarArrayBuffer& data,
                           const std::string& address,
                           int32_t port,
                           const SendToCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<Var> data_var(data);
  internal::ToVarConverter<std::string> address_var(address);
  internal::ToVarConverter<int32_t> port_var(port);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->SendTo(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      data_var.pp_var(),
      address_var.pp_var(),
      port_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::Listen(int32_t socket_id,
                           const std::string& address,
                           int32_t port,
                           const Optional<int32_t>& backlog,
                           const ListenCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<std::string> address_var(address);
  internal::ToVarConverter<int32_t> port_var(port);
  internal::ToVarConverter<Optional<int32_t> > backlog_var(backlog);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Listen(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      address_var.pp_var(),
      port_var.pp_var(),
      backlog_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::Accept(int32_t socket_id,
                           const AcceptCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->Accept(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::SetKeepAlive(int32_t socket_id,
                                 bool enable,
                                 const Optional<int32_t>& delay,
                                 const SetKeepAliveCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<bool> enable_var(enable);
  internal::ToVarConverter<Optional<int32_t> > delay_var(delay);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->SetKeepAlive(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      enable_var.pp_var(),
      delay_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::SetNoDelay(int32_t socket_id,
                               bool no_delay,
                               const SetNoDelayCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);
  internal::ToVarConverter<bool> no_delay_var(no_delay);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->SetNoDelay(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      no_delay_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::GetInfo(int32_t socket_id,
                            const GetInfoCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<int32_t> socket_id_var(socket_id);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->GetInfo(
      instance_.pp_instance(),
      socket_id_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Socket_Dev::GetNetworkList(const GetNetworkListCallback& callback) {
  if (!has_interface<PPB_Ext_Socket_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_Ext_Socket_Dev_0_1>()->GetNetworkList(
      instance_.pp_instance(),
      callback.output(),
      callback.pp_completion_callback());
}

}  // namespace socket
}  // namespace ext
}  // namespace pp
