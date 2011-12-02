// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/mock_plugin_delegate.h"

#include "base/message_loop_proxy.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
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

void MockPluginDelegate::PluginCrashed(PluginInstance* instance) {
}

void MockPluginDelegate::InstanceCreated(PluginInstance* instance) {
}

void MockPluginDelegate::InstanceDeleted(PluginInstance* instance) {
}

SkBitmap* MockPluginDelegate::GetSadPluginBitmap() {
  return NULL;
}

MockPluginDelegate::PlatformImage2D* MockPluginDelegate::CreateImage2D(
    int width,
    int height) {
  return NULL;
}

MockPluginDelegate::PlatformContext3D* MockPluginDelegate::CreateContext3D() {
  return NULL;
}

MockPluginDelegate::PlatformVideoDecoder*
MockPluginDelegate::CreateVideoDecoder(
    media::VideoDecodeAccelerator::Client* client,
    int32 command_buffer_route_id) {
  return NULL;
}

MockPluginDelegate::PlatformVideoCapture*
MockPluginDelegate::CreateVideoCapture(
    media::VideoCapture::EventHandler* handler){
  return NULL;
}

MockPluginDelegate::PlatformAudio* MockPluginDelegate::CreateAudio(
    uint32_t sample_rate,
    uint32_t sample_count,
    PlatformAudioCommonClient* client) {
  return NULL;
}

MockPluginDelegate::PlatformAudioInput* MockPluginDelegate::CreateAudioInput(
    uint32_t sample_rate,
    uint32_t sample_count,
    PlatformAudioCommonClient* client) {
  return NULL;
}

MockPluginDelegate::PpapiBroker* MockPluginDelegate::ConnectToPpapiBroker(
    PPB_Broker_Impl* client) {
  return NULL;
}

void MockPluginDelegate::NumberOfFindResultsChanged(int identifier,
                                                    int total,
                                                    bool final_result) {
}

void MockPluginDelegate::SelectedFindResultChanged(int identifier, int index) {
}

bool MockPluginDelegate::RunFileChooser(
    const WebKit::WebFileChooserParams& params,
    WebKit::WebFileChooserCompletion* chooser_completion) {
  return false;
}

bool MockPluginDelegate::AsyncOpenFile(const FilePath& path,
                                       int flags,
                                       const AsyncOpenFileCallback& callback) {
  return false;
}

bool MockPluginDelegate::AsyncOpenFileSystemURL(
    const GURL& path, int flags, const AsyncOpenFileCallback& callback) {
  return false;
}

bool MockPluginDelegate::OpenFileSystem(
    const GURL& url,
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

base::PlatformFileError MockPluginDelegate::OpenFile(
    const PepperFilePath& path,
    int flags,
    base::PlatformFile* file) {
  return base::PLATFORM_FILE_ERROR_FAILED;
}

base::PlatformFileError MockPluginDelegate::RenameFile(
    const PepperFilePath& from_path,
    const PepperFilePath& to_path) {
  return base::PLATFORM_FILE_ERROR_FAILED;
}

base::PlatformFileError MockPluginDelegate::DeleteFileOrDir(
    const PepperFilePath& path,
    bool recursive) {
  return base::PLATFORM_FILE_ERROR_FAILED;
}

base::PlatformFileError MockPluginDelegate::CreateDir(
    const PepperFilePath& path) {
  return base::PLATFORM_FILE_ERROR_FAILED;
}

base::PlatformFileError MockPluginDelegate::QueryFile(
    const PepperFilePath& path,
    base::PlatformFileInfo* info) {
  return base::PLATFORM_FILE_ERROR_FAILED;
}

base::PlatformFileError MockPluginDelegate::GetDirContents(
    const PepperFilePath& path,
    DirContents* contents) {
  return base::PLATFORM_FILE_ERROR_FAILED;
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

int32_t MockPluginDelegate::ConnectTcp(
    webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
    const char* host,
    uint16_t port) {
  return PP_ERROR_FAILED;
}

int32_t MockPluginDelegate::ConnectTcpAddress(
    webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
    const PP_NetAddress_Private* addr) {
  return PP_ERROR_FAILED;
}

int32_t MockPluginDelegate::ShowContextMenu(
    PluginInstance* instance,
    webkit::ppapi::PPB_Flash_Menu_Impl* menu,
    const gfx::Point& position) {
  return PP_ERROR_FAILED;
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

std::string MockPluginDelegate::ResolveProxy(const GURL& url) {
  return std::string();
}

void MockPluginDelegate::DidStartLoading() {
}

void MockPluginDelegate::DidStopLoading() {
}

void MockPluginDelegate::SetContentRestriction(int restrictions) {
}

void MockPluginDelegate::SaveURLAs(const GURL& url) {
}

webkit_glue::P2PTransport* MockPluginDelegate::CreateP2PTransport() {
  return NULL;
}

double MockPluginDelegate::GetLocalTimeZoneOffset(base::Time t) {
  return 0.0;
}

std::string MockPluginDelegate::GetFlashCommandLineArgs() {
  return std::string();
}

base::SharedMemory* MockPluginDelegate::CreateAnonymousSharedMemory(
    uint32_t size) {
  return NULL;
}

::ppapi::Preferences MockPluginDelegate::GetPreferences() {
  return ::ppapi::Preferences();
}

void MockPluginDelegate::LockMouse(PluginInstance* instance) {
  instance->OnLockMouseACK(PP_ERROR_FAILED);
}

void MockPluginDelegate::UnlockMouse(PluginInstance* instance) {
}

void MockPluginDelegate::DidChangeCursor(PluginInstance* instance,
                                         const WebKit::WebCursorInfo& cursor) {
}

void MockPluginDelegate::DidReceiveMouseEvent(PluginInstance* instance) {
}

bool MockPluginDelegate::IsInFullscreenMode() {
  return false;
}

}  // namespace ppapi
}  // namespace webkit
