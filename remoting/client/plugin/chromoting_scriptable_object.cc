// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_scriptable_object.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
// TODO(wez): Remove this when crbug.com/86353 is complete.
#include "ppapi/cpp/private/var_private.h"
#include "remoting/base/auth_token_util.h"
#include "remoting/client/client_config.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_xmpp_proxy.h"

using pp::Var;
using pp::VarPrivate;

namespace remoting {

namespace {

const char kApiVersionAttribute[] = "apiVersion";
const char kApiMinVersionAttribute[] = "apiMinVersion";
const char kConnectionInfoUpdate[] = "connectionInfoUpdate";
const char kDebugInfo[] = "debugInfo";
const char kDesktopHeight[] = "desktopHeight";
const char kDesktopWidth[] = "desktopWidth";
const char kDesktopSizeUpdate[] = "desktopSizeUpdate";
const char kSendIq[] = "sendIq";
const char kStatusAttribute[] = "status";
const char kErrorAttribute[] = "error";
const char kVideoBandwidthAttribute[] = "videoBandwidth";
const char kVideoFrameRateAttribute[] = "videoFrameRate";
const char kVideoCaptureLatencyAttribute[] = "videoCaptureLatency";
const char kVideoEncodeLatencyAttribute[] = "videoEncodeLatency";
const char kVideoDecodeLatencyAttribute[] = "videoDecodeLatency";
const char kVideoRenderLatencyAttribute[] = "videoRenderLatency";
const char kRoundTripLatencyAttribute[] = "roundTripLatency";

}  // namespace

ChromotingScriptableObject::ChromotingScriptableObject(
    ChromotingInstance* instance, base::MessageLoopProxy* plugin_message_loop)
    : instance_(instance),
      plugin_message_loop_(plugin_message_loop),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
}

ChromotingScriptableObject::~ChromotingScriptableObject() {
}

void ChromotingScriptableObject::Init() {
  // Property addition order should match the interface description at the
  // top of chromoting_scriptable_object.h.

  // Plugin API version.
  // This should be incremented whenever the API interface changes.
  AddAttribute(kApiVersionAttribute, Var(2));

  // This should be updated whenever we remove support for an older version
  // of the API.
  AddAttribute(kApiMinVersionAttribute, Var(2));

  // Connection status.
  AddAttribute(kStatusAttribute, Var(STATUS_UNKNOWN));

  // Connection status values.
  AddAttribute("STATUS_UNKNOWN", Var(STATUS_UNKNOWN));
  AddAttribute("STATUS_CONNECTING", Var(STATUS_CONNECTING));
  AddAttribute("STATUS_INITIALIZING", Var(STATUS_INITIALIZING));
  AddAttribute("STATUS_CONNECTED", Var(STATUS_CONNECTED));
  AddAttribute("STATUS_CLOSED", Var(STATUS_CLOSED));
  AddAttribute("STATUS_FAILED", Var(STATUS_FAILED));

  // Connection error.
  AddAttribute(kErrorAttribute, Var(ERROR_NONE));

  // Connection status values.
  AddAttribute("ERROR_NONE", Var(ERROR_NONE));
  AddAttribute("ERROR_HOST_IS_OFFLINE", Var(ERROR_HOST_IS_OFFLINE));
  AddAttribute("ERROR_SESSION_REJECTED", Var(ERROR_SESSION_REJECTED));
  AddAttribute("ERROR_INCOMPATIBLE_PROTOCOL", Var(ERROR_INCOMPATIBLE_PROTOCOL));
  AddAttribute("ERROR_FAILURE_NONE", Var(ERROR_NETWORK_FAILURE));

  // Debug info to display.
  AddAttribute(kConnectionInfoUpdate, Var());
  AddAttribute(kDebugInfo, Var());
  AddAttribute(kDesktopSizeUpdate, Var());
  AddAttribute(kSendIq, Var());
  AddAttribute(kDesktopWidth, Var(0));
  AddAttribute(kDesktopHeight, Var(0));

  // Statistics.
  AddAttribute(kVideoBandwidthAttribute, Var());
  AddAttribute(kVideoFrameRateAttribute, Var());
  AddAttribute(kVideoCaptureLatencyAttribute, Var());
  AddAttribute(kVideoEncodeLatencyAttribute, Var());
  AddAttribute(kVideoDecodeLatencyAttribute, Var());
  AddAttribute(kVideoRenderLatencyAttribute, Var());
  AddAttribute(kRoundTripLatencyAttribute, Var());

  AddMethod("connect", &ChromotingScriptableObject::DoConnect);
  AddMethod("disconnect", &ChromotingScriptableObject::DoDisconnect);
  AddMethod("setScaleToFit", &ChromotingScriptableObject::DoSetScaleToFit);
  AddMethod("onIq", &ChromotingScriptableObject::DoOnIq);
  AddMethod("releaseAllKeys", &ChromotingScriptableObject::DoReleaseAllKeys);
}

