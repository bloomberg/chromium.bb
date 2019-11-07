/*
* Copyright (C) 2013 Bloomberg Finance L.P.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#include <blpwtk2_utility.h>
#include <blpwtk2_webviewproxy.h>
#include "blpwtk2_version.h"

#include <base/command_line.h>
#include <base/json/json_writer.h>
#include <base/system/sys_info.h>
#include <content/browser/gpu/compositor_util.h>
#include <content/browser/gpu/gpu_data_manager_impl.h>
#include <content/browser/gpu/gpu_internals_ui.h>
#include <third_party/angle/src/common/version.h>

#include <fstream>

namespace blpwtk2 {

// https://chromium.googlesource.com/chromium/src/+/96bd9ef90d95efa332485195e03c63477b90bb07%5E%21
// The manually-updated software_rendering_list and gpu_driver_bug_list versions
// are replaced them with Chromium git commit hashes.
// software_rendering_list_url and gpu_driver_bug_list_url are added for
// convenience
namespace {
const std::string g_chromiumGitUrlBase{
    "https://chromium.googlesource.com/chromium/src/+/"};
const std::string g_gpuListVersion{CHROMIUM_VERSION};
const std::string g_softwareRenderingList{
    "gpu/config/software_rendering_list.json"};
const std::string g_gpuDriverBugList{
    "gpu/config/gpu_driver_bug_list.json"};

std::string createSourcePermalink(const std::string& revisionIdentifier,
                                  const std::string& filepath) {
  // Example: https://chromium.googlesource.com/chromium/src/+/67.0.3396.127/gpu/config/software_rendering_list.json
  return g_chromiumGitUrlBase + revisionIdentifier + "/" + filepath;
}

std::string getGpuInfo() {
    base::DictionaryValue gpuInfo;
    gpuInfo.Set("featureStatus", content::GetFeatureStatus());
    gpuInfo.Set("problems", content::GetProblems());

    base::ListValue* workarounds = new base::ListValue();
    for (const auto& workaround : content::GetDriverBugWorkarounds()) {
        workarounds->AppendString(workaround);
    }
    std::unique_ptr<base::Value> workarounds_v(workarounds);
    gpuInfo.Set("workarounds", std::move(workarounds_v));
    gpuInfo.Set("memoryBufferInfo", content::GpuInternalsUI::GetGpuMemoryBufferInfo());
    gpuInfo.Set("LogMessage", content::GpuDataManagerImpl::GetInstance()->GetLogMessages());
    
    gpuInfo.SetString("gpu_device_string", content::GpuDataManagerImpl::GetInstance()->GetGPUInfoForHardwareGpu().active_gpu().device_string);
    gpuInfo.SetString("gpu_vendor", content::GpuDataManagerImpl::GetInstance()->GetGPUInfoForHardwareGpu().active_gpu().vendor_string);
    gpuInfo.SetString("gpu_driver_vendor", content::GpuDataManagerImpl::GetInstance()->GetGPUInfoForHardwareGpu().active_gpu().driver_vendor);
    gpuInfo.SetString("gpu_driver_date", content::GpuDataManagerImpl::GetInstance()->GetGPUInfoForHardwareGpu().active_gpu().driver_date);
    gpuInfo.SetBoolean("gpu_active", content::GpuDataManagerImpl::GetInstance()->GetGPUInfoForHardwareGpu().active_gpu().active);
    gpuInfo.SetBoolean("software_rendering", content::GpuDataManagerImpl::GetInstance()->GetGPUInfoForHardwareGpu().software_rendering);

    gpuInfo.SetString("command_line",
        base::CommandLine::ForCurrentProcess()->GetCommandLineString());

    gpuInfo.SetString("operating_system",
        base::SysInfo::OperatingSystemName() + " " +
        base::SysInfo::OperatingSystemVersion());

    gpuInfo.SetString("angle_commit_id", ANGLE_COMMIT_HASH);
    gpuInfo.SetString("graphics_backend", "Skia");
    gpuInfo.SetString("blacklist_version", g_gpuListVersion);
    gpuInfo.SetString("driver_bug_list_version", g_gpuListVersion);
    gpuInfo.SetString(
        "software_rendering_list_url",
        createSourcePermalink(g_gpuListVersion, g_softwareRenderingList));
    gpuInfo.SetString(
        "gpu_driver_bug_list_url",
        createSourcePermalink(g_gpuListVersion, g_gpuDriverBugList));
    std::string json;
    base::JSONWriter::Write(gpuInfo, &json);
    return json;
}
}  // namespace

void DumpGpuInfo(const std::string& path) {
    std::string json = getGpuInfo();
    std::ofstream file(path, std::ios::binary);
    file << json;
    file.close();
}

std::string GetGpuInfo() {
    return getGpuInfo();
}
}  // close namespace blpwtk2

// vim: ts=4 et

