// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cjs_util.h"

#include <math.h>
#include <time.h>

#include <algorithm>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "core/fxcrt/fx_extension.h"
#include "fxjs/cjs_event_context.h"
#include "fxjs/cjs_object.h"
#include "fxjs/cjs_publicmethods.h"
#include "fxjs/cjs_runtime.h"
#include "fxjs/fx_date_helpers.h"
#include "fxjs/fxv8.h"
#include "fxjs/js_define.h"
#include "fxjs/js_resources.h"
#include "third_party/base/check_op.h"
#include "v8/include/v8-date.h"

#if BUILDFLAG(IS_ANDROID)
#include <ctype.h>
#endif

namespace {

// Map PDF-style directives to equivalent wcsftime directives. Not
// all have direct equivalents, though.
struct TbConvert {
  const wchar_t* lpszJSMark;
  const wchar_t* lpszCppMark;
};

// Map PDF-style directives lacking direct wcsftime directives to
// the value with which they will be replaced.
struct TbConvertAdditional {
  wchar_t js_mark;
  int value;
};

const TbConvert TbConvertTable[] = {
    {L"mmmm", L"%B"}, {L"mmm", L"%b"}, {L"mm", L"%m"},   {L"dddd", L"%A"},
    {L"ddd", L"%a"},  {L"dd", L"%d"},  {L"yyyy", L"%Y"}, {L"yy", L"%y"},
    {L"HH", L"%H"},   {L"hh", L"%I"},  {L"MM", L"%M"},   {L"ss", L"%S"},
    {L"TT", L"%p"},
#if BUILDFLAG(IS_WIN)
    {L"tt", L"%p"},   {L"h", L"%#I"},
#else
    {L"tt", L"%P"},   {L"h", L"%l"},
#endif
};

enum CaseMode { kPreserveCase, kUpperCase, kLowerCase };

wchar_t TranslateCase(wchar_t input, CaseMode eMode) {
  if (eMode == kLowerCase && FXSYS_iswupper(input))
    return input | 0x20;
  if (eMode == kUpperCase && FXSYS_iswlower(input))
    return input & ~0x20;
  return input;
}

}  // namespace

const JSMethodSpec CJS_Util::MethodSpecs[] = {
    {"printd", printd_static},
    {"printf", printf_static},
    {"printx", printx_static},
    {"scand", scand_static},
    {"byteToChar", byteToChar_static}};

uint32_t CJS_Util::ObjDefnID = 0;
const char CJS_Util::kName[] = "util";

// static
uint32_t CJS_Util::GetObjDefnID() {
  return ObjDefnID;
}

// static
void CJS_Util::DefineJSObjects(CFXJS_Engine* pEngine) {
  ObjDefnID = pEngine->DefineObj(CJS_Util::kName, FXJSOBJTYPE_STATIC,
                                 JSConstructor<CJS_Util>, JSDestructor);
  DefineMethods(pEngine, ObjDefnID, MethodSpecs);
}

CJS_Util::CJS_Util(v8::Local<v8::Object> pObject, CJS_Runtime* pRuntime)
    : CJS_Object(pObject, pRuntime) {}

CJS_Util::~CJS_Util() = default;

