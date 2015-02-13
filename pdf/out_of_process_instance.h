// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_OUT_OF_PROCESS_INSTANCE_H_
#define PDF_OUT_OF_PROCESS_INSTANCE_H_

#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "pdf/paint_manager.h"
#include "pdf/pdf_engine.h"
#include "pdf/preview_mode_client.h"

#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/dev/selection_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/find_private.h"
#include "ppapi/cpp/private/uma_private.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
class TextInput_Dev;
}

namespace chrome_pdf {

class OutOfProcessInstance : public pp::Instance,
                             public pp::Find_Private,
                             public pp::Printing_Dev,
                             public pp::Selection_Dev,
                             public PaintManager::Client,
                             public PDFEngine::Client,
                             public PreviewModeClient::Client {
 public:
  explicit OutOfProcessInstance(PP_Instance instance);
  virtual ~OutOfProcessInstance();

  // pp::Instance implementation.
  virtual bool Init(uint32_t argc,
                    const char* argn[],
                    const char* argv[]) override;
  virtual void HandleMessage(const pp::Var& message) override;
  virtual bool HandleInputEvent(const pp::InputEvent& event) override;
  virtual void DidChangeView(const pp::View& view) override;

  // pp::Find_Private implementation.
  virtual bool StartFind(const std::string& text, bool case_sensitive) override;
  virtual void SelectFindResult(bool forward) override;
  virtual void StopFind() override;

  // pp::PaintManager::Client implementation.
  virtual void OnPaint(const std::vector<pp::Rect>& paint_rects,
                       std::vector<PaintManager::ReadyRect>* ready,
                       std::vector<pp::Rect>* pending) override;

  // pp::Printing_Dev implementation.
  virtual uint32_t QuerySupportedPrintOutputFormats() override;
  virtual int32_t PrintBegin(
      const PP_PrintSettings_Dev& print_settings) override;
  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count) override;
  virtual void PrintEnd() override;
  virtual bool IsPrintScalingDisabled() override;

  // pp::Private implementation.
  virtual pp::Var GetLinkAtPosition(const pp::Point& point);
  virtual void GetPrintPresetOptionsFromDocument(
      PP_PdfPrintPresetOptions_Dev* options);

  // PPP_Selection_Dev implementation.
  virtual pp::Var GetSelectedText(bool html) override;

  void FlushCallback(int32_t result);
  void DidOpen(int32_t result);
  void DidOpenPreview(int32_t result);

  // Called when the timer is fired.
  void OnClientTimerFired(int32_t id);

  // Called to print without re-entrancy issues.
  void OnPrint(int32_t);

  // PDFEngine::Client implementation.
  void DocumentSizeUpdated(const pp::Size& size) override;
  void Invalidate(const pp::Rect& rect) override;
  void Scroll(const pp::Point& point) override;
  void ScrollToX(int position) override;
  void ScrollToY(int position) override;
  void ScrollToPage(int page) override;
  void NavigateTo(const std::string& url, bool open_in_new_tab) override;
  void UpdateCursor(PP_CursorType_Dev cursor) override;
  void UpdateTickMarks(const std::vector<pp::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index) override;
  void GetDocumentPassword(
      pp::CompletionCallbackWithOutput<pp::Var> callback) override;
  void Alert(const std::string& message) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  std::string GetURL() override;
  void Email(const std::string& to,
             const std::string& cc,
             const std::string& bcc,
             const std::string& subject,
             const std::string& body) override;
  void Print() override;
  void SubmitForm(const std::string& url,
                  const void* data,
                  int length) override;
  std::string ShowFileSelectionDialog() override;
  pp::URLLoader CreateURLLoader() override;
  void ScheduleCallback(int id, int delay_in_ms) override;
  void SearchString(const base::char16* string,
                    const base::char16* term,
                    bool case_sensitive,
                    std::vector<SearchStringResult>* results) override;
  void DocumentPaintOccurred() override;
  void DocumentLoadComplete(int page_count) override;
  void DocumentLoadFailed() override;
  pp::Instance* GetPluginInstance() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void DocumentLoadProgress(uint32 available, uint32 doc_size) override;
  void FormTextFieldFocusChange(bool in_focus) override;
  bool IsPrintPreview() override;
  uint32 GetBackgroundColor() override;
  void IsSelectingChanged(bool is_selecting) override;

