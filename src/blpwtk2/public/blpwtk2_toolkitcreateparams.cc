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

#include <blpwtk2_toolkitcreateparams.h>

#include <blpwtk2_channelinfo.h>
#include <blpwtk2_products.h>
#include <blpwtk2_stringref.h>

#include <base/logging.h>  // for DCHECK
#include <content/public/common/content_switches.h>

#include <string>
#include <vector>

namespace blpwtk2 {

                        // ==============================
                        // struct ToolkitCreateParamsImpl
                        // ==============================

struct ToolkitCreateParamsImpl final
{
    ThreadMode d_threadMode;
    ToolkitCreateParams::LogMessageHandler d_logMessageHandler;
    ToolkitCreateParams::ConsoleLogMessageHandler d_consoleLogMessageHandler;
    ToolkitCreateParams::WinProcExceptionFilter d_winProcExceptionFilter;
    ToolkitCreateParams::ChannelErrorHandler d_channelErrorHandler;
    int d_maxSocketsPerProxy;
    std::vector<std::string> d_commandLineSwitches;
    std::vector<std::string> d_sideLoadedFonts;
    ResourceLoader* d_inProcessResourceLoader;
    std::string d_dictionaryPath;
    std::string d_hostChannel;
    NativeFont d_tooltipFont;
    NativeColor d_activeTextSearchHighlightColor;
    NativeColor d_inactiveTextSearchHighlightColor;
    NativeColor d_activeTextSearchColor;
    std::string d_headerFooterHTMLContent;
    _invalid_parameter_handler d_invalidParameterHandler;
    _purecall_handler d_purecallHandler;
    bool d_printBackgroundGraphics;
    bool d_inProcessRendererEnabled;
    bool d_useDefaultPrintSettings;
    std::string d_subProcessModule;
    bool d_inProcessResizeOptimizationDisabled;
    std::string d_profileDirectory;
    bool d_isIsolatedProfile;
    ToolkitDelegate* d_delegate;



    // patch section: embedder ipc
    bool d_browserV8Enabled;


    // patch section: renderer ui



    ToolkitCreateParams::LogThrottleType d_logThrottleType {ToolkitCreateParams::LogThrottleType::kWarningThrottle};

    ToolkitCreateParamsImpl();
};

                        // ------------------------------
                        // struct ToolkitCreateParamsImpl
                        // ------------------------------

ToolkitCreateParamsImpl::ToolkitCreateParamsImpl()
    : d_threadMode(ThreadMode::ORIGINAL)
    , d_logMessageHandler(0)
    , d_consoleLogMessageHandler(0)
    , d_winProcExceptionFilter(0)
    , d_channelErrorHandler(0)
    , d_maxSocketsPerProxy(-1000)
    , d_inProcessResourceLoader(0)
    , d_tooltipFont(0)
    , d_activeTextSearchHighlightColor(RGB(255, 150, 50))  // Orange
    , d_inactiveTextSearchHighlightColor(RGB(255, 255, 0))  // Yellow
    , d_activeTextSearchColor(RGB(0,0,0)) // Black
    , d_invalidParameterHandler(0)
    , d_purecallHandler(0)
    , d_printBackgroundGraphics(false)
    , d_inProcessRendererEnabled(true)
    , d_useDefaultPrintSettings(false)
    , d_inProcessResizeOptimizationDisabled(false)
    , d_isIsolatedProfile(true)
    , d_delegate(nullptr)



    // patch section: embedder ipc
    , d_browserV8Enabled(false)


    // patch section: renderer ui



{
}


