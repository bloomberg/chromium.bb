// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_WEB_PLUGIN_H_
#define PDF_PDF_VIEW_WEB_PLUGIN_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "cc/paint/paint_image.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_view_plugin_base.h"
#include "pdf/post_message_receiver.h"
#include "pdf/post_message_sender.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "v8/include/v8.h"

namespace blink {
class WebAssociatedURLLoader;
class WebElement;
class WebLocalFrame;
class WebLocalFrameClient;
class WebPluginContainer;
class WebURL;
class WebURLRequest;
struct WebAssociatedURLLoaderOptions;
}  // namespace blink

namespace gfx {
class Range;
}  // namespace gfx

namespace printing {
class MetafileSkia;
}  // namespace printing

namespace chrome_pdf {

// Skeleton for a `blink::WebPlugin` to replace `OutOfProcessInstance`.
class PdfViewWebPlugin final : public PdfViewPluginBase,
                               public blink::WebPlugin,
                               public BlinkUrlLoader::Client,
                               public PostMessageReceiver::Client,
                               public SkiaGraphics::Client {
 public:
  class ContainerWrapper {
   public:
    virtual ~ContainerWrapper() = default;

    // Invalidates the entire web plugin container and schedules a paint of the
    // page in it.
    virtual void Invalidate() = 0;

    // Notify the web plugin container about the total matches of a find
    // request.
    virtual void ReportFindInPageMatchCount(int identifier,
                                            int total,
                                            bool final_update) = 0;

    // Notify the web plugin container about the selected find result in plugin.
    virtual void ReportFindInPageSelection(int identifier, int index) = 0;

    // Returns the device scale factor.
    virtual float DeviceScaleFactor() const = 0;

    // Calls underlying WebLocalFrame::SetReferrerForRequest().
    virtual void SetReferrerForRequest(blink::WebURLRequest& request,
                                       const blink::WebURL& referrer_url) = 0;

    // Calls underlying WebLocalFrame::Alert().
    virtual void Alert(const blink::WebString& message) = 0;

    // Calls underlying WebLocalFrame::Confirm().
    virtual bool Confirm(const blink::WebString& message) = 0;

    // Calls underlying WebLocalFrame::Prompt().
    virtual blink::WebString Prompt(const blink::WebString& message,
                                    const blink::WebString& default_value) = 0;

    // Calls underlying WebLocalFrame::TextSelectionChanged().
    virtual void TextSelectionChanged(const blink::WebString& selection_text,
                                      uint32_t offset,
                                      const gfx::Range& range) = 0;

    // Calls underlying WebLocalFrame::CreateAssociatedURLLoader().
    virtual std::unique_ptr<blink::WebAssociatedURLLoader>
    CreateAssociatedURLLoader(
        const blink::WebAssociatedURLLoaderOptions& options) = 0;

    // Notifies the frame widget about the text input type change.
    virtual void UpdateTextInputState() = 0;

    // Returns the local frame to which the web plugin container belongs.
    virtual blink::WebLocalFrame* GetFrame() = 0;

    // Returns the local frame's client (render frame). May be null in unit
    // tests.
    virtual blink::WebLocalFrameClient* GetWebLocalFrameClient() = 0;

    // Returns the blink web plugin container pointer that's wrapped inside this
    // object. Returns nullptr if this object is for test only.
    virtual blink::WebPluginContainer* Container() = 0;
  };

  class PrintClient {
   public:
    virtual ~PrintClient() = default;

    virtual void Print(const blink::WebElement& element) = 0;
  };

  PdfViewWebPlugin(
      mojo::AssociatedRemote<pdf::mojom::PdfService> pdf_service_remote,
      std::unique_ptr<PrintClient> print_client,
      const blink::WebPluginParams& params);
  PdfViewWebPlugin(const PdfViewWebPlugin& other) = delete;
  PdfViewWebPlugin& operator=(const PdfViewWebPlugin& other) = delete;

  // blink::WebPlugin:
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool visibility) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, size_t data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;
  bool SupportsPaginatedPrint() override;
  bool GetPrintPresetOptionsFromDocument(
      blink::WebPrintPresetOptions* print_preset_options) override;
  int PrintBegin(const blink::WebPrintParams& print_params) override;
  void PrintPage(int page_number, cc::PaintCanvas* canvas) override;
  void PrintEnd() override;
  bool HasSelection() const override;
  blink::WebString SelectionAsText() const override;
  blink::WebString SelectionAsMarkup() const override;
  bool CanEditText() const override;
  bool HasEditableText() const override;
  bool CanUndo() const override;
  bool CanRedo() const override;
  bool ExecuteEditCommand(const blink::WebString& name,
                          const blink::WebString& value) override;
  blink::WebURL LinkAtPosition(const gfx::Point& /*position*/) const override;
  bool StartFind(const blink::WebString& search_text,
                 bool case_sensitive,
                 int identifier) override;
  void SelectFindResult(bool forward, int identifier) override;
  void StopFind() override;
  bool CanRotateView() override;
  void RotateView(blink::WebPlugin::RotationType type) override;
  blink::WebTextInputType GetPluginTextInputType() override;

