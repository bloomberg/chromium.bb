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

#include <windows.h>  // NOLINT
#include <shellapi.h>
#include <commdlg.h>
#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <blpwtk2.h>

#include <third_party/blink/public/platform/web_security_origin.h>

#include <v8.h>

// assert.h must be included after all chromium related includes.
// We want to use the assert() from assert.h rather than the one
// defined in base/logging.h
#include <assert.h>

HINSTANCE g_instance = 0;
WNDPROC g_defaultEditWndProc = 0;
blpwtk2::Toolkit* g_toolkit = 0;
blpwtk2::Profile* g_profile = 0;
bool g_spellCheckEnabled;
std::set<std::string> g_languages;
std::vector<std::string> g_sideLoadedFonts;
std::string g_url;
std::string g_dictDir;
bool g_in_process_renderer = true;
bool g_custom_hit_test = false;
bool g_custom_tooltip = false;
bool g_renderer_ui = false;
HANDLE g_hJob;
MSG g_msg;
bool g_isInsideEventLoop;
bool g_webScriptContextAvailable = false;
std::string g_webScriptContextSecurityOrigin;

#define BUTTON_WIDTH 72
#define FIND_LABEL_WIDTH (BUTTON_WIDTH*3/4)
#define FIND_ENTRY_WIDTH (BUTTON_WIDTH*6/4)
#define FIND_BUTTON_WIDTH (BUTTON_WIDTH/4)
#define TIMEZONE_LABEL_WIDTH (BUTTON_WIDTH*5/4)
#define TIMEZONE_ENTRY_WIDTH (BUTTON_WIDTH*10/4)
#define TIMEZONE_BUTTON_WIDTH (BUTTON_WIDTH)
#define URLBAR_HEIGHT  24


const int BUF_SIZE = 8092;

// control ids
enum {
    IDC_BACK = 1001,
    IDC_FORWARD,
    IDC_RELOAD,
    IDC_STOP,
    IDC_FIND_ENTRY,
    IDC_TIMEZONE_ENTRY,
    IDC_FIND_PREV,
    IDC_FIND_NEXT,
    IDC_UPDATE_TZ,
};

// menu ids
enum {
    IDM_START_OF_MENU_ITEMS = 2000,
    IDM_FILE,
    IDM_NEW_WINDOW,
    IDM_CLOSE_WINDOW,
    IDM_CLEAR_WEB_CACHE,
    IDM_EXIT,
    IDM_TEST,
    IDM_TEST_V8_APPEND_ELEMENT,
    IDM_TEST_KEYBOARD_FOCUS,
    IDM_TEST_LOGICAL_FOCUS,
    IDM_TEST_LOGICAL_BLUR,
    IDM_TEST_PLAY_KEYBOARD_EVENTS,
    IDM_TEST_GET_PDF,
    IDM_TEST_GET_BITMAP,
    IDM_TEST_DUMP_LAYOUT_TREE,
    IDM_LANGUAGES,
    IDM_LANGUAGE_DE,
    IDM_LANGUAGE_EN_GB,
    IDM_LANGUAGE_EN_US,
    IDM_LANGUAGE_ES,
    IDM_LANGUAGE_FR,
    IDM_LANGUAGE_IT,
    IDM_LANGUAGE_PT_BR,
    IDM_LANGUAGE_PT_PT,
    IDM_LANGUAGE_RU,
    IDM_CUT,
    IDM_COPY,
    IDM_PASTE,
    IDM_DELETE,
    IDM_INSPECT,



    // patch section: spellcheck
    IDM_SPELLCHECK,
    IDM_SPELLCHECK_ENABLED,
    IDM_ADD_TO_DICTIONARY,


    // patch section: printing


    // patch section: diagnostics
    IDM_TEST_DUMP_GPU_INFO,


    // patch section: embedder ipc
    IDM_SEND_IPC_ASYNC,
    IDM_SEND_IPC_SYNC,


    // patch section: web script context
    IDM_TEST_ACCESS_DOM_FROM_WEB_SCRIPT_CONTEXT,



    IDM_CONTEXT_MENU_BASE_CUSTOM_TAG = 5000,
    IDM_CONTEXT_MENU_END_CUSTOM_TAG = 5999,
    IDM_CONTEXT_MENU_BASE_SPELL_TAG = 6000,
    IDM_CONTEXT_MENU_END_SPELL_TAG = 6999
};

static const char LANGUAGE_DE[] = "de-DE";
static const char LANGUAGE_EN_GB[] = "en-GB";
static const char LANGUAGE_EN_US[] = "en-US";
static const char LANGUAGE_ES[] = "es-ES";
static const char LANGUAGE_FR[] = "fr-FR";
static const char LANGUAGE_IT[] = "it-IT";
static const char LANGUAGE_PT_BR[] = "pt-BR";
static const char LANGUAGE_PT_PT[] = "pt-PT";
static const char LANGUAGE_RU[] = "ru-RU";

class Shell;
class ProcessClientDelegateImpl;
class ProcessHostDelegateImpl;
int registerShellWindowClass();
Shell* createShell(blpwtk2::Profile* profile, blpwtk2::WebView* webView = 0, bool forDevTools = false);
blpwtk2::ResourceLoader* createInProcessResourceLoader();
void populateSubmenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuItem& item);
void populateContextMenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuParams& params);
void updateSpellCheckConfig(blpwtk2::Profile* profile);
void toggleLanguage(blpwtk2::Profile* profile, const std::string& language);
const char* getHeaderFooterHTMLContent();

void testV8AppendElement(blpwtk2::WebView* webView)
{
    blpwtk2::WebFrame* mainFrame = webView->mainFrame();
    v8::Isolate* isolate = mainFrame->scriptIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> ctxt = mainFrame->mainWorldScriptContext();
    static const char SCRIPT[] =
        "var div = document.createElement('div');\n"
        "div.textContent = 'Hello From Shell Using V8!!!';\n"
        "document.body.appendChild(div);\n";

    v8::Context::Scope contextScope(ctxt);
    v8::ScriptCompiler::Source compilerSource(v8::String::NewFromUtf8(isolate, SCRIPT).ToLocalChecked());
    v8::Local<v8::Script> script = v8::ScriptCompiler::Compile(ctxt, &compilerSource).ToLocalChecked();
    assert(!script.IsEmpty());  // this should never fail to compile

    v8::TryCatch tryCatch(isolate);
    v8::MaybeLocal<v8::Value> result = script->Run(ctxt);
    if (result.IsEmpty()) {
        v8::String::Utf8Value msg(isolate, tryCatch.Exception());
        std::cout << "EXCEPTION: " << *msg << std::endl;
    }
}

void testAccessDOMFromWebScriptContext(const v8::Global<v8::Context>& webScriptContext, blpwtk2::WebView* webView)
{
    blpwtk2::WebFrame* mainFrame = webView->mainFrame();
    v8::Isolate* isolate = mainFrame->scriptIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Value> domWindow, domDocument;

    {
        v8::Local<v8::Context> ctxt = mainFrame->mainWorldScriptContext();
        v8::Context::Scope contextScope(ctxt);

        domWindow = ctxt->Global()->Get(
            v8::String::NewFromUtf8(
                isolate, "window").ToLocalChecked());
        domDocument = ctxt->Global()->Get(
            v8::String::NewFromUtf8(
                isolate, "document").ToLocalChecked());
    }

    {
        v8::Local<v8::Context> ctxt = webScriptContext.Get(isolate);
        v8::Context::Scope contextScope(ctxt);

        ctxt->Global()->Set(
            v8::String::NewFromUtf8(
                isolate, "domWindow").ToLocalChecked(),
                domWindow);
        ctxt->Global()->Set(
            v8::String::NewFromUtf8(
                isolate, "domDocument").ToLocalChecked(),
                domDocument);

        static const char SCRIPT[] =
            "domWindow.location + domDocument.body.innerHTML;\n";

        v8::ScriptCompiler::Source compilerSource(v8::String::NewFromUtf8(isolate, SCRIPT).ToLocalChecked());
        v8::Local<v8::Script> script = v8::ScriptCompiler::Compile(ctxt, &compilerSource).ToLocalChecked();
        assert(!script.IsEmpty());  // this should never fail to compile

        v8::TryCatch tryCatch(isolate);
        v8::MaybeLocal<v8::Value> result = script->Run(ctxt);
        if (result.IsEmpty()) {
            v8::String::Utf8Value msg(isolate, tryCatch.Exception());
            std::cout << "EXCEPTION: " << *msg << std::endl;
        }
        else {
            v8::Local<v8::Value> localResult = result.ToLocalChecked();
            if (localResult->IsString()) {
                v8::String::Utf8Value msg(isolate, localResult);
                std::cout << "RESULT: " << *msg << std::endl;
            }
        }
    }
}

void testPlayKeyboardEvents(HWND hwnd, blpwtk2::WebView* webView)
{
    blpwtk2::WebView::InputEvent ev = { 0 };
    ev.hwnd = hwnd;
    ev.message = WM_CHAR;
    ev.lparam = 0;
    ev.wparam = 'A';
    webView->handleInputEvents(&ev, 1);
    ev.wparam = 'B';
    webView->handleInputEvents(&ev, 1);
    ev.wparam = 'C';
    webView->handleInputEvents(&ev, 1);
}

