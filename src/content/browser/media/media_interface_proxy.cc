// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_proxy.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_sandbox_type.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "content/public/browser/browser_context.h"
#include "content/public/browser/provision_fetcher_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "content/browser/media/cdm_storage_impl.h"
#include "content/browser/media/key_system_support_impl.h"
#include "content/public/common/cdm_info.h"
#include "media/base/key_system_names.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MACOSX)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_ANDROID)
#include "content/browser/media/android/media_player_renderer.h"
#include "content/browser/media/flinging_renderer.h"
#include "media/mojo/services/mojo_renderer_service.h"  // nogncheck
#endif

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// How long an instance of the CDM service is allowed to sit idle before we
// disconnect and effectively kill it.
constexpr auto kCdmServiceIdleTimeout = base::TimeDelta::FromSeconds(5);

// The CDM name will be displayed as the process name in the Task Manager.
// Put a length limit and restrict to ASCII. Empty name is allowed, in which
// case the process name will be "media::mojom::CdmService".
bool IsValidCdmDisplayName(const std::string& cdm_name) {
  constexpr size_t kMaxCdmNameSize = 256;
  return cdm_name.size() <= kMaxCdmNameSize && base::IsStringASCII(cdm_name);
}

// A map hosts all media::mojom::CdmService remotes, each of which corresponds
// to one CDM process. There should be only one instance of this class stored in
// base::SequenceLocalStorageSlot. See below.
class CdmServiceMap {
 public:
  CdmServiceMap() = default;

  ~CdmServiceMap() {
    DVLOG(1) << __func__ << ": max_remote_count_=" << max_remote_count_;
    UMA_HISTOGRAM_COUNTS_100("Media.EME.MaxCdmProcessCount", max_remote_count_);
  }

  // Gets or creates a media::mojom::CdmService remote. The returned remote
  // might not be bound, e.g. if it's newly created.
  auto& GetOrCreateRemote(const base::Token& guid) {
    auto& remote = remotes_[guid];
    max_remote_count_ = std::max(max_remote_count_, remotes_.size());
    return remote;
  }

  void EraseRemote(const base::Token& guid) {
    DCHECK(remotes_.count(guid));
    remotes_.erase(guid);
  }

 private:
  std::map<base::Token, mojo::Remote<media::mojom::CdmService>> remotes_;
  size_t max_remote_count_ = 0;
};

CdmServiceMap& GetCdmServiceMap() {
  // NOTE: Sequence-local storage is used to limit the lifetime of the Remote
  // objects to that of the UI-thread sequence. This ensures the Remotes are
  // destroyed when the task environment is torn down and reinitialized, e.g.,
  // between unit tests.
  static base::NoDestructor<base::SequenceLocalStorageSlot<CdmServiceMap>> slot;
  return slot->GetOrCreateValue();
}

// Erases the CDM service instance for the CDM identified by |guid|.
void EraseCdmServiceForGuid(const base::Token& guid) {
  GetCdmServiceMap().EraseRemote(guid);
}

// Gets an instance of the CDM service for the CDM identified by |guid|.
// Instances are started lazily as needed.
media::mojom::CdmService& GetCdmServiceForGuid(const base::Token& guid,
                                               const std::string& cdm_name) {
  auto& remote = GetCdmServiceMap().GetOrCreateRemote(guid);
  if (!remote) {
    ServiceProcessHost::Launch(
        remote.BindNewPipeAndPassReceiver(),
        ServiceProcessHost::Options()
            .WithDisplayName(cdm_name)
            .Pass());
    remote.set_disconnect_handler(
        base::BindOnce(&EraseCdmServiceForGuid, guid));
    remote.set_idle_handler(kCdmServiceIdleTimeout,
                            base::BindRepeating(EraseCdmServiceForGuid, guid));
  }

  return *remote.get();
}
#endif  // ENABLE_LIBRARY_CDMS

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) && defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
// TODO(xhwang): Move this to a common place.
const base::FilePath::CharType kSignatureFileExtension[] =
    FILE_PATH_LITERAL(".sig");

