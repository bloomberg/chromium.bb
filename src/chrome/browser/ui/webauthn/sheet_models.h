// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

namespace gfx {
struct VectorIcon;
}

namespace ui {
class MenuModel;
}

class OtherTransportsMenuModel;

// Base class for sheets, implementing the shared behavior used on most sheets,
// as well as maintaining a weak pointer to the dialog model.
class AuthenticatorSheetModelBase
    : public AuthenticatorRequestSheetModel,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit AuthenticatorSheetModelBase(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorSheetModelBase() override;

  AuthenticatorRequestDialogModel* dialog_model() const {
    return dialog_model_;
  }

 protected:
  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override;
  bool IsBackButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnBack() override;
  void OnAccept() override;
  void OnCancel() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;

 private:
  AuthenticatorRequestDialogModel* dialog_model_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorSheetModelBase);
};

// The sheet shown for selecting the transport over which the security key
// should be accessed.
class AuthenticatorTransportSelectorSheetModel
    : public AuthenticatorSheetModelBase,
      public TransportHoverListModel::Delegate,
      public TransportHoverListModel2::Delegate {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

  // TransportHoverListModel::Delegate:
  void OnTransportSelected(AuthenticatorTransport transport) override;
  void StartPhonePairing() override;
  void StartWinNativeApi() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorInsertAndActivateUsbSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorInsertAndActivateUsbSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorInsertAndActivateUsbSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  base::string16 GetAdditionalDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorTimeoutErrorModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorNoAvailableTransportsErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorNotRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorAlreadyRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorInternalUnrecognizedErrorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorBlePowerOnManualSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnBluetoothPoweredStateChanged() override;
};

class AuthenticatorBlePowerOnAutomaticSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;

  bool busy_powering_on_ble_ = false;
};

class AuthenticatorTouchIdIncognitoBumpSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTouchIdIncognitoBumpSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorTouchIdIncognitoBumpSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorPaaskSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPaaskSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorPaaskSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorClientPinEntrySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  // Indicates whether the view should accommodate setting up a new PIN or
  // entering an existing one.
  enum class Mode { kPinEntry, kPinSetup };
  AuthenticatorClientPinEntrySheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      Mode mode);
  ~AuthenticatorClientPinEntrySheetModel() override;

  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

  void SetPinCode(base::string16 pin_code);
  void SetPinConfirmation(base::string16 pin_confirmation);

  Mode mode() const { return mode_; }

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  base::string16 GetError() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;

  base::string16 pin_code_;
  base::string16 pin_confirmation_;
  base::string16 error_;
  const Mode mode_;
};

class AuthenticatorClientPinTapAgainSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorClientPinTapAgainSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorClientPinTapAgainSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  base::string16 GetAdditionalDescription() const override;
};

class AuthenticatorBioEnrollmentSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBioEnrollmentSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorBioEnrollmentSheetModel() override;

  int max_bio_samples() {
    return dialog_model()->max_bio_samples().value_or(1);
  }
  int bio_samples_remaining() {
    return dialog_model()->bio_samples_remaining().value_or(1);
  }

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  base::string16 GetAcceptButtonLabel() const override;
  bool IsCancelButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  void OnAccept() override;
  void OnCancel() override;
};

class AuthenticatorRetryUvSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorRetryUvSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorRetryUvSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  base::string16 GetError() const override;
};

// Generic error dialog that can only be dismissed. Backwards navigation is
// not visible.
class AuthenticatorGenericErrorSheetModel : public AuthenticatorSheetModelBase {
 public:
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorSoftBlock(AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorHardBlock(AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorAuthenticatorRemoved(
      AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForMissingCapability(AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel> ForStorageFull(
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  AuthenticatorGenericErrorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      base::string16 title,
      base::string16 description);

  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;

  base::string16 title_;
  base::string16 description_;
};

class AuthenticatorResidentCredentialConfirmationSheetView
    : public AuthenticatorSheetModelBase {
 public:
  AuthenticatorResidentCredentialConfirmationSheetView(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorResidentCredentialConfirmationSheetView() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  bool IsBackButtonVisible() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  void OnAccept() override;
};

// The sheet shown when the user needs to select an account.
class AuthenticatorSelectAccountSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorSelectAccountSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorSelectAccountSheetModel() override;

  // Set the index of the currently selected row.
  void SetCurrentSelection(int selected);

  // AuthenticatorSheetModelBase:
  void OnAccept() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;

  size_t selected_ = 0;
};

class AttestationPermissionRequestSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AttestationPermissionRequestSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AttestationPermissionRequestSheetModel() override;

  // AuthenticatorSheetModelBase:
  void OnAccept() override;
  void OnCancel() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsBackButtonVisible() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  bool IsCancelButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
};

class AuthenticatorQRSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorQRSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorQRSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