void getWebViewPosition(HWND hwnd, int *left, int *top, int *width, int *height)
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    assert(0 == rect.left);
    assert(0 == rect.top);

    *left = 0;
    *top = URLBAR_HEIGHT;
    *width = rect.right;
    *height = rect.bottom - URLBAR_HEIGHT;
}

void testGetPicture(blpwtk2::NativeView hwnd,
                    blpwtk2::WebView* webView,
                    blpwtk2::WebView::DrawParams::RendererType rendererType,
                    int scaleX,
                    int scaleY)
{
    // NOTE: The PDF engine issues different commands based on the type of
    // device it is drawing to
    //
    //   Real printer:  Rasterizes the output into a bitmap and block
    //                  transfers it to the provided device context. This is
    //                  a temporary workaround in
    //                  PDFiumEngineExports::RenderPDFPageToDC
    //
    //   non-EMF files: Uses the CGdiDisplayDriver driver to issue the draw
    //                  commands on the provided device context.
    //
    //   EMF files:     Uses the CGdiPrinterDriver driver to issue the draw
    //                  commands on the provided device context.
    //
    // Using EMF files produces a very low-quality output. This is probably due
    // to an incorrect assumption by CFX_WindowsDevice::CreateDriver to assume
    // all EMF files to be compatible with a printer device rather than being
    // truely device independent.
    //
    // To ensure the test driver generates an output close to the output that
    // will be sent to the printer, we will use a non-EMF files (ie. Bitmap)
    // to test the drawContent method. Once the workaround in RenderPDFPageToDC
    // is removed and CFX_WindowsDevice::CreateDriver creates a driver that
    // issues device-independent commands, we can switch the test driver to use
    // EMF files instead.

    int left = 0, top = 0, width = 0, height = 0;

    assert(webView);
    getWebViewPosition(hwnd, &left, &top, &width, &height);

    if (!width || !height) {
        std::cout << "Unable to get bitmap of canvas. Canvas area is zero" << std::endl;
        return;
    }

    blpwtk2::NativeDeviceContext refDeviceContext = GetDC(hwnd);

#ifdef USE_EMF
    RECT rect = { 0 };
    int iWidthMM = GetDeviceCaps(refDeviceContext, HORZSIZE);
    int iHeightMM = GetDeviceCaps(refDeviceContext, VERTSIZE);
    int iWidthPels = GetDeviceCaps(refDeviceContext, HORZRES);
    int iHeightPels = GetDeviceCaps(refDeviceContext, VERTRES);

    rect.right = (scaleX * width * iWidthMM * 100) / iWidthPels;
    rect.bottom = (scaleY * height * iHeightMM * 100) / iHeightPels;

    blpwtk2::NativeDeviceContext deviceContext = CreateEnhMetaFileW(refDeviceContext, L"outputPicture.emf", &rect, L"blpwtk2_shell");
#else
    const int bytesPerRow = width * scaleX * 4;
    const long imageDataSize = bytesPerRow * height * scaleY;

    BITMAPFILEHEADER fileHeader = {
        0x4d42,
        sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageDataSize,
        0,
        0,
        sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER),
    };

    BITMAPINFO bmi = { {
            sizeof(BITMAPINFOHEADER),
            width * scaleX,
            height * scaleY,
            1,
            32,
            BI_RGB
        } };

    // Use the device context of the parent HWND as a template to create a new
    // device context.
    blpwtk2::NativeDeviceContext deviceContext = CreateCompatibleDC(refDeviceContext);
#endif

    ReleaseDC(hwnd, refDeviceContext);

    // This is the best stretch mode
    SetStretchBltMode(deviceContext, HALFTONE);

#ifndef USE_EMF
    // Create a new bitmap object
    void *buffer = (void*) NULL;
    HBITMAP bitmap = CreateDIBSection(deviceContext, &bmi, DIB_RGB_COLORS, &buffer, 0, 0);

    // Set the new bitmap object as a backing surface for the device context.
    // The original backing surface is saved in originalSurface
    HGDIOBJ originalSurface = SelectObject(deviceContext, bitmap);
#endif

    // Draw the contents of the webview onto the device context
    blpwtk2::WebView::DrawParams drawParams;
    drawParams.srcRegion = { 0, 0, width, height };
    drawParams.destWidth = width * scaleX;
    drawParams.destHeight = height * scaleY;
    drawParams.styleClass = "screen-grab";
    drawParams.rendererType = rendererType;
    drawParams.dpi = 72;

    if (rendererType == blpwtk2::WebView::DrawParams::RendererType::PDF) {
        std::vector<char> pdf_data;
        {
            blpwtk2::Blob blob;
            webView->drawContentsToBlob(&blob, drawParams);

            pdf_data.resize(blob.size());
            blob.copyTo(&pdf_data[0]);
        }

        blpwtk2::PdfUtil::RenderPDFPageToDC(pdf_data.data(),
                                            pdf_data.size(),
                                            0,
                                            deviceContext,
                                            drawParams.dpi,
                                            0,
                                            0,
                                            drawParams.destWidth,
                                            drawParams.destHeight,
                                            false,
                                            false,
                                            false,
                                            false,
                                            false,
                                            true);
    }
    else if (rendererType == blpwtk2::WebView::DrawParams::RendererType::Bitmap) {
        std::vector<char> bmp_data;
        {
            blpwtk2::Blob blob;
            webView->drawContentsToBlob(&blob, drawParams);

            bmp_data.resize(blob.size());
            blob.copyTo(&bmp_data[0]);
        }

        // Create a device context to associate with the bitmap object
        blpwtk2::NativeDeviceContext bitmapDeviceContext = CreateCompatibleDC(deviceContext);

        // Create a new bitmap object
        void *buffer = (void*)NULL;
        BITMAPINFO bmi = { *reinterpret_cast<BITMAPINFOHEADER*>(bmp_data.data() + sizeof(BITMAPFILEHEADER)) };
        HBITMAP inputBitmap = CreateDIBSection(bitmapDeviceContext, &bmi, DIB_RGB_COLORS, &buffer, 0, 0);

        // Copy bitmap data from blob into bitmap object
        memcpy(buffer,
               bmp_data.data() + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER),
               bmp_data.size() - sizeof(BITMAPFILEHEADER) - sizeof(BITMAPINFOHEADER));

        // Block transfer image from bitmap object to 'deviceContext'
        HGDIOBJ originalSurface = SelectObject(bitmapDeviceContext, inputBitmap);
        BitBlt(deviceContext,
               0,
               0,
               drawParams.destWidth,
               drawParams.destHeight,
               bitmapDeviceContext,
               0,
               0,
               SRCCOPY);

        SelectObject(bitmapDeviceContext, originalSurface);
        DeleteObject(inputBitmap);
        DeleteDC(bitmapDeviceContext);
    }

#ifdef USE_EMF
    HENHMETAFILE emf = CloseEnhMetaFile(deviceContext);
    DeleteEnhMetaFile(emf);
#else
    // The device context is switched back to use the original backing surface.
    SelectObject(deviceContext, originalSurface);

    std::ofstream file("outputBitmap.bmp", std::ios::binary);
    file.write(reinterpret_cast<char *>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<char *>(&bmi.bmiHeader), sizeof(bmi.bmiHeader));
    file.write(reinterpret_cast<char *>(buffer), imageDataSize);
    file.close();

    // Delete the bitmap object and its associated memory
    DeleteObject(bitmap);

    // Delete the device context
    DeleteDC(deviceContext);
#endif
}

class Shell : public blpwtk2::WebViewDelegate {
public:
    static std::set<Shell*> s_shells;

    HWND d_mainWnd;
    HWND d_urlEntryWnd;
    HWND d_findEntryHwnd;
    HMENU d_spellCheckMenu;
    HWND d_timezoneEntryHwnd;
    blpwtk2::WebView* d_webView;
    v8::Global<v8::Value> d_securityToken;
    v8::Global<v8::Context> d_webScriptContext;
    blpwtk2::Profile* d_profile;
    Shell* d_inspectorShell;
    Shell* d_inspectorFor;
    POINT d_contextMenuPoint;
    std::string d_findText;
    std::vector<std::string> d_contextMenuSpellReplacements;
    std::string d_misspelledWord;