                        // -------------------------
                        // class ToolkitCreateParams
                        // -------------------------

ToolkitCreateParams::ToolkitCreateParams()
    : d_impl(new ToolkitCreateParamsImpl())
{
}

ToolkitCreateParams::ToolkitCreateParams(const ToolkitCreateParams& other)
    : d_impl(new ToolkitCreateParamsImpl(*other.d_impl))
{
}

ToolkitCreateParams::~ToolkitCreateParams()
{
    delete d_impl;
}

ToolkitCreateParams& ToolkitCreateParams::operator=(
    const ToolkitCreateParams& rhs)
{
    if (this != &rhs) {
        *d_impl = *rhs.d_impl;
    }
    return *this;
}

void ToolkitCreateParams::setThreadMode(ThreadMode mode)
{
    d_impl->d_threadMode = mode;
}

void ToolkitCreateParams::enableDefaultPrintSettings()
{
    d_impl->d_useDefaultPrintSettings = true;
}

void ToolkitCreateParams::setLogMessageHandler(LogMessageHandler handler)
{
    d_impl->d_logMessageHandler = handler;
}

void ToolkitCreateParams::setConsoleLogMessageHandler(ConsoleLogMessageHandler handler)
{
    d_impl->d_consoleLogMessageHandler = handler;
}

void ToolkitCreateParams::setWinProcExceptionFilter(WinProcExceptionFilter filter)
{
    d_impl->d_winProcExceptionFilter = filter;
}

void ToolkitCreateParams::setChannelErrorHandler(ChannelErrorHandler handler)
{
    d_impl->d_channelErrorHandler = handler;
}

void ToolkitCreateParams::disableInProcessRenderer()
{
    d_impl->d_inProcessRendererEnabled = false;
}

void ToolkitCreateParams::setMaxSocketsPerProxy(int count)
{
    DCHECK(1 <= count);
    DCHECK(99 >= count);
    d_impl->d_maxSocketsPerProxy = count;
}

void ToolkitCreateParams::appendCommandLineSwitch(const StringRef& switchString)
{
    d_impl->d_commandLineSwitches.push_back(std::string());
    d_impl->d_commandLineSwitches.back().assign(switchString.data(),
                                                switchString.length());
}

void ToolkitCreateParams::appendSideLoadedFontInProcess(const StringRef& fontFile)
{
    d_impl->d_sideLoadedFonts.push_back(std::string());
    d_impl->d_sideLoadedFonts.back().assign(fontFile.data(),
                                            fontFile.length());
}

void ToolkitCreateParams::setInProcessResourceLoader(
    ResourceLoader* loader)
{
    DCHECK(loader);
    d_impl->d_inProcessResourceLoader = loader;
}

void ToolkitCreateParams::setDictionaryPath(const StringRef& path)
{
    d_impl->d_dictionaryPath.assign(path.data(), path.length());
}

void ToolkitCreateParams::setInvalidParameterHandler(_invalid_parameter_handler handler)
{
    d_impl->d_invalidParameterHandler = handler;
}

void ToolkitCreateParams::setPurecallHandler(_purecall_handler handler)
{
    d_impl->d_purecallHandler = handler;
}

void ToolkitCreateParams::setHostChannel(const StringRef& channelInfoString)
{
    CHECK(channelInfoString.isEmpty() || isValidHostChannelVersion(channelInfoString));
    d_impl->d_hostChannel.assign(
        channelInfoString.data(),
        channelInfoString.length());
}

void ToolkitCreateParams::setDelegate(ToolkitDelegate* delegate)
{
    d_impl->d_delegate = delegate;
}

// static
bool ToolkitCreateParams::isValidHostChannelVersion(const StringRef& channelInfoString)
{
    ChannelInfo channelInfo;
    return channelInfo.deserialize(std::string(channelInfoString.data(),
                                               channelInfoString.size()));
}

void ToolkitCreateParams::setTooltipStyle(NativeFont font)
{
    d_impl->d_tooltipFont = font;
}

void ToolkitCreateParams::setActiveTextSearchHighlightColor(NativeColor color)
{
    d_impl->d_activeTextSearchHighlightColor = color;
}

void ToolkitCreateParams::setInactiveTextSearchHighlightColor(NativeColor color)
{
    d_impl->d_inactiveTextSearchHighlightColor = color;
}

void ToolkitCreateParams::setActiveTextSearchColor(NativeColor color)
{
    d_impl->d_activeTextSearchColor = color;
}

void ToolkitCreateParams::setHeaderFooterHTML(const StringRef& htmlContent)
{
    d_impl->d_headerFooterHTMLContent.assign(htmlContent.data(),
                                             htmlContent.length());
}

void ToolkitCreateParams::enablePrintBackgroundGraphics()
{
    d_impl->d_printBackgroundGraphics = true;
}

void ToolkitCreateParams::setSubProcessModule(const StringRef& moduleName)
{
    d_impl->d_subProcessModule.assign(moduleName.data(),
                                      moduleName.length());
}

void ToolkitCreateParams::disableInProcessResizeOptimization()
{
    d_impl->d_inProcessResizeOptimizationDisabled = true;
}

void ToolkitCreateParams::setProfileDirectory(const StringRef& profileDir)
{
    d_impl->d_profileDirectory = std::string(profileDir.data(),
                                             profileDir.size());
}

void ToolkitCreateParams::disableIsolatedProfile()
{
    d_impl->d_isIsolatedProfile = false;
}



// patch section: embedder ipc
void ToolkitCreateParams::setBrowserV8Enabled(bool browserV8Enabled)
{
    d_impl->d_browserV8Enabled = browserV8Enabled;
}


// patch section: renderer ui



void ToolkitCreateParams::setLogThrottleType(LogThrottleType throttleType)
{
    d_impl->d_logThrottleType = throttleType;
}

ThreadMode ToolkitCreateParams::threadMode() const
{
    return d_impl->d_threadMode;
}

bool ToolkitCreateParams::useDefaultPrintSettings() const
{
    return d_impl->d_useDefaultPrintSettings;
}

ToolkitCreateParams::LogMessageHandler ToolkitCreateParams::logMessageHandler() const
{
    return d_impl->d_logMessageHandler;
}

ToolkitCreateParams::ConsoleLogMessageHandler ToolkitCreateParams::consoleLogMessageHandler() const
{
    return d_impl->d_consoleLogMessageHandler;
}

ToolkitCreateParams::WinProcExceptionFilter ToolkitCreateParams::winProcExceptionFilter() const
{
    return d_impl->d_winProcExceptionFilter;
}

ToolkitCreateParams::ChannelErrorHandler ToolkitCreateParams::channelErrorHandler() const
{
    return d_impl->d_channelErrorHandler;
}

bool ToolkitCreateParams::isInProcessRendererEnabled() const
{
    return d_impl->d_inProcessRendererEnabled;
}

bool ToolkitCreateParams::isMaxSocketsPerProxySet() const
{
    return -1000 != d_impl->d_maxSocketsPerProxy;
}

int ToolkitCreateParams::maxSocketsPerProxy() const
{
    DCHECK(isMaxSocketsPerProxySet());
    return d_impl->d_maxSocketsPerProxy;
}

size_t ToolkitCreateParams::numCommandLineSwitches() const
{
    return d_impl->d_commandLineSwitches.size();
}

StringRef ToolkitCreateParams::commandLineSwitchAt(size_t index) const
{
    DCHECK(index < d_impl->d_commandLineSwitches.size());
    return d_impl->d_commandLineSwitches[index];
}

size_t ToolkitCreateParams::numSideLoadedFonts() const
{
    return d_impl->d_sideLoadedFonts.size();
}

StringRef ToolkitCreateParams::sideLoadedFontAt(size_t index) const
{
    DCHECK(index < d_impl->d_sideLoadedFonts.size());
    return d_impl->d_sideLoadedFonts[index];
}

ResourceLoader* ToolkitCreateParams::inProcessResourceLoader() const
{
    return d_impl->d_inProcessResourceLoader;
}

StringRef ToolkitCreateParams::dictionaryPath() const
{
    return d_impl->d_dictionaryPath;
}

_invalid_parameter_handler ToolkitCreateParams::invalidParameterHandler() const
{
    return d_impl->d_invalidParameterHandler;
}

_purecall_handler ToolkitCreateParams::purecallHandler() const
{
    return d_impl->d_purecallHandler;
}

StringRef ToolkitCreateParams::hostChannel() const
{
    return d_impl->d_hostChannel;
}

ToolkitDelegate* ToolkitCreateParams::delegate() const
{
    return d_impl->d_delegate;
}

NativeFont ToolkitCreateParams::tooltipFont() const
{
    return d_impl->d_tooltipFont;
}

NativeColor ToolkitCreateParams::activeTextSearchHighlightColor() const
{
    return d_impl->d_activeTextSearchHighlightColor;
}

NativeColor ToolkitCreateParams::inactiveTextSearchHighlightColor() const
{
    return d_impl->d_inactiveTextSearchHighlightColor;
}

NativeColor ToolkitCreateParams::activeTextSearchColor() const
{
    return d_impl->d_activeTextSearchColor;
}

StringRef ToolkitCreateParams::headerFooterHTMLContent() const
{
    return d_impl->d_headerFooterHTMLContent;
}

bool  ToolkitCreateParams::isPrintBackgroundGraphicsEnabled() const
{
    return d_impl->d_printBackgroundGraphics;
}

StringRef ToolkitCreateParams::subProcessModule() const
{
    return d_impl->d_subProcessModule;
}

bool ToolkitCreateParams::isInProcessResizeOptimizationDisabled() const
{
    return d_impl->d_inProcessResizeOptimizationDisabled;
}

StringRef ToolkitCreateParams::profileDirectory() const
{
    return d_impl->d_profileDirectory;
}

bool ToolkitCreateParams::isIsolatedProfile() const
{
    return d_impl->d_profileDirectory.empty() && d_impl->d_isIsolatedProfile;
}



// patch section: embedder ipc
bool ToolkitCreateParams::browserV8Enabled() const
{
    return d_impl->d_browserV8Enabled;
}


// patch section: renderer ui



ToolkitCreateParams::LogThrottleType ToolkitCreateParams::logThrottleType() const
{
    return d_impl->d_logThrottleType;
}

}  // close namespace blpwtk2

// vim: ts=4 et

