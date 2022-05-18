// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/terminal_source.h"

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "net/base/mime_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/webui/webui_allowlist.h"

namespace {
constexpr base::FilePath::CharType kTerminalRoot[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/crosh_builtin");
constexpr char kDefaultMime[] = "text/html";

// Attempts to read |path| as plain file.  If read fails, attempts to read
// |path|.gz and decompress. Returns true if either file is read ok.
bool ReadUncompressedOrGzip(base::FilePath path, std::string* content) {
  bool result = base::ReadFileToString(path, content);
  if (!result) {
    result =
        base::ReadFileToString(base::FilePath(path.value() + ".gz"), content);
    result = compression::GzipUncompress(*content, content);
  }
  return result;
}

void ReadFile(const base::FilePath downloads,
              const std::string& relative_path,
              content::URLDataSource::GotDataCallback callback) {
  base::FilePath path;
  std::string content;
  bool result = false;

  // If chrome://flags#terminal-dev set on dev channel, check Downloads.
  if (chrome::GetChannel() <= version_info::Channel::DEV &&
      base::FeatureList::IsEnabled(chromeos::features::kTerminalDev)) {
    path = downloads.Append("crosh_builtin").Append(relative_path);
    result = ReadUncompressedOrGzip(path, &content);
  }
  if (!result) {
    path = base::FilePath(kTerminalRoot).Append(relative_path);
    result = ReadUncompressedOrGzip(path, &content);
  }

  // Terminal gets files from /usr/share/chromeos-assets/crosh-builtin.
  // In chromium tests, these files don't exist, so we serve dummy values.
  if (!result) {
    static const base::NoDestructor<base::flat_map<std::string, std::string>>
        kTestFiles({
            {"html/crosh.html", ""},
            {"html/terminal.html", "<script src='/js/terminal.js'></script>"},
            {"html/terminal_home.html", ""},
            {"js/terminal.js",
             "chrome.terminalPrivate.openVmshellProcess([], () => {})"},
        });
    auto it = kTestFiles->find(relative_path);
    if (it != kTestFiles->end()) {
      content = it->second;
      result = true;
    }
  }

  DCHECK(result) << path;

  scoped_refptr<base::RefCountedString> response =
      base::RefCountedString::TakeString(&content);
  std::move(callback).Run(response.get());
}
}  // namespace

// static
std::unique_ptr<TerminalSource> TerminalSource::ForCrosh(Profile* profile) {
  std::string default_file = "html/crosh.html";
  if (base::FeatureList::IsEnabled(chromeos::features::kCroshSWA)) {
    default_file = "html/terminal.html";
  }
  return base::WrapUnique(new TerminalSource(
      profile, chrome::kChromeUIUntrustedCroshURL, default_file, false));
}

// static
std::unique_ptr<TerminalSource> TerminalSource::ForTerminal(Profile* profile) {
  return base::WrapUnique(new TerminalSource(
      profile, chrome::kChromeUIUntrustedTerminalURL, "html/terminal.html",
      profile->GetPrefs()
          ->FindPreference(crostini::prefs::kTerminalSshAllowedByPolicy)
          ->GetValue()
          ->GetBool()));
}

TerminalSource::TerminalSource(Profile* profile,
                               std::string source,
                               std::string default_file,
                               bool ssh_allowed)
    : profile_(profile),
      source_(source),
      default_file_(default_file),
      ssh_allowed_(ssh_allowed),
      downloads_(file_manager::util::GetDownloadsFolderForProfile(profile)) {
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(profile);
  const url::Origin terminal_origin = url::Origin::Create(GURL(source));
  CHECK(!terminal_origin.opaque());
  for (auto permission :
       {ContentSettingsType::CLIPBOARD_READ_WRITE, ContentSettingsType::COOKIES,
        ContentSettingsType::IMAGES, ContentSettingsType::JAVASCRIPT,
        ContentSettingsType::NOTIFICATIONS, ContentSettingsType::POPUPS,
        ContentSettingsType::SOUND}) {
    webui_allowlist->RegisterAutoGrantedPermission(terminal_origin, permission);
  }
  webui_allowlist->RegisterAutoGrantedThirdPartyCookies(
      terminal_origin, {ContentSettingsPattern::Wildcard()});
}

TerminalSource::~TerminalSource() = default;

std::string TerminalSource::GetSource() {
  return source_;
}

void TerminalSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // skip first '/' in path.
  std::string path = url.path().substr(1);
  if (path.empty())
    path = default_file_;

  // Refresh the $i8n{themeColor} replacement for css files.
  if (base::EndsWith(path, ".css", base::CompareCase::INSENSITIVE_ASCII)) {
    replacements_["themeColor"] = base::EscapeForHTML(
        crostini::GetTerminalSettingBackgroundColor(profile_));
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, downloads_, path, std::move(callback)));
}

std::string TerminalSource::GetMimeType(const std::string& path) {
  std::string mime_type(kDefaultMime);
  std::string ext = base::FilePath(path).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

bool TerminalSource::ShouldServeMimeTypeAsContentTypeHeader() {
  // TerminalSource pages include js modules which require an explicit MimeType.
  return true;
}

const ui::TemplateReplacements* TerminalSource::GetReplacements() {
  return &replacements_;
}

std::string TerminalSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  // CSP required for SSH.
  if (ssh_allowed_) {
    switch (directive) {
      case network::mojom::CSPDirectiveName::ConnectSrc:
        return "connect-src 'self' "
               "https://*.corp.google.com:* wss://*.corp.google.com:* "
               "https://*.r.ext.google.com:* wss://*.r.ext.google.com:*;";
      case network::mojom::CSPDirectiveName::FrameAncestors:
        return "frame-ancestors 'self';";
      case network::mojom::CSPDirectiveName::FrameSrc:
        return "frame-src 'self';";
      case network::mojom::CSPDirectiveName::ObjectSrc:
        return "object-src 'self';";
      default:
        break;
    }
  }

  switch (directive) {
    case network::mojom::CSPDirectiveName::ImgSrc:
      return "img-src * data: blob:;";
    case network::mojom::CSPDirectiveName::MediaSrc:
      return "media-src data:;";
    case network::mojom::CSPDirectiveName::StyleSrc:
      return "style-src * 'unsafe-inline'; font-src *;";
    case network::mojom::CSPDirectiveName::RequireTrustedTypesFor:
      [[fallthrough]];
    case network::mojom::CSPDirectiveName::TrustedTypes:
      // TODO(crbug.com/1098685): Trusted Type remaining WebUI
      // This removes require-trusted-types-for and trusted-types directives
      // from the CSP header.
      return std::string();
    default:
      return content::URLDataSource::GetContentSecurityPolicy(directive);
  }
}
