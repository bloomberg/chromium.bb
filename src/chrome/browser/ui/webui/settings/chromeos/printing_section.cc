// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/printing_section.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetPrintingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddPrinter},
       {IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER_ALT1,
        IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PRINTING_SAVED_PRINTERS,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSavedPrinters}},
      {IDS_OS_SETTINGS_TAG_PRINTING,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPrintingDetails},
       {IDS_OS_SETTINGS_TAG_PRINTING_ALT1, IDS_OS_SETTINGS_TAG_PRINTING_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPrintingManagementSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRINT_MANAGEMENT,
       mojom::kPrintingSectionPath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPrintJobs},
       {IDS_OS_SETTINGS_TAG_PRINT_MANAGEMENT_ALT1,
        IDS_OS_SETTINGS_TAG_PRINT_MANAGEMENT_ALT2, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool IsPrintManagementEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kPrintJobManagementApp);
}

}  // namespace

PrintingSection::PrintingSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry,
                                 CupsPrintersManager* printers_manager)
    : OsSettingsSection(profile, search_tag_registry),
      printers_manager_(printers_manager) {
  registry()->AddSearchTags(GetPrintingSearchConcepts());
  if (IsPrintManagementEnabled())
    registry()->AddSearchTags(GetPrintingManagementSearchConcepts());
}

PrintingSection::~PrintingSection() = default;

void PrintingSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"printingPageTitle", IDS_SETTINGS_PRINTING},
      {"cupsPrintersTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTERS},
      {"cupsPrintersLearnMoreLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_LEARN_MORE_LABEL},
      {"addCupsPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_ADD_PRINTER},
      {"editPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_EDIT},
      {"removePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_REMOVE},
      {"setupPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SETUP_BUTTON},
      {"setupPrinterAria",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_SETUP_BUTTON_ARIA},
      {"savePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SAVE_BUTTON},
      {"savePrinterAria", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SAVE_BUTTON_ARIA},
      {"searchLabel", IDS_SETTINGS_PRINTING_CUPS_SEARCH_LABEL},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
      {"printJobsTitle",
       IDS_SETTINGS_PRINTING_PRINT_JOBS_LAUNCH_APP_TITLE_LABEL},
      {"printJobsSublabel",
       IDS_SETTINGS_PRINTING_PRINT_JOBS_LAUNCH_APP_SUBLABEL},
      {"printerDetailsTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_TITLE},
      {"printerName", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_NAME},
      {"printerModel", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_MODEL},
      {"printerQueue", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_QUEUE},
      {"savedPrintersTitle", IDS_SETTINGS_PRINTING_CUPS_SAVED_PRINTERS_TITLE},
      {"savedPrintersCountMany",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_MANY},
      {"savedPrintersCountOne",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_ONE},
      {"savedPrintersCountNone",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_NONE},
      {"showMorePrinters", IDS_SETTINGS_PRINTING_CUPS_SHOW_MORE},
      {"addPrintersNearbyTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_NEARBY_TITLE},
      {"addPrintersManuallyTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_MANUALLY_TITLE},
      {"manufacturerAndModelDialogTitle",
       IDS_SETTINGS_PRINTING_CUPS_SELECT_MANUFACTURER_AND_MODEL_TITLE},
      {"nearbyPrintersListTitle",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTERS},
      {"nearbyPrintersCountMany",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTERS_COUNT_MANY},
      {"nearbyPrintersCountOne",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTER_COUNT_ONE},
      {"nearbyPrintersCountNone",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTER_COUNT_NONE},
      {"nearbyPrintersListDescription",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_ADD_DETECTED_OR_NEW_PRINTER},
      {"manufacturerAndModelAdditionalInformation",
       IDS_SETTINGS_PRINTING_CUPS_MANUFACTURER_MODEL_ADDITIONAL_INFORMATION},
      {"addPrinterButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_ADD},
      {"printerDetailsAdvanced", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED},
      {"printerDetailsA11yLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ACCESSIBILITY_LABEL},
      {"printerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ADDRESS},
      {"printerProtocol", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_PROTOCOL},
      {"printerURI", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_URI},
      {"manuallyAddPrinterButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_MANUAL_ADD},
      {"discoverPrintersButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_DISCOVER_PRINTERS},
      {"printerProtocolIpp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPP},
      {"printerProtocolIpps", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPPS},
      {"printerProtocolHttp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTP},
      {"printerProtocolHttps",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTPS},
      {"printerProtocolAppSocket",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_APP_SOCKET},
      {"printerProtocolLpd", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_LPD},
      {"printerProtocolUsb", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_USB},
      {"printerProtocolIppUsb",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPPUSB},
      {"printerConfiguringMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CONFIGURING_MESSAGE},
      {"printerManufacturer", IDS_SETTINGS_PRINTING_CUPS_PRINTER_MANUFACTURER},
      {"selectDriver", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SELECT_DRIVER},
      {"selectDriverButtonText",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_BUTTON_SELECT_DRIVER},
      {"selectDriverButtonAriaLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_BUTTON_SELECT_DRIVER_ARIA_LABEL},
      {"selectDriverErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_INVALID_DRIVER},
      {"printerAddedSuccessfulMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_DONE_MESSAGE},
      {"printerEditedSuccessfulMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_EDITED_PRINTER_DONE_MESSAGE},
      {"printerUnavailableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_UNAVAILABLE_MESSAGE},
      {"noPrinterNearbyMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_NO_PRINTER_NEARBY},
      {"searchingNearbyPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_SEARCHING_NEARBY_PRINTER},
      {"printerAddedFailedMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_ERROR_MESSAGE},
      {"printerAddedFatalErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_FATAL_ERROR_MESSAGE},
      {"printerAddedUnreachableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PRINTER_UNREACHABLE_MESSAGE},
      {"printerAddedPpdTooLargeMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_TOO_LARGE_MESSAGE},
      {"printerAddedInvalidPpdMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_INVALID_PPD_MESSAGE},
      {"printerAddedPpdNotFoundMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_NOT_FOUND},
      {"printerAddedPpdUnretrievableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_UNRETRIEVABLE},
      {"printerAddedNativePrintersNotAllowedMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_NATIVE_PRINTERS_NOT_ALLOWED_MESSAGE},
      {"editPrinterInvalidPrinterUpdate",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_INVALID_PRINTER_UPDATE},
      {"requireNetworkMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_REQUIRE_INTERNET_MESSAGE},
      {"checkNetworkMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CHECK_CONNECTION_MESSAGE},
      {"noInternetConnection",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_NO_INTERNET_CONNECTION},
      {"checkNetworkAndTryAgain",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CONNECT_TO_NETWORK_SUBTEXT},
      {"editPrinterDialogTitle",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_DIALOG_TITLE},
      {"editPrinterButtonText", IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_BUTTON},
      {"currentPpdMessage",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_CURRENT_PPD_MESSAGE},
      {"printerEulaNotice", IDS_SETTINGS_PRINTING_CUPS_EULA_NOTICE},
      {"ippPrinterUnreachable", IDS_SETTINGS_PRINTING_CUPS_IPP_URI_UNREACHABLE},
      {"generalPrinterDialogError",
       IDS_SETTINGS_PRINTING_CUPS_DIALOG_GENERAL_ERROR},
      {"printServerButtonText", IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER},
      {"addPrintServerTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINT_SERVER_TITLE},
      {"printServerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_ADDRESS},
      {"printServerFoundZeroPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_ZERO_PRINTERS},
      {"printServerFoundOnePrinter",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_ONE_PRINTER},
      {"printServerFoundManyPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_MANY_PRINTERS},
      {"printServerInvalidUrlAddress",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_INVALID_URL_ADDRESS},
      {"printServerConnectionError",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_CONNECTION_ERROR},
      {"printServerConfigurationErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_REACHABLE_BUT_CANNOT_ADD},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("printingCUPSPrintLearnMoreUrl",
                         GetHelpUrlWithBoard(chrome::kCupsPrintLearnMoreURL));
  html_source->AddString(
      "printingCUPSPrintPpdLearnMoreUrl",
      GetHelpUrlWithBoard(chrome::kCupsPrintPPDLearnMoreURL));
  html_source->AddBoolean(
      "consumerPrintServerUiEnabled",
      base::FeatureList::IsEnabled(::features::kPrintServerUi));
  html_source->AddBoolean("printManagementEnabled", IsPrintManagementEnabled());
}

void PrintingSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<CupsPrintersHandler>(profile(), printers_manager_));
}

}  // namespace settings
}  // namespace chromeos
