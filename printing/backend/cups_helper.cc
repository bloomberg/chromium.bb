// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_helper.h"

#include <cups/ppd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"

// This section contains helper code for PPD parsing for semantic capabilities.
namespace {

const char kColorDevice[] = "ColorDevice";
const char kColorModel[] = "ColorModel";
const char kColorMode[] = "ColorMode";
const char kProcessColorModel[] = "ProcessColorModel";
const char kPrintoutMode[] = "PrintoutMode";
const char kDraftGray[] = "Draft.Gray";
const char kHighGray[] = "High.Gray";

const char kDuplex[] = "Duplex";
const char kDuplexNone[] = "None";

#if !defined(OS_MACOSX)
void ParseLpOptions(const FilePath& filepath, const std::string& printer_name,
                    int* num_options, cups_option_t** options) {
  std::string content;
  if (!file_util::ReadFileToString(filepath, &content))
    return;

  const char kDest[] = "dest";
  const char kDefault[] = "default";
  const size_t kDestLen = sizeof(kDest) - 1;
  const size_t kDefaultLen = sizeof(kDefault) - 1;
  std::vector<std::string> lines;
  base::SplitString(content, '\n', &lines);

  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = lines[i];
    if (line.empty())
      continue;

    if (base::strncasecmp (line.c_str(), kDefault, kDefaultLen) == 0 &&
        isspace(line[kDefaultLen])) {
      line = line.substr(kDefaultLen);
    } else if (base::strncasecmp (line.c_str(), kDest, kDestLen) == 0 &&
               isspace(line[kDestLen])) {
      line = line.substr(kDestLen);
    } else {
      continue;
    }

    TrimWhitespaceASCII(line, TRIM_ALL, &line);
    if (line.empty())
      continue;

    size_t space_found = line.find(' ');
    if (space_found == std::string::npos)
      continue;

    std::string name = line.substr(0, space_found);
    if (name.empty())
      continue;

    if (base::strncasecmp(printer_name.c_str(), name.c_str(),
                          name.length()) != 0) {
      continue;  // This is not the required printer.
    }

    line = line.substr(space_found + 1);
    TrimWhitespaceASCII(line, TRIM_ALL, &line);  // Remove extra spaces.
    if (line.empty())
      continue;
    // Parse the selected printer custom options.
    *num_options = cupsParseOptions(line.c_str(), 0, options);
  }
}

void MarkLpOptions(const std::string& printer_name, ppd_file_t** ppd) {
  cups_option_t* options = NULL;
  int num_options = 0;
  ppdMarkDefaults(*ppd);

  const char kSystemLpOptionPath[] = "/etc/cups/lpoptions";
  const char kUserLpOptionPath[] = ".cups/lpoptions";

  std::vector<FilePath> file_locations;
  file_locations.push_back(FilePath(kSystemLpOptionPath));
  file_locations.push_back(FilePath(
      file_util::GetHomeDir().Append(kUserLpOptionPath)));

  for (std::vector<FilePath>::const_iterator it = file_locations.begin();
       it != file_locations.end(); ++it) {
    num_options = 0;
    options = NULL;
    ParseLpOptions(*it, printer_name, &num_options, &options);
    if (num_options > 0 && options) {
      cupsMarkOptions(*ppd, num_options, options);
      cupsFreeOptions(num_options, options);
    }
  }
}
#endif  // !defined(OS_MACOSX)

bool GetBasicColorModelSettings(ppd_file_t* ppd,
                                int* color_model_for_black,
                                int* color_model_for_color,
                                bool* color_is_default) {
  ppd_option_t* color_model = ppdFindOption(ppd, kColorModel);
  if (!color_model)
    return false;

  if (ppdFindChoice(color_model, printing::kBlack))
    *color_model_for_black = printing::BLACK;
  else if (ppdFindChoice(color_model, printing::kGray))
    *color_model_for_black = printing::GRAY;
  else if (ppdFindChoice(color_model, printing::kGrayscale))
    *color_model_for_black = printing::GRAYSCALE;

  if (ppdFindChoice(color_model, printing::kColor))
    *color_model_for_color = printing::COLOR;
  else if (ppdFindChoice(color_model, printing::kCMYK))
    *color_model_for_color = printing::CMYK;
  else if (ppdFindChoice(color_model, printing::kRGB))
    *color_model_for_color = printing::RGB;
  else if (ppdFindChoice(color_model, printing::kRGBA))
    *color_model_for_color = printing::RGBA;
  else if (ppdFindChoice(color_model, printing::kRGB16))
    *color_model_for_color = printing::RGB16;
  else if (ppdFindChoice(color_model, printing::kCMY))
    *color_model_for_color = printing::CMY;
  else if (ppdFindChoice(color_model, printing::kKCMY))
    *color_model_for_color = printing::KCMY;
  else if (ppdFindChoice(color_model, printing::kCMY_K))
    *color_model_for_color = printing::CMY_K;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kColorModel);
  if (!marked_choice)
    marked_choice = ppdFindChoice(color_model, color_model->defchoice);

  if (marked_choice) {
    *color_is_default =
        (base::strcasecmp(marked_choice->choice, printing::kBlack) != 0) &&
        (base::strcasecmp(marked_choice->choice, printing::kGray) != 0) &&
        (base::strcasecmp(marked_choice->choice, printing::kGrayscale) != 0);
  }
  return true;
}