    Shell(HWND mainWnd,
          HWND urlEntryWnd,
          HWND findEntryHwnd,
          HMENU spellCheckMenu,
          HWND timezoneEntryHwnd,
          blpwtk2::Profile* profile,
          blpwtk2::WebView* webView = 0,
          bool useExternalRenderer = false)
        : d_mainWnd(mainWnd)
        , d_urlEntryWnd(urlEntryWnd)
        , d_findEntryHwnd(findEntryHwnd)
        , d_spellCheckMenu(spellCheckMenu)
        , d_timezoneEntryHwnd(timezoneEntryHwnd)
        , d_webView(webView)
        , d_profile(profile)
        , d_inspectorShell(0)
        , d_inspectorFor(0)
    {
        s_shells.insert(this);

        if (!d_webView) {
            blpwtk2::WebViewCreateParams params;
            params.setJavascriptCanAccessClipboard(true);
            params.setDOMPasteEnabled(true);
            if (g_in_process_renderer && d_profile == g_profile && !useExternalRenderer) {
                params.setRendererAffinity(::GetCurrentProcessId());
            }
            d_profile->createWebView(this, params);
        }
        else {
            d_webView->setParent(d_mainWnd);

            d_webView->show();
            d_webView->enableAltDragRubberbanding(true);
            d_webView->enableNCHitTest(g_custom_hit_test);

            SetWindowLongPtr(d_mainWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            SetWindowLongPtr(d_urlEntryWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }

        // WebView not yet available. Let's run a modal loop here
        // until it becomes available

        if (g_isInsideEventLoop) {
            g_toolkit->postHandleMessage(&g_msg);

            while (GetMessage(&g_msg, NULL, 0, 0) > 0) {
                if (!g_toolkit->preHandleMessage(&g_msg)) {
                    TranslateMessage(&g_msg);
                    DispatchMessage(&g_msg);
                }
                if (d_webView) {
                    break;
                }
                g_toolkit->postHandleMessage(&g_msg);
            }
        }
        else {
            while (GetMessage(&g_msg, NULL, 0, 0) > 0) {
                if (!g_toolkit->preHandleMessage(&g_msg)) {
                    TranslateMessage(&g_msg);
                    DispatchMessage(&g_msg);
                }
                g_toolkit->postHandleMessage(&g_msg);

                if (d_webView) {
                    break;
                }
            }
        }
    }

    ~Shell() final
    {
        SetWindowLongPtr(d_mainWnd, GWLP_USERDATA, NULL);
        SetWindowLongPtr(d_urlEntryWnd, GWLP_USERDATA, NULL);

        if (d_inspectorFor) {
            d_inspectorFor->d_inspectorShell = 0;
            d_inspectorFor = 0;
        }

        if (d_inspectorShell) {
            DestroyWindow(d_inspectorShell->d_mainWnd);
            d_inspectorShell = 0;
        }

        webView()->destroy();

        if (d_profile != g_profile) {
            // If the shell has its own profile, then the profile needs to be
            // destroyed.  g_profile gets destroyed before main() exits.
            d_profile->destroy();
        }

        s_shells.erase(this);
        if (0 == s_shells.size()) {
            PostQuitMessage(0);
        }
    }

    void resizeSubViews()
    {
        int left, top, width, height;

        if (!webView()) return;

        left = 0;
        top = 0;
        width = 0;
        height = 0;
        getWebViewPosition(d_mainWnd, &left, &top, &width, &height);

        webView()->move(left, top, width, height);

        int x = (4 * BUTTON_WIDTH) +
            FIND_LABEL_WIDTH +
            FIND_ENTRY_WIDTH +
            (2 * FIND_BUTTON_WIDTH) +
            TIMEZONE_LABEL_WIDTH +
            TIMEZONE_ENTRY_WIDTH +
            TIMEZONE_BUTTON_WIDTH;
        MoveWindow(d_urlEntryWnd, x, 0, width - x, URLBAR_HEIGHT, TRUE);
    }

    blpwtk2::WebView *webView()
    {
        assert(d_webView);
        return d_webView;
    }

    v8::Global<v8::Context>& webScriptContext() {
        return d_webScriptContext;
    }

    ///////// WebViewDelegate overrides

    void created(blpwtk2::WebView* source) override
    {
        d_webView = source;
        d_webView->setParent(d_mainWnd);

        d_webView->show();
        d_webView->enableNCHitTest(g_custom_hit_test);

        SetWindowLongPtr(d_mainWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        SetWindowLongPtr(d_urlEntryWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    // Invoked when the main frame finished loading the specified 'url'.  This
    // is the notification that guarantees that the 'mainFrame()' method on the
    // WebView can be used (for in-process WebViews, and in the renderer
    // thread).
    void didFinishLoad(blpwtk2::WebView* source, const blpwtk2::StringRef& url) override
    {
        assert(source == d_webView);
        std::string str(url.data(), url.length());
        std::cout << "DELEGATE: didFinishLoad('" << str << "')" << std::endl;

        EnableWindow(GetDlgItem(d_mainWnd, IDC_BACK), TRUE);
        EnableWindow(GetDlgItem(d_mainWnd, IDC_FORWARD), TRUE);
        EnableWindow(GetDlgItem(d_mainWnd, IDC_RELOAD), TRUE);

        if (g_webScriptContextAvailable) {
            blpwtk2::WebFrame* mainFrame = d_webView->mainFrame();
            v8::Isolate* isolate = mainFrame->scriptIsolate();
            v8::HandleScope handleScope(isolate);

            d_securityToken.Reset(isolate, v8::Symbol::New(isolate));

            v8::Local<v8::Context> webScriptContext =
                g_toolkit->createWebScriptContext(
                    blpwtk2::StringRef(g_webScriptContextSecurityOrigin));

            webScriptContext->SetSecurityToken(d_securityToken.Get(isolate));
            d_webScriptContext = v8::Global<v8::Context>(isolate, webScriptContext);

            mainFrame->mainWorldScriptContext()->SetSecurityToken(d_securityToken.Get(isolate));
        }
    }

    // Invoked when the main frame failed loading the specified 'url', or was
    // cancelled (e.g. window.stop() was called).
    void didFailLoad(blpwtk2::WebView* source,
                     const blpwtk2::StringRef& url) override
    {
        assert(source == d_webView);
        std::string str(url.data(), url.length());
        std::cout << "DELEGATE: didFailLoad('" << str << "')" << std::endl;

        EnableWindow(GetDlgItem(d_mainWnd, IDC_BACK), TRUE);
        EnableWindow(GetDlgItem(d_mainWnd, IDC_FORWARD), TRUE);
        EnableWindow(GetDlgItem(d_mainWnd, IDC_RELOAD), TRUE);
    }

    // Notification that |source| has gained focus.
    void focused(blpwtk2::WebView* source) override
    {
        assert(source == d_webView);
        std::cout << "DELEGATE: focused" << std::endl;
    }

    // Notification that |source| has lost focus.
    void blurred(blpwtk2::WebView* source) override
    {
        assert(source == d_webView);
        std::cout << "DELEGATE: blurred" << std::endl;
    }

    std::string extentionForMimeType(const blpwtk2::StringRef& mimeType)
    {
        const char* end = mimeType.data() + mimeType.length();
        const char* p = end - 1;
        while (p > mimeType.data()) {
            if ('/' == *p) {
                return std::string(p + 1, end);
            }
            --p;
        }
        return "*";
    }

    void appendStringToVector(std::vector<char> *result, const blpwtk2::StringRef& str)
    {
        result->reserve(result->size() + str.length());
        const char* p = str.data();
        const char* end = p + str.length();
        while (p < end) {
            result->push_back(*p);
            ++p;
        }
    }

    void showContextMenu(blpwtk2::WebView* source, const blpwtk2::ContextMenuParams& params) override
    {
        assert(source == d_webView);
        std::cout << "DELEGATE: showContextMenu" << std::endl;

        d_contextMenuPoint = params.pointOnScreen();
        ::ScreenToClient(d_mainWnd, &d_contextMenuPoint);
        d_contextMenuPoint.y -= URLBAR_HEIGHT;

        HMENU menu = createContextMenu(params);
        TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                       params.pointOnScreen().x, params.pointOnScreen().y,
                       0, d_mainWnd, NULL);
        DestroyMenu(menu);
    }

    void requestNCHitTest(blpwtk2::WebView* source) override
    {
        assert(source == d_webView);
        POINT pt;
        ::GetCursorPos(&pt);
        POINT ptClient = pt;
        ::ScreenToClient(d_mainWnd, &ptClient);
        RECT rcClient;
        ::GetClientRect(d_mainWnd, &rcClient);

        bool nearLeftBorder = false, nearTopBorder = false, nearRightBorder = false, nearBottomBorder = false;
        if (ptClient.x >= 0 && ptClient.x <= 50)
            nearLeftBorder = true;
        else if (ptClient.x >= rcClient.right - 50 && ptClient.x <= rcClient.right)
            nearRightBorder = true;
        if (ptClient.y >= URLBAR_HEIGHT && ptClient.y <= URLBAR_HEIGHT + 50)
            nearTopBorder = true;
        else if (ptClient.y >= rcClient.bottom - 50 && ptClient.y <= rcClient.bottom)
            nearBottomBorder = true;

        int result = HTCLIENT;
        if (nearLeftBorder) {
            if (nearTopBorder)
                result = HTTOPLEFT;
            else if (nearBottomBorder)
                result = HTBOTTOMLEFT;
            else
                result = HTLEFT;
        }
        else if (nearRightBorder) {
            if (nearTopBorder)
                result = HTTOPRIGHT;
            else if (nearBottomBorder)
                result = HTBOTTOMRIGHT;
            else
                result = HTRIGHT;
        }
        else if (nearTopBorder)
            result = HTTOP;
        else if (nearBottomBorder)
            result = HTBOTTOM;

        std::cout << "DELEGATE: requestNCHitTest(x=" << ptClient.x << ", y="
                  << ptClient.y << ", result=" << result << ")" << std::endl;
        source->onNCHitTestResult(pt.x, pt.y, result);
    }

    void ncDragBegin(blpwtk2::WebView* source,
                     int hitTestCode,
                     const POINT& startPoint) override
    {
        assert(source == d_webView);
        std::cout << "DELEGATE: ncDragBegin(" << hitTestCode << ", x="
                  << startPoint.x << ", y=" << startPoint.y << ")"
                  << std::endl;
    }

    void ncDragMove(blpwtk2::WebView* source, const POINT& movePoint) override
    {
        assert(source == d_webView);
        std::cout << "DELEGATE: ncDragMove(x=" << movePoint.x << ", y="
                  << movePoint.y << ")" << std::endl;
    }

    void ncDragEnd(blpwtk2::WebView* source, const POINT& endPoint) override
    {
        assert(source == d_webView);
        std::cout << "DELEGATE: ncDragEnd(x=" << endPoint.x << ", y="
                  << endPoint.y << ")" << std::endl;
    }

    void find()
    {
        char buf[200];
        int len = ::GetWindowTextA(d_findEntryHwnd, buf, sizeof(buf));

        d_findText.assign(buf, len);
        if (0 == len) {
            webView()->stopFind(false);
        }
        else {
            webView()->find(blpwtk2::StringRef(d_findText), false, true);
        }
    }

    void findNext(bool forward)
    {
        if (!d_findText.empty()) {
            webView()->find(blpwtk2::StringRef(d_findText), false, forward);
        }
    }

     void updateTz()
    {
        char buf[200];
        int len = ::GetWindowTextA(d_timezoneEntryHwnd, buf, sizeof(buf));
        g_toolkit->setTimeZone(std::string(buf, len));
    }

    void findState(blpwtk2::WebView* source, int numberOfMatches, int activeMatchOrdinal, bool finalUpdate) override
    {
        std::cout << "FIND: count:" << numberOfMatches << ", current:"
                  << activeMatchOrdinal << ", final:"
                  << (finalUpdate ? "yes" : "no") << std::endl;
    }

    HMENU createContextMenu(const blpwtk2::ContextMenuParams& params)
    {
        bool addSeparator = false;
        if (params.canCut() || params.canCopy() || params.canPaste() || params.canDelete())
            addSeparator = true;

        HMENU menu = CreatePopupMenu();

        if (params.numCustomItems() > 0) {
            populateContextMenu(menu, IDM_CONTEXT_MENU_BASE_CUSTOM_TAG, params);
            AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
        }

        if (params.canCut())
            AppendMenu(menu, MF_STRING, IDM_CUT, L"C&ut");
        if (params.canCopy())
            AppendMenu(menu, MF_STRING, IDM_COPY, L"&Copy");
        if (params.canPaste())
            AppendMenu(menu, MF_STRING, IDM_PASTE, L"&Paste");
        if (params.canDelete())
            AppendMenu(menu, MF_STRING, IDM_DELETE, L"&Delete");

        if (addSeparator)
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);

        AppendMenu(menu, MF_STRING, IDM_INSPECT, L"I&nspect Element");

        if (!params.misspelledWord().isEmpty()) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);

            d_misspelledWord.assign(params.misspelledWord().data(),
                                    params.misspelledWord().length());

            std::string menuText = "Add to dictionary: ";
            menuText.append(d_misspelledWord.c_str());
            AppendMenuA(menu, MF_STRING, IDM_ADD_TO_DICTIONARY, menuText.c_str());
        }

        d_contextMenuSpellReplacements.clear();

        if (params.numSpellSuggestions() > 0) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);

            for (int i = 0; i < params.numSpellSuggestions(); ++i) {
                int index = d_contextMenuSpellReplacements.size();
                std::string text(params.spellSuggestion(i).data(),
                                 params.spellSuggestion(i).length());
                d_contextMenuSpellReplacements.push_back(text);
                AppendMenuA(menu, MF_STRING, IDM_CONTEXT_MENU_BASE_SPELL_TAG + index, text.c_str());
            }
        }

