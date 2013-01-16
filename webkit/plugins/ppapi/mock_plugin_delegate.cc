// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/mock_plugin_delegate.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGamepads.h"
#include "webkit/plugins/ppapi/mock_platform_image_2d.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

MockPluginDelegate::MockPluginDelegate() {
}

MockPluginDelegate::~MockPluginDelegate() {
}

void MockPluginDelegate::PluginFocusChanged(PluginInstance* instance,
                                            bool focused) {
}

void MockPluginDelegate::PluginTextInputTypeChanged(PluginInstance* instance) {
}

void MockPluginDelegate::PluginCaretPositionChanged(PluginInstance* instance) {
}

void MockPluginDelegate::PluginRequestedCancelComposition(
    PluginInstance* instance) {
}

void MockPluginDelegate::PluginSelectionChanged(PluginInstance* instance) {
}

void MockPluginDelegate::SimulateImeSetComposition(
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
}

void MockPluginDelegate::SimulateImeConfirmComposition(const string16& text) {
}

void MockPluginDelegate::PluginCrashed(PluginInstance* instance) {
}

void MockPluginDelegate::InstanceCreated(PluginInstance* instance) {
}

void MockPluginDelegate::InstanceDeleted(PluginInstance* instance) {
}

scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
MockPluginDelegate::CreateResourceCreationAPI(PluginInstance* instance) {
  return scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>();
}

SkBitmap* MockPluginDelegate::GetSadPluginBitmap() {
  return NULL;
}

WebKit::WebPlugin* MockPluginDelegate::CreatePluginReplacement(
    const FilePath& file_path) {
  return NULL;
}

MockPluginDelegate::PlatformImage2D* MockPluginDelegate::CreateImage2D(
    int width,
    int height) {
  return new MockPlatformImage2D(width, height);
}

PluginDelegate::PlatformGraphics2D* MockPluginDelegate::GetGraphics2D(
    PluginInstance* instance,
    PP_Resource graphics_2d) {
  return NULL;
}

MockPluginDelegate::PlatformContext3D* MockPluginDelegate::CreateContext3D() {
  return NULL;
}
void MockPluginDelegate::ReparentContext(
    MockPluginDelegate::PlatformContext3D* context) {
}

MockPluginDelegate::PlatformVideoDecoder*
MockPluginDelegate::CreateVideoDecoder(
    media::VideoDecodeAccelerator::Client* client,
    int32 command_buffer_route_id) {
  return NULL;
}

MockPluginDelegate::PlatformVideoCapture*
MockPluginDelegate::CreateVideoCapture(
    const std::string& device_id,
    PlatformVideoCaptureEventHandler* handler){
  return NULL;
}

uint32_t MockPluginDelegate::GetAudioHardwareOutputSampleRate() {
  return 0;
}

uint32_t MockPluginDelegate::GetAudioHardwareOutputBufferSize() {
  return 0;
}

MockPluginDelegate::PlatformAudioOutput* MockPluginDelegate::CreateAudioOutput(
    uint32_t sample_rate,
    uint32_t sample_count,
    PlatformAudioOutputClient* client) {
  return NULL;
}

MockPluginDelegate::PlatformAudioInput* MockPluginDelegate::CreateAudioInput(
    const std::string& device_id,
    uint32_t sample_rate,
    uint32_t sample_count,
    PlatformAudioInputClient* client) {
  return NULL;
}

MockPluginDelegate::Broker* MockPluginDelegate::ConnectToBroker(
    PPB_Broker_Impl* client) {
  return NULL;
}

void MockPluginDelegate::NumberOfFindResultsChanged(int identifier,
                                                    int total,
                                                    bool final_result) {
}

void MockPluginDelegate::SelectedFindResultChanged(int identifier, int index) {
}

bool MockPluginDelegate::AsyncOpenFile(const FilePath& path,
                                       int flags,
                                       const AsyncOpenFileCallback& callback) {
  return false;
}

bool MockPluginDelegate::AsyncOpenFileSystemURL(
    const GURL& path,
    int flags,
    const AsyncOpenFileSystemURLCallback& callback) {
  return false;
}

bool MockPluginDelegate::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    long long size,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