CJS_Result CJS_Util::printf(CJS_Runtime* pRuntime,
                            const std::vector<v8::Local<v8::Value>>& params) {
  const size_t num_params = params.size();
  if (num_params < 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  // Use 'S' as a sentinel to ensure we always have some text before the first
  // format specifier.
  WideString unsafe_fmt_string = L'S' + pRuntime->ToWideString(params[0]);
  std::vector<WideString> unsafe_conversion_specifiers;

  {
    size_t offset = 0;
    while (true) {
      absl::optional<size_t> offset_end =
          unsafe_fmt_string.Find(L"%", offset + 1);
      if (!offset_end.has_value()) {
        unsafe_conversion_specifiers.push_back(
            unsafe_fmt_string.Last(unsafe_fmt_string.GetLength() - offset));
        break;
      }

      unsafe_conversion_specifiers.push_back(
          unsafe_fmt_string.Substr(offset, offset_end.value() - offset));
      offset = offset_end.value();
    }
  }

  WideString result = unsafe_conversion_specifiers[0];
  for (size_t i = 1; i < unsafe_conversion_specifiers.size(); ++i) {
    WideString fmt = unsafe_conversion_specifiers[i];
    if (i >= num_params) {
      result += fmt;
      continue;
    }

    WideString segment;
    switch (ParseDataType(&fmt)) {
      case DataType::kInt:
        segment = WideString::Format(fmt.c_str(), pRuntime->ToInt32(params[i]));
        break;
      case DataType::kDouble:
        segment =
            WideString::Format(fmt.c_str(), pRuntime->ToDouble(params[i]));
        break;
      case DataType::kString:
        segment = WideString::Format(fmt.c_str(),
                                     pRuntime->ToWideString(params[i]).c_str());
        break;
      default:
        segment = WideString::Format(L"%ls", fmt.c_str());
        break;
    }
    result += segment;
  }

  // Remove the 'S' sentinel introduced earlier.
  DCHECK_EQ(L'S', result[0]);
  auto result_view = result.AsStringView();
  return CJS_Result::Success(
      pRuntime->NewString(result_view.Last(result_view.GetLength() - 1)));
}

CJS_Result CJS_Util::printd(CJS_Runtime* pRuntime,
                            const std::vector<v8::Local<v8::Value>>& params) {
  const size_t iSize = params.size();
  if (iSize < 2)
    return CJS_Result::Failure(JSMessage::kParamError);

  if (!fxv8::IsDate(params[1]))
    return CJS_Result::Failure(JSMessage::kSecondParamNotDateError);

  v8::Local<v8::Date> v8_date = params[1].As<v8::Date>();
  if (v8_date.IsEmpty() || isnan(pRuntime->ToDouble(v8_date)))
    return CJS_Result::Failure(JSMessage::kSecondParamInvalidDateError);

  double date = FX_LocalTime(pRuntime->ToDouble(v8_date));
  int year = FX_GetYearFromTime(date);
  int month = FX_GetMonthFromTime(date) + 1;  // One-based.
  int day = FX_GetDayFromTime(date);
  int hour = FX_GetHourFromTime(date);
  int min = FX_GetMinFromTime(date);
  int sec = FX_GetSecFromTime(date);

  if (params[0]->IsNumber()) {
    WideString swResult;
    switch (pRuntime->ToInt32(params[0])) {
      case 0:
        swResult = WideString::Format(L"D:%04d%02d%02d%02d%02d%02d", year,
                                      month, day, hour, min, sec);
        break;
      case 1:
        swResult = WideString::Format(L"%04d.%02d.%02d %02d:%02d:%02d", year,
                                      month, day, hour, min, sec);
        break;
      case 2:
        swResult = WideString::Format(L"%04d/%02d/%02d %02d:%02d:%02d", year,
                                      month, day, hour, min, sec);
        break;
      default:
        return CJS_Result::Failure(JSMessage::kValueError);
    }

    return CJS_Result::Success(pRuntime->NewString(swResult.AsStringView()));
  }

  if (!params[0]->IsString())
    return CJS_Result::Failure(JSMessage::kTypeError);

  // We don't support XFAPicture at the moment.
  if (iSize > 2 && pRuntime->ToBoolean(params[2]))
    return CJS_Result::Failure(JSMessage::kNotSupportedError);

  // Convert PDF-style format specifiers to wcsftime specifiers. Remove any
  // pre-existing %-directives before inserting our own.
  std::wstring cFormat = pRuntime->ToWideString(params[0]).c_str();
  cFormat.erase(std::remove(cFormat.begin(), cFormat.end(), '%'),
                cFormat.end());

  for (size_t i = 0; i < std::size(TbConvertTable); ++i) {
    size_t nFound = 0;
    while (true) {
      nFound = cFormat.find(TbConvertTable[i].lpszJSMark, nFound);
      if (nFound == std::wstring::npos)
        break;

      cFormat.replace(nFound, wcslen(TbConvertTable[i].lpszJSMark),
                      TbConvertTable[i].lpszCppMark);
    }
  }

  if (year < 0)
    return CJS_Result::Failure(JSMessage::kValueError);

  const TbConvertAdditional cTableAd[] = {
      {L'm', month}, {L'd', day},
      {L'H', hour},  {L'h', hour > 12 ? hour - 12 : hour},
      {L'M', min},   {L's', sec},
  };

  for (size_t i = 0; i < std::size(cTableAd); ++i) {
    size_t nFound = 0;
    while (true) {
      nFound = cFormat.find(cTableAd[i].js_mark, nFound);
      if (nFound == std::wstring::npos)
        break;

      if (nFound != 0 && cFormat[nFound - 1] == L'%') {
        ++nFound;
        continue;
      }
      cFormat.replace(nFound, 1,
                      WideString::FormatInteger(cTableAd[i].value).c_str());
    }
  }

  struct tm time = {};
  time.tm_year = year - 1900;
  time.tm_mon = month - 1;
  time.tm_mday = day;
  time.tm_hour = hour;
  time.tm_min = min;
  time.tm_sec = sec;

  wchar_t buf[64] = {};
  FXSYS_wcsftime(buf, 64, cFormat.c_str(), &time);
  cFormat = buf;
  return CJS_Result::Success(pRuntime->NewString(cFormat.c_str()));
}

CJS_Result CJS_Util::printx(CJS_Runtime* pRuntime,
                            const std::vector<v8::Local<v8::Value>>& params) {
  if (params.size() < 2)
    return CJS_Result::Failure(JSMessage::kParamError);

  return CJS_Result::Success(
      pRuntime->NewString(StringPrintx(pRuntime->ToWideString(params[0]),
                                       pRuntime->ToWideString(params[1]))
                              .AsStringView()));
}

// static
WideString CJS_Util::StringPrintx(const WideString& wsFormat,
                                  const WideString& wsSource) {
  WideString wsResult;
  wsResult.Reserve(wsFormat.GetLength());
  size_t iSourceIdx = 0;
  size_t iFormatIdx = 0;
  CaseMode eCaseMode = kPreserveCase;
  bool bEscaped = false;
  while (iFormatIdx < wsFormat.GetLength()) {
    if (bEscaped) {
      bEscaped = false;
      wsResult += wsFormat[iFormatIdx];
      ++iFormatIdx;
      continue;
    }
    switch (wsFormat[iFormatIdx]) {
      case '\\': {
        bEscaped = true;
        ++iFormatIdx;
      } break;
      case '<': {
        eCaseMode = kLowerCase;
        ++iFormatIdx;
      } break;
      case '>': {
        eCaseMode = kUpperCase;
        ++iFormatIdx;
      } break;
      case '=': {
        eCaseMode = kPreserveCase;
        ++iFormatIdx;
      } break;
      case '?': {
        if (iSourceIdx < wsSource.GetLength()) {
          wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
          ++iSourceIdx;
        }
        ++iFormatIdx;
      } break;
      case 'X': {
        if (iSourceIdx < wsSource.GetLength()) {
          if (isascii(wsSource[iSourceIdx]) && isalnum(wsSource[iSourceIdx])) {
            wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
            ++iFormatIdx;
          }
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      case 'A': {
        if (iSourceIdx < wsSource.GetLength()) {
          if (isascii(wsSource[iSourceIdx]) && isalpha(wsSource[iSourceIdx])) {
            wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
            ++iFormatIdx;
          }
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      case '9': {
        if (iSourceIdx < wsSource.GetLength()) {
          if (FXSYS_IsDecimalDigit(wsSource[iSourceIdx])) {
            wsResult += wsSource[iSourceIdx];
            ++iFormatIdx;
          }
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      case '*': {
        if (iSourceIdx < wsSource.GetLength()) {
          wsResult += TranslateCase(wsSource[iSourceIdx], eCaseMode);
          ++iSourceIdx;
        } else {
          ++iFormatIdx;
        }
      } break;
      default: {
        wsResult += wsFormat[iFormatIdx];
        ++iFormatIdx;
      } break;
    }
  }
  return wsResult;
}

CJS_Result CJS_Util::scand(CJS_Runtime* pRuntime,
                           const std::vector<v8::Local<v8::Value>>& params) {
  if (params.size() < 2)
    return CJS_Result::Failure(JSMessage::kParamError);

  WideString sFormat = pRuntime->ToWideString(params[0]);
  WideString sDate = pRuntime->ToWideString(params[1]);
  double dDate = FX_GetDateTime();
  if (sDate.GetLength() > 0)
    dDate = CJS_PublicMethods::ParseDateUsingFormat(pRuntime->GetIsolate(),
                                                    sDate, sFormat, nullptr);
  if (isnan(dDate))
    return CJS_Result::Success(pRuntime->NewUndefined());

  return CJS_Result::Success(pRuntime->NewDate(dDate));
}

CJS_Result CJS_Util::byteToChar(
    CJS_Runtime* pRuntime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (params.size() < 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  int arg = pRuntime->ToInt32(params[0]);
  if (arg < 0 || arg > 255)
    return CJS_Result::Failure(JSMessage::kValueError);

  WideString wStr(static_cast<wchar_t>(arg));
  return CJS_Result::Success(pRuntime->NewString(wStr.AsStringView()));
}

// static
CJS_Util::DataType CJS_Util::ParseDataType(WideString* sFormat) {
  enum State { kBefore, kFlags, kWidth, kPrecision, kSpecifier, kAfter };

  DataType result = DataType::kInvalid;
  State state = kBefore;
  size_t precision_digits = 0;
  size_t i = 0;
  while (i < sFormat->GetLength()) {
    wchar_t c = (*sFormat)[i];
    switch (state) {
      case kBefore:
        if (c == L'%')
          state = kFlags;
        break;
      case kFlags:
        if (c == L'+' || c == L'-' || c == L'#' || c == L' ') {
          // Stay in same state.
        } else {
          state = kWidth;
          continue;  // Re-process same character.
        }
        break;
      case kWidth:
        if (c == L'*')
          return DataType::kInvalid;
        if (FXSYS_IsDecimalDigit(c)) {
          // Stay in same state.
        } else if (c == L'.') {
          state = kPrecision;
        } else {
          state = kSpecifier;
          continue;  // Re-process same character.
        }
        break;
      case kPrecision:
        if (c == L'*')
          return DataType::kInvalid;
        if (FXSYS_IsDecimalDigit(c)) {
          // Stay in same state.
          ++precision_digits;
        } else {
          state = kSpecifier;
          continue;  // Re-process same character.
        }
        break;
      case kSpecifier:
        if (c == L'c' || c == L'C' || c == L'd' || c == L'i' || c == L'o' ||
            c == L'u' || c == L'x' || c == L'X') {
          result = DataType::kInt;
        } else if (c == L'e' || c == L'E' || c == L'f' || c == L'g' ||
                   c == L'G') {
          result = DataType::kDouble;
        } else if (c == L's' || c == L'S') {
          // Map s to S since we always deal internally with wchar_t strings.
          // TODO(tsepez): Probably 100% borked. %S is not a standard
          // conversion.
          sFormat->SetAt(i, L'S');
          result = DataType::kString;
        } else {
          return DataType::kInvalid;
        }
        state = kAfter;
        break;
      case kAfter:
        if (c == L'%')
          return DataType::kInvalid;
        // Stay in same state until string exhausted.
        break;
    }
    ++i;
  }
  // See https://crbug.com/740166
  if (result == DataType::kInt && precision_digits > 2)
    return DataType::kInvalid;

  return result;
}
