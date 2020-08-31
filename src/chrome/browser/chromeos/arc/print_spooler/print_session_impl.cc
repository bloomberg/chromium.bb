// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print_spooler/print_session_impl.h"

#include <limits>
#include <string>
#include <utility>

#include "ash/public/cpp/arc_custom_tab.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/print_spooler/arc_print_spooler_util.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printing_service.h"
#include "components/arc/mojom/print_common.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/c/system/types.h"
#include "net/base/filename_util.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "printing/units.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/size.h"

namespace arc {

namespace {

constexpr int kMinimumPdfSize = 50;

// Converts a color mode to its Mojo type.
mojom::PrintColorMode ToArcColorMode(int color_mode) {
  base::Optional<bool> is_color = printing::IsColorModelSelected(color_mode);
  return is_color.value() ? mojom::PrintColorMode::COLOR
                          : mojom::PrintColorMode::MONOCHROME;
}

// Converts a duplex mode to its Mojo type.
mojom::PrintDuplexMode ToArcDuplexMode(int duplex_mode) {
  printing::mojom::DuplexMode mode =
      static_cast<printing::mojom::DuplexMode>(duplex_mode);
  switch (mode) {
    case printing::mojom::DuplexMode::kLongEdge:
      return mojom::PrintDuplexMode::LONG_EDGE;
    case printing::mojom::DuplexMode::kShortEdge:
      return mojom::PrintDuplexMode::SHORT_EDGE;
    default:
      return mojom::PrintDuplexMode::NONE;
  }
}

// Gets and builds the print attributes from the job settings.
mojom::PrintAttributesPtr GetPrintAttributes(const base::Value& job_settings) {
  // PrintMediaSize:
  const base::Value* media_size_value =
      job_settings.FindDictKey(printing::kSettingMediaSize);
  if (!media_size_value)
    return nullptr;
  // Vendor ID will be empty when Destination is Save as PDF.
  const std::string* vendor_id =
      media_size_value->FindStringKey(printing::kSettingMediaSizeVendorId);
  std::string id = "PDF";
  if (vendor_id && !vendor_id->empty()) {
    id = *vendor_id;
  }
  base::Optional<int> width_microns =
      media_size_value->FindIntKey(printing::kSettingMediaSizeWidthMicrons);
  base::Optional<int> height_microns =
      media_size_value->FindIntKey(printing::kSettingMediaSizeHeightMicrons);
  if (!width_microns.has_value() || !height_microns.has_value())
    return nullptr;
  // Swap the width and height if layout is landscape.
  base::Optional<bool> landscape =
      job_settings.FindBoolKey(printing::kSettingLandscape);
  if (!landscape.has_value())
    return nullptr;
  gfx::Size size_micron;
  if (landscape.value()) {
    size_micron = gfx::Size(height_microns.value(), width_microns.value());
  } else {
    size_micron = gfx::Size(width_microns.value(), height_microns.value());
  }
  gfx::Size size_mil =
      gfx::ScaleToRoundedSize(size_micron, 1.0f / printing::kMicronsPerMil);
  mojom::PrintMediaSizePtr media_size = mojom::PrintMediaSize::New(
      id, "ARC", size_mil.width(), size_mil.height());

  // PrintResolution:
  int horizontal_dpi = job_settings.FindIntKey(printing::kSettingDpiHorizontal)
                           .value_or(printing::kDefaultPdfDpi);
  int vertical_dpi = job_settings.FindIntKey(printing::kSettingDpiVertical)
                         .value_or(printing::kDefaultPdfDpi);

  // PrintMargins:
  // Chrome uses margins to fit content to the printable area. Android uses
  // margins to crop content to the printable area. Set margins to 0 to prevent
  // cropping.
  mojom::PrintMarginsPtr margins = mojom::PrintMargins::New(0, 0, 0, 0);

  // PrintColorMode:
  base::Optional<int> color = job_settings.FindIntKey(printing::kSettingColor);
  if (!color.has_value())
    return nullptr;
  mojom::PrintColorMode color_mode = ToArcColorMode(color.value());

  // PrintDuplexMode:
  base::Optional<int> duplex =
      job_settings.FindIntKey(printing::kSettingDuplexMode);
  if (!duplex.has_value())
    return nullptr;
  mojom::PrintDuplexMode duplex_mode = ToArcDuplexMode(duplex.value());

  return mojom::PrintAttributes::New(
      std::move(media_size), gfx::Size(horizontal_dpi, vertical_dpi),
      std::move(margins), color_mode, duplex_mode);
}

// Creates a PrintDocumentRequest from the provided |job_settings|. Uses helper
// functions to parse |job_settings|.
mojom::PrintDocumentRequestPtr PrintDocumentRequestFromJobSettings(
    const base::Value& job_settings) {
  return mojom::PrintDocumentRequest::New(
      printing::GetPageRangesFromJobSettings(job_settings),
      GetPrintAttributes(job_settings));
}

// Uses the provided ScopedHandle to read a preview document from ARC into
// read-only shared memory.
base::ReadOnlySharedMemoryRegion ReadPreviewDocument(
    mojo::ScopedHandle preview_document,
    size_t data_size) {
  base::PlatformFile platform_file;
  if (mojo::UnwrapPlatformFile(std::move(preview_document), &platform_file) !=
      MOJO_RESULT_OK) {
    return base::ReadOnlySharedMemoryRegion();
  }

  base::File src_file(platform_file);
  if (!src_file.IsValid()) {
    DPLOG(ERROR) << "Source file is invalid.";
    return base::ReadOnlySharedMemoryRegion();
  }

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(data_size);
  if (!region_mapping.IsValid())
    return std::move(region_mapping.region);

  bool success = src_file.ReadAndCheck(
      0, region_mapping.mapping.GetMemoryAsSpan<uint8_t>());
  if (!success) {
    DPLOG(ERROR) << "Error reading PDF.";
    return base::ReadOnlySharedMemoryRegion();
  }

  return std::move(region_mapping.region);
}

// Checks if |web_contents| contains a subframe with a Chrome extension URL
// that claims to be done loading.  This isn't foolproof, but it ensures that
// print preview will find the embedded plugin element instead of trying to
// print the top-level frame.
bool IsPdfPluginLoaded(content::WebContents* web_contents) {
  if (!web_contents->IsDocumentOnLoadCompletedInMainFrame()) {
    VLOG(1) << "Top-level WebContents not ready yet.";
    return false;
  }

  content::WebContents* contents_to_use =
      printing::GetWebContentsToUse(web_contents);
  if (contents_to_use == web_contents) {
    VLOG(1) << "No plugin WebContents found yet.";
    return false;
  }

  GURL url = contents_to_use->GetMainFrame()->GetLastCommittedURL();
  if (!url.SchemeIs("chrome-extension")) {
    VLOG(1) << "Plugin frame URL not loaded yet.";
    return false;
  }

  if (!contents_to_use->IsDocumentOnLoadCompletedInMainFrame()) {
    VLOG(1) << "Plugin frame still loading.";
    return false;
  }

  VLOG(1) << "PDF plugin has loaded.";
  return true;
}

}  // namespace

// static
mojo::PendingRemote<mojom::PrintSessionHost> PrintSessionImpl::Create(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab,
    mojom::PrintSessionInstancePtr instance) {
  if (!custom_tab || !instance)
    return mojo::NullRemote();

  // This object will be deleted when the mojo connection is closed.
  mojo::PendingRemote<mojom::PrintSessionHost> remote;
  new PrintSessionImpl(std::move(web_contents), std::move(custom_tab),
                       std::move(instance),
                       remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

PrintSessionImpl::PrintSessionImpl(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab,
    mojom::PrintSessionInstancePtr instance,
    mojo::PendingReceiver<mojom::PrintSessionHost> receiver)
    : ArcCustomTabModalDialogHost(std::move(custom_tab), web_contents.get()),
      instance_(std::move(instance)),
      session_receiver_(this, std::move(receiver)),
      web_contents_(std::move(web_contents)) {
  session_receiver_.set_disconnect_handler(
      base::BindOnce(&PrintSessionImpl::Close, weak_ptr_factory_.GetWeakPtr()));
  web_contents_->SetUserData(UserDataKey(), base::WrapUnique(this));

  aura::Window* window = web_contents_->GetNativeView();
  custom_tab_->Attach(window);
  window->Show();

  // TODO(jschettler): Handle this correctly once crbug.com/636642 is
  // resolved. Until then, give the PDF plugin time to load.
  VLOG(1) << "Waiting for PDF plugin to load.";
  StartPrintAfterPluginIsLoaded();
}

PrintSessionImpl::~PrintSessionImpl() {
  // Delete the saved print document now that it's no longer needed.
  base::FilePath file_path;
  if (!net::FileURLToFilePath(web_contents_->GetVisibleURL(), &file_path)) {
    LOG(ERROR) << "Failed to obtain file path from URL.";
    return;
  }

  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindOnce(&DeletePrintDocument, file_path));
}

void PrintSessionImpl::CreatePreviewDocument(
    base::Value job_settings,
    CreatePreviewDocumentCallback callback) {
  mojom::PrintDocumentRequestPtr request =
      PrintDocumentRequestFromJobSettings(job_settings);
  if (!request || !request->attributes) {
    std::move(callback).Run(base::ReadOnlySharedMemoryRegion());
    return;
  }

  int request_id = job_settings.FindIntKey(printing::kPreviewRequestID).value();
  instance_->CreatePreviewDocument(
      std::move(request),
      base::BindOnce(&PrintSessionImpl::OnPreviewDocumentCreated,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(callback)));
}

void PrintSessionImpl::OnPreviewDocumentCreated(
    int request_id,
    CreatePreviewDocumentCallback callback,
    mojo::ScopedHandle preview_document,
    int64_t data_size) {
  if (data_size < kMinimumPdfSize ||
      !base::IsValueInRangeForNumericType<size_t>(data_size)) {
    std::move(callback).Run(base::ReadOnlySharedMemoryRegion());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadPreviewDocument, std::move(preview_document),
                     static_cast<size_t>(data_size)),
      base::BindOnce(&PrintSessionImpl::OnPreviewDocumentRead,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(callback)));
}

void PrintSessionImpl::OnPreviewDocumentRead(
    int request_id,
    CreatePreviewDocumentCallback callback,
    base::ReadOnlySharedMemoryRegion preview_document_region) {
  if (!preview_document_region.IsValid()) {
    std::move(callback).Run(std::move(preview_document_region));
    return;
  }

  if (!pdf_flattener_.is_bound()) {
    GetPrintingService()->BindPdfFlattener(
        pdf_flattener_.BindNewPipeAndPassReceiver());
    pdf_flattener_.set_disconnect_handler(
        base::BindOnce(&PrintSessionImpl::OnPdfFlattenerDisconnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  bool inserted = callbacks_.emplace(request_id, std::move(callback)).second;
  DCHECK(inserted);

  pdf_flattener_->FlattenPdf(
      std::move(preview_document_region),
      base::BindOnce(&PrintSessionImpl::OnPdfFlattened,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void PrintSessionImpl::OnPdfFlattened(
    int request_id,
    base::ReadOnlySharedMemoryRegion flattened_document_region) {
  auto it = callbacks_.find(request_id);
  std::move(it->second).Run(std::move(flattened_document_region));
  callbacks_.erase(it);
}

void PrintSessionImpl::OnPdfFlattenerDisconnected() {
  for (auto& it : callbacks_)
    std::move(it.second).Run(base::ReadOnlySharedMemoryRegion());
  callbacks_.clear();
}

void PrintSessionImpl::Close() {
  web_contents_->RemoveUserData(UserDataKey());
}

void PrintSessionImpl::OnPrintPreviewClosed() {
  instance_->OnPrintPreviewClosed();
}

void PrintSessionImpl::StartPrintAfterPluginIsLoaded() {
  // We know that we loaded a PDF into our WebContents.  It takes some time for
  // the PDF plugin to load and create its document structure.  If StartPrint()
  // is called too soon, it won't find this structure and will attach to the
  // top-level frame instead of the correct PDF element.  The PDF plugin doesn't
  // have a way to notify the browser when it's ready (crbug.com/636642), so we
  // need to poll for the PDF frame to "look ready" before we start printing.
  if (!IsPdfPluginLoaded(web_contents_.get())) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PrintSessionImpl::StartPrintAfterPluginIsLoaded,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(100));
    LOG(WARNING) << "PDF plugin not ready yet.  Can't start print preview.";
    return;
  }

  // The inner doc has been marked done, but the PDF plugin might not be quite
  // done updating the DOM yet.  We don't have a way to check that, so launch
  // printing after one final delay to give that time to finish.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrintSessionImpl::StartPrintNow,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(100));
}

void PrintSessionImpl::StartPrintNow() {
  printing::StartPrint(web_contents_.get(),
                       print_renderer_receiver_.BindNewEndpointAndPassRemote(),
                       false, false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintSessionImpl)

}  // namespace arc
