#
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
    'includes': [
        'WinPrecompile.gypi',
        '../../wtf/wtf.gypi',
        '../../core/core.gypi',
        'WebKit.gypi',
        '../../core/features.gypi',
    ],
    'targets': [
        {
            'target_name': 'webkit',
            'type': '<(component)',
            'variables': { 'enable_wexit_time_destructors': 1, },
            'dependencies': [
                '../../core/core.gyp:webcore',
                '../../modules/modules.gyp:modules',
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(angle_path)/src/build_angle.gyp:translator_glsl',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                'blink_common',
            ],
            'export_dependent_settings': [
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
            ],
            'include_dirs': [
                '../../../public/web',
                'src',
                '<(angle_path)/include',
                '<(DEPTH)/third_party/skia/include/utils',
            ],
            'defines': [
                'WEBKIT_IMPLEMENTATION=1',
                'INSIDE_WEBKIT',
            ],
            'sources': [
                '<@(webcore_platform_support_files)',
                'src/ApplicationCacheHost.cpp',
                'src/ApplicationCacheHostInternal.h',
                'src/AssertMatchingEnums.cpp',
                'src/AssociatedURLLoader.cpp',
                'src/AssociatedURLLoader.h',
                'src/AsyncFileSystemChromium.cpp',
                'src/AsyncFileSystemChromium.h',
                'src/AsyncFileWriterChromium.cpp',
                'src/AsyncFileWriterChromium.h',
                'src/AutofillPopupMenuClient.cpp',
                'src/AutofillPopupMenuClient.h',
                'src/BackForwardClientImpl.cpp',
                'src/BackForwardClientImpl.h',
                'src/DateTimeChooserImpl.cpp',
                'src/DateTimeChooserImpl.h',
                'src/ChromeClientImpl.cpp',
                'src/ChromeClientImpl.h',
                'src/ColorChooserPopupUIController.cpp',
                'src/ColorChooserPopupUIController.h',
                'src/ColorChooserUIController.cpp',
                'src/ColorChooserUIController.h',
                'src/CompositionUnderlineBuilder.h',
                'src/CompositionUnderlineVectorBuilder.cpp',
                'src/CompositionUnderlineVectorBuilder.h',
                'src/ContextFeaturesClientImpl.cpp',
                'src/ContextFeaturesClientImpl.h',
                'src/ContextMenuClientImpl.cpp',
                'src/ContextMenuClientImpl.h',
                'src/DatabaseObserver.cpp',
                'src/DeviceOrientationClientProxy.cpp',
                'src/DeviceOrientationClientProxy.h',
                'src/DOMUtilitiesPrivate.cpp',
                'src/DOMUtilitiesPrivate.h',
                'src/DragClientImpl.cpp',
                'src/DragClientImpl.h',
                'src/DragScrollTimer.cpp',
                'src/DragScrollTimer.h',
                'src/EditorClientImpl.cpp',
                'src/EditorClientImpl.h',
                'src/EventListenerWrapper.cpp',
                'src/EventListenerWrapper.h',
                'src/ExternalDateTimeChooser.cpp',
                'src/ExternalDateTimeChooser.h',
                'src/ExternalPopupMenu.cpp',
                'src/ExternalPopupMenu.h',
                'src/FindInPageCoordinates.cpp',
                'src/FindInPageCoordinates.h',
                'src/FrameLoaderClientImpl.cpp',
                'src/FrameLoaderClientImpl.h',
                'src/GeolocationClientProxy.cpp',
                'src/GeolocationClientProxy.h',
                'src/GraphicsLayerFactoryChromium.cpp',
                'src/GraphicsLayerFactoryChromium.h',
                'src/gtk/WebInputEventFactory.cpp',
                'src/WebHelperPluginImpl.cpp',
                'src/WebHelperPluginImpl.h',
                'src/IDBCursorBackendProxy.cpp',
                'src/IDBCursorBackendProxy.h',
                'src/IDBDatabaseBackendProxy.cpp',
                'src/IDBDatabaseBackendProxy.h',
                'src/IDBFactoryBackendProxy.cpp',
                'src/IDBFactoryBackendProxy.h',
                'src/InbandTextTrackPrivateImpl.cpp',
                'src/InbandTextTrackPrivateImpl.h',
                'src/InspectorClientImpl.cpp',
                'src/InspectorClientImpl.h',
                'src/InspectorFrontendClientImpl.cpp',
                'src/InspectorFrontendClientImpl.h',
                'src/LinkHighlight.cpp',
                'src/LinkHighlight.h',
                'src/PrerendererClientImpl.h',
                'src/PrerendererClientImpl.cpp',
                'src/android/WebInputEventFactory.cpp',
                'src/default/WebRenderTheme.cpp',
                'src/linux/WebFontInfo.cpp',
                'src/linux/WebFontRendering.cpp',
                'src/linux/WebFontRenderStyle.cpp',
                'src/mac/WebInputEventFactory.mm',
                'src/mac/WebSubstringUtil.mm',
                'src/LocalFileSystemChromium.cpp',
                'src/MediaSourcePrivateImpl.cpp',
                'src/MediaSourcePrivateImpl.h',
                'src/NotificationPresenterImpl.h',
                'src/NotificationPresenterImpl.cpp',
                'src/painting/ContinuousPainter.h',
                'src/painting/ContinuousPainter.cpp',
                'src/painting/PaintAggregator.h',
                'src/painting/PaintAggregator.cpp',
                'src/PageOverlay.cpp',
                'src/PageOverlay.h',
                'src/PageOverlayList.cpp',
                'src/PageOverlayList.h',
                'src/PageWidgetDelegate.cpp',
                'src/PageWidgetDelegate.h',
                'src/PinchViewports.cpp',
                'src/PinchViewports.h',
                'src/PopupContainer.cpp',
                'src/PopupContainer.h',
                'src/PopupMenuChromium.cpp',
                'src/PopupMenuChromium.h',
                'src/PopupListBox.cpp',
                'src/PopupListBox.h',
                'src/ScrollbarGroup.cpp',
                'src/ScrollbarGroup.h',
                'src/SharedWorkerRepository.cpp',
                'src/SourceBufferPrivateImpl.cpp',
                'src/SourceBufferPrivateImpl.h',
                'src/SpeechInputClientImpl.cpp',
                'src/SpeechInputClientImpl.h',
                'src/SpeechRecognitionClientProxy.cpp',
                'src/SpeechRecognitionClientProxy.h',
                'src/StorageAreaProxy.cpp',
                'src/StorageAreaProxy.h',
                'src/StorageQuotaChromium.cpp',
                'src/StorageNamespaceProxy.cpp',
                'src/StorageNamespaceProxy.h',
                'src/TextFieldDecoratorImpl.h',
                'src/TextFieldDecoratorImpl.cpp',
                'src/UserMediaClientImpl.h',
                'src/UserMediaClientImpl.cpp',
                'src/ValidationMessageClientImpl.cpp',
                'src/ValidationMessageClientImpl.h',
                'src/ViewportAnchor.cpp',
                'src/ViewportAnchor.h',
                'src/WebTextCheckingCompletionImpl.h',
                'src/WebTextCheckingCompletionImpl.cpp',
                'src/WebTextCheckingResult.cpp',
                'src/WebAccessibilityObject.cpp',
                'src/WebArrayBuffer.cpp',
                'src/WebArrayBufferView.cpp',
                'src/WebBindings.cpp',
                'src/WebBlob.cpp',
                'src/WebBlobData.cpp',
                'src/WebCache.cpp',
                'src/WebCachedURLRequest.cpp',
                'src/WebColorName.cpp',
                'src/WebCrossOriginPreflightResultCache.cpp',
                'src/WebDOMActivityLogger.cpp',
                'src/WebDOMCustomEvent.cpp',
                'src/WebDOMEvent.cpp',
                'src/WebDOMEventListener.cpp',
                'src/WebDOMEventListenerPrivate.cpp',
                'src/WebDOMEventListenerPrivate.h',
                'src/WebDOMMessageEvent.cpp',
                'src/WebDOMMouseEvent.cpp',
                'src/WebDOMMutationEvent.cpp',
                'src/WebDOMProgressEvent.cpp',
                'src/WebDOMResourceProgressEvent.cpp',
                'src/WebDatabase.cpp',
                'src/WebDataSourceImpl.cpp',
                'src/WebDataSourceImpl.h',
                'src/WebDevToolsAgentImpl.cpp',
                'src/WebDevToolsAgentImpl.h',
                'src/WebDevToolsFrontendImpl.cpp',
                'src/WebDevToolsFrontendImpl.h',
                'src/WebDeviceOrientation.cpp',
                'src/WebDeviceOrientationClientMock.cpp',
                'src/WebDeviceOrientationController.cpp',
                'src/WebDocument.cpp',
                'src/WebDocumentType.cpp',
                'src/WebDragData.cpp',
                'src/WebElement.cpp',
                'src/WebEntities.cpp',
                'src/WebEntities.h',
                'src/WebFileChooserCompletionImpl.cpp',
                'src/WebFileChooserCompletionImpl.h',
                'src/WebFileSystemCallbacksImpl.cpp',
                'src/WebFileSystemCallbacksImpl.h',
                'src/WebFontCache.cpp',
                'src/WebFontDescription.cpp',
                'src/WebFontImpl.cpp',
                'src/WebFontImpl.h',
                'src/WebFormControlElement.cpp',
                'src/WebFormElement.cpp',
                'src/WebFrameImpl.cpp',
                'src/WebFrameImpl.h',
                'src/WebGeolocationController.cpp',
                'src/WebGeolocationClientMock.cpp',
                'src/WebGeolocationError.cpp',
                'src/WebGeolocationPermissionRequest.cpp',
                'src/WebGeolocationPermissionRequestManager.cpp',
                'src/WebGeolocationPosition.cpp',
                'src/WebGlyphCache.cpp',
                'src/WebHistoryItem.cpp',
                'src/WebHitTestResult.cpp',
                'src/WebIconLoadingCompletionImpl.cpp',
                'src/WebIconLoadingCompletionImpl.h',
                'src/WebIDBCallbacksImpl.cpp',
                'src/WebIDBCallbacksImpl.h',
                'src/WebIDBDatabaseCallbacksImpl.cpp',
                'src/WebIDBDatabaseCallbacksImpl.h',
                'src/WebIDBDatabaseError.cpp',
                'src/WebIDBKey.cpp',
                'src/WebIDBKeyPath.cpp',
                'src/WebIDBKeyRange.cpp',
                'src/WebIDBMetadata.cpp',
                'src/WebImageCache.cpp',
                'src/WebImageDecoder.cpp',
                'src/WebImageSkia.cpp',
                'src/WebInputElement.cpp',
                'src/WebInputEvent.cpp',
                'src/WebInputEventConversion.cpp',
                'src/WebInputEventConversion.h',
                'src/WebKit.cpp',
                'src/WebLabelElement.cpp',
                'src/WebMediaPlayerClientImpl.cpp',
                'src/WebMediaPlayerClientImpl.h',
                'src/WebMediaSourceImpl.cpp',
                'src/WebMediaSourceImpl.h',
                'src/WebMediaStreamRegistry.cpp',
                'src/WebNetworkStateNotifier.cpp',
                'src/WebNode.cpp',
                'src/WebNodeCollection.cpp',
                'src/WebNodeList.cpp',
                'src/WebNotification.cpp',
                'src/WebOptionElement.cpp',
                'src/WebPagePopupImpl.cpp',
                'src/WebPagePopupImpl.h',
                'src/WebPageSerializer.cpp',
                'src/WebPageSerializerImpl.cpp',
                'src/WebPageSerializerImpl.h',
                'src/WebPasswordFormData.cpp',
                'src/WebPasswordFormUtils.cpp',
                'src/WebPasswordFormUtils.h',
                'src/WebPerformance.cpp',
                'src/WebPluginContainerImpl.cpp',
                'src/WebPluginContainerImpl.h',
                'src/WebPluginDocument.cpp',
                'src/WebPluginLoadObserver.cpp',
                'src/WebPluginLoadObserver.h',
                'src/WebPluginScrollbarImpl.cpp',
                'src/WebPluginScrollbarImpl.h',
                'src/WebPopupMenuImpl.cpp',
                'src/WebPopupMenuImpl.h',
                'src/WebRange.cpp',
                'src/WebRuntimeFeatures.cpp',
                'src/WebScopedMicrotaskSuppression.cpp',
                'src/WebScopedUserGesture.cpp',
                'src/WebScriptController.cpp',
                'src/WebScrollbarThemePainter.cpp',
                'src/WebSearchableFormData.cpp',
                'src/WebSecurityOrigin.cpp',
                'src/WebSecurityPolicy.cpp',
                'src/WebSelectElement.cpp',
                'src/WebSerializedScriptValue.cpp',
                'src/WebSettingsImpl.cpp',
                'src/WebSettingsImpl.h',
                'src/WebSharedWorkerImpl.cpp',
                'src/WebSharedWorkerImpl.h',
                'src/WebSocket.cpp',
                'src/WebSocketImpl.cpp',
                'src/WebSocketImpl.h',
                'src/WebSpeechGrammar.cpp',
                'src/WebSpeechInputResult.cpp',
                'src/WebSpeechRecognitionHandle.cpp',
                'src/WebSpeechRecognitionResult.cpp',
                'src/WebStorageEventDispatcherImpl.cpp',
                'src/WebStorageQuotaCallbacksImpl.cpp',
                'src/WebStorageQuotaCallbacksImpl.h',
                'src/WebSurroundingText.cpp',
                'src/WebTextInputInfo.cpp',
                'src/WebTextRun.cpp',
                'src/WebURLLoadTiming.cpp',
                'src/WebScopedUserGesture.cpp',
                'src/WebTextFieldDecoratorClient.cpp',
                'src/WebUserGestureIndicator.cpp',
                'src/WebUserGestureToken.cpp',
                'src/WebUserMediaRequest.cpp',
                'src/WebViewBenchmarkSupportImpl.cpp',
                'src/WebViewBenchmarkSupportImpl.h',
                'src/WebViewImpl.cpp',
                'src/WebViewImpl.h',
                'src/WebWorkerBase.cpp',
                'src/WebWorkerBase.h',
                'src/WebWorkerClientImpl.cpp',
                'src/WebWorkerClientImpl.h',
                'src/WebWorkerInfo.cpp',
                'src/WebWorkerRunLoop.cpp',
                'src/WorkerAllowMainThreadBridgeBase.cpp',
                'src/WorkerAllowMainThreadBridgeBase.h',
                'src/WorkerAsyncFileSystemChromium.cpp',
                'src/WorkerAsyncFileSystemChromium.h',
                'src/WorkerAsyncFileWriterChromium.cpp',
                'src/WorkerAsyncFileWriterChromium.h',
                'src/WorkerFileSystemCallbacksBridge.cpp',
                'src/WorkerFileSystemCallbacksBridge.h',
                'src/WorkerFileWriterCallbacksBridge.cpp',
                'src/WorkerFileWriterCallbacksBridge.h',
                'src/WorkerStorageQuotaCallbacksBridge.cpp',
                'src/WorkerStorageQuotaCallbacksBridge.h',
                'src/win/WebInputEventFactory.cpp',
            ],
            'conditions': [
                ['component=="shared_library"', {
                    'dependencies': [
                        '../../core/core.gyp:webcore_derived',
                        '../../core/core.gyp:webcore_test_support',
                        '<(DEPTH)/base/base.gyp:test_support_base',
                        '<(DEPTH)/testing/gmock.gyp:gmock',
                        '<(DEPTH)/testing/gtest.gyp:gtest',
                        '<(DEPTH)/third_party/icu/icu.gyp:*',
                        '<(DEPTH)/third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
                        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
                        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
                        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
                        '<(DEPTH)/third_party/modp_b64/modp_b64.gyp:modp_b64',
                        '<(DEPTH)/third_party/ots/ots.gyp:ots',
                        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                        # We must not add webkit_support here because of cyclic dependency.
                    ],
                    'export_dependent_settings': [
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                    ],
                    'include_dirs': [
                        # WARNING: Do not view this particular case as a precedent for
                        # including WebCore headers in DumpRenderTree project.
                        '../../core/testing/v8', # for WebCoreTestSupport.h, needed to link in window.internals code.
                    ],
                    'sources': [
                        '<@(core_unittest_files)',
                        '<@(webkit_unittest_files)',
                        'src/WebTestingSupport.cpp',
                        'tests/WebUnitTests.cpp',   # Components test runner support.
                    ],
                    'conditions': [
                        ['OS=="win" or OS=="mac"', {
                            'dependencies': [
                                '<(DEPTH)/third_party/nss/nss.gyp:*',
                            ],
                        }],
                        ['clang==1', {
                            # FIXME: It would be nice to enable this in shared builds too,
                            # but the test files have global constructors from the GTEST macro
                            # and we pull in the test files into the webkit target in the
                            # shared build.
                            'cflags!': ['-Wglobal-constructors'],
                            'xcode_settings': {
                              'WARNING_CFLAGS!': ['-Wglobal-constructors'],
                            },
                        }],
                    ],
                    'msvs_settings': {
                      'VCLinkerTool': {
                        'conditions': [
                          ['incremental_chrome_dll==1', {
                            'UseLibraryDependencyInputs': "true",
                          }],
                        ],
                      },
                    },
                }],
                ['OS == "linux"', {
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:fontconfig',
                    ],
                    'include_dirs': [
                        '../../../public/web/linux',
                    ],
                }, {
                    'sources/': [
                        ['exclude', '/linux/'],
                    ],
                }],
                ['use_x11 == 1', {
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:x11',
                    ],
                    'include_dirs': [
                        '../../../public/web/x11',
                    ],
                }, {
                    'sources/': [
                        ['exclude', '/x11/'],
                    ]
                }],
                ['toolkit_uses_gtk == 1', {
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:gtk',
                    ],
                    'include_dirs': [
                        '../../../public/web/gtk',
                    ],
                }, { # else: toolkit_uses_gtk != 1
                    'sources/': [
                        ['exclude', '/gtk/'],
                    ],
                }],
                ['OS=="android"', {
                    'include_dirs': [
                        '../../../public/web/android',
                        '../../../public/web/linux', # We need linux/WebFontRendering.h on Android.
                    ],
                }, { # else: OS!="android"
                    'sources/': [
                        ['exclude', '/android/'],
                    ],
                }],
                ['OS=="mac"', {
                    'include_dirs': [
                        '../../../public/web/mac',
                    ],
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
                            '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                        ],
                    },
                }, { # else: OS!="mac"
                    'sources/': [
                        ['exclude', '/mac/'],
                    ],
                }],
                ['OS=="win"', {
                    'include_dirs': [
                        '../../../public/web/win',
                    ],
                }, { # else: OS!="win"
                    'sources/': [['exclude', '/win/']],
                    'variables': {
                        # FIXME: Turn on warnings on Windows.
                        'chromium_code': 1,
                    }
                }],
                ['use_default_render_theme==1', {
                    'include_dirs': [
                        '../../../public/web/default',
                    ],
                }, { # else use_default_render_theme==0
                    'sources/': [
                        ['exclude', 'src/default/WebRenderTheme.cpp'],
                    ],
                }],
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    '../../../',
                ],
            },
            'target_conditions': [
                ['OS=="android"', {
                    'sources/': [
                        ['include', '^src/linux/WebFontRendering\\.cpp$'],
                        ['include', '^src/linux/WebFontRenderStyle\\.cpp$'],
                    ],
                }],
            ],
        },
        {
            'target_name': 'webkit_test_support',
            'conditions': [
                ['component=="shared_library"', {
                    'type': 'none',
                }, {
                    'type': 'static_library',
                    'dependencies': [
                        '../../wtf/wtf.gyp:wtf',
                        '../../core/core.gyp:webcore_test_support',
                    ],
                    'include_dirs': [
                        '../../../public/web',
                        '../../core/testing/v8', # for WebCoreTestSupport.h, needed to link in window.internals code.
                        '../../../',
                    ],
                    'sources': [
                        'src/WebTestingSupport.cpp',
                    ],
                }],
            ],
        },
        {
            'target_name': 'blink_common',
            'type': '<(component)',
            'variables': { 'enable_wexit_time_destructors': 1 },
            'dependencies': [
                '../../wtf/wtf.gyp:wtf',
                '<(DEPTH)/skia/skia.gyp:skia',
            ],
            'defines': [
                'INSIDE_WEBKIT',
                'BLINK_COMMON_IMPLEMENTATION=1',
            ],
            'include_dirs': [
                '../..',
                '../../..',
            ],
            'sources': [
                '../../core/platform/chromium/support/WebFilterOperation.cpp',
                '../../core/platform/chromium/support/WebFilterOperations.cpp',
                '../../core/platform/chromium/support/WebCString.cpp',
                '../../core/platform/chromium/support/WebString.cpp',
                'src/WebCommon.cpp',
            ],
        },
    ], # targets
    'conditions': [
        ['gcc_version>=46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
        ['OS=="mac"', {
            'targets': [
                {
                    'target_name': 'copy_mesa',
                    'type': 'none',
                    'dependencies': ['<(DEPTH)/third_party/mesa/mesa.gyp:osmesa'],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/DumpRenderTree.app/Contents/MacOS/',
                        'files': ['<(PRODUCT_DIR)/osmesa.so'],
                    }],
                },
            ],
        }],
        ['clang==1', {
            'target_defaults': {
                'cflags': ['-Wglobal-constructors'],
                'xcode_settings': {
                    'WARNING_CFLAGS': ['-Wglobal-constructors'],
                },
            },
        }],
    ], # conditions
}
