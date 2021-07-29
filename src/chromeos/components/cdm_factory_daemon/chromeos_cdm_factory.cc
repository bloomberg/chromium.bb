// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "chromeos/components/cdm_factory_daemon/cdm_storage_adapter.h"
#include "chromeos/components/cdm_factory_daemon/content_decryption_module_adapter.h"
#include "chromeos/components/cdm_factory_daemon/mojom/content_decryption_module.mojom.h"
#include "media/base/content_decryption_module.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chromeos {

namespace {

// This holds the global singleton Mojo connection to the browser process.
mojo::Remote<cdm::mojom::CdmFactoryDaemon>& GetCdmFactoryDaemonRemote() {
  static base::NoDestructor<mojo::Remote<cdm::mojom::CdmFactoryDaemon>> remote;
  return *remote;
}

// This holds the task runner we are bound to.
scoped_refptr<base::SequencedTaskRunner>& GetFactoryTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>> runner;
  return *runner;
}

void CreateFactoryOnTaskRunner(
    const std::string& key_system,
    cdm::mojom::CdmFactoryDaemon::CreateFactoryCallback callback) {
  GetCdmFactoryDaemonRemote()->CreateFactory(key_system, std::move(callback));
}

void CreateFactoryCallback(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    cdm::mojom::CdmFactoryDaemon::CreateFactoryCallback callback,
    mojo::PendingRemote<cdm::mojom::CdmFactory> remote_factory) {
  runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback),
                                             std::move(remote_factory)));
}

void GetOutputProtectionOnTaskRunner(
    mojo::PendingReceiver<cdm::mojom::OutputProtection> output_protection) {
  GetCdmFactoryDaemonRemote()->GetOutputProtection(
      std::move(output_protection));
}

}  // namespace

ChromeOsCdmFactory::ChromeOsCdmFactory(
    media::mojom::FrameInterfaceFactory* frame_interfaces)
    : frame_interfaces_(frame_interfaces) {
  DCHECK(frame_interfaces_);
  DVLOG(1) << "Creating the ChromeOsCdmFactory";
}

ChromeOsCdmFactory::~ChromeOsCdmFactory() = default;

mojo::PendingReceiver<cdm::mojom::CdmFactoryDaemon>
ChromeOsCdmFactory::GetCdmFactoryDaemonReceiver() {
  mojo::PendingRemote<chromeos::cdm::mojom::CdmFactoryDaemon> browser_proxy;
  auto receiver = browser_proxy.InitWithNewPipeAndPassReceiver();
  GetCdmFactoryDaemonRemote().Bind(std::move(browser_proxy));

  GetFactoryTaskRunner() = base::SequencedTaskRunnerHandle::Get();
  return receiver;
}

void ChromeOsCdmFactory::Create(
    const std::string& key_system,
    const media::CdmConfig& cdm_config,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb) {
  DVLOG(1) << __func__ << " key system=" << key_system;
  // Check that the user has Verified Access enabled in their Chrome settings
  // and if they do not then block this connection since OEMCrypto utilizes
  // remote attestation as part of verification.
  if (!cdm_document_service_) {
    frame_interfaces_->BindEmbedderReceiver(mojo::GenericPendingReceiver(
        cdm_document_service_.BindNewPipeAndPassReceiver()));
    cdm_document_service_.set_disconnect_handler(
        base::BindOnce(&ChromeOsCdmFactory::OnVerificationMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
  }
  cdm_document_service_->IsVerifiedAccessEnabled(base::BindOnce(
      &ChromeOsCdmFactory::OnVerifiedAccessEnabled, weak_factory_.GetWeakPtr(),
      key_system, cdm_config, session_message_cb, session_closed_cb,
      session_keys_change_cb, session_expiration_update_cb,
      std::move(cdm_created_cb)));
}

// static
void ChromeOsCdmFactory::GetHwConfigData(GetHwConfigDataCB callback) {
  if (!GetFactoryTaskRunner()->RunsTasksInCurrentSequence()) {
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ChromeOsCdmFactory::GetHwConfigData,
                                  std::move(callback)));
    return;
  }
  GetCdmFactoryDaemonRemote()->GetHwConfigData(std::move(callback));
}

// static
void ChromeOsCdmFactory::GetScreenResolutions(GetScreenResolutionsCB callback) {
  if (!GetFactoryTaskRunner()->RunsTasksInCurrentSequence()) {
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ChromeOsCdmFactory::GetScreenResolutions,
                                  std::move(callback)));
    return;
  }
  GetCdmFactoryDaemonRemote()->GetScreenResolutions(std::move(callback));
}

