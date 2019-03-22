// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win.h"
#include "chrome/browser/ui/startup/credential_provider_signin_info_fetcher_win.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/syslog_logging.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/url_util.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace {

// This message must match the one sent in inline_login.js: sendLSTFetchResults.
constexpr char kLSTFetchResultsMessage[] = "lstFetchResults";

void HandleAllGcpwInfoFetched(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<CredentialProviderSigninInfoFetcher> fetcher,
    base::Value signin_result,
    base::Value fetch_result) {
  DCHECK(signin_result.is_dict());
  DCHECK(fetch_result.is_dict());
  if (!signin_result.DictEmpty() && !fetch_result.DictEmpty()) {
    std::string json_result;
    signin_result.MergeDictionary(&fetch_result);
    if (base::JSONWriter::Write(signin_result, &json_result) &&
        !json_result.empty()) {
      // The caller of this Chrome process must provide a stdout handle  to
      // which Chrome can output the results, otherwise by default the call to
      // ::GetStdHandle(STD_OUTPUT_HANDLE) will result in an invalid or null
      // handle if Chrome was started without providing a console.
      HANDLE output_handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
      if (output_handle != nullptr && output_handle != INVALID_HANDLE_VALUE) {
        DWORD written;
        if (!::WriteFile(output_handle, json_result.c_str(),
                         json_result.length(), &written, nullptr)) {
          SYSLOG(ERROR)
              << "Failed to write result of GCPW signin to inherited handle.";
        }
      }
    }
  }

  // Release the fetcher and mark it for eventual delete. It is not immediately
  // deleted here in case it still wants to do further processing after
  // returning from this callback
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, fetcher.release());

  // Release the keep_alive implicitly and allow the dialog to die.
}

void HandleSigninCompleteForGcpwLogin(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    base::Value signin_result,
    const std::string& access_token,
    const std::string& refresh_token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK(signin_result.is_dict());
  DCHECK_EQ(signin_result.DictEmpty(),
            access_token.empty() && refresh_token.empty());
  if (!signin_result.DictEmpty()) {
    // Create the fetcher and pass it to the callback so that it can be
    // deleted once it is finished.
    auto fetcher = std::make_unique<CredentialProviderSigninInfoFetcher>(
        refresh_token, url_loader_factory);
    fetcher->SetCompletionCallbackAndStart(
        access_token,
        base::BindOnce(&HandleAllGcpwInfoFetched, std::move(keep_alive),
                       std::move(fetcher), std::move(signin_result)));
  }

  // If the function has not pass ownership of the keep alive yet at this point
  // this means there was some error reading the sign in result or the result
  // was empty. In this case, return from the method which will implicitly
  // release the keep_alive which will close and release everything.
}

class CredentialProviderWebUIMessageHandler
    : public content::WebUIMessageHandler {
 public:
  explicit CredentialProviderWebUIMessageHandler(
      HandleGcpwSigninCompleteResult signin_callback)
      : signin_callback_(std::move(signin_callback)) {}

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        kLSTFetchResultsMessage,
        base::BindRepeating(
            &CredentialProviderWebUIMessageHandler::OnSigninComplete,
            base::Unretained(this)));
  }

 private:
  base::Value ParseArgs(const base::ListValue* args,
                        std::string* out_access_token,
                        std::string* out_refresh_token) {
    const base::Value* dict_result = nullptr;
    if (!args || args->empty() || !args->Get(0, &dict_result) ||
        !dict_result->is_dict()) {
      return base::Value(base::Value::Type::DICTIONARY);
    }

    const base::Value* email = dict_result->FindKeyOfType(
        credential_provider::kKeyEmail, base::Value::Type::STRING);
    const base::Value* password = dict_result->FindKeyOfType(
        credential_provider::kKeyPassword, base::Value::Type::STRING);
    const base::Value* id = dict_result->FindKeyOfType(
        credential_provider::kKeyId, base::Value::Type::STRING);
    const base::Value* access_token = dict_result->FindKeyOfType(
        credential_provider::kKeyAccessToken, base::Value::Type::STRING);
    const base::Value* refresh_token = dict_result->FindKeyOfType(
        credential_provider::kKeyRefreshToken, base::Value::Type::STRING);

    if (!email || email->GetString().empty() || !password ||
        password->GetString().empty() || !id || id->GetString().empty() ||
        !access_token || access_token->GetString().empty() || !refresh_token ||
        refresh_token->GetString().empty()) {
      return base::Value(base::Value::Type::DICTIONARY);
    }

    *out_access_token = access_token->GetString();
    *out_refresh_token = refresh_token->GetString();
    return dict_result->Clone();
  }

  void OnSigninComplete(const base::ListValue* args) {
    std::string access_token;
    std::string refresh_token;
    base::Value signin_result = ParseArgs(args, &access_token, &refresh_token);

    content::WebContents* contents = web_ui()->GetWebContents();
    content::StoragePartition* partition =
        content::BrowserContext::GetStoragePartitionForSite(
            contents->GetBrowserContext(), signin::GetSigninPartitionURL());

    // Regardless of the results of ParseArgs, |signin_callback_| will always
    // be called to allow it to release any additional references it may hold
    // (like the keep_alive in HandleSigninCompleteForGCPWLogin) or perform
    // possible error handling.
    std::move(signin_callback_)
        .Run(std::move(signin_result), access_token, refresh_token,
             partition->GetURLLoaderFactoryForBrowserProcess());
  }

  HandleGcpwSigninCompleteResult signin_callback_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderWebUIMessageHandler);
};

}  // namespace

