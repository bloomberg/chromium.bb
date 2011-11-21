// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements the JavaScript class entrypoint for the plugin instance.
// The Javascript API is defined as follows.
//
// interface ChromotingScriptableObject {
//
//   // Chromoting session API version (for this plugin).
//   // This is compared with the javascript API version to verify that they are
//   // compatible.
//   readonly attribute unsigned short apiVersion;
//
//   // The oldest API version that we support.
//   // This will differ from |apiVersion| if we decide to maintain backward
//   // compatibility with older API versions.
//   readonly attribute unsigned short apiMinVersion;
//
//   // Connection status.
//   readonly attribute unsigned short status;
//
//   // Constants for connection status.
//   const unsigned short STATUS_UNKNOWN;
//   const unsigned short STATUS_CONNECTING;
//   const unsigned short STATUS_INITIALIZING;
//   const unsigned short STATUS_CONNECTED;
//   const unsigned short STATUS_CLOSED;
//   const unsigned short STATUS_FAILED;
//
//   // Constants for connection errors.
//   const unsigned short ERROR_NONE;
//   const unsigned short ERROR_HOST_IS_OFFLINE;
//   const unsigned short ERROR_SESSION_REJECTED;
//   const unsigned short ERROR_INCOMPATIBLE_PROTOCOL;
//   const unsigned short ERROR_NETWORK_FAILURE;
//
//   // JS callback function so we can signal the JS UI when the connection
//   // status has been updated.
//   attribute Function connectionInfoUpdate;
//
//   // JS callback function to call when there is new debug info to display
//   // in the client UI.
//   attribute Function debugInfo;
//
//   attribute Function desktopSizeUpdate;
//
//   // JS callback function to send an XMPP IQ stanza for performing the
//   // signaling in a jingle connection.  The callback function should be
//   // of type void(string request_xml).
//   attribute Function sendIq;
//
//   // Dimension of the desktop area.
//   readonly attribute int desktopWidth;
//   readonly attribute int desktopHeight;
//
//   // Statistics.
//   // Video bandwidth, in bytes per second.
//   readonly attribute float videoBandwidth;
//   // Video frames received per second.
//   readonly attribute float videoFrameRate;
//   // Latency for capturing, in milliseconds.
//   readonly attribute int videoCaptureLatency;
//   // Latency for video decoding, in milliseconds.
//   readonly attribute int videoDecodeLatency;
//   // Latency for video encoding, in milliseconds.
//   readonly attribute int videoEncodeLatency;
//   // Latency for video rendering, in milliseconds.
//   readonly attribute int videoRenderLatency;
//   // Latency of input events, based on delay between sending an input event
//   // and receiving the first video packet after the event was processed.
//   readonly attribute int roundTripLatency;
//
//   // Methods for establishing a Chromoting connection.
//   //
//   // sendIq must be set and responses to calls on sendIq must
//   // be piped back into onIq().
//   //
//   // Note that auth_token_with_service should be specified as
//   // "auth_service:auth_token". For example, "oauth2:5/aBd123".
//   void connect(string host_jid, string auth_token_with_service,
//                optional string access_code);
//
//   // Terminating a Chromoting connection.
//   void disconnect();
//
//   // Method for setting scale-to-fit.
//   void setScaleToFit(bool scale_to_fit);
//
//   // Method for receiving an XMPP IQ stanza in response to a previous
//   // sendIq() invocation. Other packets will be silently dropped.
//   void onIq(string response_xml);
//
//   // Method for releasing all keys to ensure a consistent host state.
//   void releaseAllKeys();
// }

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_SCRIPTABLE_OBJECT_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_SCRIPTABLE_OBJECT_H_

#include <map>
#include <string>
#include <vector>

#include "base/task.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/var.h"

namespace base {
class MessageLoopProxy;
};  // namespace base

