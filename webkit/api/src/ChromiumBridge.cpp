/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ChromiumBridge.h"

#include <googleurl/src/url_util.h>

#include "WebClipboard.h"
#include "WebData.h"
#include "WebImage.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebMimeRegistry.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginListBuilderImpl.h"
#include "WebString.h"
#include "WebURL.h"
#include "Worker.h"
#include "WorkerContextProxy.h"

#if PLATFORM(WIN_OS)
#include "WebRect.h"
#include "WebSandboxSupport.h"
#include "WebThemeEngine.h"
#endif

#if PLATFORM(LINUX)
#include "WebSandboxSupport.h"
#include "WebFontInfo.h"
#endif

#if WEBKIT_USING_SKIA
#include "NativeImageSkia.h"
#endif

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "PluginData.h"
#include <wtf/Assertions.h>

// We are part of the WebKit implementation.
using namespace WebKit;

namespace WebCore {

// Clipboard ------------------------------------------------------------------

bool ChromiumBridge::clipboardIsFormatAvailable(
    PasteboardPrivate::ClipboardFormat format,
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitClient()->clipboard()->isFormatAvailable(
        static_cast<WebClipboard::Format>(format),
        static_cast<WebClipboard::Buffer>(buffer));
}

String ChromiumBridge::clipboardReadPlainText(
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitClient()->clipboard()->readPlainText(
        static_cast<WebClipboard::Buffer>(buffer));
}

void ChromiumBridge::clipboardReadHTML(
    PasteboardPrivate::ClipboardBuffer buffer,
    String* htmlText, KURL* sourceURL)
{
    WebURL url;
    *htmlText = webKitClient()->clipboard()->readHTML(
        static_cast<WebClipboard::Buffer>(buffer), &url);
    *sourceURL = url;
}

void ChromiumBridge::clipboardWriteSelection(const String& htmlText,
                                             const KURL& sourceURL,
                                             const String& plainText,
                                             bool writeSmartPaste)
{
    webKitClient()->clipboard()->writeHTML(
        htmlText, sourceURL, plainText, writeSmartPaste);
}

void ChromiumBridge::clipboardWritePlainText(const String& plainText)
{
    webKitClient()->clipboard()->writePlainText(plainText);
}

void ChromiumBridge::clipboardWriteURL(const KURL& url, const String& title)
{
    webKitClient()->clipboard()->writeURL(url, title);
}

void ChromiumBridge::clipboardWriteImage(NativeImagePtr image,
                                         const KURL& sourceURL,
                                         const String& title)
{
#if WEBKIT_USING_SKIA
    WebImage webImage(*image);
#else
    WebImage webImage(image);
#endif
    webKitClient()->clipboard()->writeImage(webImage, sourceURL, title);
}

// Cookies --------------------------------------------------------------------

void ChromiumBridge::setCookies(const KURL& url,
                                const KURL& firstPartyForCookies,
                                const String& cookie)
{
    webKitClient()->setCookies(url, firstPartyForCookies, cookie);
}

String ChromiumBridge::cookies(const KURL& url,
                               const KURL& firstPartyForCookies)
{
    return webKitClient()->cookies(url, firstPartyForCookies);
}

// DNS ------------------------------------------------------------------------

void ChromiumBridge::prefetchDNS(const String& hostname)
{
    webKitClient()->prefetchHostName(hostname);
}

// File ------------------------------------------------------------------------

bool ChromiumBridge::fileExists(const String& path)
{
    return webKitClient()->fileExists(path);
}

bool ChromiumBridge::deleteFile(const String& path)
{
    return webKitClient()->deleteFile(path);
}

bool ChromiumBridge::deleteEmptyDirectory(const String& path)
{
    return webKitClient()->deleteEmptyDirectory(path);
}

bool ChromiumBridge::getFileSize(const String& path, long long& result)
{
    return webKitClient()->getFileSize(path, result);
}

bool ChromiumBridge::getFileModificationTime(const String& path, time_t& result)
{
    return webKitClient()->getFileModificationTime(path, result);
}

String ChromiumBridge::directoryName(const String& path)
{
    return webKitClient()->directoryName(path);
}

String ChromiumBridge::pathByAppendingComponent(const String& path, const String& component)
{
    return webKitClient()->pathByAppendingComponent(path, component);
}

bool ChromiumBridge::makeAllDirectories(const String& path)
{
  return webKitClient()->makeAllDirectories(path);
}

String ChromiumBridge::getAbsolutePath(const String& path)
{
    return webKitClient()->getAbsolutePath(path);
}

bool ChromiumBridge::isDirectory(const String& path)
{
    return webKitClient()->isDirectory(path);
}

KURL ChromiumBridge::filePathToURL(const String& path)
{
    return webKitClient()->filePathToURL(path);
}

// Font -----------------------------------------------------------------------

#if PLATFORM(WIN_OS)
bool ChromiumBridge::ensureFontLoaded(HFONT font)
{
    WebSandboxSupport* ss = webKitClient()->sandboxSupport();

    // if there is no sandbox, then we can assume the font
    // was able to be loaded successfully already
    return ss ? ss->ensureFontLoaded(font) : true;
}
#endif

#if PLATFORM(LINUX)
String ChromiumBridge::getFontFamilyForCharacters(const UChar* characters, size_t numCharacters)
{
    if (webKitClient()->sandboxSupport())
        return webKitClient()->sandboxSupport()->getFontFamilyForCharacters(characters, numCharacters);
    else {
        WebCString family = WebFontInfo::familyForChars(characters, numCharacters);
        if (family.data())
            return WebString::fromUTF8(family.data());
        else
            return WebString();
    }
}
#endif

// HTML5 DB -------------------------------------------------------------------

#if ENABLE(DATABASE)
PlatformFileHandle ChromiumBridge::databaseOpenFile(const String& fileName, int desiredFlags, PlatformFileHandle* dirHandle)
{
  return webKitClient()->databaseOpenFile(WebString(fileName), desiredFlags, dirHandle);
}

int ChromiumBridge::databaseDeleteFile(const String& fileName, bool syncDir)
{
    return webKitClient()->databaseDeleteFile(WebString(fileName), syncDir);
}

long ChromiumBridge::databaseGetFileAttributes(const String& fileName)
{
    return webKitClient()->databaseGetFileAttributes(WebString(fileName));
}

long long ChromiumBridge::databaseGetFileSize(const String& fileName)
{
    return webKitClient()->databaseGetFileSize(WebString(fileName));
}
#endif

// Keygen ---------------------------------------------------------------------

String ChromiumBridge::signedPublicKeyAndChallengeString(
    unsigned keySizeIndex, const String& challenge, const KURL& url)
{
    return webKitClient()->signedPublicKeyAndChallengeString(keySizeIndex,
                                                             WebString(challenge),
                                                             WebURL(url));
}

// Language -------------------------------------------------------------------

String ChromiumBridge::computedDefaultLanguage()
{
    return webKitClient()->defaultLocale();
}

// LayoutTestMode -------------------------------------------------------------

bool ChromiumBridge::layoutTestMode()
{
    return WebKit::layoutTestMode();
}

// MimeType -------------------------------------------------------------------

bool ChromiumBridge::isSupportedImageMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool ChromiumBridge::isSupportedJavaScriptMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsJavaScriptMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool ChromiumBridge::isSupportedNonImageMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsNonImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

String ChromiumBridge::mimeTypeForExtension(const String& extension)
{
    return webKitClient()->mimeRegistry()->mimeTypeForExtension(extension);
}

String ChromiumBridge::mimeTypeFromFile(const String& path)
{
    return webKitClient()->mimeRegistry()->mimeTypeFromFile(path);
}

String ChromiumBridge::preferredExtensionForMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->preferredExtensionForMIMEType(mimeType);
}

