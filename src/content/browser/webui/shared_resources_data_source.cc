// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include <stddef.h>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "mojo/public/js/grit/mojo_bindings_resources_map.h"
#include "ui/base/layout.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"

#if defined(OS_CHROMEOS)
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/grit/chromeos_resources_map.h"
#endif

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace content {

namespace {

struct IdrGzipped {
  int idr;
  bool gzipped;
};
using ResourcesMap = std::unordered_map<std::string, IdrGzipped>;

#if defined(OS_CHROMEOS)
const char kPolymerHtml[] = "polymer/v1_0/polymer/polymer.html";
const char kPolymerJs[] = "polymer/v1_0/polymer/polymer-extracted.js";
const char kPolymer2Html[] = "polymer/v1_0/polymer2/polymer.html";
const char kPolymer2Js[] = "polymer/v1_0/polymer2/polymer-extracted.js";
const char kHtmlImportsJs[] = "polymer/v1_0/html-imports/html-imports.min.js";
const char kHtmlImportsV0Js[] =
    "polymer/v1_0/html-imports-v0/html-imports.min.js";

// Utility for determining if both Polymer 1 and Polymer 2 are needed.
bool UsingMultiplePolymerVersions() {
  return base::FeatureList::IsEnabled(features::kWebUIPolymer2) &&
         base::FeatureList::IsEnabled(features::kWebUIPolymer2Exceptions);
}
#endif  // defined(OS_CHROMEOS)

const std::map<std::string, std::string> CreatePathPrefixAliasesMap() {
  // TODO(rkc): Once we have a separate source for apps, remove '*/apps/'
  // aliases.
  std::map<std::string, std::string> aliases = {
      {"../../../third_party/polymer/v1_0/components-chromium/",
       "polymer/v1_0/"},
      {"../../../third_party/web-animations-js/sources/",
       "polymer/v1_0/web-animations-js/"},
      {"../../views/resources/default_100_percent/common/", "images/apps/"},
      {"../../views/resources/default_200_percent/common/", "images/2x/apps/"},
      {"../../webui/resources/cr_components/", "cr_components/"},
      {"../../webui/resources/cr_elements/", "cr_elements/"},
  };

#if defined(OS_CHROMEOS)
  if (UsingMultiplePolymerVersions())
    return aliases;
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebUIPolymer2)) {
    aliases["../../../third_party/polymer/v1_0/components-chromium/polymer2/"] =
        "polymer/v1_0/polymer/";
  } else {
    aliases
        ["../../../third_party/polymer/v1_0/components-chromium/"
         "html-imports-v0/"] = "polymer/v1_0/html-imports/";
  }
#endif  // !defined(OS_ANDROID)
  return aliases;
}

const std::map<int, std::string> CreateMojoResourceIdToAliasMap() {
  return std::map<int, std::string> {
    {IDR_MOJO_MOJO_BINDINGS_LITE_JS, "js/mojo_bindings_lite.js"},
        {IDR_MOJO_BIG_BUFFER_MOJOM_LITE_JS, "js/big_buffer.mojom-lite.js"},
        {IDR_MOJO_FILE_MOJOM_LITE_JS, "js/file.mojom-lite.js"},
        {IDR_MOJO_STRING16_MOJOM_LITE_JS, "js/string16.mojom-lite.js"},
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
        {IDR_MOJO_TIME_MOJOM_LITE_JS, "js/time.mojom-lite.js"},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  };
}

#if defined(OS_CHROMEOS)
const std::map<int, std::string> CreateChromeosMojoResourceIdToAliasMap() {
  return std::map<int, std::string>{
      {IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_LITE_JS,
       "js/chromeos/device_sync.mojom-lite.js"},
      {IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_LITE_JS,
       "js/chromeos/multidevice_setup.mojom-lite.js"},
      {IDR_MULTIDEVICE_MULTIDEVICE_SETUP_CONSTANTS_MOJOM_LITE_JS,
       "js/chromeos/multidevice_setup_constants.mojom-lite.js"},
      {IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_LITE_JS,
       "js/chromeos/multidevice_types.mojom-lite.js"},
  };
}
#endif  // !defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
bool ShouldIgnore(std::string resource) {
#if defined(OS_CHROMEOS)
  if (UsingMultiplePolymerVersions())
    return false;
#endif  // defined(OS_CHROMEOS)

  if (base::FeatureList::IsEnabled(features::kWebUIPolymer2) &&
      (base::StartsWith(
           resource,
           "../../../third_party/polymer/v1_0/components-chromium/polymer/",
           base::CompareCase::SENSITIVE) ||
       base::StartsWith(resource,
                        "../../../third_party/polymer/v1_0/components-chromium/"
                        "html-imports-v0/",
                        base::CompareCase::SENSITIVE))) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kWebUIPolymer2) &&
      (base::StartsWith(
           resource,
           "../../../third_party/polymer/v1_0/components-chromium/polymer2/",
           base::CompareCase::SENSITIVE) ||
       base::StartsWith(resource,
                        "../../../third_party/polymer/v1_0/components-chromium/"
                        "html-imports/",
                        base::CompareCase::SENSITIVE))) {
    return true;
  }

  return false;
}
#endif  // !defined(OS_ANDROID)

