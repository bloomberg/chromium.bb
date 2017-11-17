// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/fontconfig_util_linux.h"

#include <fontconfig/fontconfig.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace gfx {

const char* const kSystemFontsForFontconfig[] = {
    "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
    "/usr/share/fonts/opentype/ipafont-gothic/ipagp.ttf",
    "/usr/share/fonts/opentype/ipafont-mincho/ipam.ttf",
    "/usr/share/fonts/opentype/ipafont-mincho/ipamp.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Impact.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Italic.ttf",
};

const size_t kNumSystemFontsForFontconfig =
    arraysize(kSystemFontsForFontconfig);

const char* const kCloudStorageSyncedFonts[] = {
    // The DejaVuSans font is used by the css2.1 tests.
    "DejaVuSans.ttf", "Lohit-Devanagari.ttf", "Lohit-Tamil.ttf",
    "MuktiNarrow.ttf"};

const size_t kNumCloudStorageSyncedFonts = arraysize(kCloudStorageSyncedFonts);

const char kFontconfigFileHeader[] =
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n"
    "<fontconfig>\n";
const char kFontconfigFileFooter[] = "</fontconfig>";
const char kFontconfigMatchFontHeader[] = "  <match target=\"font\">\n";
const char kFontconfigMatchPatternHeader[] = "  <match target=\"pattern\">\n";
const char kFontconfigMatchFooter[] = "  </match>\n";

void SetUpFontconfig() {
  FcInit();

  // A primer on undocumented FcConfig reference-counting:
  //
  // - FcConfigCreate() creates a config with a refcount of 1.
  // - FcConfigReference() increments a config's refcount.
  // - FcConfigDestroy() decrements a config's refcount, deallocating the
  //   config when the count reaches 0.
  // - FcConfigSetCurrent() calls FcConfigDestroy() on the old config, and
  //   calls FcConfigReference() on the new config.
  FcConfig* config = FcConfigCreate();
  FcConfigBuildFonts(config);
  CHECK(FcConfigSetCurrent(config));
}

void TearDownFontconfig() {
  FcFini();
}

bool LoadFontIntoFontconfig(const base::FilePath& path) {
  if (!base::PathExists(path)) {
    LOG(ERROR) << "You are missing " << path.value() << ". Try re-running "
               << "build/install-build-deps.sh. "
               << "Please make sure that "
               << "third_party/content_shell_fonts/ has downloaded "
               << "and extracted the content_shell_test_fonts."
               << "Also see "
               << "https://chromium.googlesource.com/chromium/src/+/master/"
               << "docs/layout_tests_linux.md";
    return false;
  }

  if (!FcConfigAppFontAddFile(
          NULL, reinterpret_cast<const FcChar8*>(path.value().c_str()))) {
    LOG(ERROR) << "Failed to load font " << path.value();
    return false;
  }

  return true;
}

bool LoadSystemFontIntoFontconfig(const std::string& basename) {
  for (size_t i = 0; i < kNumSystemFontsForFontconfig; ++i) {
    base::FilePath path(kSystemFontsForFontconfig[i]);
    if (base::EqualsCaseInsensitiveASCII(path.BaseName().value(), basename))
      return LoadFontIntoFontconfig(path);
  }
  LOG(ERROR) << "Unable to find system font named " << basename;
  return false;
}

bool LoadCloudStorageSyncedFontIntoFontConfig(const std::string& fontfilename) {
  base::FilePath content_shell_test_fonts_path;
  PathService::Get(base::DIR_MODULE, &content_shell_test_fonts_path);
  // See third_party/content_shell_test_fonts/ for information how these fonts
  // are synced. Target directory out/<buildDir>/content_shell_test_fonts needs
  // to match what is specified as the target in
  // third_party/content_shell_test_fonts/BUILD.gn as output directory.
  base::FilePath font_file_path =
      content_shell_test_fonts_path
          .Append(FILE_PATH_LITERAL("content_shell_test_fonts"))
          .Append(FILE_PATH_LITERAL(fontfilename));
  return LoadFontIntoFontconfig(font_file_path);
}

bool LoadConfigFileIntoFontconfig(const base::FilePath& path) {
  // Unlike other FcConfig functions, FcConfigParseAndLoad() doesn't default to
  // the current config when passed NULL. So that's cool.
  if (!FcConfigParseAndLoad(FcConfigGetCurrent(),
          reinterpret_cast<const FcChar8*>(path.value().c_str()), FcTrue)) {
    LOG(ERROR) << "Fontconfig failed to load " << path.value();
    return false;
  }
  return true;
}

bool LoadConfigDataIntoFontconfig(const base::FilePath& temp_dir,
                                  const std::string& data) {
  base::FilePath path;
  if (!CreateTemporaryFileInDir(temp_dir, &path)) {
    PLOG(ERROR) << "Unable to create temporary file in " << temp_dir.value();
    return false;
  }
  if (base::WriteFile(path, data.data(), data.size()) !=
      static_cast<int>(data.size())) {
    PLOG(ERROR) << "Unable to write config data to " << path.value();
    return false;
  }
  return LoadConfigFileIntoFontconfig(path);
}

std::string CreateFontconfigEditStanza(const std::string& name,
                                       const std::string& type,
                                       const std::string& value) {
  return base::StringPrintf(
      "    <edit name=\"%s\" mode=\"assign\">\n"
      "      <%s>%s</%s>\n"
      "    </edit>\n",
      name.c_str(), type.c_str(), value.c_str(), type.c_str());
}

std::string CreateFontconfigTestStanza(const std::string& name,
                                       const std::string& op,
                                       const std::string& type,
                                       const std::string& value) {
  return base::StringPrintf(
      "    <test name=\"%s\" compare=\"%s\" qual=\"any\">\n"
      "      <%s>%s</%s>\n"
      "    </test>\n",
      name.c_str(), op.c_str(), type.c_str(), value.c_str(), type.c_str());
}

std::string CreateFontconfigAliasStanza(const std::string& original_family,
                                        const std::string& preferred_family) {
  return base::StringPrintf(
      "  <alias>\n"
      "    <family>%s</family>\n"
      "    <prefer><family>%s</family></prefer>\n"
      "  </alias>\n",
      original_family.c_str(), preferred_family.c_str());
}

}  // namespace gfx
