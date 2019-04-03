// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_PARSER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;

namespace base {
class DictionaryValue;
}

namespace blink {

class KURL;

// ManifestParser handles the logic of parsing the Web Manifest from a string.
// It implements:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-a-manifest
class MODULES_EXPORT ManifestParser {
 public:
  ManifestParser(const base::StringPiece& data,
                 const KURL& manifest_url,
                 const KURL& document_url);
  ~ManifestParser();

  // Parse the Manifest from a string using following:
  // http://w3c.github.io/manifest/#dfn-steps-for-processing-a-manifest
  void Parse();

  const Manifest& manifest() const;
  bool failed() const;

  void TakeErrors(WebVector<ManifestError>* errors);

 private:
  // Used to indicate whether to strip whitespace when parsing a string.
  enum TrimType { Trim, NoTrim };

  // Indicate whether a parsed URL should be restricted to document origin.
  enum class ParseURLOriginRestrictions {
    kNoRestrictions = 0,
    kSameOriginOnly,
  };

  // Helper function to parse booleans present on a given |dictionary| in a
  // given field identified by its |key|.
  // Returns the parsed boolean if any, or |default_value| if parsing failed.
  bool ParseBoolean(const base::DictionaryValue& dictionary,
                    const std::string& key,
                    bool default_value);

  // Helper function to parse strings present on a given |dictionary| in a given
  // field identified by its |key|.
  // Returns the parsed string if any, a null string if the parsing failed.
  base::NullableString16 ParseString(const base::DictionaryValue& dictionary,
                                     const std::string& key,
                                     TrimType trim);

  // Helper function to parse colors present on a given |dictionary| in a given
  // field identified by its |key|. Returns a null optional if the value is not
  // present or is not a valid color.
  base::Optional<SkColor> ParseColor(const base::DictionaryValue& dictionary,
                                     const std::string& key);

  // Helper function to parse URLs present on a given |dictionary| in a given
  // field identified by its |key|. The URL is first parsed as a string then
  // resolved using |base_url|. |enforce_document_origin| specified whether to
  // enforce matching of the document's and parsed URL's origins.
  // Returns a GURL. If the parsing failed or origin matching was enforced but
  // not present, the returned GURL will be empty.
  GURL ParseURL(const base::DictionaryValue& dictionary,
                const std::string& key,
                const GURL& base_url,
                ParseURLOriginRestrictions origin_restriction);

  // Parses the 'name' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-name-member
  // Returns the parsed string if any, a null string if the parsing failed.
  base::NullableString16 ParseName(const base::DictionaryValue& dictionary);

  // Parses the 'short_name' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-short-name-member
  // Returns the parsed string if any, a null string if the parsing failed.
  base::NullableString16 ParseShortName(
      const base::DictionaryValue& dictionary);

  // Parses the 'scope' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#scope-member. Returns the parsed GURL if
  // any, or start URL (falling back to document URL) without filename, path,
  // and query if there is no defined scope or if the parsing failed.
  GURL ParseScope(const base::DictionaryValue& dictionary,
                  const GURL& start_url);

  // Parses the 'start_url' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-start_url-member
  // Returns the parsed GURL if any, an empty GURL if the parsing failed.
  GURL ParseStartURL(const base::DictionaryValue& dictionary);

  // Parses the 'display' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-display-member
  // Returns the parsed DisplayMode if any, WebDisplayModeUndefined if the
  // parsing failed.
  WebDisplayMode ParseDisplay(const base::DictionaryValue& dictionary);

  // Parses the 'orientation' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-orientation-member
  // Returns the parsed WebScreenOrientationLockType if any,
  // WebScreenOrientationLockDefault if the parsing failed.
  WebScreenOrientationLockType ParseOrientation(
      const base::DictionaryValue& dictionary);

  // Parses the 'src' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-src-member-of-an-image
  // Returns the parsed GURL if any, an empty GURL if the parsing failed.
  GURL ParseIconSrc(const base::DictionaryValue& icon);

  // Parses the 'type' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-type-member-of-an-image
  // Returns the parsed string if any, an empty string if the parsing failed.
  base::string16 ParseIconType(const base::DictionaryValue& icon);

  // Parses the 'sizes' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-a-sizes-member-of-an-image
  // Returns a vector of gfx::Size with the successfully parsed sizes, if any.
  // An empty vector if the field was not present or empty. "Any" is represented
  // by gfx::Size(0, 0).
  std::vector<gfx::Size> ParseIconSizes(const base::DictionaryValue& icon);

  // Parses the 'purpose' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-a-purpose-member-of-an-image
  // Returns a vector of Manifest::Icon::IconPurpose with the successfully
  // parsed icon purposes, and nullopt if the parsing failed.
  base::Optional<std::vector<Manifest::ImageResource::Purpose>>
  ParseIconPurpose(const base::DictionaryValue& icon);

  // Parses the 'icons' field of a Manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-an-array-of-images
  // Returns a vector of Manifest::Icon with the successfully parsed icons, if
  // any. An empty vector if the field was not present or empty.
  std::vector<Manifest::ImageResource> ParseIcons(
      const base::DictionaryValue& dictionary);

  // Parses the name field of a share target file, as defined in:
  // https://github.com/WICG/web-share-target/blob/master/docs/interface.md
  // Returns the parsed string if any, an empty string if the parsing failed.
  base::string16 ParseFileFilterName(const base::DictionaryValue& file);

