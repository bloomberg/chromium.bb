# DOM

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/core/dom/README.md)

This directory contains the implementation of [DOM].

[DOM]: https://dom.spec.whatwg.org/
[DOM Standard]: https://dom.spec.whatwg.org/

Basically, this directory should contain only a file which is related to [DOM Standard].
However, for historical reasons, `core/dom` directory has been used
as if it were *misc* directory. As a result, unfortunately, this directory
contains a lot of files which are not directly related to DOM.

Please don't add unrelated files to this directory any more.  We are trying to
organize the files so that developers wouldn't get confused at seeing this
directory.

See [crbug.com/738794](http://crbug.com/738794) for tracking our efforts.

As of now, the following files might be candidates which can be put in other
appropriate directory, including, but not limited to:

## WebIDL (-> core/???)

- CommonDefinitions

## Frame (core/frame)

- ChildFrameDisconnector
- CompositorWorkerProxyClient
- ContextFeatures
- ContextFeaturesClientImpl
- ContextLifecycleNotifier
- FrameRequestCallback
- FrameRequestCallbackCollection
- ViewportDescription

## Animation  (-> core/animation)

- AnimationWorkletProxyClient
- ScriptedAnimationController

## CSSOM (-> core/css/cssom)

- ClientRect
- ClientRectList

## CSS (-> core/css)

- CSSSelectorWatch
- SelectRuleFeatureSet
- SelectorQuery

## Events (-> core/events)

- SimulatedClickOptions
- Touch
- TouchInit
- TouchList

## Style (-> core/styles)

- DocumentStyleSheetCollection
- ShadowTreeStyleSheetCollection
- StyleChangeReason
- StyleElement (-> core/html?)
- StyleEngine
- StyleEngineContext
- StyleSheetCandidate
- StyleSheetCollection
- TreeScopeStyleSheetCollection
- stylerecalc.md

## accessibility (-> modules/accessibility?)

- AXObjectCache
- AXObjectCacheBase
- AccessibleNode

## typedarray (-> new directory, core/typedarray?)

- ArrayBuffer
- ArrayBufferView
- ArrayBufferViewHelpers
- DOMArrayBuffer
- DOMArrayBufferView
- DOMArrayPiece
- DOMDataView
- DOMSharedArrayBuffer
- DOMTypedArray
- DataView
- FlexibleArrayBufferView
- Float32Array
- Float64Array
- Int16Array
- Int32Array
- Int8Array
- SharedArrayBuffer
- TypedFlexibleArrayBufferView
- Uint16Array
- Uint32Array
- Uint8Array
- Uint8ClampedArray

# URL (-> core/url?)

- URL
- URLSearchParams
- URLUtilsReadOnly

## Parser (-> core/html/parser)

- DecodedDataDocumentParser
- DocumentParser
- DocumentParserClient
- DocumentParserTiming
- RawDataDocumentParser
- ScriptableDocumentParser

## Script (-> core/html)

- FunctionStringCallback
- Script
- ScriptElementBase
- ScriptLoader
- ScriptModuleResolver
- ScriptModuleResolverImpl
- ScriptRunner
- ScriptedIdleTaskController

## HTML Element (-> core/html)

- FirstLetterPseudoElement
- PseudoElement
- PseudoElementData
- VisitedLinkState (HTMLLinkElement)

## fullscreen (-> core/fullscreen or core/html)

- DocumentFullscreen
- ElementFullscreen
- Fullscreen

## [Message Channels](https://html.spec.whatwg.org/#message-channels) (-> core/html)

- AncestorList
- ClassicPendingScript, ClassicScript
- MessageChannel
- MessagePort

## ES6 Modules (-> ???)

- Modulator
- ModulatorImpl
- ModuleMap

## [Intersection Observer](https://wicg.github.io/IntersectionObserver/) (-> core/intersection-observer)

- IntersectionObservation
- IntersectionObserver
- IntersectionObserverCallback
- IntersectionObserverController
- IntersectionObserverEntry
- IntersectionObserverInit

## Security (-> core/frame/csp or core/frame)

- RemoteSecurityContext
- SecurityContext

## Layout (-> core/layout)

- ResizeObservation
- ResizeObserver
- ResizeObserverController
- ResizeObserverEntry
- WhitespaceAttacher
- WhitespaceLayoutObjects.md

## iframe (-> core/html?)

- SandboxFlags

## Focus (-> core/page)

- ScopedWindowFocusAllowedIndicator
- SlotScopedTraversal


Note: The above classification might be wrong. Please fix this README.md, and move
files if you can, if you know more appropriate places for each file.
