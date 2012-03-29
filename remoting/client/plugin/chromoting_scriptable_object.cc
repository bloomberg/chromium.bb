// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_scriptable_object.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_split.h"
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
      plugin_message_loop_(plugin_message_loop) {
}

ChromotingScriptableObject::~ChromotingScriptableObject() {
}

void ChromotingScriptableObject::Init() {
  // Property addition order should match the interface description at the
  // top of chromoting_scriptable_object.h.

  AddAttribute(kApiVersionAttribute, Var(ChromotingInstance::kApiVersion));

  AddAttribute(kApiMinVersionAttribute,
               Var(ChromotingInstance::kApiMinScriptableVersion));

  // Connection status.
  AddAttribute(kStatusAttribute, Var(0)); // STATUS_UNKNOWN

  // Connection status values.
  // TODO(jamiewalch): Remove STATUS_UNKNOWN once all web-apps that might try
  // to access it have been upgraded.
  AddAttribute("STATUS_UNKNOWN", Var(0));
  AddAttribute("STATUS_CONNECTING", Var(ChromotingInstance::STATE_CONNECTING));
  AddAttribute("STATUS_INITIALIZING",
               Var(ChromotingInstance::STATE_INITIALIZING));
  AddAttribute("STATUS_CONNECTED", Var(ChromotingInstance::STATE_CONNECTED));
  AddAttribute("STATUS_CLOSED", Var(ChromotingInstance::STATE_CLOSED));
  AddAttribute("STATUS_FAILED", Var(ChromotingInstance::STATE_FAILED));

  // Connection error.
  AddAttribute(kErrorAttribute, Var(ChromotingInstance::ERROR_NONE));

  // Connection error values.
  AddAttribute("ERROR_NONE", Var(ChromotingInstance::ERROR_NONE));
  AddAttribute("ERROR_HOST_IS_OFFLINE",
               Var(ChromotingInstance::ERROR_HOST_IS_OFFLINE));
  AddAttribute("ERROR_SESSION_REJECTED",
               Var(ChromotingInstance::ERROR_SESSION_REJECTED));
  AddAttribute("ERROR_INCOMPATIBLE_PROTOCOL",
               Var(ChromotingInstance::ERROR_INCOMPATIBLE_PROTOCOL));
  AddAttribute("ERROR_FAILURE_NONE",
               Var(ChromotingInstance::ERROR_NETWORK_FAILURE));

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
  AddMethod("onIq", &ChromotingScriptableObject::DoOnIq);
  AddMethod("releaseAllKeys", &ChromotingScriptableObject::DoReleaseAllKeys);

  // Older versions of the web app expect a setScaleToFit method.
  AddMethod("setScaleToFit", &ChromotingScriptableObject::DoNothing);
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
    ChromotingInstance::ConnectionState state,
    ChromotingInstance::ConnectionError error) {
  VLOG(1) << "Connection status is updated: " << state;

  bool signal = false;

  int status_index = property_names_[kStatusAttribute];
  if (properties_[status_index].attribute.AsInt() != state) {
    properties_[status_index].attribute = Var(state);
    signal = true;
  }

  int error_index = property_names_[kErrorAttribute];
  if (properties_[error_index].attribute.AsInt() != error) {
    properties_[error_index].attribute = Var(error);
    signal = true;
  }

  if (signal) {
    plugin_message_loop_->PostTask(
        FROM_HERE, base::Bind(
            &ChromotingScriptableObject::DoSignalConnectionInfoChange,
            AsWeakPtr(), state, error));
  }
}

void ChromotingScriptableObject::LogDebugInfo(const std::string& info) {
  Var exception;
  VarPrivate cb = GetProperty(Var(kDebugInfo), &exception);

  if (!cb.is_undefined()) {
    // Var() means call the object directly as a function rather than calling
    // a method in the object.
    cb.Call(Var(), Var(info), &exception);

    if (!exception.is_undefined()) {
      LOG(WARNING) << "Exception when invoking debugInfo JS callback: "
                   << exception.DebugString();
    }
  }
}

void ChromotingScriptableObject::SetDesktopSize(int width, int height) {
  int width_index = property_names_[kDesktopWidth];
  int height_index = property_names_[kDesktopHeight];

  if (properties_[width_index].attribute.AsInt() != width ||
      properties_[height_index].attribute.AsInt() != height) {
    properties_[width_index].attribute = Var(width);
    properties_[height_index].attribute = Var(height);
    plugin_message_loop_->PostTask(
        FROM_HERE, base::Bind(
            &ChromotingScriptableObject::DoSignalDesktopSizeChange,
            AsWeakPtr()));
  }

  VLOG(1) << "Update desktop size to: " << width << " x " << height;
}

