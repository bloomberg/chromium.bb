// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/media_interface_factory_holder.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/common/cdm_info.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/services/media_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

#if defined(OS_WIN)
#include "base/sequenced_task_runner.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#endif

namespace content {

class RenderFrameHost;

// This implements the media::mojom::InterfaceFactory interface for a
// RenderFrameHostImpl to help create remote media components in different
// processes.
class MediaInterfaceProxy final
    : public RenderDocumentHostUserData<MediaInterfaceProxy>,
      public media::mojom::InterfaceFactory {
 public:
  ~MediaInterfaceProxy() final;

  void Bind(mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) final;
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif
#if defined(OS_ANDROID)
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_request) final;
#endif  // defined(OS_ANDROID)
#if defined(OS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver) final;
#endif  // defined(OS_WIN)
  void CreateCdm(const std::string& key_system,
                 const media::CdmConfig& cdm_config,
                 CreateCdmCallback create_cdm_cb) final;

 private:
  friend class RenderDocumentHostUserData<MediaInterfaceProxy>;
  explicit MediaInterfaceProxy(RenderFrameHost* rfh);
  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();

  // Gets services provided by the browser (at RenderFrameHost level) to the
  // mojo media (or CDM) service running remotely. |cdm_file_system_id| is
  // used to register the appropriate CdmStorage interface needed by the CDM.
  // If |cdm_file_system_id| is empty, CdmStorage interface won't be available.
  mojo::PendingRemote<media::mojom::FrameInterfaceFactory> GetFrameServices(
      const std::string& cdm_file_system_id = "");

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Gets a CdmFactory pointer for |key_system|. Returns null if unexpected
  // error happened.
  media::mojom::CdmFactory* GetCdmFactory(const std::string& key_system);

  // Connects to the CDM service associated with |cdm_guid|, adds the new
  // CdmFactoryPtr to the |cdm_factory_map_|, and returns the newly created
  // CdmFactory pointer. Returns nullptr if unexpected error happened.
  // |cdm_path| will be used to preload the CDM, if necessary.
  // |cdm_file_system_id| is used when creating the matching storage interface.
  // |cdm_name| is used as the display name of the CDM (utility) process.
  media::mojom::CdmFactory* ConnectToCdmService(const base::Token& cdm_guid,
                                                const CdmInfo& cdm_info);

  // Callback for connection error from the CdmFactoryPtr in the
  // |cdm_factory_map_| associated with |cdm_guid|.
  void OnCdmServiceConnectionError(const base::Token& cdm_guid);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Callback for for Chrome OS CDM creation to facilitate falling back to the
  // library CDM if the daemon is unavailable or other settings prevent usage of
  // it.
  void OnChromeOsCdmCreated(
      const std::string& key_system,
      const media::CdmConfig& cdm_config,
      CreateCdmCallback callback,
      mojo::PendingRemote<media::mojom::ContentDecryptionModule> receiver,
      media::mojom::CdmContextPtr cdm_context,
      const std::string& error_message);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_WIN)
  // Gets the InterfaceFactory from MediaFoundationService. May return null if
  // MediaFoundationService cannot be used or connection failed.
  InterfaceFactory* GetMediaFoundationServiceInterfaceFactory(
      const base::FilePath& cdm_path);

  void ConnectToMediaFoundationService(const base::FilePath& cdm_path);
  bool ShouldUseMediaFoundationServiceForCdm(
      const std::string& key_system,
      const media::CdmConfig& cdm_config);

  mojo::Remote<media::mojom::InterfaceFactory> mf_interface_factory_remote_;
#endif  // defined(OS_WIN)

  // Safe to hold a raw pointer since |this| is owned by RenderFrameHostImpl.
  RenderFrameHost* const render_frame_host_;

  mojo::UniqueReceiverSet<media::mojom::FrameInterfaceFactory> frame_factories_;

  // InterfacePtr to the remote InterfaceFactory implementation in the Media
  // Service hosted in the process specified by the "mojo_media_host" gn
  // argument. Available options are browser, GPU and utility processes.
  std::unique_ptr<MediaInterfaceFactoryHolder> media_interface_factory_ptr_;

  // An interface factory bound to a secondary instance of the Media Service,
  // initialized only if the embedder provides an implementation of
  // |ContentBrowserClient::RunSecondaryMediaService()|. This is used to bind to
  // CDM interfaces as well as Cast-specific Renderer interfaces when available.
  std::unique_ptr<MediaInterfaceFactoryHolder> secondary_interface_factory_;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // CDM GUID to CDM InterfaceFactory Remotes mapping, where the
  // InterfaceFactory instances live in the standalone CdmService instances.
  // These map entries effectively own the corresponding CDM processes.
  // Only using the GUID to identify the CdmFactory is sufficient because the
  // BrowserContext and Site URL should never change.
  std::map<base::Token, mojo::Remote<media::mojom::CdmFactory>>
      cdm_factory_map_;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  base::ThreadChecker thread_checker_;

  // Receivers for incoming interface requests from the the RenderFrameImpl.
  mojo::ReceiverSet<media::mojom::InterfaceFactory> receivers_;

  base::WeakPtrFactory<MediaInterfaceProxy> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