// Delegate to control a views::WebDialogView for purposes of showing a gaia
// sign in page for purposes of the credential provider.
class CredentialProviderWebDialogDelegate : public ui::WebDialogDelegate {
 public:
  // |reauth_email| is used to pre fill in the sign in dialog with the user's
  // e-mail during a reauthorize sign in. This type of sign in is used to update
  // the user's password.
  // |email_domain| is used to pre fill the email domain on Gaia's signin page
  // so that the user only needs to enter their user name.
  CredentialProviderWebDialogDelegate(
      const std::string& reauth_email,
      const std::string& email_domain,
      HandleGcpwSigninCompleteResult signin_callback)
      : reauth_email_(reauth_email),
        email_domain_(email_domain),
        signin_callback_(std::move(signin_callback)) {}

  GURL GetDialogContentURL() const override {
    signin_metrics::AccessPoint access_point =
        signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON;
    signin_metrics::Reason reason =
        signin_metrics::Reason::REASON_FETCH_LST_ONLY;

    auto base_url =
        reauth_email_.empty()
            ? signin::GetPromoURLForDialog(access_point, reason, false)
            : signin::GetReauthURLWithEmailForDialog(access_point, reason,
                                                     reauth_email_);

    if (email_domain_.empty())
      return base_url;

    return net::AppendQueryParameter(
        base_url, credential_provider::kEmailDomainSigninPromoParameter,
        email_domain_);
  }

  ui::ModalType GetDialogModalType() const override {
    return ui::MODAL_TYPE_SYSTEM;
  }

  base::string16 GetDialogTitle() const override { return base::string16(); }

  base::string16 GetAccessibleDialogTitle() const override {
    return base::string16();
  }

  std::string GetDialogName() const override {
    // Return an empty window name; otherwise chrome will try to persist the
    // window's position and DCHECK.
    return std::string();
  }

  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override {
    // The WebDialogUI will own and delete this message handler.
    handlers->push_back(
        new CredentialProviderWebUIMessageHandler(std::move(signin_callback_)));
  }

  void GetDialogSize(gfx::Size* size) const override {
    // TODO(crbug.com/901947): Figure out exactly what size the dialog should
    // be.
    size->SetSize(448, 610);
  }

  void GetMinimumDialogSize(gfx::Size* size) const override {
    GetDialogSize(size);
  }

  std::string GetDialogArgs() const override { return std::string(); }

  void OnDialogClosed(const std::string& json_retval) override {
    // Class owns itself and thus needs to be deleted eventually after the
    // closed call back has been signalled since it will no longer be accessed
    // by the WebDialogView.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override {}

  bool HandleContextMenu(const content::ContextMenuParams& params) override {
    return true;
  }

  bool ShouldShowDialogTitle() const override { return false; }

 protected:
  // E-mail used to pre-fill the e-mail field when a reauth signin is required.
  const std::string reauth_email_;

  // Default domain used for all sign in requests.
  const std::string email_domain_;

  // Callback that will be called when a valid sign in has been completed
  // through the dialog.
  mutable HandleGcpwSigninCompleteResult signin_callback_;
};

bool ValidateSigninCompleteResult(const std::string& access_token,
                                  const std::string& refresh_token,
                                  const base::Value& signin_result) {
  return !access_token.empty() && !refresh_token.empty() &&
         signin_result.is_dict();
}

void StartGCPWSignin(const base::CommandLine& command_line,
                     content::BrowserContext* context) {
  // This keep_alive is created since there is no browser created when
  // --gcpw-logon is specified. Since there is no browser there is no holder of
  // a ScopedKeepAlive present that will ensure Chrome kills itself when the
  // last keep alive is released. So instead, keep the keep alive across the
  // callbacks that will be sent during the signin process. Once the full fetch
  // of the information necesssary for the GCPW is finished (or there is a
  // failure) release the keep alive so that Chrome can shutdown.

  ShowCredentialProviderSigninDialog(
      command_line, context,
      base::BindOnce(&HandleSigninCompleteForGcpwLogin,
                     std::make_unique<ScopedKeepAlive>(
                         KeepAliveOrigin::CREDENTIAL_PROVIDER_SIGNIN_DIALOG,
                         KeepAliveRestartOption::DISABLED)));
}

views::WebDialogView* ShowCredentialProviderSigninDialog(
    const base::CommandLine& command_line,
    content::BrowserContext* context,
    HandleGcpwSigninCompleteResult signin_complete_handler) {
  DCHECK(signin_complete_handler);

  // Open a frameless window whose entire surface displays a gaia sign in web
  // page.
  std::string reauth_email =
      command_line.GetSwitchValueASCII(credential_provider::kGcpwSigninSwitch);
  std::string email_domain =
      command_line.GetSwitchValueASCII(credential_provider::kEmailDomainSwitch);

  // Delegate to handle the result of the sign in request. This will
  // delete itself eventually when it receives the OnDialogClosed call.
  auto delegate = std::make_unique<CredentialProviderWebDialogDelegate>(
      reauth_email, email_domain, std::move(signin_complete_handler));

  // The web dialog view that will contain the web ui for the login screen.
  // This view will be automatically deleted by the widget that owns it when it
  // is closed.
  auto view = std::make_unique<views::WebDialogView>(
      context, delegate.release(), new ChromeWebContentsHandler);
  views::Widget::InitParams init_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  views::WebDialogView* web_view = view.release();
  init_params.name = "GCPW";  // Used for debugging only.
  init_params.delegate = web_view;

  // This widget will automatically delete itself and its WebDialogView when the
  // dialog window is closed.
  views::Widget* widget = new views::Widget;
  widget->Init(init_params);
  widget->Show();

  return web_view;
}