// Returns the signature file path given the |file_path|. This function should
// only be used when the signature file and the file are located in the same
// directory, which is the case for the CDM and CDM adapter.
base::FilePath GetSigFilePath(const base::FilePath& file_path) {
  return file_path.AddExtension(kSignatureFileExtension);
}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

class SeatbeltExtensionTokenProviderImpl
    : public media::mojom::SeatbeltExtensionTokenProvider {
 public:
  explicit SeatbeltExtensionTokenProviderImpl(const base::FilePath& cdm_path)
      : cdm_path_(cdm_path) {}
  void GetTokens(GetTokensCallback callback) final {
    std::vector<sandbox::SeatbeltExtensionToken> tokens;

    // Allow the CDM to be loaded in the CDM service process.
    auto cdm_token = sandbox::SeatbeltExtension::Issue(
        sandbox::SeatbeltExtension::FILE_READ, cdm_path_.value());
    if (cdm_token) {
      tokens.push_back(std::move(*cdm_token));
    } else {
      std::move(callback).Run({});
      return;
    }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
    // If CDM host verification is enabled, also allow to open the CDM signature
    // file.
    auto cdm_sig_token =
        sandbox::SeatbeltExtension::Issue(sandbox::SeatbeltExtension::FILE_READ,
                                          GetSigFilePath(cdm_path_).value());
    if (cdm_sig_token) {
      tokens.push_back(std::move(*cdm_sig_token));
    } else {
      std::move(callback).Run({});
      return;
    }
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

    std::move(callback).Run(std::move(tokens));
  }

 private:
  base::FilePath cdm_path_;

  DISALLOW_COPY_AND_ASSIGN(SeatbeltExtensionTokenProviderImpl);
};

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) && defined(OS_MACOSX)

// The amount of time to allow the secondary Media Service instance to idle
// before tearing it down. Only used if the Content embedder defines how to
// launch a secondary Media Service instance.
constexpr base::TimeDelta kSecondaryInstanceIdleTimeout =
    base::TimeDelta::FromSeconds(5);

void MaybeLaunchSecondaryMediaService(
    mojo::Remote<media::mojom::MediaService>* remote) {
  *remote = GetContentClient()->browser()->RunSecondaryMediaService();
  if (*remote) {
    // If the embedder provides a secondary Media Service instance, it may run
    // out-of-process. Make sure we reset on disconnect to allow restart of
    // crashed instances, and reset on idle to allow for release of resources
    // when the service instance goes unused for a while.
    remote->reset_on_disconnect();
    remote->reset_on_idle_timeout(kSecondaryInstanceIdleTimeout);
  } else {
    // The embedder doesn't provide a secondary Media Service instance. Bind
    // permanently to a disconnected pipe which discards all calls.
    ignore_result(remote->BindNewPipeAndPassReceiver());
  }
}

// Returns a remote handle to the secondary Media Service instance, if the
// Content embedder defines how to create one. If not, this returns a non-null
// but non-functioning MediaService reference which discards all calls.
media::mojom::MediaService& GetSecondaryMediaService() {
  static base::NoDestructor<mojo::Remote<media::mojom::MediaService>> remote;
  if (!*remote)
    MaybeLaunchSecondaryMediaService(remote.get());
  return *remote->get();
}

class FrameInterfaceFactoryImpl : public media::mojom::FrameInterfaceFactory {
 public:
  FrameInterfaceFactoryImpl(RenderFrameHost* rfh,
                            const base::Token& cdm_guid,
                            const std::string& cdm_file_system_id)
      : render_frame_host_(rfh),
        cdm_guid_(cdm_guid),
        cdm_file_system_id_(cdm_file_system_id) {
  }

  void CreateProvisionFetcher(
      mojo::PendingReceiver<media::mojom::ProvisionFetcher> receiver) override {
#if BUILDFLAG(ENABLE_MOJO_CDM)
    ProvisionFetcherImpl::Create(
        BrowserContext::GetDefaultStoragePartition(
            render_frame_host_->GetProcess()->GetBrowserContext())
            ->GetURLLoaderFactoryForBrowserProcess(),
        std::move(receiver));
#endif
  }

