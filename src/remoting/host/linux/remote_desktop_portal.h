// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_H_
#define REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_H_

#include <gio/gio.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/portal_request_response.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/scoped_glib.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/screen_capture_portal_interface.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/screencast_portal.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_session_details.h"

namespace remoting {
namespace xdg_portal {

// Helper class to setup an XDG remote desktop portal session. An instance of
// this class is owned by the wayland capturer. The methods on this class are
// called from the capturer thread.
class RemoteDesktopPortal
    : public webrtc::xdg_portal::ScreenCapturePortalInterface,
      public webrtc::ScreenCastPortal::PortalNotifier {
 public:
  // |notifier| must outlive |RemoteDesktopPortal| instance and will be called
  // into from the capturer thread.
  explicit RemoteDesktopPortal(
      webrtc::ScreenCastPortal::PortalNotifier* notifier);
  RemoteDesktopPortal(const RemoteDesktopPortal&) = delete;
  RemoteDesktopPortal& operator=(const RemoteDesktopPortal&) = delete;
  ~RemoteDesktopPortal() override;

  // ScreenCapturePortalInterface overrides.
  void Start() override;
  void OnPortalDone(webrtc::xdg_portal::RequestResponse result) override;
  webrtc::xdg_portal::SessionDetails GetSessionDetails() override;
  void RequestSession(GDBusProxy* proxy) override;

  // PortalNotifier interface.
  void OnScreenCastRequestResult(webrtc::xdg_portal::RequestResponse result,
                                 uint32_t stream_node_id,
                                 int fd) override;
  void OnScreenCastSessionClosed() override;

 private:
  void Cleanup();
  void UnsubscribeSignalHandlers();
  void RequestSources();
  void SelectDevices();
  void StartRequest();
  uint32_t pipewire_stream_node_id();
  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);
  static void OnScreenCastPortalProxyRequested(GObject* object,
                                               GAsyncResult* result,
                                               gpointer user_data);
  static void OnDevicesRequestImpl(GDBusConnection* connection,
                                   const gchar* sender_name,
                                   const gchar* object_path,
                                   const gchar* interface_name,
                                   const gchar* signal_name,
                                   GVariant* parameters,
                                   gpointer user_data);
  static void OnStartRequestResponseSignal(GDBusConnection* connection,
                                           const char* sender_name,
                                           const char* object_path,
                                           const char* interface_name,
                                           const char* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);
  static void OnStartRequested(GDBusProxy* proxy,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnSourcesRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);
  static void OnSessionClosedSignal(GDBusConnection* connection,
                                    const char* sender_name,
                                    const char* object_path,
                                    const char* interface_name,
                                    const char* signal_name,
                                    GVariant* parameters,
                                    gpointer user_data);
  static void OnSessionRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnDevicesRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);

  webrtc::xdg_portal::RequestResponse screencast_portal_status_
      GUARDED_BY_CONTEXT(sequence_checker_) =
          webrtc::xdg_portal::RequestResponse::kUnknown;
  raw_ptr<GDBusConnection> connection_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<GDBusProxy> proxy_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  raw_ptr<GCancellable> cancellable_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<webrtc::ScreenCastPortal::PortalNotifier> notifier_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  std::string portal_handle_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string session_handle_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string start_handle_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string devices_handle_ GUARDED_BY_CONTEXT(sequence_checker_);
  guint session_request_signal_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  guint start_request_signal_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  guint session_closed_signal_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  guint devices_request_signal_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  std::unique_ptr<webrtc::ScreenCastPortal> screencast_portal_
      GUARDED_BY_CONTEXT(sequence_checker_);

  GMainContext* context_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace xdg_portal
}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_H_