  // PdfViewPluginBase:
  void UpdateCursor(ui::mojom::CursorType new_cursor_type) override;
  void UpdateTickMarks(const std::vector<gfx::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index) override;
  void Alert(const std::string& message) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  void SubmitForm(const std::string& url,
                  const void* data,
                  int length) override;
  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override;
  void SetSelectedText(const std::string& selected_text) override;
  bool IsValidLink(const std::string& url) override;
  std::unique_ptr<Graphics> CreatePaintGraphics(const gfx::Size& size) override;
  bool BindPaintGraphics(Graphics& graphics) override;
  void ScheduleTaskOnMainThread(const base::Location& from_here,
                                ResultCallback callback,
                                int32_t result,
                                base::TimeDelta delay) override;

  // BlinkUrlLoader::Client:
  bool IsValid() const override;
  blink::WebURL CompleteURL(const blink::WebString& partial_url) const override;
  net::SiteForCookies SiteForCookies() const override;
  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override;
  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override;

  // PostMessageReceiver::Client:
  void OnMessage(const base::Value& message) override;

  // SkiaGraphics::Client:
  void UpdateSnapshot(sk_sp<SkImage> snapshot) override;

  // Initializes the plugin using the `container_wrapper` provided by tests.
  bool InitializeForTesting(
      std::unique_ptr<ContainerWrapper> container_wrapper);

  const gfx::Rect& GetPluginRectForTesting() const { return plugin_rect(); }

 protected:
  // PdfViewPluginBase:
  base::WeakPtr<PdfViewPluginBase> GetWeakPtr() override;
  std::unique_ptr<UrlLoader> CreateUrlLoaderInternal() override;
  void DidOpen(std::unique_ptr<UrlLoader> loader, int32_t result) override;
  void SendMessage(base::Value message) override;
  void SaveAs() override;
  void InitImageData(const gfx::Size& size) override;
  void SetFormFieldInFocus(bool in_focus) override;
  void SetAccessibilityDocInfo(const AccessibilityDocInfo& doc_info) override;
  void SetAccessibilityPageInfo(AccessibilityPageInfo page_info,
                                std::vector<AccessibilityTextRunInfo> text_runs,
                                std::vector<AccessibilityCharInfo> chars,
                                AccessibilityPageObjects page_objects) override;
  void SetAccessibilityViewportInfo(
      const AccessibilityViewportInfo& viewport_info) override;
  void SetContentRestrictions(int content_restrictions) override;
  void SetPluginCanSave(bool can_save) override;
  void PluginDidStartLoading() override;
  void PluginDidStopLoading() override;
  void InvokePrintDialog() override;
  void NotifySelectionChanged(const gfx::PointF& left,
                              int left_height,
                              const gfx::PointF& right,
                              int right_height) override;
  void NotifyUnsupportedFeature() override;
  void UserMetricsRecordAction(const std::string& action) override;

 private:
  // Call `Destroy()` instead.
  ~PdfViewWebPlugin() override;

  bool InitializeCommon(std::unique_ptr<ContainerWrapper> container_wrapper);

  void OnViewportChanged(const gfx::Rect& view_rect, float new_device_scale);

  // Invalidates the entire web plugin container and schedules a paint of the
  // page in it.
  void InvalidatePluginContainer();

  // Text editing methods.
  bool SelectAll();
  bool Cut();
  bool Paste(const blink::WebString& value);
  bool Undo();
  bool Redo();

  // Callback to print without re-entrancy issues. The callback prevents the
  // invocation of printing in the middle of an event handler, which is risky;
  // see crbug.com/66334.
  // TODO(crbug.com/1217012): Re-evaluate the need for a callback when parts of
  // the plugin are moved off the main thread.
  void OnInvokePrintDialog(int32_t /*result*/);

  // May be null in unit tests.
  pdf::mojom::PdfService* GetPdfService();

  blink::WebString selected_text_;

  // The id of the current find operation, or -1 if no current operation is
  // present.
  int find_identifier_ = -1;

  blink::WebTextInputType text_input_type_ =
      blink::WebTextInputType::kWebTextInputTypeNone;

  // May be unbound in unit tests.
  mojo::AssociatedRemote<pdf::mojom::PdfService> const pdf_service_remote_;

  // May be null in unit tests, or if there is no printing support.
  std::unique_ptr<PrintClient> const print_client_;

  blink::WebPluginParams initial_params_;

  std::unique_ptr<ContainerWrapper> container_wrapper_;

  v8::Persistent<v8::Object> scriptable_receiver_;
  PostMessageSender post_message_sender_;

  cc::PaintImage snapshot_;

  // The metafile in which to save the printed output. Assigned a value only
  // between `PrintBegin()` and `PrintEnd()` calls.
  printing::MetafileSkia* printing_metafile_ = nullptr;

  // The indices of pages to print.
  std::vector<int> pages_to_print_;

  base::WeakPtrFactory<PdfViewWebPlugin> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_WEB_PLUGIN_H_
