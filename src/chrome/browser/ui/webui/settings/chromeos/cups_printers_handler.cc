// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/ppd_provider_factory.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/ash/printing/printer_event_tracker_factory.h"
#include "chrome/browser/ash/printing/printer_info.h"
#include "chrome/browser/ash/printing/server_printers_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/local_discovery/endpoint_resolver.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/settings/chromeos/server_printer_url_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/printing/ppd_line_reader.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/uri.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/google_api_keys.h"
#include "net/base/filename_util.h"
#include "net/base/ip_endpoint.h"
#include "printing/backend/print_backend.h"
#include "printing/printer_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace chromeos {
namespace settings {

namespace {

using printing::PrinterQueryResult;

constexpr int kPpdMaxLineLength = 255;

void OnRemovedPrinter(const Printer::PrinterProtocol& protocol, bool success) {
  if (success) {
    PRINTER_LOG(DEBUG) << "Printer removal succeeded.";
  } else {
    PRINTER_LOG(DEBUG) << "Printer removal failed.";
  }

  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterRemoved", protocol,
                            Printer::PrinterProtocol::kProtocolMax);
}

// Log if the IPP attributes request was succesful.
void RecordIppQueryResult(const PrinterQueryResult& result) {
  bool reachable = result != PrinterQueryResult::kHostnameResolution &&
                   result != PrinterQueryResult::kUnreachable;
  UMA_HISTOGRAM_BOOLEAN("Printing.CUPS.IppDeviceReachable", reachable);

  if (reachable) {
    // Only record whether the query was successful if we reach the printer.
    bool query_success = (result == PrinterQueryResult::kSuccess);
    UMA_HISTOGRAM_BOOLEAN("Printing.CUPS.IppAttributesSuccess", query_success);
  }
}

// Query an IPP printer to check for autoconf support where the printer is
// located at |printer_uri|.  Results are reported through |callback|.  The
// scheme of |printer_uri| must equal "ipp" or "ipps".
void QueryAutoconf(const Uri& uri, PrinterInfoCallback callback) {
  QueryIppPrinter(uri.GetHostEncoded(), uri.GetPort(),
                  uri.GetPathEncodedAsString(), uri.GetScheme() == kIppsScheme,
                  std::move(callback));
}

// Returns the list of |printers| formatted as a CupsPrintersList.
base::Value BuildCupsPrintersList(const std::vector<Printer>& printers) {
  base::Value printers_list(base::Value::Type::LIST);
  for (const Printer& printer : printers) {
    // Some of these printers could be invalid but we want to allow the user
    // to edit them. crbug.com/778383
    printers_list.Append(GetCupsPrinterInfo(printer));
  }

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey("printerList", std::move(printers_list));
  return response;
}

// Generates a Printer from |printer_dict| where |printer_dict| is a
// CupsPrinterInfo representation.  If any of the required fields are missing,
// returns nullptr.
std::unique_ptr<chromeos::Printer> DictToPrinter(
    const base::DictionaryValue& printer_dict) {
  std::string printer_id;
  std::string printer_name;
  std::string printer_description;
  std::string printer_make_and_model;
  std::string printer_address;
  std::string printer_protocol;
  std::string print_server_uri;

  if (!printer_dict.GetString("printerId", &printer_id) ||
      !printer_dict.GetString("printerName", &printer_name) ||
      !printer_dict.GetString("printerDescription", &printer_description) ||
      !printer_dict.GetString("printerMakeAndModel", &printer_make_and_model) ||
      !printer_dict.GetString("printerAddress", &printer_address) ||
      !printer_dict.GetString("printerProtocol", &printer_protocol) ||
      !printer_dict.GetString("printServerUri", &print_server_uri)) {
    return nullptr;
  }

  std::string printer_queue;
  // The protocol "socket" does not allow path.
  if (printer_protocol != "socket") {
    printer_dict.GetString("printerQueue", &printer_queue);
    // Path must start from '/' character.
    if (!printer_queue.empty() && printer_queue.front() != '/')
      printer_queue.insert(0, "/");
  }

  auto printer = std::make_unique<chromeos::Printer>(printer_id);
  printer->set_display_name(printer_name);
  printer->set_description(printer_description);
  printer->set_make_and_model(printer_make_and_model);
  printer->set_print_server_uri(print_server_uri);

  Uri uri(printer_protocol + url::kStandardSchemeSeparator + printer_address +
          printer_queue);
  if (uri.GetLastParsingError().status != Uri::ParserStatus::kNoErrors) {
    PRINTER_LOG(ERROR) << "Uri parse error: "
                       << static_cast<int>(uri.GetLastParsingError().status);
    return nullptr;
  }

  std::string message;
  if (!printer->SetUri(uri, &message)) {
    PRINTER_LOG(ERROR) << "Incorrect uri: " << message;
    return nullptr;
  }

  return printer;
}

std::string ReadFileToStringWithMaxSize(const base::FilePath& path,
                                        int max_size) {
  std::string contents;
  // This call can fail, but it doesn't matter for our purposes. If it fails,
  // we simply return an empty string for the contents, and it will be rejected
  // as an invalid PPD.
  base::ReadFileToStringWithMaxSize(path, &contents, max_size);
  return contents;
}

// Determines whether changing the URI in |existing_printer| to the URI in
// |new_printer| would be valid. Network printers are not allowed to change
// their protocol to a non-network protocol, but can change anything else.
// Non-network printers are not allowed to change anything in their URI.
bool IsValidUriChange(const Printer& existing_printer,
                      const Printer& new_printer) {
  if (new_printer.GetProtocol() == Printer::PrinterProtocol::kUnknown) {
    return false;
  }
  if (existing_printer.HasNetworkProtocol()) {
    return new_printer.HasNetworkProtocol();
  }
  return existing_printer.uri() == new_printer.uri();
}

// Assumes |info| is a dictionary.
void SetPpdReference(const Printer::PpdReference& ppd_ref, base::Value* info) {
  if (!ppd_ref.user_supplied_ppd_url.empty()) {
    info->SetKey("ppdRefUserSuppliedPpdUrl",
                 base::Value(ppd_ref.user_supplied_ppd_url));
  } else if (!ppd_ref.effective_make_and_model.empty()) {
    info->SetKey("ppdRefEffectiveMakeAndModel",
                 base::Value(ppd_ref.effective_make_and_model));
  } else {  // Must be autoconf, shouldn't be possible
    NOTREACHED() << "Succeeded in PPD matching without emm";
  }
}

Printer::PpdReference GetPpdReference(const base::Value* info) {
  const char ppd_ref_pathname[] = "printerPpdReference";
  auto* user_supplied_ppd_url =
      info->FindPath({ppd_ref_pathname, "userSuppliedPPDUrl"});
  auto* effective_make_and_model =
      info->FindPath({ppd_ref_pathname, "effectiveMakeAndModel"});
  auto* autoconf = info->FindPath({ppd_ref_pathname, "autoconf"});

  Printer::PpdReference ret;

  if (user_supplied_ppd_url) {
    ret.user_supplied_ppd_url = user_supplied_ppd_url->GetString();
  }

  if (effective_make_and_model) {
    ret.effective_make_and_model = effective_make_and_model->GetString();
  }

  if (autoconf) {
    ret.autoconf = autoconf->GetBool();
  }

  return ret;
}

GURL GenerateHttpCupsServerUrl(const GURL& server_url) {
  GURL::Replacements replacement;
  replacement.SetSchemeStr("http");
  replacement.SetPortStr("631");
  return server_url.ReplaceComponents(replacement);
}

}  // namespace