        return menu;
    }
};
std::set<Shell*> Shell::s_shells;

class ProcessClientDelegateImpl: public blpwtk2::ProcessClientDelegate {
public:
    void onRendererReceivedAsync(const blpwtk2::StringRef& message) override
    {
        std::cout << "Renderer received Async message: "
                  << std::string(message.data(), message.size())
                  << std::endl;
    }
};

class ProcessHostDelegateImpl: public blpwtk2::ProcessHostDelegate {
public:
    void onBrowserReceivedAsync(int pid, const blpwtk2::StringRef& message) override
    {
        std::cout << "Browser(pid: " << GetCurrentProcessId() << ")" << " received Async message from pid " << pid << ": "
                  << std::string(message.data(), message.size())
                  << std::endl;

        std::cout << "Browser async responses with ACK to pid " << pid << std::endl;
        g_toolkit->opaqueMessageToRendererAsync(pid, "ACK");
    }

    blpwtk2::String onBrowserReceivedSync(int pid, const blpwtk2::StringRef& message) override
    {
        std::cout << "Browser(pid: " << GetCurrentProcessId() << ")" << " received Sync message from pid " << pid << ": "
                  << std::string(message.data(), message.size())
                  << std::endl;

        std::cout << "Browser responses with ACK" << std::endl;

        return blpwtk2::String("ACK");
    }
};

void runMessageLoop()
{
    while (GetMessage(&g_msg, NULL, 0, 0) > 0) {
        g_isInsideEventLoop = true;

        if (!g_toolkit->preHandleMessage(&g_msg)) {
            TranslateMessage(&g_msg);
            DispatchMessage(&g_msg);
        }
        g_toolkit->postHandleMessage(&g_msg);
    }
}

struct HostWatcherThreadData {
    std::vector<HANDLE> d_processHandles;
    int d_mainThreadId;
};

DWORD __stdcall hostWatcherThreadFunc(LPVOID lParam)
{
    HostWatcherThreadData* data = (HostWatcherThreadData*)lParam;
    ::WaitForMultipleObjects(data->d_processHandles.size(),
                             data->d_processHandles.data(),
                             TRUE,
                             INFINITE);
    ::PostThreadMessage(data->d_mainThreadId, WM_QUIT, 0, 0);
    return 0;
}

HANDLE spawnProcess()
{
    // Generate random numbers
    int randNum1 = rand();

    char fileMappingName[64];

    snprintf(fileMappingName,
             sizeof(fileMappingName),
             "Blpwtk2ShellInit%d",
             randNum1);

    // Create a shared memory object
    void *buffer;
    {
        HANDLE handle = ::CreateFileMappingA(
                INVALID_HANDLE_VALUE,    // use paging file
                NULL,                    // default security
                PAGE_READWRITE,          // read/write access
                0,                       // maximum object size (high-order DWORD)
                BUF_SIZE,                // maximum object size (low-order DWORD)
                fileMappingName);        // name of mapping object

        assert(handle);

        buffer = ::MapViewOfFile(
                handle,              // handle to map object
                FILE_MAP_ALL_ACCESS, // read/write permission
                0,
                0,
                BUF_SIZE);

        assert(buffer);
    }

    char fileName[MAX_PATH + 1];
    ::GetModuleFileNameA(NULL, fileName, MAX_PATH);

    std::string cmdline;
    cmdline.append("\"");
    cmdline.append(fileName);
    cmdline.append("\" " + g_url);
    cmdline.append(" --file-mapping=");
    cmdline.append(fileMappingName);
    if (!g_dictDir.empty()) {
        cmdline.append(" --dict-dir=");
        cmdline.append(g_dictDir);
    }
    if (g_custom_hit_test) {
        cmdline.append(" --custom-hit-test");
    }
    if (g_custom_tooltip) {
        cmdline.append(" --custom-tooltip");
    }

    for (size_t i = 0; i < g_sideLoadedFonts.size(); ++i) {
        cmdline.append(" --sideload-font=");
        cmdline.append(g_sideLoadedFonts[i]);
    }

    // It seems like CreateProcess wants a char* instead of
    // a const char*.  So we need to make a copy to a modifiable
    // buffer.
    char cmdlineCopy[1024];
    strcpy_s(cmdlineCopy, sizeof(cmdlineCopy), cmdline.c_str());

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);

    PROCESS_INFORMATION procInfo;
    BOOL success = ::CreateProcessA(
        NULL,
        cmdlineCopy,
        NULL,
        NULL,
        FALSE,
        CREATE_BREAKAWAY_FROM_JOB,
        NULL,
        NULL,
        &si,
        &procInfo);

    if (!success) {
        DWORD lastError = ::GetLastError();
        std::cout << "CreateProcess failed: " << lastError << std::endl;
        return NULL;
    }

    if (!::AssignProcessToJobObject(g_hJob, procInfo.hProcess)) {
        DWORD lastError = ::GetLastError();
        std::cout << "AssignProcessToJobObject failed: " << lastError << std::endl;
        ::TerminateProcess(procInfo.hProcess, 1);
        return NULL;
    }

    blpwtk2::String channelId = g_toolkit->createHostChannel(procInfo.dwProcessId, false, "");

    // Write the host channel string into the shared memory
    memset(buffer, 0, BUF_SIZE);
    std::string temp(channelId.data(), channelId.size());
    snprintf((char *) buffer, BUF_SIZE, "%s", temp.c_str());

    ::CloseHandle(procInfo.hThread);
    return procInfo.hProcess;
}

