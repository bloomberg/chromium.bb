// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_MOCK_PLUGIN_DELEGATE_H_
#define WEBKIT_PLUGINS_PPAPI_MOCK_PLUGIN_DELEGATE_H_

#include "webkit/plugins/ppapi/plugin_delegate.h"

struct PP_NetAddress_Private;

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
  virtual void PluginCrashed(PluginInstance* instance);
  virtual void InstanceCreated(PluginInstance* instance);
  virtual void InstanceDeleted(PluginInstance* instance);
  virtual SkBitmap* GetSadPluginBitmap();
  virtual PlatformImage2D* CreateImage2D(int width, int height);
  virtual PlatformContext3D* CreateContext3D();
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id);
  virtual PlatformVideoCapture* CreateVideoCapture(
      media::VideoCapture::EventHandler* handler);
  virtual PlatformAudio* CreateAudio(uint32_t sample_rate,
                                     uint32_t sample_count,
                                     PlatformAudio::Client* client);
  virtual PpapiBroker* ConnectToPpapiBroker(PPB_Broker_Impl* client);
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result);
  virtual void SelectedFindResultChanged(int identifier, int index);
  virtual bool RunFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback);
  virtual bool AsyncOpenFileSystemURL(const GURL& path,
                                      int flags,
                                      const AsyncOpenFileCallback& callback);
  virtual bool OpenFileSystem(
      const GURL& url,
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
  virtual base::PlatformFileError OpenFile(const PepperFilePath& path,
                                           int flags,
                                           base::PlatformFile* file);
  virtual base::PlatformFileError RenameFile(const PepperFilePath& from_path,
                                             const PepperFilePath& to_path);
  virtual base::PlatformFileError DeleteFileOrDir(const PepperFilePath& path,
                                                  bool recursive);
  virtual base::PlatformFileError CreateDir(const PepperFilePath& path);
  virtual base::PlatformFileError QueryFile(const PepperFilePath& path,
                                            base::PlatformFileInfo* info);
  virtual base::PlatformFileError GetDirContents(const PepperFilePath& path,
                                                 DirContents* contents);
  virtual void SyncGetFileSystemPlatformPath(const GURL& url,
                                             FilePath* platform_path);
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy();
  virtual int32_t ConnectTcp(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const char* host,
      uint16_t port);
  virtual int32_t ConnectTcpAddress(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const PP_NetAddress_Private* addr);
  virtual int32_t ShowContextMenu(
      PluginInstance* instance,
      webkit::ppapi::PPB_Flash_Menu_Impl* menu,
      const gfx::Point& position);
  virtual FullscreenContainer* CreateFullscreenContainer(
      PluginInstance* instance);
  virtual gfx::Size GetScreenSize();
  virtual std::string GetDefaultEncoding();
  virtual void ZoomLimitsChanged(double minimum_factor,
                                 double maximum_factor);
  virtual std::string ResolveProxy(const GURL& url);
  virtual void DidStartLoading();
  virtual void DidStopLoading();
  virtual void SetContentRestriction(int restrictions);
  virtual void SaveURLAs(const GURL& url);
  virtual webkit_glue::P2PTransport* CreateP2PTransport();
  virtual double GetLocalTimeZoneOffset(base::Time t);
  virtual std::string GetFlashCommandLineArgs();
  virtual base::SharedMemory* CreateAnonymousSharedMemory(uint32_t size);
  virtual ::ppapi::Preferences GetPreferences();
  virtual void LockMouse(PluginInstance* instance);
  virtual void UnlockMouse(PluginInstance* instance);
  virtual void DidChangeCursor(PluginInstance* instance,
                               const WebKit::WebCursorInfo& cursor);
  virtual void DidReceiveMouseEvent(PluginInstance* instance);
  virtual bool IsInFullscreenMode();
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_MOCK_PLUGIN_DELEGATE_H_