bool ChromotingScriptableObject::HasProperty(const Var& name, Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("HasProperty expects a string for the name.");
    return false;
  }

  PropertyNameMap::const_iterator iter = property_names_.find(name.AsString());
  if (iter == property_names_.end()) {
    return false;
  }

  // TODO(ajwong): Investigate why ARM build breaks if you do:
  //    properties_[iter->second].method == NULL;
  // Somehow the ARM compiler is thinking that the above is using NULL as an
  // arithmetic expression.
  return properties_[iter->second].method == 0;
}

bool ChromotingScriptableObject::HasMethod(const Var& name, Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("HasMethod expects a string for the name.");
    return false;
  }

  PropertyNameMap::const_iterator iter = property_names_.find(name.AsString());
  if (iter == property_names_.end()) {
    return false;
  }

  // See comment from HasProperty about why to use 0 instead of NULL here.
  return properties_[iter->second].method != 0;
}

Var ChromotingScriptableObject::GetProperty(const Var& name, Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("GetProperty expects a string for the name.");
    return Var();
  }

  PropertyNameMap::const_iterator iter = property_names_.find(name.AsString());

  // No property found.
  if (iter == property_names_.end()) {
    return ScriptableObject::GetProperty(name, exception);
  }

  // If this is a statistics attribute then return the value from
  // ChromotingStats structure.
  ChromotingStats* stats = instance_->GetStats();
  if (name.AsString() == kVideoBandwidthAttribute)
    return stats ? stats->video_bandwidth()->Rate() : Var();
  if (name.AsString() == kVideoFrameRateAttribute)
    return stats ? stats->video_frame_rate()->Rate() : Var();
  if (name.AsString() == kVideoCaptureLatencyAttribute)
    return stats ? stats->video_capture_ms()->Average() : Var();
  if (name.AsString() == kVideoEncodeLatencyAttribute)
    return stats ? stats->video_encode_ms()->Average() : Var();
  if (name.AsString() == kVideoDecodeLatencyAttribute)
    return stats ? stats->video_decode_ms()->Average() : Var();
  if (name.AsString() == kVideoRenderLatencyAttribute)
    return stats ? stats->video_paint_ms()->Average() : Var();
  if (name.AsString() == kRoundTripLatencyAttribute)
    return stats ? stats->round_trip_ms()->Average() : Var();

  // TODO(ajwong): This incorrectly return a null object if a function
  // property is requested.
  return properties_[iter->second].attribute;
}

void ChromotingScriptableObject::GetAllPropertyNames(
    std::vector<Var>* properties,
    Var* exception) {
  for (size_t i = 0; i < properties_.size(); i++) {
    properties->push_back(Var(properties_[i].name));
  }
}

void ChromotingScriptableObject::SetProperty(const Var& name,
                                             const Var& value,
                                             Var* exception) {
  // TODO(ajwong): Check if all these name.is_string() sentinels are required.
  if (!name.is_string()) {
    *exception = Var("SetProperty expects a string for the name.");
    return;
  }

  // Not all properties are mutable.
  std::string property_name = name.AsString();
  if (property_name != kConnectionInfoUpdate &&
      property_name != kDebugInfo &&
      property_name != kDesktopSizeUpdate &&
      property_name != kSendIq &&
      property_name != kDesktopWidth &&
      property_name != kDesktopHeight) {
    *exception =
        Var("Cannot set property " + property_name + " on this object.");
    return;
  }

  // Since we're whitelisting the property that are settable above, we can
  // assume that the property exists in the map.
  properties_[property_names_[property_name]].attribute = value;
}