bool GetPrintOutModeColorSettings(ppd_file_t* ppd,
                                  int* color_model_for_black,
                                  int* color_model_for_color,
                                  bool* color_is_default) {
  ppd_option_t* printout_mode = ppdFindOption(ppd, kPrintoutMode);
  if (!printout_mode)
    return false;

  *color_model_for_color = printing::PRINTOUTMODE_NORMAL;
  *color_model_for_black = printing::PRINTOUTMODE_NORMAL;

  // Check to see if NORMAL_GRAY value is supported by PrintoutMode.
  // If NORMAL_GRAY is not supported, NORMAL value is used to
  // represent grayscale. If NORMAL_GRAY is supported, NORMAL is used to
  // represent color.
  if (ppdFindChoice(printout_mode, printing::kNormalGray))
    *color_model_for_black = printing::PRINTOUTMODE_NORMAL_GRAY;

  // Get the default marked choice to identify the default color setting
  // value.
  ppd_choice_t* printout_mode_choice = ppdFindMarkedChoice(ppd, kPrintoutMode);
  if (!printout_mode_choice) {
      printout_mode_choice = ppdFindChoice(printout_mode,
                                           printout_mode->defchoice);
  }
  if (printout_mode_choice) {
    if ((base::strcasecmp(printout_mode_choice->choice,
                          printing::kNormalGray) == 0) ||
        (base::strcasecmp(printout_mode_choice->choice, kHighGray) == 0) ||
        (base::strcasecmp(printout_mode_choice->choice, kDraftGray) == 0)) {
      *color_model_for_black = printing::PRINTOUTMODE_NORMAL_GRAY;
      *color_is_default = false;
    }
  }
  return true;
}

bool GetColorModeSettings(ppd_file_t* ppd,
                          int* color_model_for_black,
                          int* color_model_for_color,
                          bool* color_is_default) {
  // Samsung printers use "ColorMode" attribute in their ppds.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kColorMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, printing::kColor))
    *color_model_for_color = printing::COLORMODE_COLOR;

  if (ppdFindChoice(color_mode_option, printing::kMonochrome))
    *color_model_for_black = printing::COLORMODE_MONOCHROME;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!mode_choice) {
    mode_choice = ppdFindChoice(color_mode_option,
                                color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        (base::strcasecmp(mode_choice->choice, printing::kColor) == 0);
  }
  return true;
}

bool GetHPColorSettings(ppd_file_t* ppd,
                        int* color_model_for_black,
                        int* color_model_for_color,
                        bool* color_is_default) {
  // HP printers use "Color/Color Model" attribute in their ppds.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, printing::kColor);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, printing::kColor))
    *color_model_for_color = printing::HP_COLOR_COLOR;
  if (ppdFindChoice(color_mode_option, printing::kBlack))
    *color_model_for_black = printing::HP_COLOR_BLACK;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!mode_choice) {
    mode_choice = ppdFindChoice(color_mode_option,
                                color_mode_option->defchoice);
  }
  if (mode_choice) {
    *color_is_default =
        (base::strcasecmp(mode_choice->choice, printing::kColor) == 0);
  }
  return true;
}

