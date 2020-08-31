// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_PARSER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace blink {

class KURL;

// ManifestParser handles the logic of parsing the Web Manifest from a string.
// It implements:
// http://w3c.github.io/manifest/#dfn-steps-for-processing-a-manifest
class MODULES_EXPORT ManifestParser {
 public:
  ManifestParser(const String& data,
                 const KURL& manifest_url,
                 const KURL& document_url);
  ~ManifestParser();

  // Parse the Manifest from a string using following:
  // http://w3c.github.io/manifest/#dfn-steps-for-processing-a-manifest
  void Parse();

  const mojom::blink::ManifestPtr& manifest() const;
  bool failed() const;

  void TakeErrors(Vector<mojom::blink::ManifestErrorPtr>* errors);

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
  bool ParseBoolean(const JSONObject* object,
                    const String& key,
                    bool default_value);

  // Helper function to parse strings present on a given |dictionary| in a given
  // field identified by its |key|.
  // Returns the parsed string if any, a null optional if the parsing failed.
  base::Optional<String> ParseString(const JSONObject* object,
                                     const String& key,
                                     TrimType trim);

  // Helper function to parse strings present in a member that itself is
  // a dictionary like 'shortcut' as defined in:
  // https://w3c.github.io/manifest/#shortcutitem-and-its-members Each strings
  // will identifiable by its |key|. This helper includes the member_name in any
  // ManifestError added while parsing. This helps disambiguate member property
  // names from top level member names. Returns the parsed string if any, a null
  // optional if the parsing failed.
  base::Optional<String> ParseStringForMember(const JSONObject* object,
                                              const String& member_name,
                                              const String& key,
                                              bool required,
                                              TrimType trim);

  // Helper function to parse colors present on a given |dictionary| in a given
  // field identified by its |key|. Returns a null optional if the value is not
  // present or is not a valid color.
  base::Optional<RGBA32> ParseColor(const JSONObject* object,
                                    const String& key);

  // Helper function to parse URLs present on a given |dictionary| in a given
  // field identified by its |key|. The URL is first parsed as a string then
  // resolved using |base_url|. |enforce_document_origin| specified whether to
  // enforce matching of the document's and parsed URL's origins.
  // Returns a KURL. If the parsing failed or origin matching was enforced but
  // not present, the returned KURL will be empty.
  KURL ParseURL(const JSONObject* object,
                const String& key,
                const KURL& base_url,
                ParseURLOriginRestrictions origin_restriction);

  // Parses the 'name' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-name-member
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseName(const JSONObject* object);

  // Parses the 'short_name' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-short-name-member
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseShortName(const JSONObject* object);

  // Parses the 'scope' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#scope-member. Returns the parsed KURL if
  // any, or start URL (falling back to document URL) without filename, path,
  // and query if there is no defined scope or if the parsing failed.
  KURL ParseScope(const JSONObject* object, const KURL& start_url);

  // Parses the 'start_url' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-start_url-member
  // Returns the parsed KURL if any, an empty KURL if the parsing failed.
  KURL ParseStartURL(const JSONObject* object);

  // Parses the 'display' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-display-member
  // Returns the parsed DisplayMode if any, DisplayMode::kUndefined if the
  // parsing failed.
  blink::mojom::DisplayMode ParseDisplay(const JSONObject* object);

  // Parses the 'orientation' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-orientation-member
  // Returns the parsed WebScreenOrientationLockType if any,
  // WebScreenOrientationLockDefault if the parsing failed.
  WebScreenOrientationLockType ParseOrientation(const JSONObject* object);

  // Parses the 'src' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-src-member-of-an-image
  // Returns the parsed KURL if any, an empty KURL if the parsing failed.
  KURL ParseIconSrc(const JSONObject* icon);

  // Parses the 'type' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-type-member-of-an-image
  // Returns the parsed string if any, an empty string if the parsing failed.
  String ParseIconType(const JSONObject* icon);

  // Parses the 'sizes' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-a-sizes-member-of-an-image
  // Returns a vector of WebSize with the successfully parsed sizes, if any.
  // An empty vector if the field was not present or empty. "Any" is represented
  // by gfx::Size(0, 0).
  Vector<gfx::Size> ParseIconSizes(const JSONObject* icon);