Var ChromotingScriptableObject::Call(const Var& method_name,
                                     const std::vector<Var>& args,
                                     Var* exception) {
  PropertyNameMap::const_iterator iter =
      property_names_.find(method_name.AsString());
  if (iter == property_names_.end()) {
    return pp::deprecated::ScriptableObject::Call(method_name, args, exception);
  }

  return (this->*(properties_[iter->second].method))(args, exception);
}

void ChromotingScriptableObject::SetConnectionStatus(
    ConnectionStatus status, ConnectionError error) {
  VLOG(1) << "Connection status is updated: " << status;

  bool signal = false;

  int status_index = property_names_[kStatusAttribute];
  if (properties_[status_index].attribute.AsInt() != status) {
    properties_[status_index].attribute = Var(status);
    signal = true;
  }

  int error_index = property_names_[kErrorAttribute];
  if (properties_[error_index].attribute.AsInt() != error) {
    properties_[error_index].attribute = Var(error);
    signal = true;
  }

  if (signal)
    SignalConnectionInfoChange(status, error);
}

void ChromotingScriptableObject::LogDebugInfo(const std::string& info) {
  Var exception;
  VarPrivate cb = GetProperty(Var(kDebugInfo), &exception);

  // Var() means call the object directly as a function rather than calling
  // a method in the object.
  cb.Call(Var(), Var(info), &exception);

  if (!exception.is_undefined()) {
    LOG(WARNING) << "Exception when invoking debugInfo JS callback: "
                 << exception.DebugString();
  }
}

void ChromotingScriptableObject::SetDesktopSize(int width, int height) {
  int width_index = property_names_[kDesktopWidth];
  int height_index = property_names_[kDesktopHeight];

  if (properties_[width_index].attribute.AsInt() != width ||
      properties_[height_index].attribute.AsInt() != height) {
    properties_[width_index].attribute = Var(width);
    properties_[height_index].attribute = Var(height);
    SignalDesktopSizeChange();
  }

  VLOG(1) << "Update desktop size to: " << width << " x " << height;
}

void ChromotingScriptableObject::AttachXmppProxy(PepperXmppProxy* xmpp_proxy) {
  xmpp_proxy_ = xmpp_proxy;
}

void ChromotingScriptableObject::SendIq(const std::string& message_xml) {
  plugin_message_loop_->PostTask(
      FROM_HERE, task_factory_.NewRunnableMethod(
          &ChromotingScriptableObject::DoSendIq, message_xml));
}

void ChromotingScriptableObject::AddAttribute(const std::string& name,
                                              Var attribute) {
  property_names_[name] = properties_.size();
  properties_.push_back(PropertyDescriptor(name, attribute));
}

void ChromotingScriptableObject::AddMethod(const std::string& name,
                                           MethodHandler handler) {
  property_names_[name] = properties_.size();
  properties_.push_back(PropertyDescriptor(name, handler));
}

void ChromotingScriptableObject::SignalConnectionInfoChange(int status,
                                                            int error) {
  plugin_message_loop_->PostTask(
      FROM_HERE, task_factory_.NewRunnableMethod(
          &ChromotingScriptableObject::DoSignalConnectionInfoChange,
          status, error));
}

void ChromotingScriptableObject::SignalDesktopSizeChange() {
  plugin_message_loop_->PostTask(
      FROM_HERE, task_factory_.NewRunnableMethod(
          &ChromotingScriptableObject::DoSignalDesktopSizeChange));
}