void runHost()
{
    g_hJob = ::CreateJobObject(NULL, NULL);
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!::SetInformationJobObject(g_hJob,
                                       JobObjectExtendedLimitInformation,
                                       &info,
                                       sizeof(info))) {
            DWORD lastError = ::GetLastError();
            std::cout << "SetInformationJobObject failed: " << lastError << std::endl;
            return;
        }
    }

    HostWatcherThreadData threadData;
    threadData.d_mainThreadId = ::GetCurrentThreadId();
    for (size_t i = 0; i < 3; ++i) {
        HANDLE processHandle = spawnProcess();
        if (!processHandle) {
            return;
        }
        threadData.d_processHandles.push_back(processHandle);
    }

    HANDLE watcherThread = ::CreateThread(
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)hostWatcherThreadFunc,
        &threadData,
        0,
        NULL);

    runMessageLoop();
    ::WaitForSingleObject(watcherThread, INFINITE);
    ::CloseHandle(watcherThread);
    for (size_t i = 0; i < threadData.d_processHandles.size(); ++i) {
        ::CloseHandle(threadData.d_processHandles[i]);
    }
    ::CloseHandle(g_hJob);
}



// patch section: log message handler
void logMessageHandler(blpwtk2::ToolkitCreateParams::LogMessageSeverity severity,
                       const char* file,
                       int line,
                       const char* message)
{
    std::cout << "[" << file << ":" << line << "] " << message << std::endl;
}

void consoleLogMessageHandler(blpwtk2::ToolkitCreateParams::LogMessageSeverity severity,
                              const blpwtk2::StringRef& file,
                              unsigned line,
                              unsigned column,
                              const blpwtk2::StringRef& message,
                              const blpwtk2::StringRef& stack_trace)
{
    std::cout << "[" << std::string(file.data(), file.length()) << ":"
              << line << ":" << column << "] "
              << std::string(message.data(), message.length()) << std::endl;
     if (!stack_trace.isEmpty()) {
          std::cout << "Stack Trace:"
                    << std::string(stack_trace.data(), stack_trace.length())
                    << std::endl;
     }
}


// patch section: embedder ipc
void testBrowserV8() {
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(isolate, "'Hello' + ', World!'",
                            v8::NewStringType::kNormal).ToLocalChecked();

    v8::Local<v8::Script> script =
                v8::Script::Compile(context, source).ToLocalChecked();
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

    v8::String::Utf8Value utf8(isolate, result);
    std::cout << "Browser V8 Hello world test -- " << *utf8 << std::endl;
}



int main(int, const char**)
{
    g_instance = GetModuleHandle(NULL);

    // Seed random number generator
    srand(static_cast<unsigned>(time(nullptr)));

    g_url = "http://www.google.com";
    std::string hostChannel;
    std::string fileMapping;
    bool isProcessHost = false;
    blpwtk2::ThreadMode host = blpwtk2::ThreadMode::ORIGINAL;
    int proxyPort = -1;

    {
        int argc;
        LPWSTR *argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        if (!argv) {
            return -1;
        }

        for (int i = 1; i < argc; ++i) {
            if (0 == wcscmp(L"--original-mode-host", argv[i])) {
                host = blpwtk2::ThreadMode::ORIGINAL;
                isProcessHost = true;
            }
            else if (0 == wcscmp(L"--renderer-main-mode-host", argv[i])) {
                host = blpwtk2::ThreadMode::RENDERER_MAIN;
                isProcessHost = true;
            }
            else if (0 == wcscmp(L"--renderer-ui", argv[i])) {
                g_renderer_ui = true;
            }
            else if (0 == wcsncmp(L"--file-mapping=", argv[i], 15)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i]+15);
                fileMapping = buf;
            }
            else if (0 == wcsncmp(L"--sideload-font=", argv[i], 16)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i] + 16);
                g_sideLoadedFonts.push_back(buf);
            }
            else if (0 == wcsncmp(L"--dict-dir=", argv[i], 11)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i] + 11);
                g_dictDir = buf;
            }
            else if (0 == wcscmp(L"--custom-hit-test", argv[i])) {
                g_custom_hit_test = true;
            }
            else if (0 == wcscmp(L"--custom-tooltip", argv[i])) {
                g_custom_tooltip = true;
            }
            else if (0 == wcsncmp(L"--local-proxy=", argv[i], 14)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i]+14);
                proxyPort = atoi(buf);
            }
            else if (0 == wcsncmp(L"--web-script-context-security-origin=", argv[i], 37)) {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i]+37);
                g_webScriptContextSecurityOrigin = buf;
            }
            else if (argv[i][0] != '-') {
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "%S", argv[i]);
                g_url = buf;
            }
        }

        ::LocalFree(argv);
    }

    if (!fileMapping.empty()) {
        // Wait 500ms for the parent process to write to the shared memory.
        // Obviously this is not ideal but this is only for testing.
        Sleep(500);

        // The parent process wants to send me the host channel via shared memory.
        HANDLE handle = ::OpenFileMappingA(
                FILE_MAP_ALL_ACCESS,   // read/write access
                FALSE,                 // do not inherit the name
                fileMapping.c_str());  // name of mapping object

        assert(handle);

        void *buffer = ::MapViewOfFile(
                handle,              // handle to map object
                FILE_MAP_ALL_ACCESS, // read/write permission
                0,
                0,
                BUF_SIZE);

        assert(buffer);
        hostChannel = (char *) buffer;
    }

    if (isProcessHost && host == blpwtk2::ThreadMode::ORIGINAL) {
        g_in_process_renderer = false;
    }

    std::cout << "URL(" << g_url << ") host(" << (isProcessHost ? 1 : 0)
              << ") hostChannel(" << hostChannel << ")" << std::endl;

    blpwtk2::ToolkitCreateParams toolkitParams;

    if ((!isProcessHost || host == blpwtk2::ThreadMode::RENDERER_MAIN) &&
        (g_in_process_renderer || !hostChannel.empty())) {
        toolkitParams.setThreadMode(blpwtk2::ThreadMode::RENDERER_MAIN);
        toolkitParams.setInProcessResourceLoader(createInProcessResourceLoader());
        toolkitParams.setHostChannel(hostChannel);
        if (!g_in_process_renderer) {
            toolkitParams.disableInProcessRenderer();
        }
        // patch section: renderer ui
	else {
            toolkitParams.setRendererUIEnabled(g_renderer_ui);
        }

        // patch section: web script context
        g_webScriptContextAvailable = true;
        toolkitParams.appendCommandLineSwitch("disable-web-security");



    }
    else {
        toolkitParams.setThreadMode(blpwtk2::ThreadMode::ORIGINAL);
        toolkitParams.disableInProcessRenderer();
        toolkitParams.setBrowserV8Enabled(true);
    }

    for (size_t i = 0; i < g_sideLoadedFonts.size(); ++i) {
        toolkitParams.appendSideLoadedFontInProcess(g_sideLoadedFonts[i]);
    }

    toolkitParams.appendCommandLineSwitch("disable-layout-ng");
    toolkitParams.setHeaderFooterHTML(getHeaderFooterHTMLContent());
    toolkitParams.enablePrintBackgroundGraphics();
    toolkitParams.setDictionaryPath(g_dictDir);
    toolkitParams.setLogMessageHandler(logMessageHandler);
    toolkitParams.setConsoleLogMessageHandler(consoleLogMessageHandler);

    g_toolkit = blpwtk2::ToolkitFactory::create(toolkitParams);
    ProcessHostDelegateImpl hostIPCDelegate;
    g_toolkit->setIPCDelegate(&hostIPCDelegate);

    if (isProcessHost && host == blpwtk2::ThreadMode::ORIGINAL) {
        testBrowserV8();
        runHost();
        g_toolkit->destroy();
        g_toolkit = 0;
        return 0;
    }

    int rc = registerShellWindowClass();
    if (rc) return rc;

    g_profile = g_toolkit->getProfile(::GetCurrentProcessId(), true);

    if (proxyPort != -1) {
        blpwtk2::StringRef hostname = "127.0.0.1";
        blpwtk2::ProxyType type = blpwtk2::ProxyType::kHTTP;

        g_profile->addHttpProxy(type, hostname, proxyPort);
        g_profile->addHttpsProxy(type, hostname, proxyPort);
        g_profile->addFtpProxy(type, hostname, proxyPort);
        g_profile->addFallbackProxy(type, hostname, proxyPort);
    }

    g_spellCheckEnabled = true;
    g_languages.insert(LANGUAGE_EN_US);
    updateSpellCheckConfig(g_profile);

    // Configure custom words.
    std::vector<blpwtk2::StringRef> customWords;
    customWords.push_back("foo");
    customWords.push_back("zzzx");
    customWords.push_back("Bloomberg");
    g_profile->addCustomWords(customWords.data(), customWords.size());

    ProcessClientDelegateImpl clientIPCDelegate;
    g_profile->setIPCDelegate(&clientIPCDelegate);

    if (isProcessHost && host == blpwtk2::ThreadMode::RENDERER_MAIN) {
        runHost();
    }
    else {
        Shell* firstShell = createShell(g_profile);
        firstShell->webView()->loadUrl(g_url);
        ShowWindow(firstShell->d_mainWnd, SW_SHOW);
        UpdateWindow(firstShell->d_mainWnd);
        firstShell->d_webView->takeKeyboardFocus();
        firstShell->d_webView->setLogicalFocus(true);
        runMessageLoop();
    }

    g_profile->destroy();
    g_toolkit->destroy();
    g_toolkit = 0;
    return 0;
}

