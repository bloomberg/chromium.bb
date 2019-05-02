// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"

#include <stddef.h>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/modules/manifest/manifest_uma_util.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace blink {

namespace {

bool IsValidMimeType(const std::string& mime_type) {
  if (mime_type.length() > 0 && mime_type.at(0) == '.')
    return true;
  return net::ParseMimeTypeWithoutParameter(mime_type, nullptr, nullptr);
}

bool VerifyFiles(const std::vector<Manifest::FileFilter>& files) {
  for (const Manifest::FileFilter& file : files) {
    for (const base::string16& utf_accept : file.accept) {
      std::string accept_type =
          base::ToLowerASCII(base::UTF16ToASCII(utf_accept));
      if (!IsValidMimeType(accept_type))
        return false;
    }
  }
  return true;
}

}  // anonymous namespace

ManifestParser::ManifestParser(const base::StringPiece& data,
                               const KURL& manifest_url,
                               const KURL& document_url)
    : data_(data),
      manifest_url_(manifest_url),
      document_url_(document_url),
      failed_(false) {}

ManifestParser::~ManifestParser() {}

void ManifestParser::Parse() {
  std::string error_msg;
  int error_line = 0;
  int error_column = 0;
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          data_, base::JSON_PARSE_RFC, nullptr, &error_msg, &error_line,
          &error_column);

  if (!value) {
    AddErrorInfo(error_msg, true, error_line, error_column);
    ManifestUmaUtil::ParseFailed();
    failed_ = true;
    return;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    AddErrorInfo("root element must be a valid JSON object.", true);
    ManifestUmaUtil::ParseFailed();
    failed_ = true;
    return;
  }
  DCHECK(dictionary);

  manifest_.name = ParseName(*dictionary);
  manifest_.short_name = ParseShortName(*dictionary);
  manifest_.start_url = ParseStartURL(*dictionary);
  manifest_.scope = ParseScope(*dictionary, manifest_.start_url);
  manifest_.display = ParseDisplay(*dictionary);
  manifest_.orientation = ParseOrientation(*dictionary);
  manifest_.icons = ParseIcons(*dictionary);
  manifest_.share_target = ParseShareTarget(*dictionary);
  manifest_.file_handler = ParseFileHandler(*dictionary);
  manifest_.related_applications = ParseRelatedApplications(*dictionary);
  manifest_.prefer_related_applications =
      ParsePreferRelatedApplications(*dictionary);
  manifest_.theme_color = ParseThemeColor(*dictionary);
  manifest_.background_color = ParseBackgroundColor(*dictionary);
  manifest_.splash_screen_url = ParseSplashScreenURL(*dictionary);
  manifest_.gcm_sender_id = ParseGCMSenderID(*dictionary);

  ManifestUmaUtil::ParseSucceeded(manifest_);
}

const Manifest& ManifestParser::manifest() const {
  return manifest_;
}

void ManifestParser::TakeErrors(
    Vector<mojom::blink::ManifestErrorPtr>* errors) {
  errors->clear();
  errors->swap(errors_);
}

bool ManifestParser::failed() const {
  return failed_;
}

bool ManifestParser::ParseBoolean(const base::DictionaryValue& dictionary,
                                  const std::string& key,
                                  bool default_value) {
  if (!dictionary.HasKey(key))
    return default_value;

  bool value;
  if (!dictionary.GetBoolean(key, &value)) {
    AddErrorInfo("property '" + key + "' ignored, type " + "boolean expected.");
    return default_value;
  }

  return value;
}

base::NullableString16 ManifestParser::ParseString(
    const base::DictionaryValue& dictionary,
    const std::string& key,
    TrimType trim) {
  if (!dictionary.HasKey(key))
    return base::NullableString16();

  base::string16 value;
  if (!dictionary.GetString(key, &value)) {
    AddErrorInfo("property '" + key + "' ignored, type " + "string expected.");
    return base::NullableString16();
  }

  if (trim == Trim)
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
  return base::NullableString16(value, false);
}