CupsPrintersHandler::CupsPrintersHandler(Profile* profile,
                                         CupsPrintersManager* printers_manager)
    : CupsPrintersHandler(profile,
                          CreatePpdProvider(profile),
                          PrinterConfigurer::Create(profile),
                          printers_manager) {}

CupsPrintersHandler::CupsPrintersHandler(
    Profile* profile,
    scoped_refptr<PpdProvider> ppd_provider,
    std::unique_ptr<PrinterConfigurer> printer_configurer,
    CupsPrintersManager* printers_manager)
    : profile_(profile),
      ppd_provider_(ppd_provider),
      printer_configurer_(std::move(printer_configurer)),
      printers_manager_(printers_manager),
      endpoint_resolver_(
          std::make_unique<local_discovery::EndpointResolver>()) {}

// static
std::unique_ptr<CupsPrintersHandler> CupsPrintersHandler::CreateForTesting(
    Profile* profile,
    scoped_refptr<PpdProvider> ppd_provider,
    std::unique_ptr<PrinterConfigurer> printer_configurer,
    CupsPrintersManager* printers_manager) {
  // Using 'new' to access non-public constructor.
  return base::WrapUnique(new CupsPrintersHandler(
      profile, ppd_provider, std::move(printer_configurer), printers_manager));
}

CupsPrintersHandler::~CupsPrintersHandler() = default;

void CupsPrintersHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getCupsSavedPrintersList",
      base::BindRepeating(&CupsPrintersHandler::HandleGetCupsSavedPrintersList,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getCupsEnterprisePrintersList",
      base::BindRepeating(
          &CupsPrintersHandler::HandleGetCupsEnterprisePrintersList,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "updateCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleUpdateCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "removeCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleRemoveCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "addCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleAddCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "reconfigureCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleReconfigureCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getPrinterInfo",
      base::BindRepeating(&CupsPrintersHandler::HandleGetPrinterInfo,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getCupsPrinterManufacturersList",
      base::BindRepeating(
          &CupsPrintersHandler::HandleGetCupsPrinterManufacturers,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getCupsPrinterModelsList",
      base::BindRepeating(&CupsPrintersHandler::HandleGetCupsPrinterModels,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "selectPPDFile",
      base::BindRepeating(&CupsPrintersHandler::HandleSelectPPDFile,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "startDiscoveringPrinters",
      base::BindRepeating(&CupsPrintersHandler::HandleStartDiscovery,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "stopDiscoveringPrinters",
      base::BindRepeating(&CupsPrintersHandler::HandleStopDiscovery,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getPrinterPpdManufacturerAndModel",
      base::BindRepeating(
          &CupsPrintersHandler::HandleGetPrinterPpdManufacturerAndModel,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "addDiscoveredPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleAddDiscoveredPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "cancelPrinterSetUp",
      base::BindRepeating(&CupsPrintersHandler::HandleSetUpCancel,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getEulaUrl", base::BindRepeating(&CupsPrintersHandler::HandleGetEulaUrl,
                                        base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "queryPrintServer",
      base::BindRepeating(&CupsPrintersHandler::HandleQueryPrintServer,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "openPrintManagementApp",
      base::BindRepeating(&CupsPrintersHandler::HandleOpenPrintManagementApp,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "openScanningApp",
      base::BindRepeating(&CupsPrintersHandler::HandleOpenScanningApp,
                          base::Unretained(this)));
}

void CupsPrintersHandler::OnJavascriptAllowed() {
  DCHECK(!printers_manager_observation_.IsObserving());
  printers_manager_observation_.Observe(printers_manager_);
}

void CupsPrintersHandler::OnJavascriptDisallowed() {
  printers_manager_observation_.Reset();
}

void CupsPrintersHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void CupsPrintersHandler::HandleGetCupsSavedPrintersList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();

  std::vector<Printer> printers =
      printers_manager_->GetPrinters(PrinterClass::kSaved);

  auto response = BuildCupsPrintersList(printers);
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void CupsPrintersHandler::HandleGetCupsEnterprisePrintersList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();

  std::vector<Printer> printers =
      printers_manager_->GetPrinters(PrinterClass::kEnterprise);

  auto response = BuildCupsPrintersList(printers);
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void CupsPrintersHandler::HandleUpdateCupsPrinter(const base::ListValue* args) {
  CHECK_EQ(3U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& printer_id = args->GetList()[1].GetString();
  const std::string& printer_name = args->GetList()[2].GetString();

  Printer printer(printer_id);
  printer.set_display_name(printer_name);

  if (!profile_->GetPrefs()->GetBoolean(prefs::kUserPrintersAllowed)) {
    PRINTER_LOG(DEBUG) << "HandleUpdateCupsPrinter() called when "
                          "kUserPrintersAllowed is set to false";
    OnAddedOrEditedPrinterCommon(printer,
                                 PrinterSetupResult::kNativePrintersNotAllowed,
                                 false /* is_automatic */);
    // Logs the error and runs the callback.
    OnAddOrEditPrinterError(callback_id,
                            PrinterSetupResult::kNativePrintersNotAllowed);
    return;
  }

  OnAddedOrEditedSpecifiedPrinter(callback_id, printer,
                                  true /* is_printer_edit */,
                                  PrinterSetupResult::kEditSuccess);
}

void CupsPrintersHandler::HandleRemoveCupsPrinter(const base::ListValue* args) {
  PRINTER_LOG(USER) << "Removing printer";
  // Printer name also expected in 2nd parameter.
  const std::string& printer_id = args->GetList()[0].GetString();
  auto printer = printers_manager_->GetPrinter(printer_id);
  if (!printer)
    return;

  // Record removal before the printer is deleted.
  PrinterEventTrackerFactory::GetForBrowserContext(profile_)
      ->RecordPrinterRemoved(*printer);

  Printer::PrinterProtocol protocol = printer->GetProtocol();
  // Printer is deleted here.  Do not access after this line.
  printers_manager_->RemoveSavedPrinter(printer_id);

  DebugDaemonClient* client = DBusThreadManager::Get()->GetDebugDaemonClient();
  client->CupsRemovePrinter(printer_id,
                            base::BindOnce(&OnRemovedPrinter, protocol),
                            base::DoNothing());
}

void CupsPrintersHandler::HandleGetPrinterInfo(const base::ListValue* args) {
  DCHECK(args);
  if (args->GetList().empty() || !args->GetList()[0].is_string()) {
    NOTREACHED() << "Expected request for a promise";
    return;
  }
  const std::string& callback_id = args->GetList()[0].GetString();

  if (args->GetList().size() < 2u) {
    NOTREACHED() << "Dictionary missing";
    return;
  }

  const base::Value& printer_value = args->GetList()[1];
  if (!printer_value.is_dict()) {
    NOTREACHED() << "Dictionary missing";
    return;
  }
  const base::DictionaryValue& printer_dict =
      base::Value::AsDictionaryValue(printer_value);

  AllowJavascript();

  std::string printer_address;
  if (!printer_dict.GetString("printerAddress", &printer_address)) {
    NOTREACHED() << "Address missing";
    return;
  }

  std::string printer_queue;
  printer_dict.GetString("printerQueue", &printer_queue);
  // Path must start from '/' character.
  if (!printer_queue.empty() && printer_queue.front() != '/')
    printer_queue = "/" + printer_queue;

  std::string printer_protocol;
  if (!printer_dict.GetString("printerProtocol", &printer_protocol)) {
    NOTREACHED() << "Protocol missing";
    return;
  }

  DCHECK(printer_protocol == kIppScheme || printer_protocol == kIppsScheme)
      << "Printer info requests only supported for IPP and IPPS printers";

  Uri uri(printer_protocol + url::kStandardSchemeSeparator + printer_address +
          printer_queue);
  if (uri.GetLastParsingError().status != Uri::ParserStatus::kNoErrors ||
      !IsValidPrinterUri(uri)) {
    // Run the failure callback.
    OnAutoconfQueried(callback_id, PrinterQueryResult::kUnknownFailure,
                      printing::PrinterStatus(), "", {}, false);
    return;
  }

  PRINTER_LOG(DEBUG) << "Querying printer info";
  QueryAutoconf(uri, base::BindOnce(&CupsPrintersHandler::OnAutoconfQueried,
                                    weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnAutoconfQueriedDiscovered(
    const std::string& callback_id,
    Printer printer,
    PrinterQueryResult result,
    const printing::PrinterStatus& printer_status,
    const std::string& make_and_model,
    const std::vector<std::string>& document_formats,
    bool ipp_everywhere) {
  RecordIppQueryResult(result);

  const bool success = result == PrinterQueryResult::kSuccess;
  if (success) {
    // If we queried a valid make and model, use it.  The mDNS record isn't
    // guaranteed to have it.  However, don't overwrite it if the printer
    // advertises an empty value through printer-make-and-model.
    if (!make_and_model.empty()) {
      printer.set_make_and_model(make_and_model);
      PRINTER_LOG(DEBUG) << "Printer queried for make and model "
                         << make_and_model;
    }

    // Autoconfig available, use it.
    if (ipp_everywhere) {
      PRINTER_LOG(DEBUG) << "Performing autoconf setup";
      printer.mutable_ppd_reference()->autoconf = true;
      printer_configurer_->SetUpPrinter(
          printer,
          base::BindOnce(&CupsPrintersHandler::OnAddedDiscoveredPrinter,
                         weak_factory_.GetWeakPtr(), callback_id, printer));
      return;
    }
  }

  // We don't have enough from discovery to configure the printer.  Fill in as
  // much information as we can about the printer, and ask the user to supply
  // the rest.
  PRINTER_LOG(EVENT) << "Could not query printer.  Fallback to asking the user";
  RejectJavascriptCallback(base::Value(callback_id),
                           GetCupsPrinterInfo(printer));
}

void CupsPrintersHandler::OnAutoconfQueried(
    const std::string& callback_id,
    PrinterQueryResult result,
    const printing::PrinterStatus& printer_status,
    const std::string& make_and_model,
    const std::vector<std::string>& document_formats,
    bool ipp_everywhere) {
  RecordIppQueryResult(result);
  const bool success = result == PrinterQueryResult::kSuccess;

  if (result == PrinterQueryResult::kHostnameResolution ||
      result == PrinterQueryResult::kUnreachable) {
    PRINTER_LOG(DEBUG) << "Could not reach printer";
    RejectJavascriptCallback(
        base::Value(callback_id),
        base::Value(PrinterSetupResult::kPrinterUnreachable));
    return;
  }

  if (!success) {
    PRINTER_LOG(DEBUG) << "Could not query printer";
    base::DictionaryValue reject;
    reject.SetString("message", "Querying printer failed");
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(PrinterSetupResult::kFatalError));
    return;
  }

  PRINTER_LOG(DEBUG) << "Resolved printer information: make_and_model("
                     << make_and_model << ") autoconf(" << ipp_everywhere
                     << ")";

  // Bundle printer metadata
  base::Value info(base::Value::Type::DICTIONARY);
  info.SetKey("makeAndModel", base::Value(make_and_model));
  info.SetKey("autoconf", base::Value(ipp_everywhere));

  if (ipp_everywhere) {
    info.SetKey("ppdReferenceResolved", base::Value(true));
    ResolveJavascriptCallback(base::Value(callback_id), info);
    return;
  }

  PrinterSearchData ppd_search_data;
  ppd_search_data.discovery_type =
      PrinterSearchData::PrinterDiscoveryType::kManual;
  ppd_search_data.make_and_model.push_back(make_and_model);
  ppd_search_data.supported_document_formats = document_formats;

  // Try to resolve the PPD matching.
  ppd_provider_->ResolvePpdReference(
      ppd_search_data,
      base::BindOnce(&CupsPrintersHandler::OnPpdResolved,
                     weak_factory_.GetWeakPtr(), callback_id, std::move(info)));
}

void CupsPrintersHandler::OnPpdResolved(const std::string& callback_id,
                                        base::Value info,
                                        PpdProvider::CallbackResultCode res,
                                        const Printer::PpdReference& ppd_ref,
                                        const std::string& usb_manufacturer) {
  if (res != PpdProvider::CallbackResultCode::SUCCESS) {
    info.SetKey("ppdReferenceResolved", base::Value(false));
    ResolveJavascriptCallback(base::Value(callback_id), info);
    return;
  }

  SetPpdReference(ppd_ref, &info);
  info.SetKey("ppdReferenceResolved", base::Value(true));
  ResolveJavascriptCallback(base::Value(callback_id), info);
}

void CupsPrintersHandler::HandleAddCupsPrinter(const base::ListValue* args) {
  AllowJavascript();
  AddOrReconfigurePrinter(args, false /* is_printer_edit */);
}

void CupsPrintersHandler::HandleReconfigureCupsPrinter(
    const base::ListValue* args) {
  AllowJavascript();
  AddOrReconfigurePrinter(args, true /* is_printer_edit */);
}

void CupsPrintersHandler::AddOrReconfigurePrinter(const base::ListValue* args,
                                                  bool is_printer_edit) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  const base::Value& printer_value = args->GetList()[1];
  CHECK(printer_value.is_dict());
  const base::DictionaryValue& printer_dict =
      base::Value::AsDictionaryValue(printer_value);

  std::unique_ptr<Printer> printer = DictToPrinter(printer_dict);
  if (!printer) {
    PRINTER_LOG(ERROR) << "Failed to parse printer URI";
    OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kFatalError);
    return;
  }

  if (!profile_->GetPrefs()->GetBoolean(prefs::kUserPrintersAllowed)) {
    PRINTER_LOG(DEBUG) << "AddOrReconfigurePrinter() called when "
                          "kUserPrintersAllowed is set to false";
    OnAddedOrEditedPrinterCommon(*printer,
                                 PrinterSetupResult::kNativePrintersNotAllowed,
                                 false /* is_automatic */);
    // Used to fire the web UI listener.
    OnAddOrEditPrinterError(callback_id,
                            PrinterSetupResult::kNativePrintersNotAllowed);
    return;
  }

  // Grab the existing printer object and check that we are not making any
  // changes that will make |existing_printer_object| unusable.
  if (printer->id().empty()) {
    // If the printer object has not already been created, error out since this
    // is not a valid case.
    PRINTER_LOG(ERROR) << "Failed to parse printer ID";
    OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kFatalError);
    return;
  }

  absl::optional<Printer> existing_printer_object =
      printers_manager_->GetPrinter(printer->id());
  if (existing_printer_object) {
    if (!IsValidUriChange(*existing_printer_object, *printer)) {
      OnAddOrEditPrinterError(callback_id,
                              PrinterSetupResult::kInvalidPrinterUpdate);
      return;
    }
  }

  // Read PPD selection if it was used.
  std::string ppd_manufacturer;
  std::string ppd_model;
  printer_dict.GetString("ppdManufacturer", &ppd_manufacturer);
  printer_dict.GetString("ppdModel", &ppd_model);

  // Read user provided PPD if it was used.
  std::string printer_ppd_path;
  printer_dict.GetString("printerPPDPath", &printer_ppd_path);

  // Check if the printer already has a valid ppd_reference.
  Printer::PpdReference ppd_ref = GetPpdReference(&printer_dict);
  if (ppd_ref.IsFilled()) {
    *printer->mutable_ppd_reference() = ppd_ref;
  } else if (!printer_ppd_path.empty()) {
    GURL tmp = net::FilePathToFileURL(base::FilePath(printer_ppd_path));
    if (!tmp.is_valid()) {
      LOG(ERROR) << "Invalid ppd path: " << printer_ppd_path;
      OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kInvalidPpd);
      return;
    }
    printer->mutable_ppd_reference()->user_supplied_ppd_url = tmp.spec();
  } else if (!ppd_manufacturer.empty() && !ppd_model.empty()) {
    // Pull out the ppd reference associated with the selected manufacturer and
    // model.
    bool found = false;
    for (const auto& resolved_printer : resolved_printers_[ppd_manufacturer]) {
      if (resolved_printer.name == ppd_model) {
        *printer->mutable_ppd_reference() = resolved_printer.ppd_ref;
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "Failed to get ppd reference";
      OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kPpdNotFound);
      return;
    }

    if (printer->make_and_model().empty()) {
      // PPD Model names are actually make and model.
      printer->set_make_and_model(ppd_model);
    }
  } else {
    // TODO(https://crbug.com/738514): Support PPD guessing for non-autoconf
    // printers. i.e. !autoconf && !manufacturer.empty() && !model.empty()
    NOTREACHED()
        << "A configuration option must have been selected to add a printer";
  }

  printer_configurer_->SetUpPrinter(
      *printer,
      base::BindOnce(&CupsPrintersHandler::OnAddedOrEditedSpecifiedPrinter,
                     weak_factory_.GetWeakPtr(), callback_id, *printer,
                     is_printer_edit));
}

void CupsPrintersHandler::OnAddedOrEditedPrinterCommon(
    const Printer& printer,
    PrinterSetupResult result_code,
    bool is_automatic) {
  if (printer.IsZeroconf()) {
    UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.ZeroconfPrinterSetupResult",
                              result_code, PrinterSetupResult::kMaxValue);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterSetupResult", result_code,
                              PrinterSetupResult::kMaxValue);
  }

  switch (result_code) {
    case PrinterSetupResult::kSuccess:
      UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterAdded",
                                printer.GetProtocol(), Printer::kProtocolMax);
      PRINTER_LOG(USER) << "Performing printer setup";
      printers_manager_->PrinterInstalled(printer, is_automatic);
      printers_manager_->SavePrinter(printer);
      if (printer.IsUsbProtocol()) {
        // Record UMA for USB printer setup source.
        PrinterConfigurer::RecordUsbPrinterSetupSource(
            UsbPrinterSetupSource::kSettings);
      }
      return;
    case PrinterSetupResult::kEditSuccess:
      PRINTER_LOG(USER) << ResultCodeToMessage(result_code);
      printers_manager_->SavePrinter(printer);
      return;
    case PrinterSetupResult::kNativePrintersNotAllowed:
    case PrinterSetupResult::kBadUri:
    case PrinterSetupResult::kInvalidPrinterUpdate:
    case PrinterSetupResult::kPrinterUnreachable:
    case PrinterSetupResult::kPrinterSentWrongResponse:
    case PrinterSetupResult::kPrinterIsNotAutoconfigurable:
    case PrinterSetupResult::kPpdTooLarge:
    case PrinterSetupResult::kInvalidPpd:
    case PrinterSetupResult::kPpdNotFound:
    case PrinterSetupResult::kPpdUnretrievable:
    case PrinterSetupResult::kDbusError:
    case PrinterSetupResult::kDbusNoReply:
    case PrinterSetupResult::kDbusTimeout:
    case PrinterSetupResult::kIoError:
    case PrinterSetupResult::kMemoryAllocationError:
    case PrinterSetupResult::kFatalError:
      PRINTER_LOG(ERROR) << ResultCodeToMessage(result_code);
      break;
    case PrinterSetupResult::kComponentUnavailable:
    case PrinterSetupResult::kMaxValue:
      NOTREACHED() << ResultCodeToMessage(result_code);
      break;
  }
  // Log an event that tells us this printer setup failed, so we can get
  // statistics about which printers are giving users difficulty.
  printers_manager_->RecordSetupAbandoned(printer);
}

void CupsPrintersHandler::OnAddedDiscoveredPrinter(
    const std::string& callback_id,
    const Printer& printer,
    PrinterSetupResult result_code) {
  OnAddedOrEditedPrinterCommon(printer, result_code, /*is_automatic=*/true);
  if (result_code == PrinterSetupResult::kSuccess) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(result_code));
  } else {
    PRINTER_LOG(EVENT) << "Automatic setup failed for discovered printer.  "
                          "Fall back to manual.";
    // Could not set up printer.  Asking user for manufacturer data.
    RejectJavascriptCallback(base::Value(callback_id),
                             GetCupsPrinterInfo(printer));
  }
}

void CupsPrintersHandler::OnAddedOrEditedSpecifiedPrinter(
    const std::string& callback_id,
    const Printer& printer,
    bool is_printer_edit,
    PrinterSetupResult result_code) {
  if (is_printer_edit && result_code == PrinterSetupResult::kSuccess) {
    result_code = PrinterSetupResult::kEditSuccess;
  }
  PRINTER_LOG(EVENT) << "Add/Update manual printer: " << result_code;
  OnAddedOrEditedPrinterCommon(printer, result_code, /*is_automatic=*/false);

  if (result_code != PrinterSetupResult::kSuccess &&
      result_code != PrinterSetupResult::kEditSuccess) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(result_code));
    return;
  }

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(result_code));
}

void CupsPrintersHandler::OnAddOrEditPrinterError(
    const std::string& callback_id,
    PrinterSetupResult result_code) {
  PRINTER_LOG(EVENT) << "Add printer error: " << result_code;
  RejectJavascriptCallback(base::Value(callback_id), base::Value(result_code));
}

void CupsPrintersHandler::HandleGetCupsPrinterManufacturers(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  ppd_provider_->ResolveManufacturers(
      base::BindOnce(&CupsPrintersHandler::ResolveManufacturersDone,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::HandleGetCupsPrinterModels(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& manufacturer = args->GetList()[1].GetString();

  // Empty manufacturer queries may be triggered as a part of the ui
  // initialization, and should just return empty results.
  if (manufacturer.empty()) {
    base::DictionaryValue response;
    response.SetBoolean("success", true);
    response.SetKey("models", base::ListValue());
    ResolveJavascriptCallback(base::Value(callback_id), response);
    return;
  }

  ppd_provider_->ResolvePrinters(
      manufacturer,
      base::BindOnce(&CupsPrintersHandler::ResolvePrintersDone,
                     weak_factory_.GetWeakPtr(), manufacturer, callback_id));
}

void CupsPrintersHandler::HandleSelectPPDFile(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  webui_callback_id_ = args->GetList()[0].GetString();

  base::FilePath downloads_path =
      DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
          ->DownloadPath();

  content::WebContents* web_contents = web_ui()->GetWebContents();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  gfx::NativeWindow owning_window =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents)
                         ->window()
                         ->GetNativeWindow()
                   : gfx::kNullNativeWindow;

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.push_back({"ppd"});
  file_type_info.extensions.push_back({"ppd.gz"});
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(), downloads_path,
      &file_type_info, 0, FILE_PATH_LITERAL(""), owning_window, nullptr);
}

void CupsPrintersHandler::ResolveManufacturersDone(
    const std::string& callback_id,
    PpdProvider::CallbackResultCode result_code,
    const std::vector<std::string>& manufacturers) {
  base::ListValue manufacturers_value;
  if (result_code == PpdProvider::SUCCESS) {
    for (const std::string& manufacturer : manufacturers) {
      manufacturers_value.Append(manufacturer);
    }
  }
  base::DictionaryValue response;
  response.SetBoolean("success", result_code == PpdProvider::SUCCESS);
  response.SetKey("manufacturers", std::move(manufacturers_value));
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void CupsPrintersHandler::ResolvePrintersDone(
    const std::string& manufacturer,
    const std::string& callback_id,
    PpdProvider::CallbackResultCode result_code,
    const PpdProvider::ResolvedPrintersList& printers) {
  base::ListValue printers_value;
  if (result_code == PpdProvider::SUCCESS) {
    resolved_printers_[manufacturer] = printers;
    for (const auto& printer : printers) {
      printers_value.Append(printer.name);
    }
  }
  base::DictionaryValue response;
  response.SetBoolean("success", result_code == PpdProvider::SUCCESS);
  response.SetKey("models", std::move(printers_value));
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void CupsPrintersHandler::FileSelected(const base::FilePath& path,
                                       int index,
                                       void* params) {
  DCHECK(!webui_callback_id_.empty());

  // Load the beggining contents of the file located at |path| and callback into
  // VerifyPpdContents() in order to determine whether the file appears to be a
  // PPD file. The task's priority is USER_BLOCKING because the this task
  // updates the UI as a result of a direct user action.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFileToStringWithMaxSize, path, kPpdMaxLineLength),
      base::BindOnce(&CupsPrintersHandler::VerifyPpdContents,
                     weak_factory_.GetWeakPtr(), path));
}

void CupsPrintersHandler::VerifyPpdContents(const base::FilePath& path,
                                            const std::string& contents) {
  std::string result;
  if (PpdLineReader::ContainsMagicNumber(contents, kPpdMaxLineLength))
    result = path.value();

  ResolveJavascriptCallback(base::Value(webui_callback_id_),
                            base::Value(result));
  webui_callback_id_.clear();
}

void CupsPrintersHandler::HandleStartDiscovery(const base::ListValue* args) {
  PRINTER_LOG(DEBUG) << "Start printer discovery";
  AllowJavascript();
  discovery_active_ = true;
  OnPrintersChanged(PrinterClass::kAutomatic,
                    printers_manager_->GetPrinters(PrinterClass::kAutomatic));
  OnPrintersChanged(PrinterClass::kDiscovered,
                    printers_manager_->GetPrinters(PrinterClass::kDiscovered));
  UMA_HISTOGRAM_COUNTS_100(
      "Printing.CUPS.PrintersDiscovered",
      discovered_printers_.size() + automatic_printers_.size());
  printers_manager_->RecordNearbyNetworkPrinterCounts();
  // Scan completes immediately right now.  Emit done.
  FireWebUIListener("on-printer-discovery-done");
}

void CupsPrintersHandler::HandleStopDiscovery(const base::ListValue* args) {
  PRINTER_LOG(DEBUG) << "Stop printer discovery";
  discovered_printers_.clear();
  automatic_printers_.clear();

  // Free up memory while we're not discovering.
  discovered_printers_.shrink_to_fit();
  automatic_printers_.shrink_to_fit();
  discovery_active_ = false;
}

void CupsPrintersHandler::HandleSetUpCancel(const base::ListValue* args) {
  PRINTER_LOG(DEBUG) << "Printer setup cancelled";
  const base::Value& printer_value = args->GetList()[0];
  CHECK(printer_value.is_dict());

  std::unique_ptr<Printer> printer =
      DictToPrinter(base::Value::AsDictionaryValue(printer_value));
  if (printer) {
    printers_manager_->RecordSetupAbandoned(*printer);
  }
}

void CupsPrintersHandler::OnPrintersChanged(
    PrinterClass printer_class,
    const std::vector<Printer>& printers) {
  switch (printer_class) {
    case PrinterClass::kAutomatic:
      automatic_printers_ = printers;
      UpdateDiscoveredPrinters();
      break;
    case PrinterClass::kDiscovered:
      discovered_printers_ = printers;
      UpdateDiscoveredPrinters();
      break;
    case PrinterClass::kSaved: {
      auto printers_list = BuildCupsPrintersList(printers);
      FireWebUIListener("on-saved-printers-changed", printers_list);
      break;
    }
    case PrinterClass::kEnterprise:
      auto enterprise_printers_list = BuildCupsPrintersList(printers);
      FireWebUIListener("on-enterprise-printers-changed",
                        enterprise_printers_list);
  }
}

void CupsPrintersHandler::UpdateDiscoveredPrinters() {
  if (!discovery_active_) {
    PRINTER_LOG(DEBUG) << "Discovered printers update skipped";
    return;
  }

  std::unique_ptr<base::ListValue> automatic_printers_list =
      std::make_unique<base::ListValue>();
  for (const Printer& printer : automatic_printers_) {
    automatic_printers_list->Append(GetCupsPrinterInfo(printer));
  }

  std::unique_ptr<base::ListValue> discovered_printers_list =
      std::make_unique<base::ListValue>();
  for (const Printer& printer : discovered_printers_) {
    discovered_printers_list->Append(GetCupsPrinterInfo(printer));
  }

  PRINTER_LOG(DEBUG) << "Discovered printers updating. Automatic: "
                     << automatic_printers_list->GetList().size()
                     << " Discovered: "
                     << discovered_printers_list->GetList().size();
  FireWebUIListener("on-nearby-printers-changed", *automatic_printers_list,
                    *discovered_printers_list);
}

void CupsPrintersHandler::HandleAddDiscoveredPrinter(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& printer_id = args->GetList()[1].GetString();

  PRINTER_LOG(USER) << "Adding discovered printer";
  absl::optional<Printer> printer = printers_manager_->GetPrinter(printer_id);
  if (!printer) {
    PRINTER_LOG(ERROR) << "Discovered printer disappeared";
    // Printer disappeared, so we don't have information about it anymore and
    // can't really do much. Fail the add.
    ResolveJavascriptCallback(
        base::Value(callback_id),
        base::Value(PrinterSetupResult::kPrinterUnreachable));
    return;
  }

  if (!printer->HasUri()) {
    PRINTER_LOG(DEBUG) << "Could not parse uri";
    // The printer uri was not parsed successfully. Fail the add.
    ResolveJavascriptCallback(
        base::Value(callback_id),
        base::Value(PrinterSetupResult::kPrinterUnreachable));
    return;
  }

  if (printer->ppd_reference().autoconf ||
      !printer->ppd_reference().effective_make_and_model.empty() ||
      !printer->ppd_reference().user_supplied_ppd_url.empty()) {
    PRINTER_LOG(EVENT) << "Start setup of discovered printer";
    // If we have something that looks like a ppd reference for this printer,
    // try to configure it.
    printer_configurer_->SetUpPrinter(
        *printer,
        base::BindOnce(&CupsPrintersHandler::OnAddedDiscoveredPrinter,
                       weak_factory_.GetWeakPtr(), callback_id, *printer));
    return;
  }

  // We need a special case for USB printers here. We cannot query them
  // directly, so we have to fall back to manual configuration here.
  if (printer->IsUsbProtocol()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             GetCupsPrinterInfo(*printer));
    return;
  }

  // The mDNS record doesn't guarantee we can setup the printer.  Query it to
  // see if we want to try IPP.
  auto address = printer->GetHostAndPort();
  if (address.IsEmpty()) {
    PRINTER_LOG(ERROR) << "Address is invalid";
    OnAddedDiscoveredPrinter(callback_id, *printer,
                             PrinterSetupResult::kPrinterUnreachable);
    return;
  }
  endpoint_resolver_->Start(
      address, base::BindOnce(&CupsPrintersHandler::OnIpResolved,
                              weak_factory_.GetWeakPtr(), callback_id,
                              std::move(*printer)));
}

void CupsPrintersHandler::HandleGetPrinterPpdManufacturerAndModel(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& printer_id = args->GetList()[1].GetString();

  auto printer = printers_manager_->GetPrinter(printer_id);
  if (!printer) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  ppd_provider_->ReverseLookup(
      printer->ppd_reference().effective_make_and_model,
      base::BindOnce(&CupsPrintersHandler::OnGetPrinterPpdManufacturerAndModel,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnGetPrinterPpdManufacturerAndModel(
    const std::string& callback_id,
    PpdProvider::CallbackResultCode result_code,
    const std::string& manufacturer,
    const std::string& model) {
  if (result_code != PpdProvider::SUCCESS) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  base::DictionaryValue info;
  info.SetString("ppdManufacturer", manufacturer);
  info.SetString("ppdModel", model);
  ResolveJavascriptCallback(base::Value(callback_id), info);
}

void CupsPrintersHandler::HandleGetEulaUrl(const base::ListValue* args) {
  CHECK_EQ(3U, args->GetList().size());
  const std::string callback_id = args->GetList()[0].GetString();
  const std::string ppd_manufacturer = args->GetList()[1].GetString();
  const std::string ppd_model = args->GetList()[2].GetString();

  auto resolved_printers_it = resolved_printers_.find(ppd_manufacturer);
  if (resolved_printers_it == resolved_printers_.end()) {
    // Exit early if lookup for printers fails for |ppd_manufacturer|.
    OnGetEulaUrl(callback_id, PpdProvider::CallbackResultCode::NOT_FOUND,
                 /*license=*/std::string());
    return;
  }

  const PpdProvider::ResolvedPrintersList& printers_for_manufacturer =
      resolved_printers_it->second;

  auto printer_it = std::find_if(
      printers_for_manufacturer.begin(), printers_for_manufacturer.end(),
      [&ppd_model](const auto& elem) { return elem.name == ppd_model; });

  if (printer_it == printers_for_manufacturer.end()) {
    // Unable to find the PpdReference, resolve promise with empty string.
    OnGetEulaUrl(callback_id, PpdProvider::CallbackResultCode::NOT_FOUND,
                 /*license=*/std::string());
    return;
  }

  ppd_provider_->ResolvePpdLicense(
      printer_it->ppd_ref.effective_make_and_model,
      base::BindOnce(&CupsPrintersHandler::OnGetEulaUrl,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnGetEulaUrl(const std::string& callback_id,
                                       PpdProvider::CallbackResultCode result,
                                       const std::string& license) {
  if (result != PpdProvider::SUCCESS || license.empty()) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  GURL eula_url = PrinterConfigurer::GeneratePrinterEulaUrl(license);
  ResolveJavascriptCallback(
      base::Value(callback_id),
      eula_url.is_valid() ? base::Value(eula_url.spec()) : base::Value());
}

void CupsPrintersHandler::OnIpResolved(const std::string& callback_id,
                                       const Printer& printer,
                                       const net::IPEndPoint& endpoint) {
  bool address_resolved = endpoint.address().IsValid();
  UMA_HISTOGRAM_BOOLEAN("Printing.CUPS.AddressResolutionResult",
                        address_resolved);
  if (!address_resolved) {
    PRINTER_LOG(ERROR) << printer.make_and_model() << " IP Resolution failed";
    OnAddedDiscoveredPrinter(callback_id, printer,
                             PrinterSetupResult::kPrinterUnreachable);
    return;
  }

  PRINTER_LOG(EVENT) << printer.make_and_model() << " IP Resolution succeeded";
  const Uri uri = printer.ReplaceHostAndPort(endpoint);

  if (IsIppUri(uri)) {
    PRINTER_LOG(EVENT) << "Query printer for IPP attributes";
    QueryAutoconf(
        uri, base::BindOnce(&CupsPrintersHandler::OnAutoconfQueriedDiscovered,
                            weak_factory_.GetWeakPtr(), callback_id, printer));
    return;
  }

  PRINTER_LOG(EVENT) << "Request make and model from user";
  // If it's not an IPP printer, the user must choose a PPD.
  RejectJavascriptCallback(base::Value(callback_id),
                           GetCupsPrinterInfo(printer));
}

void CupsPrintersHandler::HandleQueryPrintServer(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& server_url = args->GetList()[1].GetString();

  absl::optional<GURL> converted_server_url =
      GenerateServerPrinterUrlWithValidScheme(server_url);
  if (!converted_server_url) {
    RejectJavascriptCallback(
        base::Value(callback_id),
        base::Value(PrintServerQueryResult::kIncorrectUrl));
    return;
  }

  // Use fallback only if HasValidServerPrinterScheme is false.
  QueryPrintServer(callback_id, converted_server_url.value(),
                   !HasValidServerPrinterScheme(GURL(server_url)));
}

void CupsPrintersHandler::QueryPrintServer(const std::string& callback_id,
                                           const GURL& server_url,
                                           bool should_fallback) {
  server_printers_fetcher_ = std::make_unique<ServerPrintersFetcher>(
      profile_, server_url, "(from user)",
      base::BindRepeating(&CupsPrintersHandler::OnQueryPrintServerCompleted,
                          weak_factory_.GetWeakPtr(), callback_id,
                          should_fallback));
}

void CupsPrintersHandler::OnQueryPrintServerCompleted(
    const std::string& callback_id,
    bool should_fallback,
    const ServerPrintersFetcher* sender,
    const GURL& server_url,
    std::vector<PrinterDetector::DetectedPrinter>&& returned_printers) {
  const PrintServerQueryResult result = sender->GetLastError();
  if (result != PrintServerQueryResult::kNoErrors) {
    if (should_fallback) {
      // Apply the fallback query.
      QueryPrintServer(callback_id, GenerateHttpCupsServerUrl(server_url),
                       /*should_fallback=*/false);
      return;
    }

    RejectJavascriptCallback(base::Value(callback_id), base::Value(result));
    return;
  }

  // Get all "saved" printers and organize them according to their URL.
  const std::vector<Printer> saved_printers =
      printers_manager_->GetPrinters(PrinterClass::kSaved);
  std::set<GURL> known_printers;
  for (const Printer& printer : saved_printers) {
    absl::optional<GURL> gurl =
        GenerateServerPrinterUrlWithValidScheme(printer.uri().GetNormalized());
    if (gurl)
      known_printers.insert(gurl.value());
  }

  // Built final list of printers and a list of current names. If "current name"
  // is a null value, then a corresponding printer is not saved in the profile
  // (it can be added).
  std::vector<Printer> printers;
  printers.reserve(returned_printers.size());
  for (PrinterDetector::DetectedPrinter& printer : returned_printers) {
    printers.push_back(std::move(printer.printer));
    absl::optional<GURL> printer_gurl = GenerateServerPrinterUrlWithValidScheme(
        printers.back().uri().GetNormalized());
    if (printer_gurl && known_printers.count(printer_gurl.value()))
      printers.pop_back();
  }

  // Delete fetcher object.
  server_printers_fetcher_.reset();

  // Create result value and finish the callback.
  base::Value result_dict = BuildCupsPrintersList(printers);
  ResolveJavascriptCallback(base::Value(callback_id), result_dict);
}

void CupsPrintersHandler::HandleOpenPrintManagementApp(
    const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  chrome::ShowPrintManagementApp(profile_);
}

void CupsPrintersHandler::HandleOpenScanningApp(const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  chrome::ShowScanningApp(profile_);
}

}  // namespace settings
}  // namespace chromeos