void AddResource(const std::string& path,
                 int resource_id,
                 bool gzipped,
                 ResourcesMap* resources_map) {
  IdrGzipped idr_gzipped = {resource_id, gzipped};
  if (!resources_map->insert(std::make_pair(path, idr_gzipped)).second)
    NOTREACHED() << "Redefinition of '" << path << "'";
}

void AddResourcesToMap(ResourcesMap* resources_map) {
  const std::map<std::string, std::string> aliases =
      CreatePathPrefixAliasesMap();

  for (size_t i = 0; i < kWebuiResourcesSize; ++i) {
    const auto& resource = kWebuiResources[i];

#if !defined(OS_ANDROID)
    if (ShouldIgnore(resource.name))
      continue;
#endif  // !defined(OS_ANDROID)

    AddResource(resource.name, resource.value, resource.gzipped, resources_map);

    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
      if (base::StartsWith(resource.name, it->first,
                           base::CompareCase::SENSITIVE)) {
        std::string resource_name(resource.name);
        AddResource(it->second + resource_name.substr(it->first.length()),
                    resource.value, resource.gzipped, resources_map);
      }
    }
  }
}

// Adds |resources| to |resources_map|, but renames each resource according to
// the scheme in |resource_aliases|, which maps from resource ID to resource
// alias. Note that resources which do not have an alias will not be added.
void AddAliasedResourcesToMap(
    const std::map<int, std::string>& resource_aliases,
    const GzippedGritResourceMap resources[],
    size_t resources_size,
    ResourcesMap* resources_map) {
  for (size_t i = 0; i < resources_size; ++i) {
    const auto& resource = resources[i];

    const auto it = resource_aliases.find(resource.value);
    if (it == resource_aliases.end())
      continue;

    AddResource(it->second, resource.value, resource.gzipped, resources_map);
  }
}

const ResourcesMap* CreateResourcesMap() {
  ResourcesMap* result = new ResourcesMap();
  AddResourcesToMap(result);
  AddAliasedResourcesToMap(CreateMojoResourceIdToAliasMap(),
                           kMojoBindingsResources, kMojoBindingsResourcesSize,
                           result);
#if defined(OS_CHROMEOS)
  AddAliasedResourcesToMap(CreateChromeosMojoResourceIdToAliasMap(),
                           kChromeosResources, kChromeosResourcesSize, result);
#endif  // !defined(OS_CHROMEOS)
  return result;
}

const ResourcesMap& GetResourcesMap() {
  // This pointer will be intentionally leaked on shutdown.
  static const ResourcesMap* resources_map = CreateResourcesMap();
  return *resources_map;
}

int GetIdrForPath(const std::string& path) {
  const ResourcesMap& resources_map = GetResourcesMap();
  auto it = resources_map.find(path);
  return it != resources_map.end() ? it->second.idr : -1;
}

}  // namespace

SharedResourcesDataSource::SharedResourcesDataSource() {
}

SharedResourcesDataSource::~SharedResourcesDataSource() {
}

std::string SharedResourcesDataSource::GetSource() const {
  return kChromeUIResourcesHost;
}

