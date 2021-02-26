// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/seat.h"

#include <memory>
#include "ui/gfx/geometry/point_f.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif  // defined(OS_CHROMEOS)

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/exo/data_source.h"
#include "components/exo/drag_drop_operation.h"
#include "components/exo/mime_utils.h"
#include "components/exo/seat_observer.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/xkb_tracker.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"

namespace exo {
namespace {

Surface* GetEffectiveFocus(aura::Window* window) {
  if (!window)
    return nullptr;
  Surface* const surface = Surface::AsSurface(window);
  if (surface)
    return surface;
  // Fallback to main surface.
  aura::Window* const top_level_window = window->GetToplevelWindow();
  if (!top_level_window)
    return nullptr;
  return GetShellMainSurface(top_level_window);
}

}  // namespace

Seat::Seat() : changing_clipboard_data_to_selection_source_(false) {
  WMHelper::GetInstance()->AddFocusObserver(this);
  // Prepend handler as it's critical that we see all events.
  WMHelper::GetInstance()->PrependPreTargetHandler(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  // TODO(reveman): Need to handle the mus case where PlatformEventSource is
  // null. https://crbug.com/856230
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
#if defined(OS_CHROMEOS)
  ui_lock_controller_ = std::make_unique<UILockController>(this);

  // Seat needs to be registered as observers before any Keyboard,
  // because Keyboard expects that the XkbTracker is up-to-date when its
  // observer method is called.
  xkb_tracker_ = std::make_unique<XkbTracker>();
  ash::ImeControllerImpl* ime_controller = ash::Shell::Get()->ime_controller();
  xkb_tracker_->UpdateKeyboardLayout(ime_controller->keyboard_layout_name());
  ime_controller->AddObserver(this);
#endif
}

Seat::~Seat() {
  Shutdown();
}

void Seat::Shutdown() {
  if (shutdown_)
    return;
  shutdown_ = true;
  DCHECK(!selection_source_) << "DataSource must be released before Seat";
#if defined(OS_CHROMEOS)
  ash::Shell::Get()->ime_controller()->RemoveObserver(this);
#endif
  WMHelper::GetInstance()->RemoveFocusObserver(this);
  WMHelper::GetInstance()->RemovePreTargetHandler(this);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void Seat::AddObserver(SeatObserver* observer) {
  observers_.AddObserver(observer);
}

void Seat::RemoveObserver(SeatObserver* observer) {
  observers_.RemoveObserver(observer);
}

Surface* Seat::GetFocusedSurface() {
  return GetEffectiveFocus(WMHelper::GetInstance()->GetFocusedWindow());
}

void Seat::StartDrag(FileHelper* file_helper,
                     DataSource* source,
                     Surface* origin,
                     Surface* icon,
                     ui::mojom::DragEventSource event_source) {
  // DragDropOperation manages its own lifetime.
  drag_drop_operation_ = DragDropOperation::Create(
      file_helper, source, origin, icon, last_pointer_location_, event_source);
}

void Seat::SetLastPointerLocation(const gfx::PointF& last_pointer_location) {
  last_pointer_location_ = last_pointer_location;
}

void Seat::AbortPendingDragOperation() {
  if (drag_drop_operation_)
    drag_drop_operation_->AbortIfPending();
}

void Seat::SetSelection(DataSource* source) {
  Surface* focused_surface = GetFocusedSurface();
  if (!source || !source->CanBeDataSourceForCopy(focused_surface))
    return;

  if (selection_source_) {
    if (selection_source_->get() == source)
      return;
    selection_source_->get()->Cancelled();
  }
  selection_source_ = std::make_unique<ScopedDataSource>(source, this);
  scoped_refptr<RefCountedScopedClipboardWriter> writer =
      base::MakeRefCounted<RefCountedScopedClipboardWriter>();

  base::RepeatingClosure data_read_callback = base::BarrierClosure(
      kMaxClipboardDataTypes,
      base::BindOnce(&Seat::OnAllReadsFinished, weak_ptr_factory_.GetWeakPtr(),
                     writer));

  source->GetDataForPreferredMimeTypes(
      base::BindOnce(&Seat::OnTextRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnRTFRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnHTMLRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnImageRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnFilenamesRead, weak_ptr_factory_.GetWeakPtr(),
                     writer, data_read_callback),
      data_read_callback);
}

class Seat::RefCountedScopedClipboardWriter
    : public ui::ScopedClipboardWriter,
      public base::RefCounted<RefCountedScopedClipboardWriter> {
 public:
  explicit RefCountedScopedClipboardWriter()
      : ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste,
                              std::make_unique<ui::DataTransferEndpoint>(
                                  ui::EndpointType::kGuestOs)) {}

 private:
  friend class base::RefCounted<RefCountedScopedClipboardWriter>;
  virtual ~RefCountedScopedClipboardWriter() = default;
};

void Seat::OnTextRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      base::OnceClosure callback,
                      const std::string& mime_type,
                      base::string16 data) {
  writer->WriteText(std::move(data));
  std::move(callback).Run();
}

void Seat::OnRTFRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                     base::OnceClosure callback,
                     const std::string& mime_type,
                     const std::vector<uint8_t>& data) {
  writer->WriteRTF(
      std::string(reinterpret_cast<const char*>(data.data()), data.size()));
  std::move(callback).Run();
}