  void CreateCdmStorage(
      mojo::PendingReceiver<media::mojom::CdmStorage> receiver) override {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    // Only provide CdmStorageImpl when we have a valid |cdm_file_system_id|,
    // which is currently only set for the CdmService (not the MediaService).
    if (cdm_file_system_id_.empty())
      return;

    CdmStorageImpl::Create(render_frame_host_, cdm_file_system_id_,
                           std::move(receiver));
#endif
  }

  void BindEmbedderReceiver(mojo::GenericPendingReceiver receiver) override {
    GetContentClient()->browser()->BindMediaServiceReceiver(
        render_frame_host_, std::move(receiver));
  }

 private:
  RenderFrameHost* const render_frame_host_;
  const base::Token cdm_guid_;
  const std::string cdm_file_system_id_;
};

}  // namespace

MediaInterfaceProxy::MediaInterfaceProxy(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
    base::OnceClosure error_handler)
    : render_frame_host_(render_frame_host),
      receiver_(this, std::move(receiver)) {
  DVLOG(1) << __func__;
  DCHECK(render_frame_host_);
  DCHECK(!error_handler.is_null());

  auto frame_factory_getter =
      base::BindRepeating(&MediaInterfaceProxy::GetFrameServices,
                          base::Unretained(this), base::Token(), std::string());
  media_interface_factory_ptr_ = std::make_unique<MediaInterfaceFactoryHolder>(
      base::BindRepeating(&GetMediaService), frame_factory_getter);
  secondary_interface_factory_ = std::make_unique<MediaInterfaceFactoryHolder>(
      base::BindRepeating(&GetSecondaryMediaService), frame_factory_getter);

  receiver_.set_disconnect_handler(std::move(error_handler));

  // |cdm_factory_map_| will be lazily connected in GetCdmFactory().
}

MediaInterfaceProxy::~MediaInterfaceProxy() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaInterfaceProxy::CreateAudioDecoder(
    mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateAudioDecoder(std::move(receiver));
}

void MediaInterfaceProxy::CreateVideoDecoder(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateVideoDecoder(std::move(receiver));
}

void MediaInterfaceProxy::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateDefaultRenderer(audio_device_id, std::move(receiver));
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void MediaInterfaceProxy::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // CastRenderer is always hosted in the secondary Media Service instance.
  // This may not be running in some test environments (e.g.
  // content_browsertests) even though renderers may still request to bind it.
  InterfaceFactory* factory = secondary_interface_factory_->Get();
  if (factory)
    factory->CreateCastRenderer(overlay_plane_id, std::move(receiver));
}
#endif

#if defined(OS_ANDROID)
void MediaInterfaceProxy::CreateFlingingRenderer(
    const std::string& presentation_id,
    mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
        client_extension,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<FlingingRenderer> flinging_renderer =
      FlingingRenderer::Create(render_frame_host_, presentation_id,
                               std::move(client_extension));

  if (!flinging_renderer)
    return;

  media::MojoRendererService::Create(nullptr, std::move(flinging_renderer),
                                     std::move(receiver));
}

void MediaInterfaceProxy::CreateMediaPlayerRenderer(
    mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media::MojoRendererService::Create(
      nullptr,
      std::make_unique<MediaPlayerRenderer>(
          render_frame_host_->GetProcess()->GetID(),
          render_frame_host_->GetRoutingID(),
          static_cast<RenderFrameHostImpl*>(render_frame_host_)
              ->delegate()
              ->GetAsWebContents(),
          std::move(renderer_extension_receiver),
          std::move(client_extension_remote)),
      std::move(receiver));
}
#endif

void MediaInterfaceProxy::CreateCdm(
    const std::string& key_system,
    mojo::PendingReceiver<media::mojom::ContentDecryptionModule> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  auto* factory = GetCdmFactory(key_system);
#elif BUILDFLAG(ENABLE_CAST_RENDERER)
  // CDM service lives together with renderer service if cast renderer is
  // enabled, because cast renderer creates its own audio/video decoder. Note
  // that in content_browsertests (and Content Shell in general) we don't have
  // an a cast renderer and this interface will be unbound.
  auto* factory = secondary_interface_factory_->Get();
#else
  // CDM service lives together with audio/video decoder service.
  auto* factory = media_interface_factory_ptr_->Get();
#endif

  if (factory)
    factory->CreateCdm(key_system, std::move(receiver));
}

