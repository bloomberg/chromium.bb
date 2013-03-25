// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_USER_INTERFACE_H_
#define REMOTING_HOST_HOST_USER_INTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/ui_strings.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ChromotingHost;
class DisconnectWindow;

class HostUserInterface : public HostStatusObserver {
 public:
  HostUserInterface(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const UiStrings& ui_strings);
  virtual ~HostUserInterface();

  // Initialize the OS-specific UI objects.
  // Init must be called from |ui_task_runner_|.
  virtual void Init();

  // Start the HostUserInterface for |host|. |disconnect_callback| will be
  // called on |ui_task_runner| to notify the caller that the connection should
  // be disconnected. |host| must remain valid until OnShutdown() is called.
  // Start must be called from |network_task_runner_|.
  virtual void Start(ChromotingHost* host,
                     const base::Closure& disconnect_callback);

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 protected:
  const std::string& get_authenticated_jid() const {
    return authenticated_jid_;
  }
  ChromotingHost* get_host() const { return host_; }

  const UiStrings& ui_strings() const { return ui_strings_; }

  base::SingleThreadTaskRunner* network_task_runner() const;
  base::SingleThreadTaskRunner* ui_task_runner() const;

  // Invokes the session disconnect callback passed to Start().
  void DisconnectSession() const;

  virtual void ProcessOnClientAuthenticated(const std::string& username);
  virtual void ProcessOnClientDisconnected();

  ChromotingHost* host_;

  // Used to ask the host to disconnect the session.
  base::Closure disconnect_callback_;

  // Provide a user interface allowing the host user to close the connection.
  scoped_ptr<DisconnectWindow> disconnect_window_;

 private:
  // Invoked from the UI thread when the user clicks on the Disconnect button
  // to disconnect the session.
  void OnDisconnectCallback();

  // The JID of the currently-authenticated user (or an empty string if no user
  // is connected).
  std::string authenticated_jid_;

  // Thread on which the ChromotingHost processes network events.
  // Notifications from the host, and some calls into it, use this thread.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Thread on which to run the user interface.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // TODO(alexeypa): move |ui_strings_| to DesktopEnvironmentFactory.
  UiStrings ui_strings_;

  // WeakPtr used to avoid tasks accessing the client after it is deleted.
  base::WeakPtrFactory<HostUserInterface> weak_factory_;
  base::WeakPtr<HostUserInterface> weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(HostUserInterface);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_USER_INTERFACE_H_