namespace remoting {

class ChromotingInstance;
class PepperXmppProxy;

class ChromotingScriptableObject
    : public pp::deprecated::ScriptableObject,
      public base::SupportsWeakPtr<ChromotingScriptableObject> {
 public:
  // These state values are duplicated in the JS code. Remember to update both
  // copies when making changes.
  enum ConnectionStatus {
    // TODO(jamiewalch): Remove STATUS_UNKNOWN once all web-apps that might try
    // to access it have been upgraded.
    STATUS_UNKNOWN = 0,
    STATUS_CONNECTING,
    STATUS_INITIALIZING,
    STATUS_CONNECTED,
    STATUS_CLOSED,
    STATUS_FAILED,
  };

  // These state values are duplicated in the JS code. Remember to update both
  // copies when making changes.
  enum ConnectionError {
    ERROR_NONE = 0,
    ERROR_HOST_IS_OFFLINE,
    ERROR_SESSION_REJECTED,
    ERROR_INCOMPATIBLE_PROTOCOL,
    ERROR_NETWORK_FAILURE,
  };

  ChromotingScriptableObject(
      ChromotingInstance* instance,
      base::MessageLoopProxy* plugin_message_loop);
  virtual ~ChromotingScriptableObject();

  virtual void Init();

  // Override the ScriptableObject functions.
  virtual bool HasProperty(const pp::Var& name, pp::Var* exception) OVERRIDE;
  virtual bool HasMethod(const pp::Var& name, pp::Var* exception) OVERRIDE;
  virtual pp::Var GetProperty(const pp::Var& name, pp::Var* exception) OVERRIDE;
  virtual void GetAllPropertyNames(std::vector<pp::Var>* properties,
                                   pp::Var* exception) OVERRIDE;
  virtual void SetProperty(const pp::Var& name,
                           const pp::Var& value,
                           pp::Var* exception) OVERRIDE;
  virtual pp::Var Call(const pp::Var& method_name,
                       const std::vector<pp::Var>& args,
                       pp::Var* exception) OVERRIDE;

  void SetConnectionStatus(ConnectionStatus status, ConnectionError error);
  void LogDebugInfo(const std::string& info);
  void SetDesktopSize(int width, int height);

  // Attaches the XmppProxy used for issuing and receivng IQ stanzas for
  // initializing a jingle connection from within the sandbox.
  void AttachXmppProxy(PepperXmppProxy* xmpp_proxy);

  // Sends an IQ stanza, serialized as an xml string, into Javascript for
  // handling.
  void SendIq(const std::string& request_xml);

 private:
  typedef std::map<std::string, int> PropertyNameMap;
  typedef pp::Var (ChromotingScriptableObject::*MethodHandler)(
      const std::vector<pp::Var>& args, pp::Var* exception);
  struct PropertyDescriptor {
    PropertyDescriptor(const std::string& n, pp::Var a)
        : type(NONE), name(n), attribute(a), method(NULL) {
    }

    PropertyDescriptor(const std::string& n, MethodHandler m)
        : type(NONE), name(n), method(m) {
    }

    enum Type {
      NONE,
      ATTRIBUTE,
      METHOD,
    } type;

    std::string name;
    pp::Var attribute;
    MethodHandler method;
  };

  // Routines to add new attribute, method properties.
  void AddAttribute(const std::string& name, pp::Var attribute);
  void AddMethod(const std::string& name, MethodHandler handler);

  void SignalConnectionInfoChange(int status, int error);
  void SignalDesktopSizeChange();

  // Calls to these methods are posted to the plugin thread so that we
  // call JavaScript with clean stack. This is necessary because
  // JavaScript event handlers may destroy the plugin.
  void DoSignalConnectionInfoChange(int status, int error);
  void DoSignalDesktopSizeChange();
  void DoSendIq(const std::string& message_xml);

  pp::Var DoConnect(const std::vector<pp::Var>& args, pp::Var* exception);
  pp::Var DoDisconnect(const std::vector<pp::Var>& args, pp::Var* exception);

  // This method is called by JS to set scale-to-fit.
  pp::Var DoSetScaleToFit(const std::vector<pp::Var>& args, pp::Var* exception);

  // This method is called by Javascript to provide responses to sendIq()
  // requests.
  pp::Var DoOnIq(const std::vector<pp::Var>& args, pp::Var* exception);

  // This method is called by Javascript when the plugin loses input focus to
  // release all pressed keys.
  pp::Var DoReleaseAllKeys(const std::vector<pp::Var>& args,
                           pp::Var* exception);

  PropertyNameMap property_names_;
  std::vector<PropertyDescriptor> properties_;
  scoped_refptr<PepperXmppProxy> xmpp_proxy_;

  ChromotingInstance* instance_;

  scoped_refptr<base::MessageLoopProxy> plugin_message_loop_;
  ScopedRunnableMethodFactory<ChromotingScriptableObject> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingScriptableObject);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_SCRIPTABLE_OBJECT_H_