mojo::PendingRemote<media::mojom::FrameInterfaceFactory>
MediaInterfaceProxy::GetFrameServices(const base::Token& cdm_guid,
                                      const std::string& cdm_file_system_id) {
  mojo::PendingRemote<media::mojom::FrameInterfaceFactory> factory;
  frame_factories_.Add(std::make_unique<FrameInterfaceFactoryImpl>(
                           render_frame_host_, cdm_guid, cdm_file_system_id),
                       factory.InitWithNewPipeAndPassReceiver());
  return factory;
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

media::mojom::CdmFactory* MediaInterfaceProxy::GetCdmFactory(
    const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<CdmInfo> cdm_info =
      KeySystemSupportImpl::GetCdmInfoForKeySystem(key_system);
  if (!cdm_info) {
    NOTREACHED() << "No valid CdmInfo for " << key_system;
    return nullptr;
  }
  if (cdm_info->path.empty()) {
    NOTREACHED() << "CDM path for " << key_system << " is empty.";
    return nullptr;
  }
  if (!CdmStorageImpl::IsValidCdmFileSystemId(cdm_info->file_system_id)) {
    NOTREACHED() << "Invalid file system ID " << cdm_info->file_system_id;
    return nullptr;
  }
  if (!IsValidCdmDisplayName(cdm_info->name)) {
    NOTREACHED() << "Invalid CDM display name " << cdm_info->name;
    return nullptr;
  }

  auto& cdm_guid = cdm_info->guid;

  auto found = cdm_factory_map_.find(cdm_guid);
  if (found != cdm_factory_map_.end())
    return found->second.get();

  return ConnectToCdmService(cdm_guid, cdm_info->path, cdm_info->file_system_id,
                             cdm_info->name);
}

media::mojom::CdmFactory* MediaInterfaceProxy::ConnectToCdmService(
    const base::Token& cdm_guid,
    const base::FilePath& cdm_path,
    const std::string& cdm_file_system_id,
    const std::string& cdm_name) {
  DVLOG(1) << __func__ << ": cdm_guid = " << cdm_guid.ToString();

  DCHECK(!cdm_factory_map_.count(cdm_guid));

  media::mojom::CdmService& cdm_service =
      GetCdmServiceForGuid(cdm_guid, cdm_name);

#if defined(OS_MACOSX)
  // LoadCdm() should always be called before CreateInterfaceFactory().
  mojo::PendingRemote<media::mojom::SeatbeltExtensionTokenProvider>
      token_provider_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SeatbeltExtensionTokenProviderImpl>(cdm_path),
      token_provider_remote.InitWithNewPipeAndPassReceiver());

  cdm_service.LoadCdm(cdm_path, std::move(token_provider_remote));
#else
  cdm_service.LoadCdm(cdm_path);
#endif  // defined(OS_MACOSX)

  mojo::Remote<media::mojom::CdmFactory> cdm_factory_remote;
  cdm_service.CreateCdmFactory(cdm_factory_remote.BindNewPipeAndPassReceiver(),
                               GetFrameServices(cdm_guid, cdm_file_system_id));
  cdm_factory_remote.set_disconnect_handler(
      base::BindOnce(&MediaInterfaceProxy::OnCdmServiceConnectionError,
                     base::Unretained(this), cdm_guid));

  auto* cdm_factory = cdm_factory_remote.get();
  cdm_factory_map_.emplace(cdm_guid, std::move(cdm_factory_remote));
  return cdm_factory;
}

void MediaInterfaceProxy::OnCdmServiceConnectionError(
    const base::Token& cdm_guid) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(cdm_factory_map_.count(cdm_guid));
  cdm_factory_map_.erase(cdm_guid);
}

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace content