  // Parses the 'purpose' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-a-purpose-member-of-an-image
  // Returns a vector of ManifestImageResource::Purpose with the successfully
  // parsed icon purposes, and nullopt if the parsing failed.
  base::Optional<Vector<mojom::blink::ManifestImageResource::Purpose>>
  ParseIconPurpose(const JSONObject* icon);

  // Parses the 'icons' field of a Manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-an-array-of-images
  // Returns a vector of ManifestImageResourcePtr with the successfully parsed
  // icons, if any. An empty vector if the field was not present or empty.
  Vector<mojom::blink::ManifestImageResourcePtr> ParseIcons(
      const JSONObject* object);

  // Parses the 'name' field of a shortcut, as defined in:
  // https://w3c.github.io/manifest/#shortcuts-member
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseShortcutName(const JSONObject* shortcut);

  // Parses the 'short_name' field of a shortcut, as defined in:
  // https://w3c.github.io/manifest/#shortcuts-member
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseShortcutShortName(const JSONObject* shortcut);

  // Parses the 'description' field of a shortcut, as defined in:
  // https://w3c.github.io/manifest/#shortcuts-member
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseShortcutDescription(const JSONObject* shortcut);

  // Parses the 'url' field of an icon, as defined in:
  // https://w3c.github.io/manifest/#shortcuts-member
  // Returns the parsed KURL if any, an empty KURL if the parsing failed.
  KURL ParseShortcutUrl(const JSONObject* shortcut);

  // Parses the 'shortcuts' field of a Manifest, as defined in:
  // https://w3c.github.io/manifest/#shortcuts-member
  // Returns a vector of ManifestShortcutPtr with the successfully parsed
  // shortcuts, if any. An empty vector if the field was not present or empty.
  Vector<mojom::blink::ManifestShortcutItemPtr> ParseShortcuts(
      const JSONObject* object);

  // Parses the name field of a share target file, as defined in:
  // https://wicg.github.io/web-share-target/level-2/#sharetargetfiles-and-its-members
  // Returns the parsed string if any, an empty string if the parsing failed.
  String ParseFileFilterName(const JSONObject* file);

  // Parses the accept field of a file filter, as defined in:
  // https://wicg.github.io/web-share-target/level-2/#sharetargetfiles-and-its-members
  // Returns the vector of parsed strings if any exist, an empty vector if the
  // parsing failed or no accept instances were provided.
  Vector<String> ParseFileFilterAccept(const JSONObject* file);

  // Parses the |key| field of |from| as a list of FileFilters.
  // This is used to parse |file_handlers| and |share_target.params.files|
  // Returns a parsed vector of share target files.
  Vector<mojom::blink::ManifestFileFilterPtr> ParseTargetFiles(
      const String& key,
      const JSONObject* from);

  // Parses a single FileFilter (see above comment) and appends it to
  // the given |files| vector.
  void ParseFileFilter(const JSONObject* file_dictionary,
                       Vector<mojom::blink::ManifestFileFilterPtr>* files);

  // Parses the method field of a Share Target, as defined in:
  // https://wicg.github.io/web-share-target/#sharetarget-and-its-members
  // Returns an optional share target method enum object.
  base::Optional<mojom::blink::ManifestShareTarget::Method>
  ParseShareTargetMethod(const JSONObject* share_target_dict);

  // Parses the enctype field of a Share Target, as defined in:
  // https://wicg.github.io/web-share-target/#sharetarget-and-its-members
  // Returns an optional share target enctype enum object.
  base::Optional<mojom::blink::ManifestShareTarget::Enctype>
  ParseShareTargetEnctype(const JSONObject* share_target_dict);

  // Parses the 'params' field of a Share Target, as defined in:
  // https://wicg.github.io/web-share-target/#sharetarget-and-its-members
  // Returns a parsed mojom::blink:ManifestShareTargetParamsPtr, not all fields
  // need to be populated.
  mojom::blink::ManifestShareTargetParamsPtr ParseShareTargetParams(
      const JSONObject* share_target_params);