void adjustMenuItemStateFlag(HMENU menu, int menuItem, int state, bool on)
{
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;
    GetMenuItemInfo(menu, menuItem, TRUE, &mii);
    if (on) {
        mii.fState |= state;
    }
    else {
        mii.fState &= ~state;
    }
    SetMenuItemInfo(menu, menuItem, TRUE, &mii);
}

LRESULT CALLBACK shellWndProc(HWND hwnd,        // handle to window
                              UINT uMsg,        // message identifier
                              WPARAM wParam,    // first message parameter
                              LPARAM lParam)    // second message parameter
{
    Shell* shell = reinterpret_cast<Shell*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!shell) return DefWindowProc(hwnd, uMsg, wParam, lParam);

    int wmId;
    Shell* newShell;
    switch(uMsg) {
    case WM_COMMAND:
        wmId = LOWORD(wParam);
        if (wmId >= IDM_CONTEXT_MENU_BASE_CUSTOM_TAG && wmId < IDM_CONTEXT_MENU_END_CUSTOM_TAG) {
            shell->webView()->performCustomContextMenuAction(wmId - IDM_CONTEXT_MENU_BASE_CUSTOM_TAG);
            return 0;
        }
        if (wmId >= IDM_CONTEXT_MENU_BASE_SPELL_TAG && wmId < IDM_CONTEXT_MENU_END_SPELL_TAG) {
            int idx = (wmId - IDM_CONTEXT_MENU_BASE_SPELL_TAG);
            shell->webView()->replaceMisspelledRange(shell->d_contextMenuSpellReplacements[idx]);
            return 0;
        }
        switch (wmId) {
        case IDC_BACK:
            shell->webView()->goBack();
            shell->webView()->takeKeyboardFocus();
            shell->webView()->setLogicalFocus(true);
            return 0;
        case IDC_FORWARD:
            shell->webView()->goForward();
            shell->webView()->takeKeyboardFocus();
            shell->webView()->setLogicalFocus(true);
            return 0;
        case IDC_RELOAD:
            shell->webView()->reload();
            shell->webView()->takeKeyboardFocus();
            shell->webView()->setLogicalFocus(true);
            return 0;
        case IDC_FIND_ENTRY:
            if (HIWORD(wParam) == EN_CHANGE) {
                shell->find();
            }
            return 0;
        case IDC_FIND_PREV:
        case IDC_FIND_NEXT:
            shell->findNext(wmId == IDC_FIND_NEXT);
            return 0;
        case IDC_TIMEZONE_ENTRY:
            if (HIWORD(wParam) == EN_CHANGE) {
                shell->find();
            }
            return 0;
        case IDC_UPDATE_TZ:
            shell->updateTz();
            return 0;
        case IDC_STOP:
            shell->webView()->stop();
            shell->d_webView->takeKeyboardFocus();
            shell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_NEW_WINDOW:
            newShell = createShell(shell->d_profile);
            newShell->webView()->loadUrl(g_url);
            ShowWindow(newShell->d_mainWnd, SW_SHOW);
            UpdateWindow(newShell->d_mainWnd);
            newShell->d_webView->takeKeyboardFocus();
            newShell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_CLOSE_WINDOW:
            DestroyWindow(shell->d_mainWnd);
            return 0;
        case IDM_CLEAR_WEB_CACHE:
            g_profile->clearWebCache();
            return 0;
        case IDM_TEST_V8_APPEND_ELEMENT:
            testV8AppendElement(shell->webView());
            return 0;
        case IDM_TEST_ACCESS_DOM_FROM_WEB_SCRIPT_CONTEXT:
            testAccessDOMFromWebScriptContext(
                shell->webScriptContext(), shell->webView());
            return 0;
        case IDM_TEST_PLAY_KEYBOARD_EVENTS:
            testPlayKeyboardEvents(shell->d_mainWnd, shell->webView());
            return 0;



        // patch section: spellcheck
        case IDM_SPELLCHECK_ENABLED:
            g_spellCheckEnabled = !g_spellCheckEnabled;
            updateSpellCheckConfig(shell->d_profile);
            return 0;


        // patch section: diagnostics
        case IDM_TEST_DUMP_GPU_INFO:
            shell->d_profile->dumpDiagnostics(
                    blpwtk2::Profile::DiagnosticInfoType::GPU, "gpuInfo.txt");
            return 0;


        // patch section: web cache


        // patch section: focus
        case IDM_TEST_KEYBOARD_FOCUS:
            shell->d_webView->takeKeyboardFocus();
            return 0;
        case IDM_TEST_LOGICAL_FOCUS:
            shell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_TEST_LOGICAL_BLUR:
            shell->d_webView->setLogicalFocus(false);
            return 0;


        // patch section: web script context


        // patch section: embedder ipc
        case IDM_SEND_IPC_ASYNC:
            std::cout << "ASYNC IPC from renderer to browser: 'Hello Browser'" << std::endl;
            g_profile->opaqueMessageToBrowserAsync("Hello Browser");
            return 0;
        case IDM_SEND_IPC_SYNC:
        {
            std::cout << "SYNC IPC from renderer to browser: 'Hello Browser'" << std::endl;
            blpwtk2::String result = g_profile->opaqueMessageToBrowserSync("Hello Browser");

            std::cout << "Renderer received SYNC response: " << std::string(result.data(), result.size()) << std::endl;
            return 0;
        }


        // patch section: screen printing
        case IDM_TEST_GET_PDF:
            testGetPicture(shell->d_mainWnd, shell->d_webView, blpwtk2::WebView::DrawParams::RendererType::PDF, 2, 2);
            return 0;
        case IDM_TEST_GET_BITMAP:
            testGetPicture(shell->d_mainWnd, shell->d_webView, blpwtk2::WebView::DrawParams::RendererType::Bitmap, 2, 2);
            return 0;



        case IDM_LANGUAGE_DE:
            toggleLanguage(shell->d_profile, LANGUAGE_DE);
            return 0;
        case IDM_LANGUAGE_EN_GB:
            toggleLanguage(shell->d_profile, LANGUAGE_EN_GB);
            return 0;
        case IDM_LANGUAGE_EN_US:
            toggleLanguage(shell->d_profile, LANGUAGE_EN_US);
            return 0;
        case IDM_LANGUAGE_ES:
            toggleLanguage(shell->d_profile, LANGUAGE_ES);
            return 0;
        case IDM_LANGUAGE_FR:
            toggleLanguage(shell->d_profile, LANGUAGE_FR);
            return 0;
        case IDM_LANGUAGE_IT:
            toggleLanguage(shell->d_profile, LANGUAGE_IT);
            return 0;
        case IDM_LANGUAGE_PT_BR:
            toggleLanguage(shell->d_profile, LANGUAGE_PT_BR);
            return 0;
        case IDM_LANGUAGE_PT_PT:
            toggleLanguage(shell->d_profile, LANGUAGE_PT_PT);
            return 0;
        case IDM_LANGUAGE_RU:
            toggleLanguage(shell->d_profile, LANGUAGE_RU);
            return 0;
        case IDM_CUT:
            shell->webView()->cutSelection();
            return 0;
        case IDM_COPY:
            shell->webView()->copySelection();
            return 0;
        case IDM_PASTE:
            shell->webView()->paste();
            return 0;
        case IDM_DELETE:
            shell->webView()->deleteSelection();
            return 0;
        case IDM_INSPECT:
            if (shell->d_inspectorShell) {
                BringWindowToTop(shell->d_inspectorShell->d_mainWnd);
                shell->d_inspectorShell->webView()->inspectElementAt(shell->d_contextMenuPoint);
                shell->d_inspectorShell->d_webView->takeKeyboardFocus();
                shell->d_inspectorShell->d_webView->setLogicalFocus(true);
                return 0;
            }
            {
                blpwtk2::Profile* profile = g_profile;
                shell->d_inspectorShell = createShell(profile, 0, true);
            }
            shell->d_inspectorShell->d_inspectorFor = shell;
            ShowWindow(shell->d_inspectorShell->d_mainWnd, SW_SHOW);
            UpdateWindow(shell->d_inspectorShell->d_mainWnd);
            shell->d_inspectorShell->webView()->loadInspector(::GetCurrentProcessId(), shell->webView()->getRoutingId());
            shell->d_inspectorShell->webView()->inspectElementAt(shell->d_contextMenuPoint);
            shell->d_inspectorShell->d_webView->takeKeyboardFocus();
            shell->d_inspectorShell->d_webView->setLogicalFocus(true);
            return 0;
        case IDM_ADD_TO_DICTIONARY:
            {
                blpwtk2::StringRef word = shell->d_misspelledWord;
                shell->d_profile->addCustomWords(&word, 1);
            }
            return 0;
        case IDM_EXIT:
            std::vector<Shell*> shells(Shell::s_shells.begin(), Shell::s_shells.end());
            for (int i = 0, size = shells.size(); i < size; ++i)
                DestroyWindow(shells[i]->d_mainWnd);
            return 0;
        }
        break;
    case WM_INITMENUPOPUP: {
            HMENU menu = (HMENU)wParam;

            adjustMenuItemStateFlag(shell->d_spellCheckMenu, 1, MFS_DISABLED, !g_spellCheckEnabled);
            adjustMenuItemStateFlag(shell->d_spellCheckMenu, 2, MFS_DISABLED, !g_spellCheckEnabled);
            CheckMenuItem(menu, IDM_SPELLCHECK_ENABLED, g_spellCheckEnabled ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_DE, g_languages.find(LANGUAGE_DE) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_EN_GB, g_languages.find(LANGUAGE_EN_GB) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_EN_US, g_languages.find(LANGUAGE_EN_US) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_ES, g_languages.find(LANGUAGE_ES) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_FR, g_languages.find(LANGUAGE_FR) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_IT, g_languages.find(LANGUAGE_IT) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_PT_BR, g_languages.find(LANGUAGE_PT_BR) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_PT_PT, g_languages.find(LANGUAGE_PT_PT) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(menu, IDM_LANGUAGE_RU, g_languages.find(LANGUAGE_RU) != g_languages.end() ? MF_CHECKED : MF_UNCHECKED);
        }
        break;
    case WM_WINDOWPOSCHANGED:
        shell->webView()->rootWindowPositionChanged();
        break;
    case WM_SETTINGCHANGE:
        shell->webView()->rootWindowSettingsChanged();
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = ::BeginPaint(hwnd, &ps);
            ::FillRect(hdc, &ps.rcPaint, (HBRUSH)::GetStockObject(BLACK_BRUSH));
            ::EndPaint(hwnd, &ps);
        }
        return 0;
    case WM_DESTROY:
        delete shell;
        return 0;
    case WM_SIZE:
        shell->resizeSubViews();
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK urlEntryWndProc(HWND hwnd,        // handle to window
                                 UINT uMsg,        // message identifier
                                 WPARAM wParam,    // first message parameter
                                 LPARAM lParam)    // second message parameter
{
    Shell* shell = reinterpret_cast<Shell*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!shell) return CallWindowProc(g_defaultEditWndProc, hwnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_CHAR:
        if (wParam == VK_RETURN) {
            const int MAX_URL_LENGTH = 255;
            char str[MAX_URL_LENGTH + 1];  // Leave room for adding a NULL;
            *((WORD*)str) = MAX_URL_LENGTH;
            LRESULT str_len = SendMessageA(hwnd, EM_GETLINE, 0, (LPARAM)str);
            if (str_len > 0) {
                str[str_len] = 0;  // EM_GETLINE doesn't NULL terminate.
                shell->webView()->loadUrl(str);
                shell->webView()->takeKeyboardFocus();
                shell->webView()->setLogicalFocus(true);
            }
            return 0;
        }
    }

    return CallWindowProc(g_defaultEditWndProc, hwnd, uMsg, wParam, lParam);
}