  // PreviewModeClient::Client implementation.
  void PreviewDocumentLoadComplete() override;
  void PreviewDocumentLoadFailed() override;

  // Helper functions for implementing PPP_PDF.
  void RotateClockwise();
  void RotateCounterclockwise();

 private:
  void ResetRecentlySentFindUpdate(int32_t);

  // Called whenever the plugin geometry changes to update the location of the
  // background parts, and notifies the pdf engine.
  void OnGeometryChanged(double old_zoom, float old_device_scale);

  // Figures out the location of any background rectangles (i.e. those that
  // aren't painted by the PDF engine).
  void CalculateBackgroundParts();

  // Computes document width/height in device pixels, based on current zoom and
  // device scale
  int GetDocumentPixelWidth() const;
  int GetDocumentPixelHeight() const;

  // Draws a rectangle with the specified dimensions and color in our buffer.
  void FillRect(const pp::Rect& rect, uint32 color);

  void LoadUrl(const std::string& url);
  void LoadPreviewUrl(const std::string& url);
  void LoadUrlInternal(const std::string& url, pp::URLLoader* loader,
                       void (OutOfProcessInstance::* method)(int32_t));

  // Creates a URL loader and allows it to access all urls, i.e. not just the
  // frame's origin.
  pp::URLLoader CreateURLLoaderInternal();

  // Figure out the initial page to display based on #page=N and #nameddest=foo
  // in the |url_|.
  // Returns -1 if there is no valid fragment. The returned value is 0-based,
  // whereas page=N is 1-based.
  int GetInitialPage(const std::string& url);

  void FormDidOpen(int32_t result);

  std::string GetLocalizedString(PP_ResourceString id);

  void UserMetricsRecordAction(const std::string& action);

  enum DocumentLoadState {
    LOAD_STATE_LOADING,
    LOAD_STATE_COMPLETE,
    LOAD_STATE_FAILED,
  };

  // Set new zoom scale.
  void SetZoom(double scale);

  // Reduces the document to 1 page and appends |print_preview_page_count_|
  // blank pages to the document for print preview.
  void AppendBlankPrintPreviewPages();

  // Process the preview page data information. |src_url| specifies the preview
  // page data location. The |src_url| is in the format:
  // chrome://print/id/page_number/print.pdf
  // |dst_page_index| specifies the blank page index that needs to be replaced
  // with the new page data.
  void ProcessPreviewPageInfo(const std::string& src_url, int dst_page_index);
  // Load the next available preview page into the blank page.
  void LoadAvailablePreviewPage();

  // Bound the given scroll offset to the document.
  pp::FloatPoint BoundScrollOffsetToDocument(
      const pp::FloatPoint& scroll_offset);

  pp::ImageData image_data_;
  // Used when the plugin is embedded in a page and we have to create the loader
  // ourself.
  pp::CompletionCallbackFactory<OutOfProcessInstance> loader_factory_;
  pp::URLLoader embed_loader_;
  pp::URLLoader embed_preview_loader_;

  PP_CursorType_Dev cursor_;  // The current cursor.

  pp::CompletionCallbackFactory<OutOfProcessInstance> timer_factory_;

  // Size, in pixels, of plugin rectangle.
  pp::Size plugin_size_;
  // Size, in DIPs, of plugin rectangle.
  pp::Size plugin_dip_size_;
  // Remaining area, in pixels, to render the pdf in after accounting for
  // horizontal centering.
  pp::Rect available_area_;
  // Size of entire document in pixels (i.e. if each page is 800 pixels high and
  // there are 10 pages, the height will be 8000).
  pp::Size document_size_;

