/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
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

#include <blpwtk2_dragdrop.h>

#include <base/strings/utf_string_conversions.h>
#include <content/common/drag_messages.h>
#include <content/public/common/drop_data.h>
#include <net/base/filename_util.h>
#include <third_party/blink/public/platform/web_input_event.h>
#include <ui/base/clipboard/clipboard.h>
#include <ui/base/clipboard/clipboard_format_type.h>
#include <ui/base/clipboard/clipboard_constants.h>
#include <ui/base/clipboard/custom_data_helper.h>
#include <ui/base/dragdrop/drag_drop_types.h>
#include <ui/base/dragdrop/drag_source_win.h>
#include <ui/base/dragdrop/os_exchange_data.h>
#include <ui/base/dragdrop/os_exchange_data_provider_factory.h>
#include <ui/base/dragdrop/os_exchange_data_provider_win.h>

namespace blpwtk2 {

namespace {

int MakeWebInputEventModifiers()
{
    return
        (!!(GetKeyState(VK_SHIFT)   & 0x8000)     ? blink::WebInputEvent::kShiftKey   : 0) |
        (!!(GetKeyState(VK_CONTROL) & 0x8000)     ? blink::WebInputEvent::kControlKey : 0) |
        (!!(GetKeyState(VK_MENU)    & 0x8000)     ? blink::WebInputEvent::kAltKey     : 0) |
        ((!!(GetKeyState(VK_LWIN)    & 0x8000) &&
          !!(GetKeyState(VK_RWIN)    & 0x8000))   ? blink::WebInputEvent::kMetaKey    : 0) |
        ((!!(GetKeyState(VK_MENU)    & 0x8000) &&
          !!(GetKeyState(VK_CONTROL) & 0x8000))   ? blink::WebInputEvent::kAltGrKey   : 0) |
        (!!(GetKeyState(VK_CAPITAL) & 0x0001)     ? blink::WebInputEvent::kCapsLockOn : 0) |
        (!!(GetKeyState(VK_NUMLOCK) & 0x0001)     ? blink::WebInputEvent::kNumLockOn  : 0) |
        (!!(GetKeyState(VK_LBUTTON) & 0x0080)     ? blink::WebInputEvent::kLeftButtonDown : 0) |
        (!!(GetKeyState(VK_MBUTTON) & 0x0080)     ? blink::WebInputEvent::kMiddleButtonDown : 0) |
        (!!(GetKeyState(VK_RBUTTON) & 0x0080)     ? blink::WebInputEvent::kRightButtonDown : 0);
}

DWORD MakeDROPEFFECT(blink::WebDragOperationsMask drag_operations)
{
    return ((!!(drag_operations & blink::kWebDragOperationCopy)? DROPEFFECT_COPY : DROPEFFECT_NONE) |
            (!!(drag_operations & blink::kWebDragOperationMove)? DROPEFFECT_MOVE : DROPEFFECT_NONE) |
            (!!(drag_operations & blink::kWebDragOperationLink)? DROPEFFECT_LINK : DROPEFFECT_NONE));
}

blink::WebDragOperationsMask MakeWebDragOperations(DWORD effect)
{
    return (blink::WebDragOperationsMask)
            ((!!(effect & DROPEFFECT_COPY)? blink::kWebDragOperationCopy : blink::kWebDragOperationNone) |
             (!!(effect & DROPEFFECT_MOVE)? blink::kWebDragOperationMove : blink::kWebDragOperationNone) |
             (!!(effect & DROPEFFECT_LINK)? blink::kWebDragOperationLink : blink::kWebDragOperationNone));
}

const ui::ClipboardFormatType& GetFileSystemFileFormatType()
{
    static base::NoDestructor<ui::ClipboardFormatType> format(
        ui::ClipboardFormatType::GetType("chromium/x-file-system-files"));

    return *format;
}

bool ReadFileSystemFilesFromPickle(
    const base::Pickle& pickle,
    std::vector<content::DropData::FileSystemFileInfo>* file_system_files) {
  base::PickleIterator iter(pickle);

  uint32_t num_files = 0;
  if (!iter.ReadUInt32(&num_files))
    return false;
  file_system_files->resize(num_files);

  for (uint32_t i = 0; i < num_files; ++i) {
    std::string url_string;
    int64_t size = 0;
    if (!iter.ReadString(&url_string) || !iter.ReadInt64(&size))
      return false;

    GURL url(url_string);
    if (!url.is_valid())
      return false;

    (*file_system_files)[i].url = url;
    (*file_system_files)[i].size = size;
  }
  return true;
}

void WriteFileSystemFilesToPickle(
    const std::vector<content::DropData::FileSystemFileInfo>& file_system_files,
    base::Pickle* pickle) {
  pickle->WriteUInt32(file_system_files.size());
  for (size_t i = 0; i < file_system_files.size(); ++i) {
    pickle->WriteString(file_system_files[i].url.spec());
    pickle->WriteInt64(file_system_files[i].size);
  }
}

std::unique_ptr<content::DropData> MakeDropData(const ui::OSExchangeData& data,
                                                const bool extractData) {

    auto drop_data = std::make_unique<content::DropData>();

    drop_data->did_originate_from_renderer = data.DidOriginateFromRenderer();

    base::string16 plain_text;
    data.GetString(&plain_text);
    if (!plain_text.empty())
        drop_data->text = base::NullableString16(plain_text, false);

    GURL url;
    base::string16 url_title;
    data.GetURLAndTitle(
        ui::OSExchangeData::DO_NOT_CONVERT_FILENAMES, &url, &url_title);
    if (url.is_valid()) {
        drop_data->url = url;
        drop_data->url_title = url_title;
    }

    base::string16 html;
    GURL html_base_url;
    data.GetHtml(&html, &html_base_url);
    if (!html.empty())
        drop_data->html = base::NullableString16(html, false);
    if (html_base_url.is_valid())
        drop_data->html_base_url = html_base_url;

    data.GetFilenames(&drop_data->filenames);

    base::Pickle pickle;
    std::vector<content::DropData::FileSystemFileInfo> file_system_files;
    if (data.GetPickledData(GetFileSystemFileFormatType(), &pickle) &&
        ReadFileSystemFilesFromPickle(pickle, &file_system_files))
    drop_data->file_system_files = file_system_files;

    if (data.GetPickledData(ui::ClipboardFormatType::GetWebCustomDataType(), &pickle))
        ui::ReadCustomDataIntoMap(
            pickle.data(), pickle.size(), &drop_data->custom_data);

    // Only if custom drag-and-drop topics is available in blpwtk2:
    std::vector<FORMATETC> custom_data_formats;
    data.provider().EnumerateCustomData(&custom_data_formats);
    for (const auto& format_etc : custom_data_formats) {
        std::wstring key = L"blp_" + std::to_wstring(format_etc.cfFormat);
        base::string16 value;
        if (extractData) {
            data.provider().GetCustomData(format_etc, &value);
        }
        drop_data->custom_data.insert(std::make_pair(key, value));
    }

    return drop_data;
}

std::vector<content::DropData::Metadata> MakeDropDataMetadata(const content::DropData& drop_data) {
    std::vector<content::DropData::Metadata> metadata;

    if (!drop_data.text.is_null()) {
        metadata.push_back(content::DropData::Metadata::CreateForMimeType(
            content::DropData::Kind::STRING,
            base::ASCIIToUTF16(ui::kMimeTypeText)));
    }

    if (drop_data.url.is_valid()) {
        metadata.push_back(content::DropData::Metadata::CreateForMimeType(
            content::DropData::Kind::STRING,
            base::ASCIIToUTF16(ui::kMimeTypeURIList)));
    }

    if (!drop_data.html.is_null()) {
        metadata.push_back(content::DropData::Metadata::CreateForMimeType(
            content::DropData::Kind::STRING,
            base::ASCIIToUTF16(ui::kMimeTypeHTML)));
    }

    // On Aura, filenames are available before drop.
    for (const auto& file_info : drop_data.filenames) {
        if (!file_info.path.empty()) {
            metadata.push_back(content::DropData::Metadata::CreateForFilePath(file_info.path));
        }
    }

    // On Android, only files' mime types are available before drop.
    for (const auto& mime_type : drop_data.file_mime_types) {
        if (!mime_type.empty()) {
            metadata.push_back(content::DropData::Metadata::CreateForMimeType(
                content::DropData::Kind::FILENAME, mime_type));
        }
    }

    for (const auto& file_system_file : drop_data.file_system_files) {
        if (!file_system_file.url.is_empty()) {
            metadata.push_back(
                content::DropData::Metadata::CreateForFileSystemUrl(file_system_file.url));
        }
    }

    for (const auto& custom_data_item : drop_data.custom_data) {
        metadata.push_back(content::DropData::Metadata::CreateForMimeType(
            content::DropData::Kind::STRING, custom_data_item.first));
    }

    return metadata;
}

void PrepareDragForFileContents(const content::DropData& drop_data,
                                ui::OSExchangeData::Provider* provider) {
    auto file_name = drop_data.GetSafeFilenameForImageFileContents();

    if (file_name) {
        provider->SetFileContents(*file_name, drop_data.file_contents);
    }
}

std::unique_ptr<ui::OSExchangeData> MakeOSExchangeData(const content::DropData& drop_data)
{
    std::unique_ptr<ui::OSExchangeData::Provider> provider =
        ui::OSExchangeDataProviderFactory::CreateProvider();

#if defined(OS_WIN)
    // Put download before file contents to prefer the download of a image over
    // its thumbnail link.
    if (!drop_data.download_metadata.empty()) {
    }
#endif
#if (!defined(OS_CHROMEOS) && defined(USE_X11)) || defined(OS_WIN)
    // We set the file contents before the URL because the URL also sets file
    // contents (to a .URL shortcut).  We want to prefer file content data over
    // a shortcut so we add it first.
    if (!drop_data.file_contents.empty())
        PrepareDragForFileContents(drop_data, provider.get());
#endif
    // Call SetString() before SetURL() when we actually have a custom string.
    // SetURL() will itself do SetString() when a string hasn't been set yet,
    // but we want to prefer drop_data.text.string() over the URL string if it
    // exists.
    if (!drop_data.text.string().empty())
        provider->SetString(drop_data.text.string());
    if (drop_data.url.is_valid())
        provider->SetURL(drop_data.url, drop_data.url_title);
    if (!drop_data.html.string().empty())
        provider->SetHtml(drop_data.html.string(), drop_data.html_base_url);
    if (!drop_data.filenames.empty())
        provider->SetFilenames(drop_data.filenames);
    if (!drop_data.file_system_files.empty()) {
        base::Pickle pickle;
        WriteFileSystemFilesToPickle(drop_data.file_system_files, &pickle);
        provider->SetPickledData(GetFileSystemFileFormatType(), pickle);
    }

    // Only if custom drag-and-drop topics is available in blpwtk2:
    if (!drop_data.custom_data.empty()) {
        std::unordered_map<base::string16, base::string16> custom_data;

        for (auto it = drop_data.custom_data.begin();
             it != drop_data.custom_data.end(); ++it) {
        // Look for a special format topic.  In addition to adding them as chromium
        // WebCustomDataFormat, also add these formats separately to clipboard.
        int format = 0;
        std::wstring sft;
        if (it->first.compare(0, 4, L"blp_") == 0) {
            sft = it->first.substr(4);
            format = std::stoi(sft);
        }

        if (format) {
            FORMATETC formatetc;
            formatetc.cfFormat = format;
            formatetc.ptd = NULL;
            formatetc.dwAspect = DVASPECT_CONTENT;
            formatetc.lindex = -1;
            formatetc.tymed = TYMED_HGLOBAL;

            provider->SetCustomData(formatetc, it->second);
            custom_data.insert(std::make_pair(sft, it->second));
        }
        else {
            custom_data.insert(std::make_pair(it->first, it->second));
        }
        }

        base::Pickle pickle;
        ui::WriteCustomDataToPickle(custom_data, &pickle);
        provider->SetPickledData(
            ui::ClipboardFormatType::GetWebCustomDataType(),
            pickle);
    }

    return std::make_unique<ui::OSExchangeData>(std::move(provider));
}

}

DragDropDelegate::~DragDropDelegate()
{
}

DragDrop::DragDrop(HWND hwnd, DragDropDelegate *delegate)
: ui::DropTargetWin()
, d_hwnd(hwnd)
, d_delegate(delegate)
{
    Init(hwnd);
    DCHECK(d_delegate);
}

DragDrop::~DragDrop()
{
}

void DragDrop::StartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask operations_allowed,
    const SkBitmap& bitmap,
    const gfx::Vector2d& bitmap_offset_in_dip,
    const content::DragEventSourceInfo& event_info)
{
    auto data = MakeOSExchangeData(drop_data);

    data->MarkOriginatedFromRenderer();

    data->provider().SetDragImage(
        gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 1.0f)),
        bitmap_offset_in_dip);

    auto source = ui::DragSourceWin::Create();
    source->set_data(data.get());
    ui::OSExchangeDataProviderWin::GetDataObjectImpl(*data)->set_in_drag_loop(true);

    DWORD effect;

    HRESULT result =
        DoDragDrop(
            ui::OSExchangeDataProviderWin::GetIDataObject(*data),
            source.Get(),
            MakeDROPEFFECT(operations_allowed),
            &effect);

    POINT screen_pt;
    GetCursorPos(&screen_pt);

    POINT client_pt = screen_pt;
    ScreenToClient(d_hwnd, &client_pt);

    d_delegate->DragSourceEnded(
        gfx::PointF(gfx::Point(client_pt)), gfx::PointF(gfx::Point(screen_pt)),
        (result == DRAGDROP_S_DROP     ?
         MakeWebDragOperations(effect) :
         blink::kWebDragOperationNone));

    d_delegate->DragSourceSystemEnded();
}

