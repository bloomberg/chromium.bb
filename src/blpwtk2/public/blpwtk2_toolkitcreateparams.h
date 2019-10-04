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

#ifndef INCLUDED_BLPWTK2_TOOLKITCREATEPARAMS_H
#define INCLUDED_BLPWTK2_TOOLKITCREATEPARAMS_H

#include <blpwtk2_config.h>

#include <blpwtk2_threadmode.h>

#include <stdlib.h>  // for _invalid_parameter_handler and _purecall_handler

namespace blpwtk2 {

class ResourceLoader;
class StringRef;
class WebViewHostObserver;
struct ToolkitCreateParamsImpl;

                        // =====================
                        // class ToolkitDelegate
                        // =====================

// This can be implemented by the embedder to provide embedder-side
// functionality at the toolkit level.
class BLPWTK2_EXPORT ToolkitDelegate {
  public:



    // patch section: devtools integration



};

                        // =========================
                        // class ToolkitCreateParams
                        // =========================

// This class contains parameters that are passed to blpwtk2 when initializing
// the toolkit.
class ToolkitCreateParams
{
    ToolkitCreateParamsImpl *d_impl;

  public:
    enum LogMessageSeverity {
        kSeverityVerbose = 0,
        kSeverityInfo = 1,
        kSeverityWarning = 2,
        kSeverityError = 3,
        kSeverityFatal = 4,
    };

    enum class LogThrottleType {
        kNoThrottle = 0,
        kWarningThrottle, /* Throttle warning, error, fatal messages */
    };

    // The callback function that will be invoked whenever a log message
    // happens.
    typedef void(*LogMessageHandler)(LogMessageSeverity severity,
                                     const char* file,
                                     int line,
                                     const char* message);

    // The callback function that will be invoked whenever a log message
    // is printed to the Web Console
    typedef void(*ConsoleLogMessageHandler)(LogMessageSeverity severity,
                                            const StringRef& file,
                                            unsigned line,
                                            unsigned column,
                                            const StringRef& message,
                                            const StringRef& stack_trace);

    typedef int(__cdecl *WinProcExceptionFilter)(EXCEPTION_POINTERS* info);
        // The callback function that will be invoked whenever SEH exceptions
        // are caught in win procs.

    typedef void(*ChannelErrorHandler)(int reserved);
        // The callback function that will be invoked whenever a channel error
        // happens.

    BLPWTK2_EXPORT ToolkitCreateParams();
    BLPWTK2_EXPORT ToolkitCreateParams(const ToolkitCreateParams&);
    BLPWTK2_EXPORT ~ToolkitCreateParams();
    BLPWTK2_EXPORT ToolkitCreateParams& operator=(const ToolkitCreateParams&);

    BLPWTK2_EXPORT void setThreadMode(ThreadMode mode);
        // By default, blpwtk2 uses 'ThreadMode::ORIGINAL'.  Use this method
        // to change the thread mode.

    BLPWTK2_EXPORT void enableDefaultPrintSettings();
        // By default, initiating a document print will cause the browser to
        // open a print dialog to ask for the target printing device.
        // Calling this will disable the print dialog and use the default
        // printing device on the system.

    BLPWTK2_EXPORT void setLogMessageHandler(LogMessageHandler handler);
        // By default, log messages go to a "blpwtk2.log" file and to debug output.
        // Use this method to install a custom log message handler instead.  Note
        // that the handler callback can be invoked from any thread.

    BLPWTK2_EXPORT void setConsoleLogMessageHandler(ConsoleLogMessageHandler handler);
        // Use this method to install a custom log message handler for the
        // Web Console. This handler is only used for in-process renderers.

    BLPWTK2_EXPORT void setWinProcExceptionFilter(WinProcExceptionFilter filter);
        // Use this method to install a custom filter that will be invoked
        // whenever SEH exceptions happen inside win procs.

    BLPWTK2_EXPORT void setChannelErrorHandler(ChannelErrorHandler handler);
        // Use this method to install a channel error handler.

    BLPWTK2_EXPORT void disableInProcessRenderer();
        // By default, the in-process renderer is enabled.  This uses some
        // additional resources, even if in-process WebViews are not created.
        // Call this method to disable the in-process renderer completely.
        // It is then undefined behavior to create WebViews using
        // 'IN_PROCESS_RENDERER'.

    BLPWTK2_EXPORT void setMaxSocketsPerProxy(int count);
        // Set the maximum number of sockets per proxy, up to a maximum of 99.
        // Note that each Profile maintains its own pool of connections, so
        // this is actually the maximum number of sockets per proxy
        // *per profile*.  The behavior is undefined if 'count' is
        // less than 1, or more than 99.

    BLPWTK2_EXPORT void appendCommandLineSwitch(const StringRef& switchString);
        // Add the specified 'switchString' to the list of command-line
        // switches.  A list of switches can be found at:
        // http://peter.sh/experiments/chromium-command-line-switches/
        // Note, however, that blpwtk2 is based on a different version of
        // chromium, so it may not support *all* the switches mentioned on
        // that page.

    BLPWTK2_EXPORT void setInProcessResourceLoader(ResourceLoader*);
        // Install a custom ResourceLoader.  Note that this is only valid when
        // using the 'RENDERER_MAIN' thread-mode, and will only be used for
        // in-process renderers.

    BLPWTK2_EXPORT void setDictionaryPath(const StringRef& path);
        // By default, blpwtk2 will look for .bdic files in the application's
        // working directory.  Use this method to change the path where
        // blpwtk2 would look for the .bdic files.  Note that this is only
        // used if spellchecking is enabled in one of the Profile objects.