void SharedResourcesDataSource::StartDataRequest(
    const std::string& path,
    const ResourceRequestInfo::WebContentsGetter& wc_getter,
    const URLDataSource::GotDataCallback& callback) {
  std::string updated_path = path;
#if defined(OS_CHROMEOS)
  // If this is a Polymer request and multiple Polymer versions are enabled,
  // return the Polymer 2 path unless the request is from the
  // |disabled_polymer2_host_|.
  if ((path == kPolymerHtml || path == kPolymerJs || path == kHtmlImportsJs) &&
      UsingMultiplePolymerVersions()) {
    bool polymer2 = !IsPolymer2DisabledForPage(wc_getter);
    if (polymer2 && (path == kPolymerHtml || path == kPolymerJs)) {
      updated_path = path == kPolymerHtml ? kPolymer2Html : kPolymer2Js;
    } else if (!polymer2 && path == kHtmlImportsJs) {
      updated_path = kHtmlImportsV0Js;
    }
  }
#endif  // defined(OS_CHROMEOS)

  int idr = GetIdrForPath(updated_path);
  DCHECK_NE(-1, idr) << " path: " << updated_path;
  scoped_refptr<base::RefCountedMemory> bytes;

  if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS) {
    std::string css = webui::GetWebUiCssTextDefaults();
    bytes = base::RefCountedString::TakeString(&css);
  } else if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS_MD) {
    std::string css = webui::GetWebUiCssTextDefaultsMd();
    bytes = base::RefCountedString::TakeString(&css);
  } else {
    bytes = GetContentClient()->GetDataResourceBytes(idr);
  }

  callback.Run(bytes.get());
}

bool SharedResourcesDataSource::AllowCaching() const {
  // Should not be cached to reflect dynamically-generated contents that may
  // depend on the current locale.
  return false;
}

std::string SharedResourcesDataSource::GetMimeType(
    const std::string& path) const {
  if (path.empty())
    return "text/html";

#if defined(OS_WIN)
  base::FilePath file(base::UTF8ToWide(path));
  std::string extension = base::WideToUTF8(file.FinalExtension());
#else
  base::FilePath file(path);
  std::string extension = file.FinalExtension();
#endif

  if (!extension.empty())
    extension.erase(0, 1);

  if (extension == "html")
    return "text/html";

  if (extension == "css")
    return "text/css";

  if (extension == "js")
    return "application/javascript";

  if (extension == "png")
    return "image/png";

  if (extension == "gif")
    return "image/gif";

  if (extension == "svg")
    return "image/svg+xml";

  if (extension == "woff2")
    return "application/font-woff2";

  NOTREACHED() << path;
  return "text/plain";
}

bool SharedResourcesDataSource::ShouldServeMimeTypeAsContentTypeHeader() const {
  return true;
}

scoped_refptr<base::SingleThreadTaskRunner>
SharedResourcesDataSource::TaskRunnerForRequestPath(
    const std::string& path) const {
  // Since WebContentsGetter can only be run on the UI thread, always return
  // a task runner if we need to choose between Polymer resources based on the
  // WebContents that is requesting the resource.
  // TODO (rbpotter): Remove this once the OOBE Polymer 2 migration is complete.
#if defined(OS_CHROMEOS)
  if (UsingMultiplePolymerVersions())
    return base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI});
#endif  // defined(OS_CHROMEOS)

  int idr = GetIdrForPath(path);
  if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS ||
      idr == IDR_WEBUI_CSS_TEXT_DEFAULTS_MD) {
    // Use UI thread to load CSS since its construction touches non-thread-safe
    // gfx::Font names in ui::ResourceBundle.
    return base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI});
  }

  return nullptr;
}

std::string
SharedResourcesDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) const {
  // For now we give access only for "chrome://*" origins.
  // According to CORS spec, Access-Control-Allow-Origin header doesn't support
  // wildcards, so we need to set its value explicitly by passing the |origin|
  // back.
  std::string allowed_origin_prefix = kChromeUIScheme;
  allowed_origin_prefix += "://";
  if (!base::StartsWith(origin, allowed_origin_prefix,
                        base::CompareCase::SENSITIVE)) {
    return "null";
  }
  return origin;
}

bool SharedResourcesDataSource::IsGzipped(const std::string& path) const {
  auto it = GetResourcesMap().find(path);
  DCHECK(it != GetResourcesMap().end()) << "missing shared resource: " << path;
  return it != GetResourcesMap().end() ? it->second.gzipped : false;
}

#if defined(OS_CHROMEOS)
void SharedResourcesDataSource::DisablePolymer2ForHost(
    const std::string& host) {
  DCHECK(disabled_polymer2_host_.empty() || host == disabled_polymer2_host_);
  disabled_polymer2_host_ = host;
}

// Returns true if the WebContents making the request has disabled Polymer 2.
bool SharedResourcesDataSource::IsPolymer2DisabledForPage(
    const ResourceRequestInfo::WebContentsGetter& wc_getter) {
  // Return false in these cases, which sometimes occur in tests.
  if (!wc_getter)
    return false;

  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents)
    return false;

  return web_contents->GetLastCommittedURL().host_piece() ==
         disabled_polymer2_host_;
}
#endif  // defined(OS_CHROMEOS)
}  // namespace content