void DragDrop::UpdateDragCursor(blink::WebDragOperation drag_operation)
{
    d_current_drag_operation = drag_operation;
}

// ui::DropTargetWin overrides:
DWORD DragDrop::OnDragEnter(
    IDataObject* data_object,
    DWORD key_state,
    POINT screen_pt,
    DWORD effect)
{
    auto data_provider = std::make_unique<ui::OSExchangeDataProviderWin>(data_object);
    auto data = std::make_unique<ui::OSExchangeData>(std::move(data_provider));
    auto drag_data = MakeDropData(*data, false);
    auto drag_data_metadata = MakeDropDataMetadata(*drag_data);

    POINT client_pt = screen_pt;
    ScreenToClient(d_hwnd, &client_pt);

    d_delegate->DragTargetEnter(
        drag_data_metadata,
        gfx::PointF(gfx::Point(client_pt)), gfx::PointF(gfx::Point(screen_pt)),
        MakeWebDragOperations(effect),
        MakeWebInputEventModifiers());

    return MakeDROPEFFECT(blink::kWebDragOperationNone);
}

DWORD DragDrop::OnDragOver(
    IDataObject* data_object,
    DWORD key_state,
    POINT screen_pt,
    DWORD effect)
{
    POINT client_pt = screen_pt;
    ScreenToClient(d_hwnd, &client_pt);

    d_delegate->DragTargetOver(
        gfx::PointF(gfx::Point(client_pt)), gfx::PointF(gfx::Point(screen_pt)),
        MakeWebDragOperations(effect),
        MakeWebInputEventModifiers());

    return MakeDROPEFFECT(d_current_drag_operation);
}

void DragDrop::OnDragLeave(IDataObject* data_object)
{
    d_delegate->DragTargetLeave();
}

DWORD DragDrop::OnDrop(
    IDataObject* data_object,
    DWORD key_state,
    POINT screen_pt,
    DWORD effect)
{
    auto data_provider = std::make_unique<ui::OSExchangeDataProviderWin>(data_object);
    auto data = std::make_unique<ui::OSExchangeData>(std::move(data_provider));
    auto drag_data = MakeDropData(*data, true);

    POINT client_pt = screen_pt;
    ScreenToClient(d_hwnd, &client_pt);

    d_delegate->DragTargetDrop(
        *drag_data,
        gfx::PointF(gfx::Point(client_pt)), gfx::PointF(gfx::Point(screen_pt)),
        MakeWebInputEventModifiers());

    return MakeDROPEFFECT(d_current_drag_operation);
}

}  // close namespace blpwtk2