bool MockPluginDelegate::MakeDirectory(
    const GURL& path,
    bool recursive,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

bool MockPluginDelegate::Query(
    const GURL& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

bool MockPluginDelegate::Touch(
    const GURL& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

bool MockPluginDelegate::SetLength(
    const GURL& path,
    int64_t length,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

bool MockPluginDelegate::Delete(
    const GURL& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

bool MockPluginDelegate::Rename(
    const GURL& file_path,
    const GURL& new_file_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

bool MockPluginDelegate::ReadDirectory(
    const GURL& directory_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  return false;
}

void MockPluginDelegate::QueryAvailableSpace(
    const GURL& origin, quota::StorageType type,
    const AvailableSpaceCallback& callback) {
}

void MockPluginDelegate::WillUpdateFile(const GURL& file_path) {
}

void MockPluginDelegate::DidUpdateFile(const GURL& file_path, int64_t delta) {
}

void MockPluginDelegate::SyncGetFileSystemPlatformPath(
    const GURL& url,
    FilePath* platform_path) {
  DCHECK(platform_path);
  *platform_path = FilePath();
}

scoped_refptr<base::MessageLoopProxy>
MockPluginDelegate::GetFileThreadMessageLoopProxy() {
  return scoped_refptr<base::MessageLoopProxy>();
}

uint32 MockPluginDelegate::TCPSocketCreate() {
  return 0;
}

void MockPluginDelegate::TCPSocketConnect(PPB_TCPSocket_Private_Impl* socket,
                                          uint32 socket_id,
                                          const std::string& host,
                                          uint16_t port) {
}

void MockPluginDelegate::TCPSocketConnectWithNetAddress(
    PPB_TCPSocket_Private_Impl* socket,
    uint32 socket_id,
    const PP_NetAddress_Private& addr) {
}

void MockPluginDelegate::TCPSocketSSLHandshake(
    uint32 socket_id,
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
}

void MockPluginDelegate::TCPSocketRead(uint32 socket_id,
                                       int32_t bytes_to_read) {
}

void MockPluginDelegate::TCPSocketWrite(uint32 socket_id,
                                        const std::string& buffer) {
}

void MockPluginDelegate::TCPSocketDisconnect(uint32 socket_id) {
}

void MockPluginDelegate::RegisterTCPSocket(PPB_TCPSocket_Private_Impl* socket,
                                           uint32 socket_id) {
}

uint32 MockPluginDelegate::UDPSocketCreate() {
  return 0;
}

void MockPluginDelegate::UDPSocketSetBoolSocketFeature(
    PPB_UDPSocket_Private_Impl* socket,
    uint32 socket_id,
    int32_t name,
    bool value) {
}

void MockPluginDelegate::UDPSocketBind(PPB_UDPSocket_Private_Impl* socket,
                                       uint32 socket_id,
                                       const PP_NetAddress_Private& addr) {
}

void MockPluginDelegate::UDPSocketRecvFrom(uint32 socket_id,
                                           int32_t num_bytes) {
}

void MockPluginDelegate::UDPSocketSendTo(uint32 socket_id,
                                         const std::string& buffer,
                                         const PP_NetAddress_Private& addr) {
}

void MockPluginDelegate::UDPSocketClose(uint32 socket_id) {
}

void MockPluginDelegate::TCPServerSocketListen(
    PP_Resource socket_resource,
    const PP_NetAddress_Private& addr,
    int32_t backlog) {
}

void MockPluginDelegate::TCPServerSocketAccept(uint32 server_socket_id) {
}

void MockPluginDelegate::TCPServerSocketStopListening(
    PP_Resource socket_resource,
    uint32 socket_id) {
}

void MockPluginDelegate::RegisterHostResolver(
    ::ppapi::PPB_HostResolver_Shared* host_resolver,
    uint32 host_resolver_id) {
}

void MockPluginDelegate::HostResolverResolve(
    uint32 host_resolver_id,
    const ::ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint* hint) {
}

void MockPluginDelegate::UnregisterHostResolver(uint32 host_resolver_id) {
}

bool MockPluginDelegate::AddNetworkListObserver(
    webkit_glue::NetworkListObserver* observer) {
  return false;
}

void MockPluginDelegate::RemoveNetworkListObserver(
    webkit_glue::NetworkListObserver* observer) {
}

bool MockPluginDelegate::X509CertificateParseDER(
    const std::vector<char>& der,
    ::ppapi::PPB_X509Certificate_Fields* fields) {
  return false;
}

FullscreenContainer* MockPluginDelegate::CreateFullscreenContainer(
    PluginInstance* instance) {
  return NULL;
}

gfx::Size MockPluginDelegate::GetScreenSize() {
  return gfx::Size(1024, 768);
}

std::string MockPluginDelegate::GetDefaultEncoding() {
  return "iso-8859-1";
}

void MockPluginDelegate::ZoomLimitsChanged(double minimum_factor,
                                           double maximum_factor) {
}

void MockPluginDelegate::DidStartLoading() {
}

void MockPluginDelegate::DidStopLoading() {
}

void MockPluginDelegate::SetContentRestriction(int restrictions) {
}

void MockPluginDelegate::SaveURLAs(const GURL& url) {
}

base::SharedMemory* MockPluginDelegate::CreateAnonymousSharedMemory(
    size_t size) {
  return NULL;
}

::ppapi::Preferences MockPluginDelegate::GetPreferences() {
  return ::ppapi::Preferences();
}

bool MockPluginDelegate::LockMouse(PluginInstance* instance) {
  return false;
}

void MockPluginDelegate::UnlockMouse(PluginInstance* instance) {
}

bool MockPluginDelegate::IsMouseLocked(PluginInstance* instance) {
  return false;
}

void MockPluginDelegate::DidChangeCursor(PluginInstance* instance,
                                         const WebKit::WebCursorInfo& cursor) {
}

void MockPluginDelegate::DidReceiveMouseEvent(PluginInstance* instance) {
}

void MockPluginDelegate::SampleGamepads(WebKit::WebGamepads* data) {
  data->length = 0;
}

bool MockPluginDelegate::IsInFullscreenMode() {
  return false;
}

bool MockPluginDelegate::IsPageVisible() const {
  return true;
}

int MockPluginDelegate::EnumerateDevices(
    PP_DeviceType_Dev type,
    const EnumerateDevicesCallback& callback) {
  return -1;
}

void MockPluginDelegate::StopEnumerateDevices(int request_id) {
}

}  // namespace ppapi
}  // namespace webkit