void ChromotingScriptableObject::DoSignalConnectionInfoChange(int status,
                                                              int error) {
  Var exception;
  VarPrivate cb = GetProperty(Var(kConnectionInfoUpdate), &exception);

  // |this| must not be touched after Call() returns.
  cb.Call(Var(), Var(status), Var(error), &exception);

  if (!exception.is_undefined())
    LOG(ERROR) << "Exception when invoking connectionInfoUpdate JS callback.";
}

void ChromotingScriptableObject::DoSignalDesktopSizeChange() {
  Var exception;
  VarPrivate cb = GetProperty(Var(kDesktopSizeUpdate), &exception);

  // |this| must not be touched after Call() returns.
  cb.Call(Var(), &exception);

  if (!exception.is_undefined()) {
    LOG(ERROR) << "Exception when invoking JS callback"
               << exception.DebugString();
  }
}

void ChromotingScriptableObject::DoSendIq(const std::string& message_xml) {
  Var exception;
  VarPrivate cb = GetProperty(Var(kSendIq), &exception);

  // |this| must not be touched after Call() returns.
  cb.Call(Var(), Var(message_xml), &exception);

  if (!exception.is_undefined())
    LOG(ERROR) << "Exception when invoking sendiq JS callback.";
}

Var ChromotingScriptableObject::DoConnect(const std::vector<Var>& args,
                                          Var* exception) {
  // Parameter order is:
  //   host_jid
  //   host_public_key
  //   client_jid
  //   access_code (optional)
  unsigned int arg = 0;
  if (!args[arg].is_string()) {
    *exception = Var("The host_jid must be a string.");
    return Var();
  }
  std::string host_jid = args[arg++].AsString();

  if (!args[arg].is_string()) {
    *exception = Var("The host_public_key must be a string.");
    return Var();
  }
  std::string host_public_key = args[arg++].AsString();

  if (!args[arg].is_string()) {
    *exception = Var("The client_jid must be a string.");
    return Var();
  }
  std::string client_jid = args[arg++].AsString();

  std::string access_code;
  if (args.size() > arg) {
    if (!args[arg].is_string()) {
      *exception = Var("The access code must be a string.");
      return Var();
    }
    access_code = args[arg++].AsString();
  }

  if (args.size() != arg) {
    *exception = Var("Too many agruments passed to connect().");
    return Var();
  }

  VLOG(1) << "Connecting to host. "
          << "client_jid: " << client_jid << ", host_jid: " << host_jid
          << ", access_code: " << access_code;
  ClientConfig config;
  config.local_jid = client_jid;
  config.host_jid = host_jid;
  config.host_public_key = host_public_key;
  config.access_code = access_code;
  instance_->Connect(config);

  return Var();
}

Var ChromotingScriptableObject::DoDisconnect(const std::vector<Var>& args,
                                             Var* exception) {
  VLOG(1) << "Disconnecting from host.";
  instance_->Disconnect();
  return Var();
}

Var ChromotingScriptableObject::DoSetScaleToFit(const std::vector<Var>& args,
                                                Var* exception) {
  if (args.size() != 1) {
    *exception = Var("Usage: setScaleToFit(scale_to_fit)");
    return Var();
  }

  if (!args[0].is_bool()) {
    *exception = Var("scale_to_fit must be a boolean.");
    return Var();
  }

  VLOG(1) << "Setting scale-to-fit.";
  instance_->SetScaleToFit(args[0].AsBool());
  return Var();
}

Var ChromotingScriptableObject::DoOnIq(const std::vector<Var>& args,
                                       Var* exception) {
  if (args.size() != 1) {
    *exception = Var("Usage: onIq(response_xml)");
    return Var();
  }

  if (!args[0].is_string()) {
    *exception = Var("response_xml must be a string.");
    return Var();
  }

  xmpp_proxy_->OnIq(args[0].AsString());

  return Var();
}

Var ChromotingScriptableObject::DoReleaseAllKeys(
    const std::vector<pp::Var>& args, pp::Var* exception) {
  if (args.size() != 0) {
    *exception = Var("Usage: DoReleaseAllKeys()");
    return Var();
  }
  instance_->ReleaseAllKeys();
  return Var();
}

}  // namespace remoting