    BLPWTK2_EXPORT void setInvalidParameterHandler(_invalid_parameter_handler handler);
        // Set the CRT's invalid parameter handler.  If this is not set, then
        // a default handler will be installed, which sets a breakpoint and
        // exits the application.

    BLPWTK2_EXPORT void setPurecallHandler(_purecall_handler handler);
        // Set the CRT's purecall handler.  If this is not set, then a default
        // handler will be installed, which sets a breakpoint and exits the
        // application.

    BLPWTK2_EXPORT void setHostChannel(const StringRef& channelInfoString);
        // By default, blpwtk2 will allocate new browser process resources for
        // each blpwtk2 process.  However, the 'Toolkit::createHostChannel'
        // method can be used to setup an IPC channel that this process can
        // use to share the same browser process resources.  Use this method
        // to set the channel-info that will be used to connect this process
        // to the browser process.  This channel-info must have been obtained
        // from 'Toolkit::createHostChannel' in another process using the same
        // version of blpwtk2 (i.e. 'isValidHostChannelVersion' must return
        // true).

    BLPWTK2_EXPORT void setDelegate(ToolkitDelegate* delegate);
        // Set a new toolkit delegate. From this point on, all callbacks will
        // be sent to the new delegate.

    BLPWTK2_EXPORT static bool isValidHostChannelVersion(
        const StringRef& channelInfoString);
        // Return true if the specified 'channelInfoString' was obtained from a
        // process using the same version of blpwtk2, and false otherwise.
        // It is undefined behavior to use a channel-info obtained from a
        // different version of blpwtk2.

    BLPWTK2_EXPORT void setTooltipStyle(NativeFont font);

    BLPWTK2_EXPORT void setActiveTextSearchHighlightColor(NativeColor color);
        // Set the highlight color of the active item in text searches.
        // The default color is orange.  Note that this only works for
        // in-process renderers currently.

    BLPWTK2_EXPORT void setInactiveTextSearchHighlightColor(NativeColor color);
        // Set the highlight color for inactive items in text searches.
        // The default color is yellow.  Note that this only works for
        // in-process renderers currently.

    BLPWTK2_EXPORT void setActiveTextSearchColor(NativeColor color);

    BLPWTK2_EXPORT void setHeaderFooterHTML(const StringRef& htmlContent);
        // This method is used to set the HTML file used to format header and
        // footer of printed pages.

    BLPWTK2_EXPORT void enablePrintBackgroundGraphics();
        // This method enables printing background graphics.

    BLPWTK2_EXPORT void setSubProcessModule(const StringRef& moduleName);
        // Set the name of the module that subprocesses will load.  By default,
        // subprocesses load the blpwtk2 dll.  The module specified here must
        // export the SubProcessMain symbol.

    BLPWTK2_EXPORT void disableInProcessResizeOptimization();
        // Disables an optimization used when resizing a WebView from an
        // in-process renderer (the optimization can cause undesirable
        // rendering artifacts when the WebView is hosted in a deeply-nested
        // window hierarchy).

    BLPWTK2_EXPORT void setProfileDirectory(const StringRef& profileDir);
        // Binds the toolkit to a specific browser context.  The browser
        // groups RenderProcesses based on the profile directory and each
        // group shares the same browser context.  The exception to this rule
        // is when 'profileDir' is empty, in which case an incognito browser
        // context is created that is not shared by any other RenderProcess.
    
    BLPWTK2_EXPORT void disableIsolatedProfile();
        // By default, an empty profile directory will cause the browser to
        // create an isolated incognito context.  Calling this function will
        // override the default behavior and so the browser will share a
        // common incognito context across multiple RenderProcess instances,
        // assuming their toolkits were initialized with an empty profile
        // directory.



    // patch section: embedder ipc


    // patch section: renderer ui



    BLPWTK2_EXPORT void setLogThrottleType(LogThrottleType throttleType);

    // ACCESSORS
    ThreadMode threadMode() const;
    bool useDefaultPrintSettings() const;
    LogMessageHandler logMessageHandler() const;
    ConsoleLogMessageHandler consoleLogMessageHandler() const;
    WinProcExceptionFilter winProcExceptionFilter() const;
    ChannelErrorHandler channelErrorHandler() const;
    bool isInProcessRendererEnabled() const;
    bool isMaxSocketsPerProxySet() const;
    int maxSocketsPerProxy() const;
    size_t numCommandLineSwitches() const;
    StringRef commandLineSwitchAt(size_t index) const;
    ResourceLoader* inProcessResourceLoader() const;
    StringRef dictionaryPath() const;
    _invalid_parameter_handler invalidParameterHandler() const;
    _purecall_handler purecallHandler() const;
    StringRef hostChannel() const;
    ToolkitDelegate* delegate() const;
    NativeFont tooltipFont() const;
    NativeColor activeTextSearchHighlightColor() const;
    NativeColor inactiveTextSearchHighlightColor() const;
    NativeColor activeTextSearchColor() const;
    StringRef headerFooterHTMLContent() const;
    bool isPrintBackgroundGraphicsEnabled() const;
    StringRef subProcessModule() const;
    bool isInProcessResizeOptimizationDisabled() const;
    StringRef profileDirectory() const;
    bool isIsolatedProfile() const;
    LogThrottleType logThrottleType() const;


    // patch section: embedder ipc


    // patch section: renderer ui



};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_TOOLKITCREATEPARAMS_H

// vim: ts=4 et