// Plugin ---------------------------------------------------------------------

bool ChromiumBridge::plugins(bool refresh, Vector<PluginInfo*>* results)
{
    WebPluginListBuilderImpl builder(results);
    webKitClient()->getPluginList(refresh, &builder);
    return true;  // FIXME: There is no need for this function to return a value.
}

NPObject* ChromiumBridge::pluginScriptableObject(Widget* widget)
{
    if (!widget)
        return NULL;

    ASSERT(!widget->isFrameView());

    // NOTE:  We have to trust that the widget passed to us here is a
    // WebPluginContainerImpl.  There isn't a way to dynamically verify it,
    // since the derived class (Widget) has no identifier.
    return static_cast<WebPluginContainerImpl*>(widget)->scriptableObject();
}

// Resources ------------------------------------------------------------------

PassRefPtr<Image> ChromiumBridge::loadPlatformImageResource(const char* name)
{
    const WebData& resource = webKitClient()->loadResource(name);
    if (resource.isEmpty())
        return Image::nullImage();

    RefPtr<Image> image = BitmapImage::create();
    image->setData(resource, true);
    return image;
}

// Sandbox --------------------------------------------------------------------

bool ChromiumBridge::sandboxEnabled()
{
    return webKitClient()->sandboxEnabled();
}

// SharedTimers ---------------------------------------------------------------

void ChromiumBridge::setSharedTimerFiredFunction(void (*func)())
{
    webKitClient()->setSharedTimerFiredFunction(func);
}

void ChromiumBridge::setSharedTimerFireTime(double fireTime)
{
    webKitClient()->setSharedTimerFireTime(fireTime);
}

void ChromiumBridge::stopSharedTimer()
{
    webKitClient()->stopSharedTimer();
}

// StatsCounters --------------------------------------------------------------

void ChromiumBridge::decrementStatsCounter(const char* name)
{
    webKitClient()->decrementStatsCounter(name);
}

void ChromiumBridge::incrementStatsCounter(const char* name)
{
    webKitClient()->incrementStatsCounter(name);
}

// Sudden Termination ---------------------------------------------------------

void ChromiumBridge::suddenTerminationChanged(bool enabled)
{
    webKitClient()->suddenTerminationChanged(enabled);
}

// SystemTime -----------------------------------------------------------------