  // Parses the 'share_target' field of a Manifest, as defined in:
  // https://wicg.github.io/web-share-target/#share_target-member
  // Returns the parsed Web Share target. The returned Share Target is null if
  // the field didn't exist, parsing failed, or it was empty.
  base::Optional<mojom::blink::ManifestShareTargetPtr> ParseShareTarget(
      const JSONObject* object);

  // Parses the 'file_handlers' field of a Manifest, as defined in:
  // https://github.com/WICG/file-handling/blob/master/explainer.md
  // Returns the parsed list of FileHandlers. The returned FileHandlers are
  // empty if the field didn't exist, parsing failed, or the input list was
  // empty.
  Vector<mojom::blink::ManifestFileHandlerPtr> ParseFileHandlers(
      const JSONObject* object);

  // Parses a FileHandler from an entry in the 'file_handlers' list, as
  // defined in: https://github.com/WICG/file-handling/blob/master/explainer.md.
  // Returns |base::nullopt| if the FileHandler was invalid, or a
  // FileHandler, if parsing succeeded.
  base::Optional<mojom::blink::ManifestFileHandlerPtr> ParseFileHandler(
      const JSONObject* file_handler_entry);

  // Parses the 'accept' field of a FileHandler, as defined in:
  // https://github.com/WICG/file-handling/blob/master/explainer.md.
  // Returns the parsed accept map. Invalid accept entries are ignored.
  HashMap<String, Vector<String>> ParseFileHandlerAccept(
      const JSONObject* accept);

  // Parses an extension in the 'accept' field of a FileHandler, as defined in:
  // https://github.com/WICG/file-handling/blob/master/explainer.md. Returns
  // whether the parsing was successful and, if so, populates |output| with the
  // parsed extension.
  bool ParseFileHandlerAcceptExtension(const JSONValue* extension,
                                       String* ouput);

  // Parses the 'platform' field of a related application, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-platform-member-of-an-application
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseRelatedApplicationPlatform(const JSONObject* application);

  // Parses the 'url' field of a related application, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-url-member-of-an-application
  // Returns the parsed KURL if any, a null optional if the parsing failed.
  base::Optional<KURL> ParseRelatedApplicationURL(
      const JSONObject* application);

  // Parses the 'id' field of a related application, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-id-member-of-an-application
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseRelatedApplicationId(const JSONObject* application);

  // Parses the 'related_applications' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-related_applications-member
  // Returns a vector of ManifestRelatedApplicationPtr with the successfully
  // parsed applications, if any. An empty vector if the field was not present
  // or empty.
  Vector<mojom::blink::ManifestRelatedApplicationPtr> ParseRelatedApplications(
      const JSONObject* object);

  // Parses the 'prefer_related_applications' field on the manifest, as defined
  // in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-prefer_related_applications-member
  // returns true iff the field could be parsed as the boolean true.
  bool ParsePreferRelatedApplications(const JSONObject* object);

  // Parses the 'theme_color' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-theme_color-member
  // Returns the parsed theme color if any, or a null optional otherwise.
  base::Optional<RGBA32> ParseThemeColor(const JSONObject* object);

  // Parses the 'background_color' field of the manifest, as defined in:
  // https://w3c.github.io/manifest/#dfn-steps-for-processing-the-background_color-member
  // Returns the parsed background color if any, or a null optional otherwise.
  base::Optional<RGBA32> ParseBackgroundColor(const JSONObject* object);

  // Parses the 'gcm_sender_id' field of the manifest.
  // This is a proprietary extension of the Web Manifest specification.
  // Returns the parsed string if any, a null string if the parsing failed.
  String ParseGCMSenderID(const JSONObject* object);

  void AddErrorInfo(const String& error_msg,
                    bool critical = false,
                    int error_line = 0,
                    int error_column = 0);

  const String data_;
  KURL manifest_url_;
  KURL document_url_;

  bool failed_;
  mojom::blink::ManifestPtr manifest_;
  Vector<mojom::blink::ManifestErrorPtr> errors_;

  DISALLOW_COPY_AND_ASSIGN(ManifestParser);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_PARSER_H_