  // Parses the accept field of a file filter, as defined in:
  // https://wicg.github.io/web-share-target/level-2/#sharetargetfiles-and-its-members
  // Returns the vector of parsed strings if any exist, an empty vector if the
  // parsing failed or no accept instances were provided.
  std::vector<base::string16> ParseFileFilterAccept(
      const base::DictionaryValue& file);

  // Parses the |key| field of |from| as a list of FileFilters.
  // This is used to parse |file_handlers| and |share_target.params.files|
  // Returns a parsed vector of share target files.
  std::vector<Manifest::FileFilter> ParseTargetFiles(
      const base::StringPiece& key,
      const base::DictionaryValue& from);

  // Parses a single FileFilter (see above comment) and appends it to
  // the given |files| vector.
  void ParseFileFilter(const base::DictionaryValue& file_dictionary,
                       std::vector<Manifest::FileFilter>* files);

  // Parses the method field of a Share Target, as defined in:
  // https://github.com/WICG/web-share-target/blob/master/docs/interface.md
  // Returns an optional share target method enum object..
  base::Optional<Manifest::ShareTarget::Method> ParseShareTargetMethod(
      const base::DictionaryValue& share_target_dict);

  // Parses the enctype field of a Share Target, as defined in:
  // https://github.com/WICG/web-share-target/blob/master/docs/interface.md
  // Returns an optional share target enctype enum object.
  base::Optional<Manifest::ShareTarget::Enctype> ParseShareTargetEnctype(
      const base::DictionaryValue& share_target_dict);

  // Parses the 'params' field of a Share Target, as defined in:
  // https://wicg.github.io/web-share-target/level-2/#sharetargetparams-and-its-members
  // Returns a parsed Manifest::ShareTargetParams, not all fields need to be
  // populated.
  Manifest::ShareTargetParams ParseShareTargetParams(
      const base::DictionaryValue& share_target_params);

  // Parses the 'share_target' field of a Manifest, as defined in:
  // https://github.com/WICG/web-share-target/blob/master/docs/interface.md
  // Returns the parsed Web Share target. The returned Share Target is null if
  // the field didn't exist, parsing failed, or it was empty.
  base::Optional<Manifest::ShareTarget> ParseShareTarget(
      const base::DictionaryValue& dictionary);

  // Parses the 'file_handler' field of a Manifest, as defined in:
  // https://github.com/WICG/file-handling/blob/master/explainer.md
  // Returns the parsed file handler information. The returned FileHandler is
  // null if the field didn't exist, parsing failed, or it was empty.
  base::Optional<Manifest::FileHandler> ParseFileHandler(
      const base::DictionaryValue& dictionary);

  // Parses the 'platform' field of a related application, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-platform-member-of-an-application
  // Returns the parsed string if any, a null string if the parsing failed.
  base::NullableString16 ParseRelatedApplicationPlatform(
      const base::DictionaryValue& application);

  // Parses the 'url' field of a related application, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-url-member-of-an-application
  // Returns the parsed GURL if any, an empty GURL if the parsing failed.
  GURL ParseRelatedApplicationURL(const base::DictionaryValue& application);

  // Parses the 'id' field of a related application, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-id-member-of-an-application
  // Returns the parsed string if any, a null string if the parsing failed.
  base::NullableString16 ParseRelatedApplicationId(
      const base::DictionaryValue& application);

  // Parses the 'related_applications' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-related_applications-member
  // Returns a vector of Manifest::RelatedApplication with the successfully
  // parsed applications, if any. An empty vector if the field was not present
  // or empty.
  std::vector<Manifest::RelatedApplication> ParseRelatedApplications(
      const base::DictionaryValue& dictionary);

  // Parses the 'prefer_related_applications' field on the manifest, as defined
  // in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-prefer_related_applications-member
  // returns true iff the field could be parsed as the boolean true.
  bool ParsePreferRelatedApplications(const base::DictionaryValue& dictionary);

  // Parses the 'theme_color' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-theme_color-member
  // Returns the parsed theme color if any, or a null optional otherwise.
  base::Optional<SkColor> ParseThemeColor(
      const base::DictionaryValue& dictionary);

  // Parses the 'background_color' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-background_color-member
  // Returns the parsed background color if any, or a null optional otherwise.
  base::Optional<SkColor> ParseBackgroundColor(
      const base::DictionaryValue& dictionary);

  // Parses the 'splash_screen_url' field of the manifest.
  // Returns the parsed GURL if any, an empty GURL if the parsing failed.
  GURL ParseSplashScreenURL(const base::DictionaryValue& dictionary);

  // Parses the 'gcm_sender_id' field of the manifest.
  // This is a proprietary extension of the Web Manifest specification.
  // Returns the parsed string if any, a null string if the parsing failed.
  base::NullableString16 ParseGCMSenderID(
      const base::DictionaryValue& dictionary);

  void AddErrorInfo(const std::string& error_msg,
                    bool critical = false,
                    int error_line = 0,
                    int error_column = 0);

  const base::StringPiece& data_;
  GURL manifest_url_;
  GURL document_url_;

  bool failed_;
  Manifest manifest_;
  Vector<ManifestError> errors_;

  DISALLOW_COPY_AND_ASSIGN(ManifestParser);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_PARSER_H_