double ChromiumBridge::currentTime()
{
    return webKitClient()->currentTime();
}

// Theming --------------------------------------------------------------------

#if PLATFORM(WIN_OS)

void ChromiumBridge::paintButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintMenuList(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintMenuList(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintScrollbarArrow(
    GraphicsContext* gc, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintScrollbarArrow(
        gc->platformContext()->canvas(), state, classicState, rect);
}

void ChromiumBridge::paintScrollbarThumb(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintScrollbarThumb(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintScrollbarTrack(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const IntRect& alignRect)
{
    webKitClient()->themeEngine()->paintScrollbarTrack(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        alignRect);
}

void ChromiumBridge::paintTextField(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const Color& color, bool fillContentArea,
    bool drawEdges)
{
    // Fallback to white when |color| is invalid.
    RGBA32 backgroundColor = color.isValid() ? color.rgb() : Color::white;

    webKitClient()->themeEngine()->paintTextField(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        backgroundColor, fillContentArea, drawEdges);
}

void ChromiumBridge::paintTrackbar(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintTrackbar(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

#endif

// Trace Event ----------------------------------------------------------------

void ChromiumBridge::traceEventBegin(const char* name, void* id, const char* extra)
{
    webKitClient()->traceEventBegin(name, id, extra);
}

void ChromiumBridge::traceEventEnd(const char* name, void* id, const char* extra)
{
    webKitClient()->traceEventEnd(name, id, extra);
}

// Visited Links --------------------------------------------------------------

WebCore::LinkHash ChromiumBridge::visitedLinkHash(const UChar* url,
                                                  unsigned length)
{
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;
    if (!url_util::Canonicalize(url, length, NULL, &buffer, &parsed))
        return 0;  // Invalid URLs are unvisited.
    return webKitClient()->visitedLinkHash(buffer.data(), buffer.length());
}

WebCore::LinkHash ChromiumBridge::visitedLinkHash(const WebCore::KURL& base,
                                                  const WebCore::AtomicString& attributeURL)
{
    // Resolve the relative URL using googleurl and pass the absolute URL up to
    // the embedder. We could create a GURL object from the base and resolve
    // the relative URL that way, but calling the lower-level functions
    // directly saves us the string allocation in most cases.
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;

#if USE(GOOGLEURL)
    const WebCore::CString& cstr = base.utf8String();
    const char* data = cstr.data();
    int length = cstr.length();
    const url_parse::Parsed& srcParsed = base.parsed();
#else
    // When we're not using GoogleURL, first canonicalize it so we can resolve it
    // below.
    url_canon::RawCanonOutput<2048> srcCanon;
    url_parse::Parsed srcParsed;
    WebCore::String str = base.string();
    if (!url_util::Canonicalize(str.characters(), str.length(), NULL, &srcCanon, &srcParsed))
        return 0;
    const char* data = srcCanon.data();
    int length = srcCanon.length();
#endif

    if (!url_util::ResolveRelative(data, length, srcParsed, attributeURL.characters(),
                                   attributeURL.length(), NULL, &buffer, &parsed))
        return 0;  // Invalid resolved URL.

    return webKitClient()->visitedLinkHash(buffer.data(), buffer.length());
}

bool ChromiumBridge::isLinkVisited(WebCore::LinkHash visitedLinkHash)
{
    return webKitClient()->isLinkVisited(visitedLinkHash);
}

// These are temporary methoeds that the WebKit layer can use to call to the
// Glue layer. Once the Glue layer moves entirely into the WebKit layer, these
// methods will be deleted.

String ChromiumBridge::uiResourceProtocol()
{
    return webKitClient()->uiResourceProtocol();
}

void ChromiumBridge::notifyJSOutOfMemory(WebCore::Frame* frame)
{
    return webKitClient()->notifyJSOutOfMemory(frame);
}

int ChromiumBridge::screenDepth(Widget* widget)
{
    return webKitClient()->screenDepth(widget);
}

int ChromiumBridge::screenDepthPerComponent(Widget* widget)
{
    return webKitClient()->screenDepthPerComponent(widget);
}

bool ChromiumBridge::screenIsMonochrome(Widget* widget)
{
    return webKitClient()->screenIsMonochrome(widget);
}

IntRect ChromiumBridge::screenRect(Widget* widget)
{
    return webKitClient()->screenRect(widget);
}

IntRect ChromiumBridge::screenAvailableRect(Widget* widget)
{
    return webKitClient()->screenAvailableRect(widget);
}

bool ChromiumBridge::popupsAllowed(NPP npp)
{
    return webKitClient()->popupsAllowed(npp);
}

void ChromiumBridge::widgetSetCursor(Widget* widget, const Cursor& cursor)
{
    return webKitClient()->widgetSetCursor(widget, cursor);
}

void ChromiumBridge::widgetSetFocus(Widget* widget)
{
    return webKitClient()->widgetSetFocus(widget);
}

WorkerContextProxy* WorkerContextProxy::create(Worker* worker)
{
    return webKitClient()->createWorkerContextProxy(worker);
}

} // namespace WebCore
