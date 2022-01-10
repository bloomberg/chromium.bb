// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/printing/print_servers_manager.h"
#include "chrome/common/buildflags.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_job_constants.h"

namespace base {
class DictionaryValue;
}

namespace printing {

namespace mojom {
enum class PrinterType;
}

class PrinterHandler;
class PrintPreviewHandler;

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandlerChromeOS : public content::WebUIMessageHandler,
                                    public crosapi::mojom::PrintServerObserver {
 public:
  PrintPreviewHandlerChromeOS();
  PrintPreviewHandlerChromeOS(const PrintPreviewHandlerChromeOS&) = delete;
  PrintPreviewHandlerChromeOS& operator=(const PrintPreviewHandlerChromeOS&) =
      delete;
  ~PrintPreviewHandlerChromeOS() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;
  void OnJavascriptAllowed() override;

 protected:
  // Protected so unit tests can override.
  virtual PrinterHandler* GetPrinterHandler(mojom::PrinterType printer_type);

 private:
  friend class PrintPreviewHandlerChromeOSTest;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  friend class TestPrintServersManager;
#endif
  class AccessTokenService;

  PrintPreviewHandler* GetPrintPreviewHandler();

  void MaybeAllowJavascript();

  // Grants an extension access to a provisional printer.  First element of
  // |args| is the provisional printer ID.
  void HandleGrantExtensionPrinterAccess(const base::ListValue* args);

  // Performs printer setup. First element of |args| is the printer name.
  void HandlePrinterSetup(const base::ListValue* args);

  // Generates new token and sends back to UI.
  void HandleGetAccessToken(const base::ListValue* args);

  // Gets the EULA URL.
  void HandleGetEulaUrl(const base::ListValue* args);

  // Send OAuth2 access token.
  void SendAccessToken(const std::string& callback_id,
                       const std::string& access_token);

  // Send the EULA URL;
  void SendEulaUrl(const std::string& callback_id, const std::string& eula_url);

  // Send the result of performing printer setup. |settings_info| contains
  // printer capabilities.
  void SendPrinterSetup(const std::string& callback_id,
                        const std::string& printer_name,
                        base::Value settings_info);

  // Called when an extension reports information requested for a provisional
  // printer.
  // |callback_id|: The javascript callback to resolve or reject.
  // |printer_info|: The data reported by the extension.
  void OnGotExtensionPrinterInfo(const std::string& callback_id,
                                 const base::DictionaryValue& printer_info);

  // Called to initiate a status request for a printer.
  void HandleRequestPrinterStatusUpdate(const base::ListValue* args);

  // crosapi::mojom::PrintServerObserver Implementation
  void OnPrintServersChanged(
      crosapi::mojom::PrintServersConfigPtr ptr) override;
  void OnServerPrintersChanged() override;

  // Loads printers corresponding to the print server(s).  First element of
  // |args| is the print server IDs.
  void HandleChoosePrintServers(const base::ListValue* args);

  // Gets the list of print servers and fetching mode.
  void HandleGetPrintServersConfig(const base::ListValue* args);

  // Holds token service to get OAuth2 access tokens.
  std::unique_ptr<AccessTokenService> token_service_;

  mojo::Receiver<crosapi::mojom::PrintServerObserver> receiver_{this};

  // Used to transmit mojo interface method calls to ash chrome.
  // Null if the interface is unavailable.
  // Note that this is not propagated to LocalPrinterHandlerLacros.
  // The pointer is constant - if ash crashes and the mojo connection is lost,
  // lacros will automatically be restarted.
  crosapi::mojom::LocalPrinter* local_printer_ = nullptr;

  base::WeakPtrFactory<PrintPreviewHandlerChromeOS> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_CHROMEOS_H_