base::Optional<SkColor> ManifestParser::ParseColor(
    const base::DictionaryValue& dictionary,
    const std::string& key) {
  base::NullableString16 parsed_color = ParseString(dictionary, key, Trim);
  if (parsed_color.is_null())
    return base::nullopt;

  Color color;
  WebString color_string = WebString::FromUTF16(parsed_color);

  if (!CSSParser::ParseColor(color, WTF::String(color_string), true)) {
    AddErrorInfo("property '" + key + "' ignored, '" +
                 base::UTF16ToUTF8(parsed_color.string()) + "' is not a " +
                 "valid color.");
    return base::nullopt;
  }

  return color.Rgb();
}

GURL ManifestParser::ParseURL(const base::DictionaryValue& dictionary,
                              const std::string& key,
                              const GURL& base_url,
                              ParseURLOriginRestrictions origin_restriction) {
  base::NullableString16 url_str = ParseString(dictionary, key, NoTrim);
  if (url_str.is_null())
    return GURL();

  GURL resolved = base_url.Resolve(url_str.string());
  if (!resolved.is_valid()) {
    AddErrorInfo("property '" + key + "' ignored, URL is invalid.");
    return GURL();
  }

  switch (origin_restriction) {
    case ParseURLOriginRestrictions::kSameOriginOnly:
      if (resolved.GetOrigin() != document_url_.GetOrigin()) {
        AddErrorInfo("property '" + key +
                     "' ignored, should be same origin as document.");
        return GURL();
      }
      return resolved;
    case ParseURLOriginRestrictions::kNoRestrictions:
      return resolved;
  }

  NOTREACHED();
  return GURL();
}

base::NullableString16 ManifestParser::ParseName(
    const base::DictionaryValue& dictionary) {
  return ParseString(dictionary, "name", Trim);
}

base::NullableString16 ManifestParser::ParseShortName(
    const base::DictionaryValue& dictionary) {
  return ParseString(dictionary, "short_name", Trim);
}

GURL ManifestParser::ParseStartURL(const base::DictionaryValue& dictionary) {
  return ParseURL(dictionary, "start_url", manifest_url_,
                  ParseURLOriginRestrictions::kSameOriginOnly);
}