void ChromeOsCdmFactory::OnVerifiedAccessEnabled(
    const std::string& key_system,
    const media::CdmConfig& cdm_config,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb,
    bool enabled) {
  if (!enabled) {
    DVLOG(1)
        << "Not using Chrome OS CDM factory due to Verified Access disabled";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cdm_created_cb), nullptr,
                                  "Verified Access is disabled."));
    return;
  }
  // If we haven't retrieved the remote CDM factory, do that first.
  if (!remote_factory_) {
    // Now invoke the call to create the Mojo interface for the CDM factory. We
    // need to invoke the CreateFactory call on the factory task runner, but
    // we then need to process the callback on the current runner, so there's a
    // few layers of indirection here.
    GetFactoryTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CreateFactoryOnTaskRunner, key_system,
            base::BindOnce(
                &CreateFactoryCallback, base::ThreadTaskRunnerHandle::Get(),
                base::BindOnce(
                    &ChromeOsCdmFactory::OnCreateFactory,
                    weak_factory_.GetWeakPtr(), cdm_config, session_message_cb,
                    session_closed_cb, session_keys_change_cb,
                    session_expiration_update_cb, std::move(cdm_created_cb)))));
    return;
  }

  // Create the remote CDM in the daemon and then pass that into our adapter
  // that converts the media::ContentDecryptionModule/Decryptor calls into
  // chromeos::cdm::mojom::ContentDecryptionModule calls.
  CreateCdm(cdm_config, session_message_cb, session_closed_cb,
            session_keys_change_cb, session_expiration_update_cb,
            std::move(cdm_created_cb));
}

void ChromeOsCdmFactory::OnCreateFactory(
    const media::CdmConfig& cdm_config,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb,
    mojo::PendingRemote<cdm::mojom::CdmFactory> remote_factory) {
  DVLOG(1) << __func__;
  if (!remote_factory) {
    LOG(ERROR) << "Failed creating the remote CDM factory";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cdm_created_cb), nullptr,
                                  "Remote factory creation failed."));
    return;
  }
  // Check if this is bound already, which could happen due to asynchronous
  // calls.
  if (!remote_factory_) {
    remote_factory_.Bind(std::move(remote_factory));
    remote_factory_.set_disconnect_handler(
        base::BindOnce(&ChromeOsCdmFactory::OnFactoryMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
  }

  // We have the factory bound, create the CDM.
  CreateCdm(cdm_config, session_message_cb, session_closed_cb,
            session_keys_change_cb, session_expiration_update_cb,
            std::move(cdm_created_cb));
}

void ChromeOsCdmFactory::CreateCdm(
    const media::CdmConfig& /*cdm_config*/,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    media::CdmCreatedCB cdm_created_cb) {
  DVLOG(1) << __func__;
  // Create the storage implementation we are sending to Chrome OS.
  mojo::PendingAssociatedRemote<cdm::mojom::CdmStorage> storage_remote;
  std::unique_ptr<CdmStorageAdapter> storage =
      std::make_unique<CdmStorageAdapter>(
          frame_interfaces_,
          storage_remote.InitWithNewEndpointAndPassReceiver());

  // Create the remote interface for the CDM in Chrome OS.
  mojo::AssociatedRemote<cdm::mojom::ContentDecryptionModule> cros_cdm;
  mojo::PendingAssociatedReceiver<cdm::mojom::ContentDecryptionModule>
      cros_cdm_pending_receiver = cros_cdm.BindNewEndpointAndPassReceiver();

  // Create the adapter that proxies calls between
  // media::ContentDecryptionModule and
  // chromeos::cdm::mojom::ContentDecryptionModule.
  scoped_refptr<ContentDecryptionModuleAdapter> cdm =
      base::WrapRefCounted<ContentDecryptionModuleAdapter>(
          new ContentDecryptionModuleAdapter(
              std::move(storage), std::move(cros_cdm), session_message_cb,
              session_closed_cb, session_keys_change_cb,
              session_expiration_update_cb));

  // Create the OutputProtection interface to pass to the CDM.
  mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection_remote;
  GetFactoryTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetOutputProtectionOnTaskRunner,
          output_protection_remote.InitWithNewPipeAndPassReceiver()));

  // Now create the remote CDM instance that links everything up.
  remote_factory_->CreateCdm(cdm->GetClientInterface(),
                             std::move(storage_remote),
                             std::move(output_protection_remote),
                             base::UnguessableToken::Create().ToString(),
                             std::move(cros_cdm_pending_receiver));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cdm_created_cb), std::move(cdm), ""));
}

void ChromeOsCdmFactory::OnFactoryMojoConnectionError() {
  DVLOG(1) << __func__;
  remote_factory_.reset();
}

void ChromeOsCdmFactory::OnVerificationMojoConnectionError() {
  DVLOG(1) << __func__;
  cdm_document_service_.reset();
}

}  // namespace chromeos