void ChromotingScriptableObject::SendIq(const std::string& message_xml) {
  plugin_message_loop_->PostTask(
      FROM_HERE, base::Bind(
          &ChromotingScriptableObject::DoSendIq, AsWeakPtr(), message_xml));
}

void ChromotingScriptableObject::AddAttribute(const std::string& name,
                                              const Var& attribute) {
  property_names_[name] = properties_.size();
  properties_.push_back(PropertyDescriptor(name, attribute));
}

void ChromotingScriptableObject::AddMethod(const std::string& name,
                                           MethodHandler handler) {
  property_names_[name] = properties_.size();
  properties_.push_back(PropertyDescriptor(name, handler));
}

void ChromotingScriptableObject::DoSignalConnectionInfoChange(int state,
                                                              int error) {
  Var exception;
  VarPrivate cb = GetProperty(Var(kConnectionInfoUpdate), &exception);

  if (!cb.is_undefined()) {
    // |this| must not be touched after Call() returns.
    cb.Call(Var(), Var(state), Var(error), &exception);

    if (!exception.is_undefined())
      LOG(ERROR) << "Exception when invoking connectionInfoUpdate JS callback.";
  }
}

void ChromotingScriptableObject::DoSignalDesktopSizeChange() {
  Var exception;
  VarPrivate cb = GetProperty(Var(kDesktopSizeUpdate), &exception);

  if (!cb.is_undefined()) {
    // |this| must not be touched after Call() returns.
    cb.Call(Var(), &exception);

    if (!exception.is_undefined()) {
      LOG(ERROR) << "Exception when invoking JS callback"
                 << exception.DebugString();
    }
  }
}

void ChromotingScriptableObject::DoSendIq(const std::string& message_xml) {
  Var exception;
  VarPrivate cb = GetProperty(Var(kSendIq), &exception);

  if (!cb.is_undefined()) {
    // |this| must not be touched after Call() returns.
    cb.Call(Var(), Var(message_xml), &exception);

    if (!exception.is_undefined())
      LOG(ERROR) << "Exception when invoking sendiq JS callback.";
  }
}

Var ChromotingScriptableObject::DoConnect(const std::vector<Var>& args,
                                          Var* exception) {
  // Parameter order is:
  //   host_jid
  //   host_public_key
  //   client_jid
  //   shared_secret
  //   authentication_methods
  //   authentication_tag
  ClientConfig config;

  unsigned int arg = 0;
  if (!args[arg].is_string()) {
    *exception = Var("The host_jid must be a string.");
    return Var();
  }
  config.host_jid = args[arg++].AsString();

  if (!args[arg].is_string()) {
    *exception = Var("The host_public_key must be a string.");
    return Var();
  }
  config.host_public_key = args[arg++].AsString();

  if (!args[arg].is_string()) {
    *exception = Var("The client_jid must be a string.");
    return Var();
  }
  config.local_jid = args[arg++].AsString();

  if (!args[arg].is_string()) {
    *exception = Var("The shared_secret must be a string.");
    return Var();
  }
  config.shared_secret = args[arg++].AsString();

  // Older versions of the webapp do not supply the following two
  // parameters.

  // By default use V1 authentication.
  config.use_v1_authenticator = true;
  if (args.size() > arg) {
    if (!args[arg].is_string()) {
      *exception = Var("The authentication_methods must be a string.");
      return Var();
    }

    if (!ChromotingInstance::ParseAuthMethods(
            args[arg++].AsString(), &config)) {
      *exception = Var("No valid authentication methods specified.");
      return Var();
    }
  }

  if (args.size() > arg) {
    if (!args[arg].is_string()) {
      *exception = Var("The authentication_tag must be a string.");
      return Var();
    }
    config.authentication_tag = args[arg++].AsString();
  }

  if (args.size() != arg) {
    *exception = Var("Too many agruments passed to connect().");
    return Var();
  }

  VLOG(1) << "Connecting to host. "
          << "client_jid: " << config.local_jid
          << ", host_jid: " << config.host_jid;
  instance_->Connect(config);

  return Var();
}

Var ChromotingScriptableObject::DoDisconnect(const std::vector<Var>& args,
                                             Var* exception) {
  VLOG(1) << "Disconnecting from host.";
  instance_->Disconnect();
  return Var();
}

Var ChromotingScriptableObject::DoNothing(const std::vector<Var>& args,
                                          Var* exception) {
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

  instance_->OnIncomingIq(args[0].AsString());

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