int registerShellWindowClass()
{
    WNDCLASSEX wcx;

    // Fill in the window class structure with parameters
    // that describe the main window.

    wcx.cbSize = sizeof(wcx);               // size of structure
    wcx.style = CS_HREDRAW | CS_VREDRAW;    // redraw if size changes
    wcx.lpfnWndProc = shellWndProc;         // points to window procedure
    wcx.cbClsExtra = 0;                     // no extra class memory
    wcx.cbWndExtra = 0;                     // no extra window memory
    wcx.hInstance = g_instance;             // handle to instance
    wcx.hIcon = LoadIcon(NULL, IDI_APPLICATION);    // predefined app. icon
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);      // predefined arrow
    wcx.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);    // white background brush
    wcx.lpszMenuName =  NULL;               // name of menu resource
    wcx.lpszClassName = L"ShellWClass";     // name of window class
    wcx.hIconSm = (HICON)LoadImage(g_instance,  // small class icon
                                   MAKEINTRESOURCE(5),
                                   IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON),
                                   LR_DEFAULTCOLOR);

    // Register the window class.  RegisterClassEx returns 0 for failure!!
    return RegisterClassEx(&wcx) == 0 ? -1 : 0;
}

Shell* createShell(blpwtk2::Profile* profile, blpwtk2::WebView* webView, bool forDevTools)
{
    // Create the main window.
    HWND mainWnd = CreateWindow(L"ShellWClass",      // name of window class
                                L"Sample",           // title-bar string
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, // top-level window
                                CW_USEDEFAULT,       // default horizontal position
                                CW_USEDEFAULT,       // default vertical position
                                1600,                // same width,
                                1200,                // and height as content_shell
                                (HWND) NULL,         // no owner window
                                (HMENU) NULL,        // use class menu
                                g_instance,          // handle to application instance
                                (LPVOID) NULL);      // no window-creation data
    assert(mainWnd);

    HMENU menu = CreateMenu();
    HMENU fileMenu = CreateMenu();
    AppendMenu(fileMenu, MF_STRING, IDM_NEW_WINDOW, L"&New Window");
    AppendMenu(fileMenu, MF_STRING, IDM_CLOSE_WINDOW, L"&Close Window");
    AppendMenu(fileMenu, MF_STRING, IDM_CLEAR_WEB_CACHE, L"Clear Web Cache");
    AppendMenu(fileMenu, MF_SEPARATOR, 0, 0);
    AppendMenu(fileMenu, MF_STRING, IDM_INSPECT, L"Inspect...");
    AppendMenu(fileMenu, MF_SEPARATOR, 0, 0);
    AppendMenu(fileMenu, MF_STRING, IDM_EXIT, L"E&xit");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)fileMenu, L"&File");
    HMENU testMenu = CreateMenu();
    AppendMenu(testMenu, MF_STRING, IDM_TEST_V8_APPEND_ELEMENT, L"Append Element Using &V8");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_KEYBOARD_FOCUS, L"Test Keyboard Focus");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_LOGICAL_FOCUS, L"Test Logical Focus");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_LOGICAL_BLUR, L"Test Logical Blur");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_PLAY_KEYBOARD_EVENTS, L"Test Play Keyboard Events");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_GET_PDF, L"Test Capture PDF");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_GET_BITMAP, L"Test Capture Bitmap");
    AppendMenu(testMenu, MF_STRING, IDM_TEST_DUMP_LAYOUT_TREE, L"Dump Layout Tree");



    // patch section: diagnostics
    AppendMenu(testMenu, MF_STRING, IDM_TEST_DUMP_GPU_INFO, L"Dump GPU Information");


    // patch section: embedder ipc
    HMENU ipcMenu = CreateMenu();
    AppendMenu(ipcMenu, MF_STRING, IDM_SEND_IPC_SYNC, L"Send IPC sync from renderer to browser");
    AppendMenu(ipcMenu, MF_STRING, IDM_SEND_IPC_ASYNC, L"Send IPC async from renderer to browser");
    AppendMenu(testMenu, MF_POPUP, (UINT_PTR)ipcMenu, L"&IPC");


    // patch section: web script context
    if (g_webScriptContextAvailable) {
        AppendMenu(testMenu, MF_STRING, IDM_TEST_ACCESS_DOM_FROM_WEB_SCRIPT_CONTEXT, L"Access the DOM from a 'web script context'");
    }



    AppendMenu(menu, MF_POPUP, (UINT_PTR)testMenu, L"&Test");
    HMENU spellCheckMenu = CreateMenu();
    AppendMenu(spellCheckMenu, MF_STRING, IDM_SPELLCHECK_ENABLED, L"Enable &Spellcheck");
    HMENU languagesMenu = CreateMenu();
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_DE, L"&German");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_EN_GB, L"&English (Great Britain)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_EN_US, L"English (&United States)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_ES, L"&Spanish");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_FR, L"&French");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_IT, L"I&talian");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_PT_BR, L"Portuguese (&Brazil)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_PT_PT, L"Portuguese (&Portugal)");
    AppendMenu(languagesMenu, MF_STRING, IDM_LANGUAGE_RU, L"&Russian");
    AppendMenu(spellCheckMenu, MF_POPUP, (UINT_PTR)languagesMenu, L"&Languages");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)spellCheckMenu, L"&Spelling");
    SetMenu(mainWnd, menu);

    HWND hwnd;
    int x = 0;

    hwnd = CreateWindow(L"BUTTON", L"Back",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_BACK, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"Forward",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_FORWARD, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"Reload",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_RELOAD, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"Stop",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                        x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_STOP, g_instance, 0);
    assert(hwnd);
    x += BUTTON_WIDTH;

    hwnd = CreateWindow(L"STATIC", L"Find: ",
                        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
                        x, 0, FIND_LABEL_WIDTH, URLBAR_HEIGHT,
                        mainWnd, 0, g_instance, 0);
    assert(hwnd);
    x += FIND_LABEL_WIDTH;

    HWND findEntryHwnd = CreateWindow(L"EDIT", 0,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                        ES_AUTOVSCROLL | ES_AUTOHSCROLL,  x, 0, FIND_ENTRY_WIDTH,
                        URLBAR_HEIGHT, mainWnd, (HMENU)IDC_FIND_ENTRY, g_instance, 0);
    assert(findEntryHwnd);
    x += FIND_ENTRY_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"\u2191",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        x, 0, FIND_BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_FIND_PREV, g_instance, 0);
    assert(hwnd);
    x += FIND_BUTTON_WIDTH;

    hwnd = CreateWindow(L"BUTTON", L"\u2193",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        x, 0, FIND_BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_FIND_NEXT, g_instance, 0);
    assert(hwnd);
    x += FIND_BUTTON_WIDTH;

    hwnd = CreateWindow(L"STATIC", L"Time Zone: ",
                        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
                        x, 0, TIMEZONE_LABEL_WIDTH, URLBAR_HEIGHT,
                        mainWnd, 0, g_instance, 0);
    assert(hwnd);
    x += TIMEZONE_LABEL_WIDTH;
     HWND timezoneEntryHwnd = CreateWindow(L"EDIT", 0,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                        ES_AUTOVSCROLL | ES_AUTOHSCROLL,  x, 0, TIMEZONE_ENTRY_WIDTH,
                        URLBAR_HEIGHT, mainWnd, (HMENU) NULL, g_instance, 0);
    assert(timezoneEntryHwnd);
    x += TIMEZONE_ENTRY_WIDTH;
     hwnd = CreateWindow(L"BUTTON", L"Update",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        x, 0, TIMEZONE_BUTTON_WIDTH, URLBAR_HEIGHT,
                        mainWnd, (HMENU)IDC_UPDATE_TZ, g_instance, 0);
    assert(hwnd);
    x += TIMEZONE_BUTTON_WIDTH;

    // This control is positioned by resizeSubViews.
    HWND urlEntryWnd = CreateWindow(L"EDIT", 0,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                                    ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                    x, 0, 0, 0, mainWnd, 0, g_instance, 0);
    assert(urlEntryWnd);

    if (!g_defaultEditWndProc)
        g_defaultEditWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(urlEntryWnd, GWLP_WNDPROC));
    SetWindowLongPtr(urlEntryWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(urlEntryWndProc));

    return new Shell(mainWnd, urlEntryWnd, findEntryHwnd, spellCheckMenu,
                     timezoneEntryHwnd, profile, webView, forDevTools);
}