bool GetProcessColorModelSettings(ppd_file_t* ppd,
                                  int* color_model_for_black,
                                  int* color_model_for_color,
                                  bool* color_is_default) {
  // Canon printers use "ProcessColorModel" attribute in their ppds.
  ppd_option_t* color_mode_option =  ppdFindOption(ppd, kProcessColorModel);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, printing::kRGB))
    *color_model_for_color = printing::PROCESSCOLORMODEL_RGB;
  else if (ppdFindChoice(color_mode_option, printing::kCMYK))
    *color_model_for_color = printing::PROCESSCOLORMODEL_CMYK;

  if (ppdFindChoice(color_mode_option, printing::kGreyscale))
    *color_model_for_black = printing::PROCESSCOLORMODEL_GREYSCALE;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kProcessColorModel);
  if (!mode_choice) {
    mode_choice = ppdFindChoice(color_mode_option,
                                color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        (base::strcasecmp(mode_choice->choice, printing::kGreyscale) != 0);
  }
  return true;
}

bool GetColorModelSettings(ppd_file_t* ppd,
                           int* cm_black,
                           int* cm_color,
                           bool* is_color) {
  bool is_color_device = false;
  ppd_attr_t* attr = ppdFindAttr(ppd, kColorDevice, NULL);
  if (attr && attr->value)
    is_color_device = ppd->color_device;

  *is_color = is_color_device;
  return (is_color_device &&
          GetBasicColorModelSettings(ppd, cm_black, cm_color, is_color)) ||
      GetPrintOutModeColorSettings(ppd, cm_black, cm_color, is_color) ||
      GetColorModeSettings(ppd, cm_black, cm_color, is_color) ||
      GetHPColorSettings(ppd, cm_black, cm_color, is_color) ||
      GetProcessColorModelSettings(ppd, cm_black, cm_color, is_color);
}

}  // namespace

namespace printing {

// Default port for IPP print servers.
static const int kDefaultIPPServerPort = 631;

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
HttpConnectionCUPS::HttpConnectionCUPS(const GURL& print_server_url,
                                       http_encryption_t encryption)
    : http_(NULL) {
  // If we have an empty url, use default print server.
  if (print_server_url.is_empty())
    return;

  int port = print_server_url.IntPort();
  if (port == url_parse::PORT_UNSPECIFIED)
    port = kDefaultIPPServerPort;

  http_ = httpConnectEncrypt(print_server_url.host().c_str(), port,
                             encryption);
  if (http_ == NULL) {
    LOG(ERROR) << "CP_CUPS: Failed connecting to print server: " <<
               print_server_url;
  }
}

HttpConnectionCUPS::~HttpConnectionCUPS() {
  if (http_ != NULL)
    httpClose(http_);
}

void HttpConnectionCUPS::SetBlocking(bool blocking) {
  httpBlocking(http_, blocking ?  1 : 0);
}

http_t* HttpConnectionCUPS::http() {
  return http_;
}

bool parsePpdCapabilities(
    const std::string& printer_name,
    const std::string& printer_capabilities,
    PrinterSemanticCapsAndDefaults* printer_info) {
  FilePath ppd_file_path;
  if (!file_util::CreateTemporaryFile(&ppd_file_path))
    return false;

  int data_size = printer_capabilities.length();
  if (data_size != file_util::WriteFile(
                       ppd_file_path,
                       printer_capabilities.data(),
                       data_size)) {
    file_util::Delete(ppd_file_path, false);
    return false;
  }

  ppd_file_t* ppd = ppdOpenFile(ppd_file_path.value().c_str());
  if (!ppd)
    return false;

  printing::PrinterSemanticCapsAndDefaults caps;
#if !defined(OS_MACOSX)
  MarkLpOptions(printer_name, &ppd);
#endif
  ppd_choice_t* duplex_choice = ppdFindMarkedChoice(ppd, kDuplex);
  if (!duplex_choice) {
    ppd_option_t* option = ppdFindOption(ppd, kDuplex);
    if (option)
      duplex_choice = ppdFindChoice(option, option->defchoice);
  }

  if (duplex_choice) {
    caps.duplex_capable = true;
    if (base::strcasecmp(duplex_choice->choice, kDuplexNone) != 0)
      caps.duplex_default = printing::LONG_EDGE;
    else
      caps.duplex_default = printing::SIMPLEX;
  }

  bool is_color = false;
  int cm_color = 0, cm_black = 0;
  if (!GetColorModelSettings(ppd, &cm_black, &cm_color, &is_color)) {
    VLOG(1) << "Unknown printer color model";
  }

  caps.color_capable = (cm_color && cm_black && (cm_color != cm_black));
  caps.color_default = is_color;

  ppdClose(ppd);
  file_util::Delete(ppd_file_path, false);

  *printer_info = caps;
  return true;
}

}  // namespace printing
