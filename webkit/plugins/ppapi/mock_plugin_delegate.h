// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_MOCK_PLUGIN_DELEGATE_H_
#define WEBKIT_PLUGINS_PPAPI_MOCK_PLUGIN_DELEGATE_H_

#include "webkit/plugins/ppapi/plugin_delegate.h"

struct PP_NetAddress_Private;
namespace ppapi { class PPB_X509Certificate_Fields; }

namespace webkit {
namespace ppapi {

class MockPluginDelegate : public PluginDelegate {
 public:
  MockPluginDelegate();
  virtual ~MockPluginDelegate();

  virtual void PluginFocusChanged(PluginInstance* instance, bool focused);
  virtual void PluginTextInputTypeChanged(PluginInstance* instance);
  virtual void PluginCaretPositionChanged(PluginInstance* instance);
  virtual void PluginRequestedCancelComposition(PluginInstance* instance);
  virtual void PluginSelectionChanged(PluginInstance* instance);
  virtual void SimulateImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  virtual void SimulateImeConfirmComposition(const string16& text);
  virtual void PluginCrashed(PluginInstance* instance);
  virtual void InstanceCreated(PluginInstance* instance);
  virtual void InstanceDeleted(PluginInstance* instance);
  virtual scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
      CreateResourceCreationAPI(PluginInstance* instance);
  virtual SkBitmap* GetSadPluginBitmap();
  virtual WebKit::WebPlugin* CreatePluginReplacement(const FilePath& file_path);
  virtual PlatformImage2D* CreateImage2D(int width, int height);
  virtual PlatformGraphics2D* GetGraphics2D(PluginInstance* instance,
                                            PP_Resource graphics_2d);
  virtual PlatformContext3D* CreateContext3D();
  virtual void ReparentContext(PlatformContext3D*);
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id);
  virtual PlatformVideoCapture* CreateVideoCapture(
      const std::string& device_id,
      PlatformVideoCaptureEventHandler* handler);
  virtual uint32_t GetAudioHardwareOutputSampleRate();
  virtual uint32_t GetAudioHardwareOutputBufferSize();
  virtual PlatformAudioOutput* CreateAudioOutput(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioOutputClient* client);
  virtual PlatformAudioInput* CreateAudioInput(
      const std::string& device_id,
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioInputClient* client);
  virtual Broker* ConnectToBroker(PPB_Broker_Impl* client);
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result);
  virtual void SelectedFindResultChanged(int identifier, int index);
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback);
  virtual bool AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      const AsyncOpenFileSystemURLCallback& callback);
  virtual bool OpenFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      long long size,
      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool MakeDirectory(
      const GURL& path,
      bool recursive,
      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Query(const GURL& path,
                     fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Touch(const GURL& path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool SetLength(const GURL& path,
                         int64_t length,
                         fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Delete(const GURL& path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool Rename(const GURL& file_path,
                      const GURL& new_file_path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual bool ReadDirectory(
      const GURL& directory_path,
      fileapi::FileSystemCallbackDispatcher* dispatcher);
  virtual void QueryAvailableSpace(const GURL& origin,
                                   quota::StorageType type,
                                   const AvailableSpaceCallback& callback);
  virtual void WillUpdateFile(const GURL& file_path);
  virtual void DidUpdateFile(const GURL& file_path, int64_t delta);
  virtual void SyncGetFileSystemPlatformPath(const GURL& url,
                                             FilePath* platform_path);
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy();
  virtual uint32 TCPSocketCreate();
  virtual void TCPSocketConnect(PPB_TCPSocket_Private_Impl* socket,
                                uint32 socket_id,
                                const std::string& host,
                                uint16_t port);
  virtual void TCPSocketConnectWithNetAddress(
      PPB_TCPSocket_Private_Impl* socket,
      uint32 socket_id,
      const PP_NetAddress_Private& addr);
  virtual void TCPSocketSSLHandshake(
      uint32 socket_id,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs);
  virtual void TCPSocketRead(uint32 socket_id, int32_t bytes_to_read);
  virtual void TCPSocketWrite(uint32 socket_id, const std::string& buffer);
  virtual void TCPSocketDisconnect(uint32 socket_id);
  virtual void RegisterTCPSocket(PPB_TCPSocket_Private_Impl* socket,
                                 uint32 socket_id);
  virtual uint32 UDPSocketCreate();
  virtual void UDPSocketSetBoolSocketFeature(PPB_UDPSocket_Private_Impl* socket,
                                             uint32 socket_id,
                                             int32_t name,
                                             bool value);
  virtual void UDPSocketBind(PPB_UDPSocket_Private_Impl* socket,
                             uint32 socket_id,
                             const PP_NetAddress_Private& addr);
  virtual void UDPSocketRecvFrom(uint32 socket_id, int32_t num_bytes);
  virtual void UDPSocketSendTo(uint32 socket_id,
                               const std::string& buffer,
                               const PP_NetAddress_Private& addr);
  virtual void UDPSocketClose(uint32 socket_id);
  virtual void TCPServerSocketListen(PP_Resource socket_resource,
                                     const PP_NetAddress_Private& addr,
                                     int32_t backlog);
  virtual void TCPServerSocketAccept(uint32 server_socket_id);
  virtual void TCPServerSocketStopListening(PP_Resource socket_resource,
                                            uint32 socket_id);
  virtual void RegisterHostResolver(
      ::ppapi::PPB_HostResolver_Shared* host_resolver,
      uint32 host_resolver_id);
  virtual void HostResolverResolve(
      uint32 host_resolver_id,
      const ::ppapi::HostPortPair& host_port,
      const PP_HostResolver_Private_Hint* hint);
  virtual void UnregisterHostResolver(uint32 host_resolver_id);
  // Add/remove a network list observer.
  virtual bool AddNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) OVERRIDE;
  virtual void RemoveNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) OVERRIDE;
  virtual bool X509CertificateParseDER(
      const std::vector<char>& der,
      ::ppapi::PPB_X509Certificate_Fields* fields);
  virtual FullscreenContainer* CreateFullscreenContainer(
      PluginInstance* instance);
  virtual gfx::Size GetScreenSize();
  virtual std::string GetDefaultEncoding();
  virtual void ZoomLimitsChanged(double minimum_factor,
                                 double maximum_factor);
  virtual void DidStartLoading();
  virtual void DidStopLoading();
  virtual void SetContentRestriction(int restrictions);
  virtual void SaveURLAs(const GURL& url);
  virtual base::SharedMemory* CreateAnonymousSharedMemory(size_t size);
  virtual ::ppapi::Preferences GetPreferences();
  virtual bool LockMouse(PluginInstance* instance);
  virtual void UnlockMouse(PluginInstance* instance);
  virtual bool IsMouseLocked(PluginInstance* instance);
  virtual void DidChangeCursor(PluginInstance* instance,
                               const WebKit::WebCursorInfo& cursor);
  virtual void DidReceiveMouseEvent(PluginInstance* instance);
  virtual void SampleGamepads(WebKit::WebGamepads* data) OVERRIDE;
  virtual bool IsInFullscreenMode();
  virtual bool IsPageVisible() const;
  virtual int EnumerateDevices(PP_DeviceType_Dev type,
                               const EnumerateDevicesCallback& callback);
  virtual void StopEnumerateDevices(int request_id);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_MOCK_PLUGIN_DELEGATE_H_