void populateMenuItem(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuItem& item)
{
    UINT flags =  MF_STRING | (item.enabled() ? MF_ENABLED : MF_GRAYED);
    std::string label(item.label().data(),
                      item.label().length());
    if (item.type() == blpwtk2::ContextMenuItem::Type::OPTION) {
        AppendMenuA(menu, flags, menuIdStart + item.action(), label.c_str());
    }
    else if (item.type() == blpwtk2::ContextMenuItem::Type::CHECKABLE_OPTION) {
        flags = flags | (item.checked() ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuA(menu, flags, menuIdStart + item.action(), label.c_str());
    } else if (item.type() ==  blpwtk2::ContextMenuItem::Type::SEPARATOR) {
        AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    } else if (item.type() == blpwtk2::ContextMenuItem::Type::SUBMENU) {
        HMENU popupMenu = CreatePopupMenu();
        flags = flags | MF_POPUP;
        AppendMenuA(menu, flags, (UINT_PTR)popupMenu, label.c_str());
        populateSubmenu(popupMenu, menuIdStart, item);
    }
}

void populateContextMenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuParams& params)
{
    for (int i = 0; i < params.numCustomItems(); ++i) {
        populateMenuItem(menu, menuIdStart, params.customItem(i));
    }
}

void populateSubmenu(HMENU menu, int menuIdStart, const blpwtk2::ContextMenuItem& item)
{
    for (int i = 0; i < item.numSubMenuItems(); ++i) {
        populateMenuItem(menu, menuIdStart,item.subMenuItem(i));
    }
}

void updateSpellCheckConfig(blpwtk2::Profile* profile)
{
    std::vector<blpwtk2::StringRef> languages;
    for (std::set<std::string>::const_iterator it = g_languages.begin();
                                               it != g_languages.end();
                                               ++it) {
        languages.push_back(it->c_str());
    }
    profile->setLanguages(languages.data(), languages.size());
    profile->enableSpellCheck(g_spellCheckEnabled);
}

void toggleLanguage(blpwtk2::Profile* profile, const std::string& language)
{
    if (g_languages.find(language) == g_languages.end()) {
        g_languages.insert(language);
    }
    else {
        g_languages.erase(language);
    }
    updateSpellCheckConfig(profile);
}

class DummyResourceLoader : public blpwtk2::ResourceLoader {
    // This dummy ResourceLoader handles all "http://cdrive/" requests
    // and responds with the file at the specified path in the C drive.  For
    // example:
    //
    //     http://cdrive/stuff/test.html
    //
    // will return the contents of:
    //
    //     C:\stuff\test.html

public:
    static const char PREFIX[];

    bool canHandleURL(const blpwtk2::StringRef& url) override
    {
        if (url.length() <= strlen(PREFIX))
            return false;
        blpwtk2::StringRef prefix(url.data(), strlen(PREFIX));
        if (!prefix.equals(PREFIX))
            return false;
        return true;
    }

    void start(const blpwtk2::StringRef& url,
               blpwtk2::ResourceContext* context,
               void** userData) override
    {
        assert(canHandleURL(url));

        std::string filePath = "C:\\";
        filePath.append(url.data() + strlen(PREFIX),
                        url.length() - strlen(PREFIX));
        std::replace(filePath.begin(), filePath.end(), '/', '\\');

        std::ifstream fstream(filePath.c_str());
        char buffer[1024];
        if (!fstream.is_open()) {
            context->replaceStatusLine("HTTP/1.1 404 Not Found");
            strcpy_s(buffer, sizeof(buffer), "The specified file was not found.");
            context->addResponseData(buffer, strlen(buffer));
        }
        else {
            while (!fstream.eof()) {
                fstream.read(buffer, sizeof(buffer));
                if (fstream.bad()) {
                    // some other failure
                    context->failed();
                    break;
                }

                assert((std::size_t) fstream.gcount() <= sizeof(buffer));
                context->addResponseData(buffer, (int)fstream.gcount());
            }
        }
        context->finish();
    }

    void cancel(blpwtk2::ResourceContext* context,
                void* userData) override
    {
        assert(false);  // everything is loaded in start(), so we should never
                        // get canceled
    }
};
const char DummyResourceLoader::PREFIX[] = "http://cdrive/";

blpwtk2::ResourceLoader* createInProcessResourceLoader()
{
    return new DummyResourceLoader();
}

const char* getHeaderFooterHTMLContent() {
  return R"DeLiMeTeR(<!DOCTYPE html>
<html>
<head>
<style>
  body {
    margin: 0px;
    width: 0px;
  }
  .row {
    display: table-row;
    vertical-align: inherit;
  }
  #header, #footer {
    display: table;
    table-layout:fixed;
    width: inherit;
  }
  #header {
    vertical-align: top;
  }
  #footer {
    vertical-align: bottom;
  }
  .text {
    display: table-cell;
    font-family: sans-serif;
    font-size: 8px;
    vertical-align: inherit;
    white-space: nowrap;
  }
  #page_number {
    text-align: right;
  }
  #title {
    text-align: center;
  }
  #date, #url {
    padding-left: 0.7cm;
    padding-right: 0.1cm;
  }
  #title, #page_number {
    padding-left: 0.1cm;
    padding-right: 0.7cm;
  }
  #title, #url {
    overflow: hidden;
    text-overflow: ellipsis;
  }
  #title, #date {
    padding-bottom: 0cm;
    padding-top: 0.4cm;
  }
  #page_number, #url {
    padding-bottom: 0.4cm;
    padding-top: 0cm;
  }
</style>
<script>
function pixels(value) {
  return value + 'px';
}
function setup(options) {
  var body = document.querySelector('body');
  var header = document.querySelector('#header');
  var content = document.querySelector('#content');
  var footer = document.querySelector('#footer');
  body.style.width = pixels(options['width']);
  body.style.height = pixels(options['height']);
  header.style.height = pixels(options['topMargin']);
  content.style.height = pixels(options['height'] - options['topMargin'] - options['bottomMargin']);
  footer.style.height = pixels(options['bottomMargin']);
  document.querySelector('#date span').innerText =
    new Date(options['date']).toLocaleDateString();
  document.querySelector('#title span').innerText = options['title'];
  document.querySelector('#url span').innerText = options['url'];
  document.querySelector('#page_number span').innerText = options['pageNumber'];
  document.querySelector('#date').style.width =
    pixels(document.querySelector('#date span').offsetWidth);
  document.querySelector('#page_number').style.width =
    pixels(document.querySelector('#page_number span').offsetWidth);
  if (header.offsetHeight > options['topMargin'] + 1) {
    header.style.display = 'none';
    content.style.height = pixels(options['height'] - options['bottomMargin']);
  }
  if (footer.offsetHeight > options['bottomMargin'] + 1) {
     footer.style.display = 'none';
  }
}
</script>
</head>
<body>
<div id="header">
  <div class="row">
    <div id="date" class="text"><span/></div>
    <div id="title" class="text"><span/></div>
  </div>
</div>
<div id="content">
</div>
<div id="footer">
  <div class="row">
    <div id="url" class="text"><span/></div>
    <div id="page_number" class="text"><span/></div>
  </div>
</div>
</body>
</html>
)DeLiMeTeR";
}