GURL ManifestParser::ParseScope(const base::DictionaryValue& dictionary,
                                const GURL& start_url) {
  GURL scope = ParseURL(dictionary, "scope", manifest_url_,
                        ParseURLOriginRestrictions::kSameOriginOnly);

  // This will change to remove the |document_url_| fallback in the future.
  // See https://github.com/w3c/manifest/issues/668.
  const GURL& default_value = start_url.is_empty() ? document_url_ : start_url;
  DCHECK(default_value.is_valid());

  if (scope.is_empty())
    return default_value.GetWithoutFilename();

  if (default_value.GetOrigin() != scope.GetOrigin() ||
      !base::StartsWith(default_value.path(), scope.path(),
                        base::CompareCase::SENSITIVE)) {
    AddErrorInfo(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.");
    return default_value.GetWithoutFilename();
  }

  DCHECK(scope.is_valid());
  return scope;
}

WebDisplayMode ManifestParser::ParseDisplay(
    const base::DictionaryValue& dictionary) {
  base::NullableString16 display = ParseString(dictionary, "display", Trim);
  if (display.is_null())
    return kWebDisplayModeUndefined;

  WebDisplayMode display_enum =
      WebDisplayModeFromString(base::UTF16ToUTF8(display.string()));
  if (display_enum == kWebDisplayModeUndefined)
    AddErrorInfo("unknown 'display' value ignored.");
  return display_enum;
}

WebScreenOrientationLockType ManifestParser::ParseOrientation(
    const base::DictionaryValue& dictionary) {
  base::NullableString16 orientation =
      ParseString(dictionary, "orientation", Trim);

  if (orientation.is_null())
    return kWebScreenOrientationLockDefault;

  WebScreenOrientationLockType orientation_enum =
      WebScreenOrientationLockTypeFromString(
          base::UTF16ToUTF8(orientation.string()));
  if (orientation_enum == kWebScreenOrientationLockDefault)
    AddErrorInfo("unknown 'orientation' value ignored.");
  return orientation_enum;
}

GURL ManifestParser::ParseIconSrc(const base::DictionaryValue& icon) {
  return ParseURL(icon, "src", manifest_url_,
                  ParseURLOriginRestrictions::kNoRestrictions);
}

base::string16 ManifestParser::ParseIconType(
    const base::DictionaryValue& icon) {
  base::NullableString16 nullable_string = ParseString(icon, "type", Trim);
  if (nullable_string.is_null())
    return base::string16();
  return nullable_string.string();
}

std::vector<gfx::Size> ManifestParser::ParseIconSizes(
    const base::DictionaryValue& icon) {
  base::NullableString16 sizes_str = ParseString(icon, "sizes", NoTrim);
  std::vector<gfx::Size> sizes;

  if (sizes_str.is_null())
    return sizes;

  WebVector<WebSize> web_sizes = WebIconSizesParser::ParseIconSizes(
      WebString::FromUTF16(sizes_str.string()));
  sizes.resize(web_sizes.size());
  for (size_t i = 0; i < web_sizes.size(); ++i)
    sizes[i] = gfx::Size(web_sizes[i]);
  if (sizes.empty()) {
    AddErrorInfo("found icon with no valid size.");
  }
  return sizes;
}

base::Optional<std::vector<Manifest::ImageResource::Purpose>>
ManifestParser::ParseIconPurpose(const base::DictionaryValue& icon) {
  base::NullableString16 purpose_str = ParseString(icon, "purpose", NoTrim);
  std::vector<Manifest::ImageResource::Purpose> purposes;

  if (purpose_str.is_null()) {
    purposes.push_back(Manifest::ImageResource::Purpose::ANY);
    return purposes;
  }

  std::vector<base::string16> keywords =
      base::SplitString(purpose_str.string(), base::ASCIIToUTF16(" "),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // "any" is the default if there are no other keywords.
  if (keywords.empty()) {
    purposes.push_back(Manifest::ImageResource::Purpose::ANY);
    return purposes;
  }

  bool unrecognised_purpose = false;

  for (const base::string16& keyword : keywords) {
    if (base::LowerCaseEqualsASCII(keyword, "any")) {
      purposes.push_back(Manifest::ImageResource::Purpose::ANY);
    } else if (base::LowerCaseEqualsASCII(keyword, "badge")) {
      purposes.push_back(Manifest::ImageResource::Purpose::BADGE);
    } else if (base::LowerCaseEqualsASCII(keyword, "maskable")) {
      purposes.push_back(Manifest::ImageResource::Purpose::MASKABLE);
    } else {
      unrecognised_purpose = true;
    }
  }

  // This implies there was at least one purpose given, but none recognised.
  // Instead of defaulting to "any" (which would not be future proof),
  // invalidate the whole icon.
  if (purposes.empty()) {
    AddErrorInfo("found icon with no valid purpose; ignoring it.");
    return base::nullopt;
  }

  if (unrecognised_purpose) {
    AddErrorInfo(
        "found icon with one or more invalid purposes; those purposes are "
        "ignored.");
  }

  return purposes;
}

std::vector<Manifest::ImageResource> ManifestParser::ParseIcons(
    const base::DictionaryValue& dictionary) {
  std::vector<Manifest::ImageResource> icons;
  if (!dictionary.HasKey("icons"))
    return icons;

  const base::ListValue* icons_list = nullptr;
  if (!dictionary.GetList("icons", &icons_list)) {
    AddErrorInfo("property 'icons' ignored, type array expected.");
    return icons;
  }

  for (size_t i = 0; i < icons_list->GetSize(); ++i) {
    const base::DictionaryValue* icon_dictionary = nullptr;
    if (!icons_list->GetDictionary(i, &icon_dictionary))
      continue;

    Manifest::ImageResource icon;
    icon.src = ParseIconSrc(*icon_dictionary);
    // An icon MUST have a valid src. If it does not, it MUST be ignored.
    if (!icon.src.is_valid())
      continue;

    icon.type = ParseIconType(*icon_dictionary);
    icon.sizes = ParseIconSizes(*icon_dictionary);
    auto purpose = ParseIconPurpose(*icon_dictionary);
    if (!purpose)
      continue;

    icon.purpose = std::move(*purpose);

    icons.push_back(icon);
  }

  return icons;
}

base::string16 ManifestParser::ParseFileFilterName(
    const base::DictionaryValue& file) {
  if (!file.HasKey("name")) {
    AddErrorInfo("property 'name' missing.");
    return base::string16();
  }

  base::string16 value;
  if (!file.GetString("name", &value)) {
    AddErrorInfo("property 'name' ignored, type string expected.");
    return base::string16();
  }
  return value;
}

std::vector<base::string16> ManifestParser::ParseFileFilterAccept(
    const base::DictionaryValue& dictionary) {
  std::vector<base::string16> accept_types;
  if (!dictionary.HasKey("accept")) {
    return accept_types;
  }

  base::string16 accept_str;
  if (dictionary.GetString("accept", &accept_str)) {
    accept_types.push_back(accept_str);
    return accept_types;
  }

  const base::ListValue* accept_list = nullptr;
  if (!dictionary.GetList("accept", &accept_list)) {
    // 'accept' property is the wrong type. Returning an empty vector here
    // causes the 'files' entry to be discarded.
    AddErrorInfo("property 'accept' ignored, type array or string expected.");
    return accept_types;
  }

  for (const base::Value& accept_value : accept_list->GetList()) {
    if (!accept_value.is_string()) {
      // A particular 'accept' entry is invalid - just drop that one entry.
      AddErrorInfo("'accept' entry ignored, expected to be of type string.");
      continue;
    }
    accept_types.push_back(base::ASCIIToUTF16(accept_value.GetString()));
  }

  return accept_types;
}

std::vector<Manifest::FileFilter> ManifestParser::ParseTargetFiles(
    const base::StringPiece& key,
    const base::DictionaryValue& from) {
  std::vector<Manifest::FileFilter> files;
  if (!from.HasKey(key))
    return files;

  const base::ListValue* file_list = nullptr;
  if (!from.GetList(key, &file_list)) {
    // https://wicg.github.io/web-share-target/level-2/#share_target-member
    // step 5 indicates that the 'files' attribute is allowed to be a single
    // (non-array) FileFilter.
    const base::DictionaryValue* file_dictionary = nullptr;
    if (!from.GetDictionary(key, &file_dictionary)) {
      AddErrorInfo(
          "property 'files' ignored, type array or FileFilter expected.");
      return files;
    }

    ParseFileFilter(*file_dictionary, &files);

    return files;
  }

  for (const base::Value& file_value : file_list->GetList()) {
    const base::DictionaryValue* file_dictionary = nullptr;
    if (!file_value.GetAsDictionary(&file_dictionary)) {
      AddErrorInfo("files must be a sequence of non-empty file entries.");
      continue;
    }

    ParseFileFilter(*file_dictionary, &files);
  }

  return files;
}

void ManifestParser::ParseFileFilter(
    const base::DictionaryValue& file_dictionary,
    std::vector<Manifest::FileFilter>* files) {
  Manifest::FileFilter file;
  file.name = ParseFileFilterName(file_dictionary);
  if (file.name.empty()) {
    // https://wicg.github.io/web-share-target/level-2/#share_target-member
    // step 7.1 requires that we invalidate this FileFilter if 'name' is an
    // empty string. We also invalidate if 'name' is undefined or not a
    // string.
    return;
  }

  file.accept = ParseFileFilterAccept(file_dictionary);
  if (file.accept.empty())
    return;

  files->push_back(file);
}

base::Optional<Manifest::ShareTarget::Method>
ManifestParser::ParseShareTargetMethod(
    const base::DictionaryValue& share_target_dict) {
  if (!share_target_dict.HasKey("method")) {
    AddErrorInfo(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.");
    return base::Optional<Manifest::ShareTarget::Method>(
        Manifest::ShareTarget::Method::kGet);
  }

  base::string16 value;
  if (!share_target_dict.GetString("method", &value))
    return base::nullopt;

  std::string method = base::ToUpperASCII(base::UTF16ToASCII(value));
  if (method == "GET")
    return Manifest::ShareTarget::Method::kGet;
  if (method == "POST")
    return Manifest::ShareTarget::Method::kPost;

  return base::nullopt;
}

base::Optional<Manifest::ShareTarget::Enctype>
ManifestParser::ParseShareTargetEnctype(
    const base::DictionaryValue& share_target_dict) {
  if (!share_target_dict.HasKey("enctype")) {
    AddErrorInfo(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded");
    return base::Optional<Manifest::ShareTarget::Enctype>(
        Manifest::ShareTarget::Enctype::kApplication);
  }

  base::string16 value;
  if (!share_target_dict.GetString("enctype", &value)) {
    return base::nullopt;
  }

  std::string enctype = base::ToLowerASCII(base::UTF16ToASCII(value));
  if (enctype == "application/x-www-form-urlencoded") {
    return base::Optional<Manifest::ShareTarget::Enctype>(
        Manifest::ShareTarget::Enctype::kApplication);
  }

  if (enctype == "multipart/form-data") {
    return base::Optional<Manifest::ShareTarget::Enctype>(
        Manifest::ShareTarget::Enctype::kMultipart);
  }

  return base::nullopt;
}

Manifest::ShareTargetParams ManifestParser::ParseShareTargetParams(
    const base::DictionaryValue& share_target_params) {
  Manifest::ShareTargetParams params;
  // NOTE: These are key names for query parameters, which are filled with share
  // data. As such, |params.url| is just a string.
  params.text = ParseString(share_target_params, "text", Trim);
  params.title = ParseString(share_target_params, "title", Trim);
  params.url = ParseString(share_target_params, "url", Trim);
  params.files = ParseTargetFiles("files", share_target_params);
  return params;
}

base::Optional<Manifest::ShareTarget> ManifestParser::ParseShareTarget(
    const base::DictionaryValue& dictionary) {
  if (!dictionary.HasKey("share_target"))
    return base::nullopt;

  Manifest::ShareTarget share_target;
  const base::DictionaryValue* share_target_dict = nullptr;
  dictionary.GetDictionary("share_target", &share_target_dict);
  share_target.action = ParseURL(*share_target_dict, "action", manifest_url_,
                                 ParseURLOriginRestrictions::kSameOriginOnly);
  if (!share_target.action.is_valid()) {
    AddErrorInfo(
        "property 'share_target' ignored. Property 'action' is "
        "invalid.");
    return base::nullopt;
  }

  base::Optional<Manifest::ShareTarget::Method> method =
      ParseShareTargetMethod(*share_target_dict);
  base::Optional<Manifest::ShareTarget::Enctype> enctype =
      ParseShareTargetEnctype(*share_target_dict);

  const base::DictionaryValue* share_target_params_dict = nullptr;
  if (!share_target_dict->GetDictionary("params", &share_target_params_dict)) {
    AddErrorInfo(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.");
    return base::nullopt;
  }

  share_target.params = ParseShareTargetParams(*share_target_params_dict);

  if (method == base::nullopt) {
    AddErrorInfo(
        "invalid method. Allowed methods are:"
        "GET and POST.");
    return base::nullopt;
  }

  if (enctype == base::nullopt) {
    AddErrorInfo(
        "invalid enctype. Allowed enctypes are:"
        "application/x-www-form-urlencoded and multipart/form-data.");
    return base::nullopt;
  }

  if (method == base::Optional<Manifest::ShareTarget::Method>(
                    Manifest::ShareTarget::Method::kGet)) {
    share_target.method = Manifest::ShareTarget::Method::kGet;
  } else {
    share_target.method = Manifest::ShareTarget::Method::kPost;
  }

  if (enctype == base::Optional<Manifest::ShareTarget::Enctype>(
                     Manifest::ShareTarget::Enctype::kMultipart)) {
    share_target.enctype = Manifest::ShareTarget::Enctype::kMultipart;
  } else {
    share_target.enctype = Manifest::ShareTarget::Enctype::kApplication;
  }

  if (share_target.method == Manifest::ShareTarget::Method::kGet) {
    if (share_target.enctype == Manifest::ShareTarget::Enctype::kMultipart) {
      AddErrorInfo(
          "invalid enctype for GET method. Only "
          "application/x-www-form-urlencoded is allowed.");
      return base::nullopt;
    }
  }

  if (share_target.params.files.size() > 0) {
    if (share_target.method != Manifest::ShareTarget::Method::kPost ||
        share_target.enctype != Manifest::ShareTarget::Enctype::kMultipart) {
      AddErrorInfo("files are only supported with multipart/form-data POST.");
      return base::nullopt;
    }
  }

  if (!VerifyFiles(share_target.params.files)) {
    AddErrorInfo("invalid mime type inside files.");
    return base::nullopt;
  }

  return base::Optional<Manifest::ShareTarget>(std::move(share_target));
}

base::Optional<Manifest::FileHandler> ManifestParser::ParseFileHandler(
    const base::DictionaryValue& dictionary) {
  const base::DictionaryValue* file_handler_value;
  if (!dictionary.GetDictionary("file_handler", &file_handler_value))
    return base::nullopt;

  Manifest::FileHandler file_handler;
  file_handler.action = ParseURL(*file_handler_value, "action", manifest_url_,
                                 ParseURLOriginRestrictions::kSameOriginOnly);
  if (!file_handler.action.is_valid()) {
    AddErrorInfo(
        "property 'file_handler' ignored. Property 'action' is "
        "invalid.");
    return base::nullopt;
  }

  file_handler.files = ParseTargetFiles("files", *file_handler_value);
  if (file_handler.files.size() == 0) {
    AddErrorInfo("no file handlers were specified.");
    return base::nullopt;
  }

  return base::Optional<Manifest::FileHandler>(std::move(file_handler));
}

base::NullableString16 ManifestParser::ParseRelatedApplicationPlatform(
    const base::DictionaryValue& application) {
  return ParseString(application, "platform", Trim);
}

GURL ManifestParser::ParseRelatedApplicationURL(
    const base::DictionaryValue& application) {
  return ParseURL(application, "url", manifest_url_,
                  ParseURLOriginRestrictions::kNoRestrictions);
}

base::NullableString16 ManifestParser::ParseRelatedApplicationId(
    const base::DictionaryValue& application) {
  return ParseString(application, "id", Trim);
}

std::vector<Manifest::RelatedApplication>
ManifestParser::ParseRelatedApplications(
    const base::DictionaryValue& dictionary) {
  std::vector<Manifest::RelatedApplication> applications;
  if (!dictionary.HasKey("related_applications"))
    return applications;

  const base::ListValue* applications_list = nullptr;
  if (!dictionary.GetList("related_applications", &applications_list)) {
    AddErrorInfo(
        "property 'related_applications' ignored,"
        " type array expected.");
    return applications;
  }

  for (size_t i = 0; i < applications_list->GetSize(); ++i) {
    const base::DictionaryValue* application_dictionary = nullptr;
    if (!applications_list->GetDictionary(i, &application_dictionary))
      continue;

    Manifest::RelatedApplication application;
    application.platform =
        ParseRelatedApplicationPlatform(*application_dictionary);
    // "If platform is undefined, move onto the next item if any are left."
    if (application.platform.is_null()) {
      AddErrorInfo(
          "'platform' is a required field, related application"
          " ignored.");
      continue;
    }

    application.id = ParseRelatedApplicationId(*application_dictionary);
    application.url = ParseRelatedApplicationURL(*application_dictionary);
    // "If both id and url are undefined, move onto the next item if any are
    // left."
    if (application.url.is_empty() && application.id.is_null()) {
      AddErrorInfo(
          "one of 'url' or 'id' is required, related application"
          " ignored.");
      continue;
    }

    applications.push_back(application);
  }

  return applications;
}

bool ManifestParser::ParsePreferRelatedApplications(
    const base::DictionaryValue& dictionary) {
  return ParseBoolean(dictionary, "prefer_related_applications", false);
}

base::Optional<SkColor> ManifestParser::ParseThemeColor(
    const base::DictionaryValue& dictionary) {
  return ParseColor(dictionary, "theme_color");
}

base::Optional<SkColor> ManifestParser::ParseBackgroundColor(
    const base::DictionaryValue& dictionary) {
  return ParseColor(dictionary, "background_color");
}

GURL ManifestParser::ParseSplashScreenURL(
    const base::DictionaryValue& dictionary) {
  return ParseURL(dictionary, "splash_screen_url", manifest_url_,
                  ParseURLOriginRestrictions::kSameOriginOnly);
}

base::NullableString16 ManifestParser::ParseGCMSenderID(
    const base::DictionaryValue& dictionary) {
  return ParseString(dictionary, "gcm_sender_id", Trim);
}

void ManifestParser::AddErrorInfo(const std::string& error_msg,
                                  bool critical,
                                  int error_line,
                                  int error_column) {
  mojom::blink::ManifestErrorPtr error = mojom::blink::ManifestError::New(
      String(error_msg.c_str()), critical, error_line, error_column);
  errors_.push_back(std::move(error));
}

}  // namespace blink