  double zoom_;  // Current zoom factor.

  float device_scale_;  // Current device scale factor.
  // True if the plugin is full-page.
  bool full_;

  PaintManager paint_manager_;

  struct BackgroundPart {
    pp::Rect location;
    uint32 color;
  };
  std::vector<BackgroundPart> background_parts_;

  struct PrintSettings {
    PrintSettings() {
      Clear();
    }
    void Clear() {
      is_printing = false;
      print_pages_called_ = false;
      memset(&pepper_print_settings, 0, sizeof(pepper_print_settings));
    }
    // This is set to true when PrintBegin is called and false when PrintEnd is
    // called.
    bool is_printing;
    // To know whether this was an actual print operation, so we don't double
    // count UMA logging.
    bool print_pages_called_;
    PP_PrintSettings_Dev pepper_print_settings;
  };

  PrintSettings print_settings_;

  scoped_ptr<PDFEngine> engine_;

  // This engine is used to render the individual preview page data. This is
  // used only in print preview mode. This will use |PreviewModeClient|
  // interface which has very limited access to the pp::Instance.
  scoped_ptr<PDFEngine> preview_engine_;

  std::string url_;

  // Used for submitting forms.
  pp::CompletionCallbackFactory<OutOfProcessInstance> form_factory_;
  pp::URLLoader form_loader_;

  // Used for printing without re-entrancy issues.
  pp::CompletionCallbackFactory<OutOfProcessInstance> print_callback_factory_;

  // The callback for receiving the password from the page.
  scoped_ptr<pp::CompletionCallbackWithOutput<pp::Var> > password_callback_;

  // True if we haven't painted the plugin viewport yet.
  bool first_paint_;

  DocumentLoadState document_load_state_;
  DocumentLoadState preview_document_load_state_;

  // A UMA resource for histogram reporting.
  pp::UMAPrivate uma_;

  // Used so that we only tell the browser once about an unsupported feature, to
  // avoid the infobar going up more than once.
  bool told_browser_about_unsupported_feature_;

  // Keeps track of which unsupported features we reported, so we avoid spamming
  // the stats if a feature shows up many times per document.
  std::set<std::string> unsupported_features_reported_;

  // Number of pages in print preview mode, 0 if not in print preview mode.
  int print_preview_page_count_;
  std::vector<int> print_preview_page_numbers_;

  // Used to manage loaded print preview page information. A |PreviewPageInfo|
  // consists of data source url string and the page index in the destination
  // document.
  typedef std::pair<std::string, int> PreviewPageInfo;
  std::queue<PreviewPageInfo> preview_pages_info_;

  // Used to signal the browser about focus changes to trigger the OSK.
  // TODO(abodenha@chromium.org) Implement full IME support in the plugin.
  // http://crbug.com/132565
  scoped_ptr<pp::TextInput_Dev> text_input_;

  // The last document load progress value sent to the web page.
  double last_progress_sent_;

  // Whether an update to the number of find results found was sent less than
  // |kFindResultCooldownMs| milliseconds ago.
  bool recently_sent_find_update_;

  // The tickmarks.
  std::vector<pp::Rect> tickmarks_;

  // Whether the plugin has received a viewport changed message. Nothing should
  // be painted until this is received.
  bool received_viewport_message_;

  // If true, this means we told the RenderView that we're starting a network
  // request so that it can start the throbber. We will tell it again once the
  // document finishes loading.
  bool did_call_start_loading_;

  // If this is true, then don't scroll the plugin in response to DidChangeView
  // messages. This will be true when the extension page is in the process of
  // zooming the plugin so that flickering doesn't occur while zooming.
  bool stop_scrolling_;

  // The background color of the PDF viewer.
  uint32 background_color_;
};

}  // namespace chrome_pdf

#endif  // PDF_OUT_OF_PROCESS_INSTANCE_H_