void Seat::OnHTMLRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      base::OnceClosure callback,
                      const std::string& mime_type,
                      base::string16 data) {
  writer->WriteHTML(std::move(data), std::string());
  std::move(callback).Run();
}

void Seat::OnImageRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                       base::OnceClosure callback,
                       const std::string& mime_type,
                       const std::vector<uint8_t>& data) {
#if defined(OS_CHROMEOS)
  data_decoder::DecodeImageIsolated(
      data, data_decoder::mojom::ImageCodec::DEFAULT, false,
      std::numeric_limits<int64_t>::max(), gfx::Size(),
      base::BindOnce(&Seat::OnImageDecoded, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), writer));
#else
  std::move(callback).Run();
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_CHROMEOS)
void Seat::OnImageDecoded(base::OnceClosure callback,
                          scoped_refptr<RefCountedScopedClipboardWriter> writer,
                          const SkBitmap& bitmap) {
  if (!bitmap.isNull() && !bitmap.empty())
    writer->WriteImage(bitmap);
  std::move(callback).Run();
}
#endif  // defined(OS_CHROMEOS)

void Seat::OnFilenamesRead(
    scoped_refptr<RefCountedScopedClipboardWriter> writer,
    base::OnceClosure callback,
    const std::string& mime_type,
    const std::vector<uint8_t>& data) {
  std::move(callback).Run();
}

void Seat::OnAllReadsFinished(
    scoped_refptr<RefCountedScopedClipboardWriter> writer) {
  // We need to destroy the ScopedClipboardWriter in this call, before
  // |auto_reset| is destroyed, so if there are outstanding references that
  // would prevent that, reschedule this task.
  if (!writer->HasOneRef()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Seat::OnAllReadsFinished,
                       weak_ptr_factory_.GetWeakPtr(), std::move(writer)));
    return;
  }
  base::AutoReset<bool> auto_reset(
      &changing_clipboard_data_to_selection_source_, true);
  writer.reset();
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void Seat::OnWindowFocused(aura::Window* gained_focus,
                           aura::Window* lost_focus) {
  Surface* const surface = GetEffectiveFocus(gained_focus);
  for (auto& observer : observers_) {
    observer.OnSurfaceFocusing(surface);
  }
  for (auto& observer : observers_) {
    observer.OnSurfaceFocused(surface);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::PlatformEventObserver overrides:

void Seat::WillProcessEvent(const ui::PlatformEvent& event) {
  switch (ui::EventTypeFromNative(event)) {
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      physical_code_for_currently_processing_event_ = ui::CodeFromNative(event);
      break;
    default:
      break;
  }
}

void Seat::DidProcessEvent(const ui::PlatformEvent& event) {
  switch (ui::EventTypeFromNative(event)) {
    case ui::ET_KEY_PRESSED:
      physical_code_for_currently_processing_event_ = ui::DomCode::NONE;
      break;
    case ui::ET_KEY_RELEASED:
      // Remove this from the pressed key map because when IME is active we can
      // end up getting the DidProcessEvent call before we get the OnKeyEvent
      // callback and then the key will end up being stuck pressed.
      if (physical_code_for_currently_processing_event_ != ui::DomCode::NONE) {
        pressed_keys_.erase(physical_code_for_currently_processing_event_);
        physical_code_for_currently_processing_event_ = ui::DomCode::NONE;
      }
      break;
    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Seat::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore synthetic key repeat events.
  if (event->is_repeat())
    return;
  if (physical_code_for_currently_processing_event_ != ui::DomCode::NONE) {
    switch (event->type()) {
      case ui::ET_KEY_PRESSED:
        pressed_keys_.insert(
            {physical_code_for_currently_processing_event_, event->code()});
        break;
      case ui::ET_KEY_RELEASED:
        pressed_keys_.erase(physical_code_for_currently_processing_event_);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
#if defined(OS_CHROMEOS)
  xkb_tracker_->UpdateKeyboardModifiers(event->flags());
#endif
}

////////////////////////////////////////////////////////////////////////////////
// ui::ClipboardObserver overrides:

void Seat::OnClipboardDataChanged() {
  if (!selection_source_ || changing_clipboard_data_to_selection_source_)
    return;
  selection_source_->get()->Cancelled();
  selection_source_.reset();
}

#if defined(OS_CHROMEOS)
////////////////////////////////////////////////////////////////////////////////
// ash::ImeControllerImpl::Observer overrides:

void Seat::OnCapsLockChanged(bool enabled) {}

void Seat::OnKeyboardLayoutNameChanged(const std::string& layout_name) {
  xkb_tracker_->UpdateKeyboardLayout(layout_name);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// DataSourceObserver overrides:

void Seat::OnDataSourceDestroying(DataSource* source) {
  if (selection_source_ && selection_source_->get() == source)
    selection_source_.reset();
}

}  // namespace exo
