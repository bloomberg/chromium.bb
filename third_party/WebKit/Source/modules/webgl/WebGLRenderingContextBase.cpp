/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLRenderingContextBase.h"

#include <memory>

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/HTMLCanvasElementOrOffscreenCanvas.h"
#include "bindings/modules/v8/WebGLAny.h"
#include "build/build_config.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutBox.h"
#include "core/origin_trials/OriginTrials.h"
#include "core/probe/CoreProbes.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "core/typed_arrays/FlexibleArrayBufferView.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/ANGLEInstancedArrays.h"
#include "modules/webgl/EXTBlendMinMax.h"
#include "modules/webgl/EXTFragDepth.h"
#include "modules/webgl/EXTShaderTextureLOD.h"
#include "modules/webgl/EXTTextureFilterAnisotropic.h"
#include "modules/webgl/GLStringQuery.h"
#include "modules/webgl/OESElementIndexUint.h"
#include "modules/webgl/OESStandardDerivatives.h"
#include "modules/webgl/OESTextureFloat.h"
#include "modules/webgl/OESTextureFloatLinear.h"
#include "modules/webgl/OESTextureHalfFloat.h"
#include "modules/webgl/OESTextureHalfFloatLinear.h"
#include "modules/webgl/OESVertexArrayObject.h"
#include "modules/webgl/WebGLActiveInfo.h"
#include "modules/webgl/WebGLBuffer.h"
#include "modules/webgl/WebGLCompressedTextureASTC.h"
#include "modules/webgl/WebGLCompressedTextureATC.h"
#include "modules/webgl/WebGLCompressedTextureETC.h"
#include "modules/webgl/WebGLCompressedTextureETC1.h"
#include "modules/webgl/WebGLCompressedTexturePVRTC.h"
#include "modules/webgl/WebGLCompressedTextureS3TC.h"
#include "modules/webgl/WebGLCompressedTextureS3TCsRGB.h"
#include "modules/webgl/WebGLContextAttributeHelpers.h"
#include "modules/webgl/WebGLContextEvent.h"
#include "modules/webgl/WebGLContextGroup.h"
#include "modules/webgl/WebGLDebugRendererInfo.h"
#include "modules/webgl/WebGLDebugShaders.h"
#include "modules/webgl/WebGLDepthTexture.h"
#include "modules/webgl/WebGLDrawBuffers.h"
#include "modules/webgl/WebGLFramebuffer.h"
#include "modules/webgl/WebGLLoseContext.h"
#include "modules/webgl/WebGLProgram.h"
#include "modules/webgl/WebGLRenderbuffer.h"
#include "modules/webgl/WebGLShader.h"
#include "modules/webgl/WebGLShaderPrecisionFormat.h"
#include "modules/webgl/WebGLUniformLocation.h"
#include "modules/webgl/WebGLVertexArrayObject.h"
#include "modules/webgl/WebGLVertexArrayObjectOES.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WaitableEvent.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"

namespace blink {

namespace {

const double kSecondsBetweenRestoreAttempts = 1.0;
const int kMaxGLErrorsAllowedToConsole = 256;
const unsigned kMaxGLActiveContextsOnWorker = 4;

#if defined(OS_ANDROID)
const unsigned kMaxGLActiveContexts = 8;
#else   // defined(OS_ANDROID)
const unsigned kMaxGLActiveContexts = 16;
#endif  // defined(OS_ANDROID)

unsigned CurrentMaxGLContexts() {
  return IsMainThread() ? kMaxGLActiveContexts : kMaxGLActiveContextsOnWorker;
}

using WebGLRenderingContextBaseSet =
    PersistentHeapHashSet<WeakMember<WebGLRenderingContextBase>>;
WebGLRenderingContextBaseSet& ActiveContexts() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<WebGLRenderingContextBaseSet>,
                                  active_contexts, ());
  if (!active_contexts.IsSet())
    active_contexts->RegisterAsStaticReference();
  return *active_contexts;
}

using WebGLRenderingContextBaseMap =
    PersistentHeapHashMap<WeakMember<WebGLRenderingContextBase>, int>;
WebGLRenderingContextBaseMap& ForciblyEvictedContexts() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<WebGLRenderingContextBaseMap>,
                                  forcibly_evicted_contexts, ());
  if (!forcibly_evicted_contexts.IsSet())
    forcibly_evicted_contexts->RegisterAsStaticReference();
  return *forcibly_evicted_contexts;
}

}  // namespace

ScopedRGBEmulationColorMask::ScopedRGBEmulationColorMask(
    WebGLRenderingContextBase* context,
    GLboolean* color_mask,
    DrawingBuffer* drawing_buffer)
    : context_(context),
      requires_emulation_(drawing_buffer->RequiresAlphaChannelToBePreserved()) {
  if (requires_emulation_) {
    context_->active_scoped_rgb_emulation_color_masks_++;
    memcpy(color_mask_, color_mask, 4 * sizeof(GLboolean));
    context_->ContextGL()->ColorMask(color_mask_[0], color_mask_[1],
                                     color_mask_[2], false);
  }
}

ScopedRGBEmulationColorMask::~ScopedRGBEmulationColorMask() {
  if (requires_emulation_) {
    DCHECK(context_->active_scoped_rgb_emulation_color_masks_);
    context_->active_scoped_rgb_emulation_color_masks_--;
    context_->ContextGL()->ColorMask(color_mask_[0], color_mask_[1],
                                     color_mask_[2], color_mask_[3]);
  }
}

void WebGLRenderingContextBase::ForciblyLoseOldestContext(
    const String& reason) {
  WebGLRenderingContextBase* candidate = OldestContext();
  if (!candidate)
    return;

  candidate->PrintWarningToConsole(reason);
  probe::didFireWebGLWarning(candidate->canvas());

  // This will call deactivateContext once the context has actually been lost.
  candidate->ForceLostContext(WebGLRenderingContextBase::kSyntheticLostContext,
                              WebGLRenderingContextBase::kWhenAvailable);
}

WebGLRenderingContextBase* WebGLRenderingContextBase::OldestContext() {
  if (ActiveContexts().IsEmpty())
    return nullptr;

  WebGLRenderingContextBase* candidate = *(ActiveContexts().begin());
  DCHECK(!candidate->isContextLost());
  for (WebGLRenderingContextBase* context : ActiveContexts()) {
    DCHECK(!context->isContextLost());
    if (context->ContextGL()->GetLastFlushIdCHROMIUM() <
        candidate->ContextGL()->GetLastFlushIdCHROMIUM()) {
      candidate = context;
    }
  }

  return candidate;
}

WebGLRenderingContextBase* WebGLRenderingContextBase::OldestEvictedContext() {
  if (ForciblyEvictedContexts().IsEmpty())
    return nullptr;

  WebGLRenderingContextBase* candidate = nullptr;
  int generation = -1;
  for (WebGLRenderingContextBase* context : ForciblyEvictedContexts().Keys()) {
    if (!candidate || ForciblyEvictedContexts().at(context) < generation) {
      candidate = context;
      generation = ForciblyEvictedContexts().at(context);
    }
  }

  return candidate;
}

void WebGLRenderingContextBase::ActivateContext(
    WebGLRenderingContextBase* context) {
  unsigned max_gl_contexts = CurrentMaxGLContexts();
  unsigned removed_contexts = 0;
  while (ActiveContexts().size() >= max_gl_contexts &&
         removed_contexts < max_gl_contexts) {
    ForciblyLoseOldestContext(
        "WARNING: Too many active WebGL contexts. Oldest context will be "
        "lost.");
    removed_contexts++;
  }

  DCHECK(!context->isContextLost());
  ActiveContexts().insert(context);
}

void WebGLRenderingContextBase::DeactivateContext(
    WebGLRenderingContextBase* context) {
  ActiveContexts().erase(context);
}

void WebGLRenderingContextBase::AddToEvictedList(
    WebGLRenderingContextBase* context) {
  static int generation = 0;
  ForciblyEvictedContexts().Set(context, generation++);
}

void WebGLRenderingContextBase::RemoveFromEvictedList(
    WebGLRenderingContextBase* context) {
  ForciblyEvictedContexts().erase(context);
}

void WebGLRenderingContextBase::RestoreEvictedContext(
    WebGLRenderingContextBase* context) {
  // These two sets keep weak references to their contexts;
  // verify that the GC already removed the |context| entries.
  DCHECK(!ForciblyEvictedContexts().Contains(context));
  DCHECK(!ActiveContexts().Contains(context));

  unsigned max_gl_contexts = CurrentMaxGLContexts();
  // Try to re-enable the oldest inactive contexts.
  while (ActiveContexts().size() < max_gl_contexts &&
         ForciblyEvictedContexts().size()) {
    WebGLRenderingContextBase* evicted_context = OldestEvictedContext();
    if (!evicted_context->restore_allowed_) {
      ForciblyEvictedContexts().erase(evicted_context);
      continue;
    }

    IntSize desired_size = DrawingBuffer::AdjustSize(
        evicted_context->ClampedCanvasSize(), IntSize(),
        evicted_context->max_texture_size_);

    // If there's room in the pixel budget for this context, restore it.
    if (!desired_size.IsEmpty()) {
      ForciblyEvictedContexts().erase(evicted_context);
      evicted_context->ForceRestoreContext();
    }
    break;
  }
}

namespace {

GLint Clamp(GLint value, GLint min, GLint max) {
  if (value < min)
    value = min;
  if (value > max)
    value = max;
  return value;
}

// Return true if a character belongs to the ASCII subset as defined in
// GLSL ES 1.0 spec section 3.1.
bool ValidateCharacter(unsigned char c) {
  // Printing characters are valid except " $ ` @ \ ' DEL.
  if (c >= 32 && c <= 126 && c != '"' && c != '$' && c != '`' && c != '@' &&
      c != '\\' && c != '\'')
    return true;
  // Horizontal tab, line feed, vertical tab, form feed, carriage return
  // are also valid.
  if (c >= 9 && c <= 13)
    return true;
  return false;
}

bool IsPrefixReserved(const String& name) {
  if (name.StartsWith("gl_") || name.StartsWith("webgl_") ||
      name.StartsWith("_webgl_"))
    return true;
  return false;
}

// Strips comments from shader text. This allows non-ASCII characters
// to be used in comments without potentially breaking OpenGL
// implementations not expecting characters outside the GLSL ES set.
class StripComments {
 public:
  StripComments(const String& str)
      : parse_state_(kBeginningOfLine),
        source_string_(str),
        length_(str.length()),
        position_(0) {
    Parse();
  }

  String Result() { return builder_.ToString(); }

 private:
  bool HasMoreCharacters() const { return (position_ < length_); }

  void Parse() {
    while (HasMoreCharacters()) {
      Process(Current());
      // process() might advance the position.
      if (HasMoreCharacters())
        Advance();
    }
  }

  void Process(UChar);

  bool Peek(UChar& character) const {
    if (position_ + 1 >= length_)
      return false;
    character = source_string_[position_ + 1];
    return true;
  }

  UChar Current() {
    SECURITY_DCHECK(position_ < length_);
    return source_string_[position_];
  }

  void Advance() { ++position_; }

  static bool IsNewline(UChar character) {
    // Don't attempt to canonicalize newline related characters.
    return (character == '\n' || character == '\r');
  }

  void Emit(UChar character) { builder_.Append(character); }

  enum ParseState {
    // Have not seen an ASCII non-whitespace character yet on
    // this line. Possible that we might see a preprocessor
    // directive.
    kBeginningOfLine,

    // Have seen at least one ASCII non-whitespace character
    // on this line.
    kMiddleOfLine,

    // Handling a preprocessor directive. Passes through all
    // characters up to the end of the line. Disables comment
    // processing.
    kInPreprocessorDirective,

    // Handling a single-line comment. The comment text is
    // replaced with a single space.
    kInSingleLineComment,

    // Handling a multi-line comment. Newlines are passed
    // through to preserve line numbers.
    kInMultiLineComment
  };

  ParseState parse_state_;
  String source_string_;
  unsigned length_;
  unsigned position_;
  StringBuilder builder_;
};

void StripComments::Process(UChar c) {
  if (IsNewline(c)) {
    // No matter what state we are in, pass through newlines
    // so we preserve line numbers.
    Emit(c);

    if (parse_state_ != kInMultiLineComment)
      parse_state_ = kBeginningOfLine;

    return;
  }

  UChar temp = 0;
  switch (parse_state_) {
    case kBeginningOfLine:
      if (WTF::IsASCIISpace(c)) {
        Emit(c);
        break;
      }

      if (c == '#') {
        parse_state_ = kInPreprocessorDirective;
        Emit(c);
        break;
      }

      // Transition to normal state and re-handle character.
      parse_state_ = kMiddleOfLine;
      Process(c);
      break;

    case kMiddleOfLine:
      if (c == '/' && Peek(temp)) {
        if (temp == '/') {
          parse_state_ = kInSingleLineComment;
          Emit(' ');
          Advance();
          break;
        }

        if (temp == '*') {
          parse_state_ = kInMultiLineComment;
          // Emit the comment start in case the user has
          // an unclosed comment and we want to later
          // signal an error.
          Emit('/');
          Emit('*');
          Advance();
          break;
        }
      }

      Emit(c);
      break;

    case kInPreprocessorDirective:
      // No matter what the character is, just pass it
      // through. Do not parse comments in this state. This
      // might not be the right thing to do long term, but it
      // should handle the #error preprocessor directive.
      Emit(c);
      break;

    case kInSingleLineComment:
      // Line-continuation characters are processed before comment processing.
      // Advance string if a new line character is immediately behind
      // line-continuation character.
      if (c == '\\') {
        if (Peek(temp) && IsNewline(temp))
          Advance();
      }

      // The newline code at the top of this function takes care
      // of resetting our state when we get out of the
      // single-line comment. Swallow all other characters.
      break;

    case kInMultiLineComment:
      if (c == '*' && Peek(temp) && temp == '/') {
        Emit('*');
        Emit('/');
        parse_state_ = kMiddleOfLine;
        Advance();
        break;
      }

      // Swallow all other characters. Unclear whether we may
      // want or need to just emit a space per character to try
      // to preserve column numbers for debugging purposes.
      break;
  }
}

static bool g_should_fail_context_creation_for_testing = false;
}  // namespace

class ScopedTexture2DRestorer {
  STACK_ALLOCATED();

 public:
  explicit ScopedTexture2DRestorer(WebGLRenderingContextBase* context)
      : context_(context) {}

  ~ScopedTexture2DRestorer() { context_->RestoreCurrentTexture2D(); }

 private:
  Member<WebGLRenderingContextBase> context_;
};

class ScopedFramebufferRestorer {
  STACK_ALLOCATED();

 public:
  explicit ScopedFramebufferRestorer(WebGLRenderingContextBase* context)
      : context_(context) {}

  ~ScopedFramebufferRestorer() { context_->RestoreCurrentFramebuffer(); }

 private:
  Member<WebGLRenderingContextBase> context_;
};

class ScopedUnpackParametersResetRestore {
  STACK_ALLOCATED();

 public:
  explicit ScopedUnpackParametersResetRestore(
      WebGLRenderingContextBase* context,
      bool enabled = true)
      : context_(context), enabled_(enabled) {
    if (enabled)
      context_->ResetUnpackParameters();
  }

  ~ScopedUnpackParametersResetRestore() {
    if (enabled_)
      context_->RestoreUnpackParameters();
  }

 private:
  Member<WebGLRenderingContextBase> context_;
  bool enabled_;
};

static void FormatWebGLStatusString(const StringView& gl_info,
                                    const StringView& info_string,
                                    StringBuilder& builder) {
  if (info_string.IsEmpty())
    return;
  builder.Append(", ");
  builder.Append(gl_info);
  builder.Append(" = ");
  builder.Append(info_string);
}

static String ExtractWebGLContextCreationError(
    const Platform::GraphicsInfo& info) {
  StringBuilder builder;
  builder.Append("Could not create a WebGL context");
  FormatWebGLStatusString(
      "VENDOR",
      info.vendor_id ? String::Format("0x%04x", info.vendor_id) : "0xffff",
      builder);
  FormatWebGLStatusString(
      "DEVICE",
      info.device_id ? String::Format("0x%04x", info.device_id) : "0xffff",
      builder);
  FormatWebGLStatusString("GL_VENDOR", info.vendor_info, builder);
  FormatWebGLStatusString("GL_RENDERER", info.renderer_info, builder);
  FormatWebGLStatusString("GL_VERSION", info.driver_version, builder);
  FormatWebGLStatusString("Sandboxed", info.sandboxed ? "yes" : "no", builder);
  FormatWebGLStatusString("Optimus", info.optimus ? "yes" : "no", builder);
  FormatWebGLStatusString("AMD switchable", info.amd_switchable ? "yes" : "no",
                          builder);
  FormatWebGLStatusString(
      "Reset notification strategy",
      String::Format("0x%04x", info.reset_notification_strategy).Utf8().data(),
      builder);
  FormatWebGLStatusString("GPU process crash count",
                          String::Number(info.process_crash_count), builder);
  FormatWebGLStatusString("ErrorMessage", info.error_message.Utf8().data(),
                          builder);
  builder.Append('.');
  return builder.ToString();
}

struct ContextProviderCreationInfo {
  // Inputs.
  Platform::ContextAttributes context_attributes;
  Platform::GraphicsInfo* gl_info;
  KURL url;
  // Outputs.
  std::unique_ptr<WebGraphicsContext3DProvider> created_context_provider;
};

static void CreateContextProviderOnMainThread(
    ContextProviderCreationInfo* creation_info,
    WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());
  creation_info->created_context_provider =
      Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
          creation_info->context_attributes, creation_info->url, 0,
          creation_info->gl_info);
  waitable_event->Signal();
}

static std::unique_ptr<WebGraphicsContext3DProvider>
CreateContextProviderOnWorkerThread(
    Platform::ContextAttributes context_attributes,
    Platform::GraphicsInfo* gl_info,
    const KURL& url) {
  WaitableEvent waitable_event;
  ContextProviderCreationInfo creation_info;
  creation_info.context_attributes = context_attributes;
  creation_info.gl_info = gl_info;
  creation_info.url = url.Copy();
  RefPtr<WebTaskRunner> task_runner =
      Platform::Current()->MainThread()->GetWebTaskRunner();
  task_runner->PostTask(
      BLINK_FROM_HERE, CrossThreadBind(&CreateContextProviderOnMainThread,
                                       CrossThreadUnretained(&creation_info),
                                       CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();
  return std::move(creation_info.created_context_provider);
}

bool WebGLRenderingContextBase::SupportOwnOffscreenSurface(
    ExecutionContext* execution_context) {
  // If there's a possibility this context may be used with WebVR make sure it
  // is created with an offscreen surface that can be swapped out for a
  // VR-specific surface if needed.
  //
  // At this time, treat this as an experimental rendering optimization
  // that needs a separate opt-in. See crbug.com/691102 for details.
  if (RuntimeEnabledFeatures::WebVRExperimentalRenderingEnabled()) {
    if (RuntimeEnabledFeatures::WebVREnabled() ||
        OriginTrials::webVREnabled(execution_context)) {
      DVLOG(1) << "Requesting supportOwnOffscreenSurface";
      return true;
    }
  }
  return false;
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::CreateContextProviderInternal(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributes& attributes,
    unsigned web_gl_version) {
  DCHECK(host);
  ExecutionContext* execution_context = host->GetTopExecutionContext();
  DCHECK(execution_context);

  Platform::ContextAttributes context_attributes = ToPlatformContextAttributes(
      attributes, web_gl_version,
      SupportOwnOffscreenSurface(execution_context));

  Platform::GraphicsInfo gl_info;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider;
  const auto& url = execution_context->Url();
  if (IsMainThread()) {
    context_provider =
        Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
            context_attributes, url, 0, &gl_info);
  } else {
    context_provider =
        CreateContextProviderOnWorkerThread(context_attributes, &gl_info, url);
  }
  if (context_provider && !context_provider->BindToCurrentThread()) {
    context_provider = nullptr;
    gl_info.error_message =
        String("bindToCurrentThread failed: " + String(gl_info.error_message));
  }
  if (!context_provider || g_should_fail_context_creation_for_testing) {
    g_should_fail_context_creation_for_testing = false;
    host->HostDispatchEvent(WebGLContextEvent::Create(
        EventTypeNames::webglcontextcreationerror, false, true,
        ExtractWebGLContextCreationError(gl_info)));
    return nullptr;
  }
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  if (!String(gl->GetString(GL_EXTENSIONS))
           .Contains("GL_OES_packed_depth_stencil")) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        EventTypeNames::webglcontextcreationerror, false, true,
        "OES_packed_depth_stencil support is required."));
    return nullptr;
  }
  return context_provider;
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::CreateWebGraphicsContext3DProvider(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributes& attributes,
    unsigned web_gl_version) {
  if (!host->IsWebGLAllowed()) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        EventTypeNames::webglcontextcreationerror, false, true,
        "Web page was not allowed to create a WebGL context."));
    return nullptr;
  }

  return CreateContextProviderInternal(host, attributes, web_gl_version);
}

void WebGLRenderingContextBase::ForceNextWebGLContextCreationToFail() {
  g_should_fail_context_creation_for_testing = true;
}

ImageBitmap* WebGLRenderingContextBase::TransferToImageBitmapBase(
    ScriptState* script_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasTransferToImageBitmapWebGL;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  if (!GetDrawingBuffer())
    return nullptr;
  return ImageBitmap::Create(GetDrawingBuffer()->TransferToStaticBitmapImage());
}

ScriptPromise WebGLRenderingContextBase::commit(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasCommitWebGL;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  int width = GetDrawingBuffer()->Size().Width();
  int height = GetDrawingBuffer()->Size().Height();
  if (!GetDrawingBuffer()) {
    bool is_web_gl_software_rendering = false;
    return host()->Commit(nullptr, SkIRect::MakeWH(width, height),
                          is_web_gl_software_rendering, script_state,
                          exception_state);
  }

  RefPtr<StaticBitmapImage> image;
  if (CreationAttributes().preserveDrawingBuffer()) {
    SkImageInfo image_info =
        SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                          CreationAttributes().alpha() ? kPremul_SkAlphaType
                                                       : kOpaque_SkAlphaType);
    image = StaticBitmapImage::Create(MakeImageSnapshot(image_info));
  } else {
    image = GetDrawingBuffer()->TransferToStaticBitmapImage();
  }

  return host()->Commit(
      std::move(image), SkIRect::MakeWH(width, height),
      GetDrawingBuffer()->ContextProvider()->IsSoftwareRendering(),
      script_state, exception_state);
}

PassRefPtr<Image> WebGLRenderingContextBase::GetImage(
    AccelerationHint hint,
    SnapshotReason reason) const {
  if (!GetDrawingBuffer())
    return nullptr;

  GetDrawingBuffer()->ResolveAndBindForReadAndDraw();
  IntSize size = ClampedCanvasSize();
  OpacityMode opacity_mode =
      CreationAttributes().hasAlpha() ? kNonOpaque : kOpaque;
  std::unique_ptr<AcceleratedImageBufferSurface> surface =
      WTF::MakeUnique<AcceleratedImageBufferSurface>(size, opacity_mode);
  if (!surface->IsValid())
    return nullptr;
  std::unique_ptr<ImageBuffer> buffer = ImageBuffer::Create(std::move(surface));
  if (!buffer->CopyRenderingResultsFromDrawingBuffer(GetDrawingBuffer(),
                                                     kBackBuffer)) {
    // copyRenderingResultsFromDrawingBuffer is expected to always succeed
    // because we've explicitly created an Accelerated surface and have already
    // validated it.
    NOTREACHED();
    return nullptr;
  }
  return buffer->NewImageSnapshot(hint, reason);
}

sk_sp<SkImage> WebGLRenderingContextBase::MakeImageSnapshot(
    SkImageInfo& image_info) {
  GetDrawingBuffer()->ResolveAndBindForReadAndDraw();
  gpu::gles2::GLES2Interface* gl = SharedGpuContext::Gl();

  SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(
      SharedGpuContext::Gr(), SkBudgeted::kYes, image_info, 0,
      image_info.alphaType() == kOpaque_SkAlphaType ? nullptr
                                                    : &disable_lcd_props);
  const GrGLTextureInfo* texture_info = skia::GrBackendObjectToGrGLTextureInfo(
      surface->getTextureHandle(SkSurface::kDiscardWrite_TextureHandleAccess));
  GLuint texture_id = texture_info->fID;
  GLenum texture_target = texture_info->fTarget;

  GetDrawingBuffer()->CopyToPlatformTexture(
      gl, texture_target, texture_id, true, false, IntPoint(0, 0),
      IntRect(IntPoint(0, 0), GetDrawingBuffer()->Size()), kBackBuffer);
  return surface->makeImageSnapshot();
}

ImageData* WebGLRenderingContextBase::ToImageData(SnapshotReason reason) {
  ImageData* image_data = nullptr;
  // TODO(ccameron): WebGL should produce sRGB images.
  // https://crbug.com/672299
  if (GetDrawingBuffer()) {
    // For un-premultiplied data
    image_data = PaintRenderingResultsToImageData(kBackBuffer);
    if (image_data) {
      return image_data;
    }

    int width = GetDrawingBuffer()->Size().Width();
    int height = GetDrawingBuffer()->Size().Height();
    SkImageInfo image_info =
        SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                          CreationAttributes().alpha() ? kPremul_SkAlphaType
                                                       : kOpaque_SkAlphaType);
    sk_sp<SkImage> snapshot = MakeImageSnapshot(image_info);
    if (snapshot) {
      image_data = ImageData::Create(GetDrawingBuffer()->Size());
      snapshot->readPixels(image_info, image_data->data()->Data(),
                           image_info.minRowBytes(), 0, 0);
    }
  }
  return image_data;
}

namespace {

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedInternalFormatsOESDepthTex[] = {
    GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static const GLenum kSupportedInternalFormatsEXTsRGB[] = {
    GL_SRGB, GL_SRGB_ALPHA_EXT,
};

// ES3 enums supported by both CopyTexImage and TexImage.
static const GLenum kSupportedInternalFormatsES3[] = {
    GL_R8,           GL_RG8,      GL_RGB565,   GL_RGB8,       GL_RGBA4,
    GL_RGB5_A1,      GL_RGBA8,    GL_RGB10_A2, GL_RGB10_A2UI, GL_SRGB8,
    GL_SRGB8_ALPHA8, GL_R8I,      GL_R8UI,     GL_R16I,       GL_R16UI,
    GL_R32I,         GL_R32UI,    GL_RG8I,     GL_RG8UI,      GL_RG16I,
    GL_RG16UI,       GL_RG32I,    GL_RG32UI,   GL_RGBA8I,     GL_RGBA8UI,
    GL_RGBA16I,      GL_RGBA16UI, GL_RGBA32I,  GL_RGBA32UI,
};

// ES3 enums only supported by TexImage
static const GLenum kSupportedInternalFormatsTexImageES3[] = {
    GL_R8_SNORM,
    GL_R16F,
    GL_R32F,
    GL_RG8_SNORM,
    GL_RG16F,
    GL_RG32F,
    GL_RGB8_SNORM,
    GL_R11F_G11F_B10F,
    GL_RGB9_E5,
    GL_RGB16F,
    GL_RGB32F,
    GL_RGB8UI,
    GL_RGB8I,
    GL_RGB16UI,
    GL_RGB16I,
    GL_RGB32UI,
    GL_RGB32I,
    GL_RGBA8_SNORM,
    GL_RGBA16F,
    GL_RGBA32F,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH_COMPONENT32F,
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

// Exposed by EXT_color_buffer_float
static const GLenum kSupportedInternalFormatsCopyTexImageFloatES3[] = {
    GL_R16F,   GL_R32F,    GL_RG16F,   GL_RG32F,         GL_RGB16F,
    GL_RGB32F, GL_RGBA16F, GL_RGBA32F, GL_R11F_G11F_B10F};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedInternalFormatsTexImageSourceES3[] = {
    GL_R8,      GL_R16F,           GL_R32F,         GL_R8UI,     GL_RG8,
    GL_RG16F,   GL_RG32F,          GL_RG8UI,        GL_RGB8,     GL_SRGB8,
    GL_RGB565,  GL_R11F_G11F_B10F, GL_RGB9_E5,      GL_RGB16F,   GL_RGB32F,
    GL_RGB8UI,  GL_RGBA8,          GL_SRGB8_ALPHA8, GL_RGB5_A1,  GL_RGBA4,
    GL_RGBA16F, GL_RGBA32F,        GL_RGBA8UI,      GL_RGB10_A2,
};

// ES2 enums
// Internalformat must equal format in ES2.
static const GLenum kSupportedFormatsES2[] = {
    GL_RGB, GL_RGBA, GL_LUMINANCE_ALPHA, GL_LUMINANCE, GL_ALPHA,
};

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedFormatsOESDepthTex[] = {
    GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static const GLenum kSupportedFormatsEXTsRGB[] = {
    GL_SRGB, GL_SRGB_ALPHA_EXT,
};

// ES3 enums
static const GLenum kSupportedFormatsES3[] = {
    GL_RED,           GL_RED_INTEGER,  GL_RG,
    GL_RG_INTEGER,    GL_RGB,          GL_RGB_INTEGER,
    GL_RGBA,          GL_RGBA_INTEGER, GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedFormatsTexImageSourceES3[] = {
    GL_RED, GL_RED_INTEGER, GL_RG,   GL_RG_INTEGER,
    GL_RGB, GL_RGB_INTEGER, GL_RGBA, GL_RGBA_INTEGER,
};

// ES2 enums
static const GLenum kSupportedTypesES2[] = {
    GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT_5_6_5, GL_UNSIGNED_SHORT_4_4_4_4,
    GL_UNSIGNED_SHORT_5_5_5_1,
};

// Exposed by GL_OES_texture_float
static const GLenum kSupportedTypesOESTexFloat[] = {
    GL_FLOAT,
};

// Exposed by GL_OES_texture_half_float
static const GLenum kSupportedTypesOESTexHalfFloat[] = {
    GL_HALF_FLOAT_OES,
};

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedTypesOESDepthTex[] = {
    GL_UNSIGNED_SHORT, GL_UNSIGNED_INT, GL_UNSIGNED_INT_24_8,
};

// ES3 enums
static const GLenum kSupportedTypesES3[] = {
    GL_BYTE,
    GL_UNSIGNED_SHORT,
    GL_SHORT,
    GL_UNSIGNED_INT,
    GL_INT,
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_INT_2_10_10_10_REV,
    GL_UNSIGNED_INT_10F_11F_11F_REV,
    GL_UNSIGNED_INT_5_9_9_9_REV,
    GL_UNSIGNED_INT_24_8,
    GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedTypesTexImageSourceES3[] = {
    GL_HALF_FLOAT, GL_FLOAT, GL_UNSIGNED_INT_10F_11F_11F_REV,
    GL_UNSIGNED_INT_2_10_10_10_REV,
};

}  // namespace

WebGLRenderingContextBase::WebGLRenderingContextBase(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const CanvasContextCreationAttributes& requested_attributes,
    unsigned version)
    : WebGLRenderingContextBase(
          host,
          TaskRunnerHelper::Get(TaskType::kWebGL,
                                host->GetTopExecutionContext()),
          std::move(context_provider),
          requested_attributes,
          version) {}

WebGLRenderingContextBase::WebGLRenderingContextBase(
    CanvasRenderingContextHost* host,
    RefPtr<WebTaskRunner> task_runner,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const CanvasContextCreationAttributes& requested_attributes,
    unsigned version)
    : CanvasRenderingContext(host, requested_attributes),
      context_group_(this, new WebGLContextGroup()),
      is_hidden_(false),
      context_lost_mode_(kNotLostContext),
      auto_recovery_method_(kManual),
      dispatch_context_lost_event_timer_(
          task_runner,
          this,
          &WebGLRenderingContextBase::DispatchContextLostEvent),
      restore_allowed_(false),
      restore_timer_(task_runner,
                     this,
                     &WebGLRenderingContextBase::MaybeRestoreContext),
      bound_array_buffer_(this, nullptr),
      bound_vertex_array_object_(this, nullptr),
      current_program_(this, nullptr),
      framebuffer_binding_(this, nullptr),
      renderbuffer_binding_(this, nullptr),
      generated_image_cache_(4),
      synthesized_errors_to_console_(true),
      num_gl_errors_to_console_allowed_(kMaxGLErrorsAllowedToConsole),
      one_plus_max_non_default_texture_unit_(0),
      is_web_gl2_formats_types_added_(false),
      is_web_gl2_tex_image_source_formats_types_added_(false),
      is_web_gl2_internal_formats_copy_tex_image_added_(false),
      is_oes_texture_float_formats_types_added_(false),
      is_oes_texture_half_float_formats_types_added_(false),
      is_web_gl_depth_texture_formats_types_added_(false),
      is_ext_srgb_formats_types_added_(false),
      is_ext_color_buffer_float_formats_added_(false),
      version_(version) {
  DCHECK(context_provider);

  context_group_->AddContext(this);

  max_viewport_dims_[0] = max_viewport_dims_[1] = 0;
  context_provider->ContextGL()->GetIntegerv(GL_MAX_VIEWPORT_DIMS,
                                             max_viewport_dims_);

  RefPtr<DrawingBuffer> buffer;
  buffer = CreateDrawingBuffer(std::move(context_provider));
  if (!buffer) {
    context_lost_mode_ = kSyntheticLostContext;
    return;
  }

  drawing_buffer_ = std::move(buffer);
  GetDrawingBuffer()->Bind(GL_FRAMEBUFFER);
  SetupFlags();

#define ADD_VALUES_TO_SET(set, values)                    \
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(values); ++i) { \
    set.insert(values[i]);                                \
  }

  ADD_VALUES_TO_SET(supported_internal_formats_, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                    kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                    kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_tex_image_source_formats_, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_types_, kSupportedTypesES2);
  ADD_VALUES_TO_SET(supported_tex_image_source_types_, kSupportedTypesES2);
}

PassRefPtr<DrawingBuffer> WebGLRenderingContextBase::CreateDrawingBuffer(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider) {
  bool premultiplied_alpha = CreationAttributes().premultipliedAlpha();
  bool want_alpha_channel = CreationAttributes().alpha();
  bool want_depth_buffer = CreationAttributes().depth();
  bool want_stencil_buffer = CreationAttributes().stencil();
  bool want_antialiasing = CreationAttributes().antialias();
  DrawingBuffer::PreserveDrawingBuffer preserve =
      CreationAttributes().preserveDrawingBuffer() ? DrawingBuffer::kPreserve
                                                   : DrawingBuffer::kDiscard;
  DrawingBuffer::WebGLVersion web_gl_version = DrawingBuffer::kWebGL1;
  if (Version() == 1) {
    web_gl_version = DrawingBuffer::kWebGL1;
  } else if (Version() == 2) {
    web_gl_version = DrawingBuffer::kWebGL2;
  } else {
    NOTREACHED();
  }

  // On Mac OS, DrawingBuffer is using an IOSurface as its backing storage, this
  // allows WebGL-rendered canvases to be composited by the OS rather than
  // Chrome.
  // IOSurfaces are only compatible with the GL_TEXTURE_RECTANGLE_ARB binding
  // target. So to avoid the knowledge of GL_TEXTURE_RECTANGLE_ARB type textures
  // being introduced into more areas of the code, we use the code path of
  // non-WebGLImageChromium for OffscreenCanvas.
  // See detailed discussion in crbug.com/649668.
  DrawingBuffer::ChromiumImageUsage chromium_image_usage =
      host()->IsOffscreenCanvas() ? DrawingBuffer::kDisallowChromiumImage
                                  : DrawingBuffer::kAllowChromiumImage;

  return DrawingBuffer::Create(
      std::move(context_provider), this, ClampedCanvasSize(),
      premultiplied_alpha, want_alpha_channel, want_depth_buffer,
      want_stencil_buffer, want_antialiasing, preserve, web_gl_version,
      chromium_image_usage, color_params());
}

void WebGLRenderingContextBase::InitializeNewContext() {
  DCHECK(!isContextLost());
  DCHECK(GetDrawingBuffer());

  marked_canvas_dirty_ = false;
  animation_frame_in_progress_ = false;
  active_texture_unit_ = 0;
  pack_alignment_ = 4;
  unpack_alignment_ = 4;
  unpack_flip_y_ = false;
  unpack_premultiply_alpha_ = false;
  unpack_colorspace_conversion_ = GC3D_BROWSER_DEFAULT_WEBGL;
  bound_array_buffer_ = nullptr;
  current_program_ = nullptr;
  framebuffer_binding_ = nullptr;
  renderbuffer_binding_ = nullptr;
  depth_mask_ = true;
  stencil_enabled_ = false;
  stencil_mask_ = 0xFFFFFFFF;
  stencil_mask_back_ = 0xFFFFFFFF;
  stencil_func_ref_ = 0;
  stencil_func_ref_back_ = 0;
  stencil_func_mask_ = 0xFFFFFFFF;
  stencil_func_mask_back_ = 0xFFFFFFFF;
  num_gl_errors_to_console_allowed_ = kMaxGLErrorsAllowedToConsole;

  clear_color_[0] = clear_color_[1] = clear_color_[2] = clear_color_[3] = 0;
  scissor_enabled_ = false;
  clear_depth_ = 1;
  clear_stencil_ = 0;
  color_mask_[0] = color_mask_[1] = color_mask_[2] = color_mask_[3] = true;

  GLint num_combined_texture_image_units = 0;
  ContextGL()->GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                           &num_combined_texture_image_units);
  texture_units_.clear();
  texture_units_.resize(num_combined_texture_image_units);

  GLint num_vertex_attribs = 0;
  ContextGL()->GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &num_vertex_attribs);
  max_vertex_attribs_ = num_vertex_attribs;

  max_texture_size_ = 0;
  ContextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  max_texture_level_ =
      WebGLTexture::ComputeLevelCount(max_texture_size_, max_texture_size_, 1);
  max_cube_map_texture_size_ = 0;
  ContextGL()->GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE,
                           &max_cube_map_texture_size_);
  max3d_texture_size_ = 0;
  max3d_texture_level_ = 0;
  max_array_texture_layers_ = 0;
  if (IsWebGL2OrHigher()) {
    ContextGL()->GetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3d_texture_size_);
    max3d_texture_level_ = WebGLTexture::ComputeLevelCount(
        max3d_texture_size_, max3d_texture_size_, max3d_texture_size_);
    ContextGL()->GetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,
                             &max_array_texture_layers_);
  }
  max_cube_map_texture_level_ = WebGLTexture::ComputeLevelCount(
      max_cube_map_texture_size_, max_cube_map_texture_size_, 1);
  max_renderbuffer_size_ = 0;
  ContextGL()->GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size_);

  // These two values from EXT_draw_buffers are lazily queried.
  max_draw_buffers_ = 0;
  max_color_attachments_ = 0;

  back_draw_buffer_ = GL_BACK;

  read_buffer_of_default_framebuffer_ = GL_BACK;

  default_vertex_array_object_ = WebGLVertexArrayObject::Create(
      this, WebGLVertexArrayObjectBase::kVaoTypeDefault);

  bound_vertex_array_object_ = default_vertex_array_object_;

  vertex_attrib_type_.resize(max_vertex_attribs_);

  ContextGL()->Viewport(0, 0, drawingBufferWidth(), drawingBufferHeight());
  scissor_box_[0] = scissor_box_[1] = 0;
  scissor_box_[2] = drawingBufferWidth();
  scissor_box_[3] = drawingBufferHeight();
  ContextGL()->Scissor(scissor_box_[0], scissor_box_[1], scissor_box_[2],
                       scissor_box_[3]);

  GetDrawingBuffer()->ContextProvider()->SetLostContextCallback(
      ConvertToBaseCallback(WTF::Bind(
          &WebGLRenderingContextBase::ForceLostContext,
          WrapWeakPersistent(this), WebGLRenderingContextBase::kRealLostContext,
          WebGLRenderingContextBase::kAuto)));
  GetDrawingBuffer()->ContextProvider()->SetErrorMessageCallback(
      ConvertToBaseCallback(
          WTF::Bind(&WebGLRenderingContextBase::OnErrorMessage,
                    WrapWeakPersistent(this))));

  // If WebGL 2, the PRIMITIVE_RESTART_FIXED_INDEX should be always enabled.
  // See the section <Primitive Restart is Always Enabled> in WebGL 2 spec:
  // https://www.khronos.org/registry/webgl/specs/latest/2.0/#4.1.4
  if (IsWebGL2OrHigher())
    ContextGL()->Enable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

  // This ensures that the context has a valid "lastFlushID" and won't be
  // mistakenly identified as the "least recently used" context.
  ContextGL()->Flush();

  for (int i = 0; i < kWebGLExtensionNameCount; ++i)
    extension_enabled_[i] = false;

  is_web_gl2_formats_types_added_ = false;
  is_web_gl2_tex_image_source_formats_types_added_ = false;
  is_web_gl2_internal_formats_copy_tex_image_added_ = false;
  is_oes_texture_float_formats_types_added_ = false;
  is_oes_texture_half_float_formats_types_added_ = false;
  is_web_gl_depth_texture_formats_types_added_ = false;
  is_ext_srgb_formats_types_added_ = false;
  is_ext_color_buffer_float_formats_added_ = false;

  supported_internal_formats_.clear();
  ADD_VALUES_TO_SET(supported_internal_formats_, kSupportedFormatsES2);
  supported_tex_image_source_internal_formats_.clear();
  ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                    kSupportedFormatsES2);
  supported_internal_formats_copy_tex_image_.clear();
  ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                    kSupportedFormatsES2);
  supported_formats_.clear();
  ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsES2);
  supported_tex_image_source_formats_.clear();
  ADD_VALUES_TO_SET(supported_tex_image_source_formats_, kSupportedFormatsES2);
  supported_types_.clear();
  ADD_VALUES_TO_SET(supported_types_, kSupportedTypesES2);
  supported_tex_image_source_types_.clear();
  ADD_VALUES_TO_SET(supported_tex_image_source_types_, kSupportedTypesES2);

  // The DrawingBuffer was unable to store the state that dirtied when it was
  // initialized. Restore it now.
  GetDrawingBuffer()->RestoreAllState();
  ActivateContext(this);
}

void WebGLRenderingContextBase::SetupFlags() {
  DCHECK(GetDrawingBuffer());
  if (canvas()) {
    if (Page* p = canvas()->GetDocument().GetPage()) {
      synthesized_errors_to_console_ =
          p->GetSettings().GetWebGLErrorsToConsoleEnabled();
    }
  }

  is_depth_stencil_supported_ =
      ExtensionsUtil()->IsExtensionEnabled("GL_OES_packed_depth_stencil");
}

void WebGLRenderingContextBase::AddCompressedTextureFormat(GLenum format) {
  if (!compressed_texture_formats_.Contains(format))
    compressed_texture_formats_.push_back(format);
}

void WebGLRenderingContextBase::RemoveAllCompressedTextureFormats() {
  compressed_texture_formats_.clear();
}

// Helper function for V8 bindings to identify what version of WebGL a
// CanvasRenderingContext supports.
unsigned WebGLRenderingContextBase::GetWebGLVersion(
    const CanvasRenderingContext* context) {
  if (!context->Is3d())
    return 0;
  return static_cast<const WebGLRenderingContextBase*>(context)->Version();
}

WebGLRenderingContextBase::~WebGLRenderingContextBase() {
  // Now that the context and context group no longer hold on to the
  // objects they create, and now that the objects are eagerly finalized
  // rather than the context, there is very little useful work that this
  // destructor can do, since it's not allowed to touch other on-heap
  // objects. All it can do is destroy its underlying context, which, if
  // there are no other contexts in the same share group, will cause all of
  // the underlying graphics resources to be deleted. (Currently, it's
  // always the case that there are no other contexts in the same share
  // group -- resource sharing between WebGL contexts is not yet
  // implemented, and due to its complex semantics, it's doubtful that it
  // ever will be.)
  DestroyContext();

  // Now that this context is destroyed, see if there's a
  // previously-evicted one that should be restored.
  RestoreEvictedContext(this);
}

void WebGLRenderingContextBase::DestroyContext() {
  if (!GetDrawingBuffer())
    return;

  extensions_util_.reset();

  std::unique_ptr<WTF::Closure> null_closure;
  std::unique_ptr<WTF::Function<void(const char*, int32_t)>> null_function;
  GetDrawingBuffer()->ContextProvider()->SetLostContextCallback(
      ConvertToBaseCallback(std::move(null_closure)));
  GetDrawingBuffer()->ContextProvider()->SetErrorMessageCallback(
      ConvertToBaseCallback(std::move(null_function)));

  DCHECK(GetDrawingBuffer());
  drawing_buffer_->BeginDestruction();
  drawing_buffer_.Clear();
}

void WebGLRenderingContextBase::MarkContextChanged(
    ContentChangeType change_type) {
  if (framebuffer_binding_ || isContextLost())
    return;

  if (!GetDrawingBuffer()->MarkContentsChanged() && marked_canvas_dirty_) {
    return;
  }

  if (!canvas())
    return;

  marked_canvas_dirty_ = true;

  if (!animation_frame_in_progress_) {
    animation_frame_in_progress_ = true;
    LayoutBox* layout_box = canvas()->GetLayoutBox();
    if (layout_box && layout_box->HasAcceleratedCompositing()) {
      layout_box->ContentChanged(change_type);
    }
    IntSize canvas_size = ClampedCanvasSize();
    DidDraw(SkIRect::MakeXYWH(0, 0, canvas_size.Width(), canvas_size.Height()));
  }
}

void WebGLRenderingContextBase::FinalizeFrame() {
  animation_frame_in_progress_ = false;
}

void WebGLRenderingContextBase::OnErrorMessage(const char* message,
                                               int32_t id) {
  if (synthesized_errors_to_console_)
    PrintGLErrorToConsole(message);
  probe::didFireWebGLErrorOrWarning(canvas(), message);
}

WebGLRenderingContextBase::HowToClear
WebGLRenderingContextBase::ClearIfComposited(GLbitfield mask) {
  if (isContextLost())
    return kSkipped;

  if (!GetDrawingBuffer()->BufferClearNeeded() ||
      (mask && framebuffer_binding_))
    return kSkipped;

  Nullable<WebGLContextAttributes> context_attributes;
  getContextAttributes(context_attributes);
  if (context_attributes.IsNull()) {
    // Unlikely, but context was lost.
    return kSkipped;
  }

  // Determine if it's possible to combine the clear the user asked for and this
  // clear.
  bool combined_clear = mask && !scissor_enabled_;

  ContextGL()->Disable(GL_SCISSOR_TEST);
  if (combined_clear && (mask & GL_COLOR_BUFFER_BIT)) {
    ContextGL()->ClearColor(color_mask_[0] ? clear_color_[0] : 0,
                            color_mask_[1] ? clear_color_[1] : 0,
                            color_mask_[2] ? clear_color_[2] : 0,
                            color_mask_[3] ? clear_color_[3] : 0);
  } else {
    ContextGL()->ClearColor(0, 0, 0, 0);
  }
  ContextGL()->ColorMask(
      true, true, true,
      !GetDrawingBuffer()->RequiresAlphaChannelToBePreserved());
  GLbitfield clear_mask = GL_COLOR_BUFFER_BIT;
  if (context_attributes.Get().depth()) {
    if (!combined_clear || !depth_mask_ || !(mask & GL_DEPTH_BUFFER_BIT))
      ContextGL()->ClearDepthf(1.0f);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
    ContextGL()->DepthMask(true);
  }
  if (context_attributes.Get().stencil() ||
      GetDrawingBuffer()->HasImplicitStencilBuffer()) {
    if (combined_clear && (mask & GL_STENCIL_BUFFER_BIT))
      ContextGL()->ClearStencil(clear_stencil_ & stencil_mask_);
    else
      ContextGL()->ClearStencil(0);
    clear_mask |= GL_STENCIL_BUFFER_BIT;
    ContextGL()->StencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
  }

  ContextGL()->ColorMask(
      true, true, true,
      !GetDrawingBuffer()->DefaultBufferRequiresAlphaChannelToBePreserved());
  GetDrawingBuffer()->ClearFramebuffers(clear_mask);

  // Call the DrawingBufferClient method to restore scissor test, mask, and
  // clear values, because we dirtied them above.
  DrawingBufferClientRestoreScissorTest();
  DrawingBufferClientRestoreMaskAndClearValues();

  GetDrawingBuffer()->SetBufferClearNeeded(false);

  return combined_clear ? kCombinedClear : kJustClear;
}

void WebGLRenderingContextBase::MarkCompositedAndClearBackbufferIfNeeded() {
  MarkLayerComposited();
  ClearIfComposited();
}

void WebGLRenderingContextBase::RestoreScissorEnabled() {
  if (isContextLost())
    return;

  if (scissor_enabled_) {
    ContextGL()->Enable(GL_SCISSOR_TEST);
  } else {
    ContextGL()->Disable(GL_SCISSOR_TEST);
  }
}

void WebGLRenderingContextBase::RestoreScissorBox() {
  if (isContextLost())
    return;

  ContextGL()->Scissor(scissor_box_[0], scissor_box_[1], scissor_box_[2],
                       scissor_box_[3]);
}

void WebGLRenderingContextBase::RestoreClearColor() {
  if (isContextLost())
    return;

  ContextGL()->ClearColor(clear_color_[0], clear_color_[1], clear_color_[2],
                          clear_color_[3]);
}

void WebGLRenderingContextBase::RestoreColorMask() {
  if (isContextLost())
    return;

  ContextGL()->ColorMask(color_mask_[0], color_mask_[1], color_mask_[2],
                         color_mask_[3]);
}

void WebGLRenderingContextBase::MarkLayerComposited() {
  if (!isContextLost())
    GetDrawingBuffer()->SetBufferClearNeeded(true);
}

void WebGLRenderingContextBase::SetIsHidden(bool hidden) {
  is_hidden_ = hidden;
  if (GetDrawingBuffer())
    GetDrawingBuffer()->SetIsHidden(hidden);

  if (!hidden && isContextLost() && restore_allowed_ &&
      auto_recovery_method_ == kAuto) {
    DCHECK(!restore_timer_.IsActive());
    restore_timer_.StartOneShot(0, BLINK_FROM_HERE);
  }
}

bool WebGLRenderingContextBase::PaintRenderingResultsToCanvas(
    SourceDrawingBuffer source_buffer) {
  if (isContextLost())
    return false;

  bool must_clear_now = ClearIfComposited() != kSkipped;
  if (!marked_canvas_dirty_ && !must_clear_now)
    return false;

  canvas()->ClearCopiedImage();
  marked_canvas_dirty_ = false;

  if (!canvas()->GetOrCreateImageBuffer())
    return false;

  ScopedTexture2DRestorer restorer(this);
  ScopedFramebufferRestorer fbo_restorer(this);

  GetDrawingBuffer()->ResolveAndBindForReadAndDraw();
  if (!canvas()
           ->GetOrCreateImageBuffer()
           ->CopyRenderingResultsFromDrawingBuffer(GetDrawingBuffer(),
                                                   source_buffer)) {
    // Currently, copyRenderingResultsFromDrawingBuffer is expected to always
    // succeed because cases where canvas()-buffer() is not accelerated are
    // handle before reaching this point.  If that assumption ever stops holding
    // true, we may need to implement a fallback right here.
    NOTREACHED();
    return false;
  }

  return true;
}

ImageData* WebGLRenderingContextBase::PaintRenderingResultsToImageData(
    SourceDrawingBuffer source_buffer) {
  if (isContextLost())
    return nullptr;
  if (CreationAttributes().premultipliedAlpha())
    return nullptr;

  ClearIfComposited();
  GetDrawingBuffer()->ResolveAndBindForReadAndDraw();
  ScopedFramebufferRestorer restorer(this);
  int width, height;
  WTF::ArrayBufferContents contents;
  if (!GetDrawingBuffer()->PaintRenderingResultsToImageData(
          width, height, source_buffer, contents))
    return nullptr;
  DOMArrayBuffer* image_data_pixels = DOMArrayBuffer::Create(contents);

  return ImageData::Create(
      IntSize(width, height),
      NotShared<DOMUint8ClampedArray>(DOMUint8ClampedArray::Create(
          image_data_pixels, 0, image_data_pixels->ByteLength())));
}

void WebGLRenderingContextBase::Reshape(int width, int height) {
  if (isContextLost())
    return;

  GLint buffer = 0;
  if (IsWebGL2OrHigher()) {
    // This query returns client side cached binding, so it's trivial.
    // If it changes in the future, such query is heavy and should be avoided.
    ContextGL()->GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
    if (buffer) {
      ContextGL()->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
  }

  // This is an approximation because at WebGLRenderingContextBase level we
  // don't know if the underlying FBO uses textures or renderbuffers.
  GLint max_size = std::min(max_texture_size_, max_renderbuffer_size_);
  GLint max_width = std::min(max_size, max_viewport_dims_[0]);
  GLint max_height = std::min(max_size, max_viewport_dims_[1]);
  width = Clamp(width, 1, max_width);
  height = Clamp(height, 1, max_height);

  // Limit drawing buffer area to 4k*4k to avoid memory exhaustion. Width or
  // height may be larger than 4k as long as it's within the max viewport
  // dimensions and total area remains within the limit.
  // For example: 5120x2880 should be fine.
  const int kMaxArea = 4096 * 4096;
  int current_area = width * height;
  if (current_area > kMaxArea) {
    // If we've exceeded the area limit scale the buffer down, preserving
    // ascpect ratio, until it fits.
    float scale_factor =
        sqrtf(static_cast<float>(kMaxArea) / static_cast<float>(current_area));
    width = std::max(1, static_cast<int>(width * scale_factor));
    height = std::max(1, static_cast<int>(height * scale_factor));
  }

  // We don't have to mark the canvas as dirty, since the newly created image
  // buffer will also start off clear (and this matches what reshape will do).
  GetDrawingBuffer()->Resize(IntSize(width, height));

  if (buffer) {
    ContextGL()->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                            static_cast<GLuint>(buffer));
  }
}

int WebGLRenderingContextBase::drawingBufferWidth() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->Size().Width();
}

int WebGLRenderingContextBase::drawingBufferHeight() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->Size().Height();
}

void WebGLRenderingContextBase::activeTexture(GLenum texture) {
  if (isContextLost())
    return;
  if (texture - GL_TEXTURE0 >= texture_units_.size()) {
    SynthesizeGLError(GL_INVALID_ENUM, "activeTexture",
                      "texture unit out of range");
    return;
  }
  active_texture_unit_ = texture - GL_TEXTURE0;
  ContextGL()->ActiveTexture(texture);
}

void WebGLRenderingContextBase::attachShader(WebGLProgram* program,
                                             WebGLShader* shader) {
  if (isContextLost() || !ValidateWebGLObject("attachShader", program) ||
      !ValidateWebGLObject("attachShader", shader))
    return;
  if (!program->AttachShader(shader)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "attachShader",
                      "shader attachment already has shader");
    return;
  }
  ContextGL()->AttachShader(ObjectOrZero(program), ObjectOrZero(shader));
  shader->OnAttached();
}

void WebGLRenderingContextBase::bindAttribLocation(WebGLProgram* program,
                                                   GLuint index,
                                                   const String& name) {
  if (isContextLost() || !ValidateWebGLObject("bindAttribLocation", program))
    return;
  if (!ValidateLocationLength("bindAttribLocation", name))
    return;
  if (IsPrefixReserved(name)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindAttribLocation",
                      "reserved prefix");
    return;
  }
  ContextGL()->BindAttribLocation(ObjectOrZero(program), index,
                                  name.Utf8().data());
}

bool WebGLRenderingContextBase::CheckObjectToBeBound(const char* function_name,
                                                     WebGLObject* object,
                                                     bool& deleted) {
  deleted = false;
  if (isContextLost())
    return false;
  if (object) {
    if (!object->Validate(ContextGroup(), this)) {
      SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                        "object not from this context");
      return false;
    }
    deleted = !object->HasObject();
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateAndUpdateBufferBindTarget(
    const char* function_name,
    GLenum target,
    WebGLBuffer* buffer) {
  if (!ValidateBufferTarget(function_name, target))
    return false;

  if (buffer && buffer->GetInitialTarget() &&
      buffer->GetInitialTarget() != target) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "buffers can not be used with multiple targets");
    return false;
  }

  switch (target) {
    case GL_ARRAY_BUFFER:
      bound_array_buffer_ = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      bound_vertex_array_object_->SetElementArrayBuffer(buffer);
      break;
    default:
      NOTREACHED();
      return false;
  }

  if (buffer && !buffer->GetInitialTarget())
    buffer->SetInitialTarget(target);
  return true;
}

void WebGLRenderingContextBase::bindBuffer(GLenum target, WebGLBuffer* buffer) {
  bool deleted;
  if (!CheckObjectToBeBound("bindBuffer", buffer, deleted))
    return;
  if (deleted) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindBuffer",
                      "attempt to bind a deleted buffer");
    return;
  }
  if (!ValidateAndUpdateBufferBindTarget("bindBuffer", target, buffer))
    return;
  ContextGL()->BindBuffer(target, ObjectOrZero(buffer));
}

void WebGLRenderingContextBase::bindFramebuffer(GLenum target,
                                                WebGLFramebuffer* buffer) {
  bool deleted;
  if (!CheckObjectToBeBound("bindFramebuffer", buffer, deleted))
    return;
  if (deleted) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindFramebuffer",
                      "attempt to bind a deleted framebuffer");
    return;
  }

  if (target != GL_FRAMEBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "bindFramebuffer", "invalid target");
    return;
  }

  SetFramebuffer(target, buffer);
}

void WebGLRenderingContextBase::bindRenderbuffer(
    GLenum target,
    WebGLRenderbuffer* render_buffer) {
  bool deleted;
  if (!CheckObjectToBeBound("bindRenderbuffer", render_buffer, deleted))
    return;
  if (deleted) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindRenderbuffer",
                      "attempt to bind a deleted renderbuffer");
    return;
  }
  if (target != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "bindRenderbuffer", "invalid target");
    return;
  }
  renderbuffer_binding_ = render_buffer;
  ContextGL()->BindRenderbuffer(target, ObjectOrZero(render_buffer));
  if (render_buffer)
    render_buffer->SetHasEverBeenBound();
}

void WebGLRenderingContextBase::bindTexture(GLenum target,
                                            WebGLTexture* texture) {
  bool deleted;
  if (!CheckObjectToBeBound("bindTexture", texture, deleted))
    return;
  if (deleted) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindTexture",
                      "attempt to bind a deleted texture");
    return;
  }
  if (texture && texture->GetTarget() && texture->GetTarget() != target) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindTexture",
                      "textures can not be used with multiple targets");
    return;
  }

  if (target == GL_TEXTURE_2D) {
    texture_units_[active_texture_unit_].texture2d_binding_ =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else if (target == GL_TEXTURE_CUBE_MAP) {
    texture_units_[active_texture_unit_].texture_cube_map_binding_ =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else if (IsWebGL2OrHigher() && target == GL_TEXTURE_2D_ARRAY) {
    texture_units_[active_texture_unit_].texture2d_array_binding_ =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else if (IsWebGL2OrHigher() && target == GL_TEXTURE_3D) {
    texture_units_[active_texture_unit_].texture3d_binding_ =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else {
    SynthesizeGLError(GL_INVALID_ENUM, "bindTexture", "invalid target");
    return;
  }

  ContextGL()->BindTexture(target, ObjectOrZero(texture));
  if (texture) {
    texture->SetTarget(target);
    one_plus_max_non_default_texture_unit_ =
        max(active_texture_unit_ + 1, one_plus_max_non_default_texture_unit_);
  } else {
    // If the disabled index is the current maximum, trace backwards to find the
    // new max enabled texture index
    if (one_plus_max_non_default_texture_unit_ == active_texture_unit_ + 1) {
      FindNewMaxNonDefaultTextureUnit();
    }
  }

  // Note: previously we used to automatically set the TEXTURE_WRAP_R
  // repeat mode to CLAMP_TO_EDGE for cube map textures, because OpenGL
  // ES 2.0 doesn't expose this flag (a bug in the specification) and
  // otherwise the application has no control over the seams in this
  // dimension. However, it appears that supporting this properly on all
  // platforms is fairly involved (will require a HashMap from texture ID
  // in all ports), and we have not had any complaints, so the logic has
  // been removed.
}

void WebGLRenderingContextBase::blendColor(GLfloat red,
                                           GLfloat green,
                                           GLfloat blue,
                                           GLfloat alpha) {
  if (isContextLost())
    return;
  ContextGL()->BlendColor(red, green, blue, alpha);
}

void WebGLRenderingContextBase::blendEquation(GLenum mode) {
  if (isContextLost() || !ValidateBlendEquation("blendEquation", mode))
    return;
  ContextGL()->BlendEquation(mode);
}

void WebGLRenderingContextBase::blendEquationSeparate(GLenum mode_rgb,
                                                      GLenum mode_alpha) {
  if (isContextLost() ||
      !ValidateBlendEquation("blendEquationSeparate", mode_rgb) ||
      !ValidateBlendEquation("blendEquationSeparate", mode_alpha))
    return;
  ContextGL()->BlendEquationSeparate(mode_rgb, mode_alpha);
}

void WebGLRenderingContextBase::blendFunc(GLenum sfactor, GLenum dfactor) {
  if (isContextLost() ||
      !ValidateBlendFuncFactors("blendFunc", sfactor, dfactor))
    return;
  ContextGL()->BlendFunc(sfactor, dfactor);
}

void WebGLRenderingContextBase::blendFuncSeparate(GLenum src_rgb,
                                                  GLenum dst_rgb,
                                                  GLenum src_alpha,
                                                  GLenum dst_alpha) {
  // Note: Alpha does not have the same restrictions as RGB.
  if (isContextLost() ||
      !ValidateBlendFuncFactors("blendFuncSeparate", src_rgb, dst_rgb))
    return;
  ContextGL()->BlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
}

void WebGLRenderingContextBase::BufferDataImpl(GLenum target,
                                               long long size,
                                               const void* data,
                                               GLenum usage) {
  WebGLBuffer* buffer = ValidateBufferDataTarget("bufferData", target);
  if (!buffer)
    return;

  if (!ValidateBufferDataUsage("bufferData", usage))
    return;

  if (!ValidateValueFitNonNegInt32("bufferData", "size", size))
    return;

  buffer->SetSize(size);

  ContextGL()->BufferData(target, static_cast<GLsizeiptr>(size), data, usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           long long size,
                                           GLenum usage) {
  if (isContextLost())
    return;
  BufferDataImpl(target, size, 0, usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           DOMArrayBuffer* data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  if (!data) {
    SynthesizeGLError(GL_INVALID_VALUE, "bufferData", "no data");
    return;
  }
  BufferDataImpl(target, data->ByteLength(), data->Data(), usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           MaybeShared<DOMArrayBufferView> data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  DCHECK(data);
  BufferDataImpl(target, data.View()->byteLength(),
                 data.View()->BaseAddressMaybeShared(), usage);
}

void WebGLRenderingContextBase::BufferSubDataImpl(GLenum target,
                                                  long long offset,
                                                  GLsizeiptr size,
                                                  const void* data) {
  WebGLBuffer* buffer = ValidateBufferDataTarget("bufferSubData", target);
  if (!buffer)
    return;
  if (!ValidateValueFitNonNegInt32("bufferSubData", "offset", offset))
    return;
  if (!data)
    return;
  if (offset + static_cast<long long>(size) > buffer->GetSize()) {
    SynthesizeGLError(GL_INVALID_VALUE, "bufferSubData", "buffer overflow");
    return;
  }

  ContextGL()->BufferSubData(target, static_cast<GLintptr>(offset), size, data);
}

void WebGLRenderingContextBase::bufferSubData(GLenum target,
                                              long long offset,
                                              DOMArrayBuffer* data) {
  if (isContextLost())
    return;
  DCHECK(data);
  BufferSubDataImpl(target, offset, data->ByteLength(), data->Data());
}

void WebGLRenderingContextBase::bufferSubData(
    GLenum target,
    long long offset,
    const FlexibleArrayBufferView& data) {
  if (isContextLost())
    return;
  DCHECK(data);
  BufferSubDataImpl(target, offset, data.ByteLength(),
                    data.BaseAddressMaybeOnStack());
}

bool WebGLRenderingContextBase::ValidateFramebufferTarget(GLenum target) {
  if (target == GL_FRAMEBUFFER)
    return true;
  return false;
}

WebGLFramebuffer* WebGLRenderingContextBase::GetFramebufferBinding(
    GLenum target) {
  if (target == GL_FRAMEBUFFER)
    return framebuffer_binding_.Get();
  return nullptr;
}

WebGLFramebuffer* WebGLRenderingContextBase::GetReadFramebufferBinding() {
  return framebuffer_binding_.Get();
}

GLenum WebGLRenderingContextBase::checkFramebufferStatus(GLenum target) {
  if (isContextLost())
    return GL_FRAMEBUFFER_UNSUPPORTED;
  if (!ValidateFramebufferTarget(target)) {
    SynthesizeGLError(GL_INVALID_ENUM, "checkFramebufferStatus",
                      "invalid target");
    return 0;
  }
  WebGLFramebuffer* framebuffer_binding = GetFramebufferBinding(target);
  if (framebuffer_binding) {
    const char* reason = "framebuffer incomplete";
    GLenum status = framebuffer_binding->CheckDepthStencilStatus(&reason);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      EmitGLWarning("checkFramebufferStatus", reason);
      return status;
    }
  }
  return ContextGL()->CheckFramebufferStatus(target);
}

void WebGLRenderingContextBase::clear(GLbitfield mask) {
  if (isContextLost())
    return;
  if (mask &
      ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
    SynthesizeGLError(GL_INVALID_VALUE, "clear", "invalid mask");
    return;
  }
  const char* reason = "framebuffer incomplete";
  if (framebuffer_binding_ && framebuffer_binding_->CheckDepthStencilStatus(
                                  &reason) != GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.Get());

  if (ClearIfComposited(mask) != kCombinedClear) {
    // If clearing the default back buffer's depth buffer, also clear the
    // stencil buffer, if one was allocated implicitly. This avoids performance
    // problems on some GPUs.
    if (!framebuffer_binding_ &&
        GetDrawingBuffer()->HasImplicitStencilBuffer() &&
        (mask & GL_DEPTH_BUFFER_BIT)) {
      // It shouldn't matter what value it's cleared to, since in other queries
      // in the API, we claim that the stencil buffer doesn't exist.
      mask |= GL_STENCIL_BUFFER_BIT;
    }
    ContextGL()->Clear(mask);
  }
  MarkContextChanged(kCanvasChanged);
}

void WebGLRenderingContextBase::clearColor(GLfloat r,
                                           GLfloat g,
                                           GLfloat b,
                                           GLfloat a) {
  if (isContextLost())
    return;
  if (std::isnan(r))
    r = 0;
  if (std::isnan(g))
    g = 0;
  if (std::isnan(b))
    b = 0;
  if (std::isnan(a))
    a = 1;
  clear_color_[0] = r;
  clear_color_[1] = g;
  clear_color_[2] = b;
  clear_color_[3] = a;
  ContextGL()->ClearColor(r, g, b, a);
}

void WebGLRenderingContextBase::clearDepth(GLfloat depth) {
  if (isContextLost())
    return;
  clear_depth_ = depth;
  ContextGL()->ClearDepthf(depth);
}

void WebGLRenderingContextBase::clearStencil(GLint s) {
  if (isContextLost())
    return;
  clear_stencil_ = s;
  ContextGL()->ClearStencil(s);
}

void WebGLRenderingContextBase::colorMask(GLboolean red,
                                          GLboolean green,
                                          GLboolean blue,
                                          GLboolean alpha) {
  if (isContextLost())
    return;
  color_mask_[0] = red;
  color_mask_[1] = green;
  color_mask_[2] = blue;
  color_mask_[3] = alpha;
  ContextGL()->ColorMask(red, green, blue, alpha);
}

void WebGLRenderingContextBase::compileShader(WebGLShader* shader) {
  if (isContextLost() || !ValidateWebGLObject("compileShader", shader))
    return;
  ContextGL()->CompileShader(ObjectOrZero(shader));
}

void WebGLRenderingContextBase::compressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    MaybeShared<DOMArrayBufferView> data) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("compressedTexImage2D", target))
    return;
  if (!ValidateCompressedTexFormat("compressedTexImage2D", internalformat))
    return;
  ContextGL()->CompressedTexImage2D(target, level, internalformat, width,
                                    height, border, data.View()->byteLength(),
                                    data.View()->BaseAddressMaybeShared());
}

void WebGLRenderingContextBase::compressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    MaybeShared<DOMArrayBufferView> data) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("compressedTexSubImage2D", target))
    return;
  if (!ValidateCompressedTexFormat("compressedTexSubImage2D", format))
    return;
  ContextGL()->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format,
      data.View()->byteLength(), data.View()->BaseAddressMaybeShared());
}

bool WebGLRenderingContextBase::ValidateSettableTexFormat(
    const char* function_name,
    GLenum format) {
  if (IsWebGL2OrHigher())
    return true;

  if (WebGLImageConversion::GetChannelBitsByFormat(format) &
      WebGLImageConversion::kChannelDepthStencil) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "format can not be set, only rendered to");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateCopyTexFormat(const char* function_name,
                                                      GLenum internalformat) {
  if (!is_web_gl2_internal_formats_copy_tex_image_added_ &&
      IsWebGL2OrHigher()) {
    ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                      kSupportedInternalFormatsES3);
    is_web_gl2_internal_formats_copy_tex_image_added_ = true;
  }
  if (!is_ext_color_buffer_float_formats_added_ &&
      ExtensionEnabled(kEXTColorBufferFloatName)) {
    ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                      kSupportedInternalFormatsCopyTexImageFloatES3);
    is_ext_color_buffer_float_formats_added_ = true;
  }

  if (supported_internal_formats_copy_tex_image_.find(internalformat) ==
      supported_internal_formats_copy_tex_image_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid internalformat");
    return false;
  }

  return true;
}

void WebGLRenderingContextBase::copyTexImage2D(GLenum target,
                                               GLint level,
                                               GLenum internalformat,
                                               GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("copyTexImage2D", target))
    return;
  if (!ValidateCopyTexFormat("copyTexImage2D", internalformat))
    return;
  if (!ValidateSettableTexFormat("copyTexImage2D", internalformat))
    return;
  WebGLFramebuffer* read_framebuffer_binding = nullptr;
  if (!ValidateReadBufferAndGetInfo("copyTexImage2D", read_framebuffer_binding))
    return;
  ClearIfComposited();
  ScopedDrawingBufferBinder binder(GetDrawingBuffer(),
                                   read_framebuffer_binding);
  ContextGL()->CopyTexImage2D(target, level, internalformat, x, y, width,
                              height, border);
}

void WebGLRenderingContextBase::copyTexSubImage2D(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLint x,
                                                  GLint y,
                                                  GLsizei width,
                                                  GLsizei height) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("copyTexSubImage2D", target))
    return;
  WebGLFramebuffer* read_framebuffer_binding = nullptr;
  if (!ValidateReadBufferAndGetInfo("copyTexSubImage2D",
                                    read_framebuffer_binding))
    return;
  ClearIfComposited();
  ScopedDrawingBufferBinder binder(GetDrawingBuffer(),
                                   read_framebuffer_binding);
  ContextGL()->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

WebGLBuffer* WebGLRenderingContextBase::createBuffer() {
  if (isContextLost())
    return nullptr;
  return WebGLBuffer::Create(this);
}

WebGLFramebuffer* WebGLRenderingContextBase::createFramebuffer() {
  if (isContextLost())
    return nullptr;
  return WebGLFramebuffer::Create(this);
}

WebGLTexture* WebGLRenderingContextBase::createTexture() {
  if (isContextLost())
    return nullptr;
  return WebGLTexture::Create(this);
}

WebGLProgram* WebGLRenderingContextBase::createProgram() {
  if (isContextLost())
    return nullptr;
  return WebGLProgram::Create(this);
}

WebGLRenderbuffer* WebGLRenderingContextBase::createRenderbuffer() {
  if (isContextLost())
    return nullptr;
  return WebGLRenderbuffer::Create(this);
}

void WebGLRenderingContextBase::SetBoundVertexArrayObject(
    WebGLVertexArrayObjectBase* array_object) {
  if (array_object)
    bound_vertex_array_object_ = array_object;
  else
    bound_vertex_array_object_ = default_vertex_array_object_;
}

WebGLShader* WebGLRenderingContextBase::createShader(GLenum type) {
  if (isContextLost())
    return nullptr;
  if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER) {
    SynthesizeGLError(GL_INVALID_ENUM, "createShader", "invalid shader type");
    return nullptr;
  }

  return WebGLShader::Create(this, type);
}

void WebGLRenderingContextBase::cullFace(GLenum mode) {
  if (isContextLost())
    return;
  ContextGL()->CullFace(mode);
}

bool WebGLRenderingContextBase::DeleteObject(WebGLObject* object) {
  if (isContextLost() || !object)
    return false;
  if (!object->Validate(ContextGroup(), this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "delete",
                      "object does not belong to this context");
    return false;
  }
  if (object->HasObject()) {
    // We need to pass in context here because we want
    // things in this context unbound.
    object->DeleteObject(ContextGL());
  }
  return true;
}

void WebGLRenderingContextBase::deleteBuffer(WebGLBuffer* buffer) {
  if (!DeleteObject(buffer))
    return;
  RemoveBoundBuffer(buffer);
}

void WebGLRenderingContextBase::deleteFramebuffer(
    WebGLFramebuffer* framebuffer) {
  if (!DeleteObject(framebuffer))
    return;
  if (framebuffer == framebuffer_binding_) {
    framebuffer_binding_ = nullptr;
    // Have to call drawingBuffer()->bind() here to bind back to internal fbo.
    GetDrawingBuffer()->Bind(GL_FRAMEBUFFER);
  }
}

void WebGLRenderingContextBase::deleteProgram(WebGLProgram* program) {
  DeleteObject(program);
  // We don't reset m_currentProgram to 0 here because the deletion of the
  // current program is delayed.
}

void WebGLRenderingContextBase::deleteRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  if (!DeleteObject(renderbuffer))
    return;
  if (renderbuffer == renderbuffer_binding_) {
    renderbuffer_binding_ = nullptr;
  }
  if (framebuffer_binding_)
    framebuffer_binding_->RemoveAttachmentFromBoundFramebuffer(GL_FRAMEBUFFER,
                                                               renderbuffer);
  if (GetFramebufferBinding(GL_READ_FRAMEBUFFER))
    GetFramebufferBinding(GL_READ_FRAMEBUFFER)
        ->RemoveAttachmentFromBoundFramebuffer(GL_READ_FRAMEBUFFER,
                                               renderbuffer);
}

void WebGLRenderingContextBase::deleteShader(WebGLShader* shader) {
  DeleteObject(shader);
}

void WebGLRenderingContextBase::deleteTexture(WebGLTexture* texture) {
  if (!DeleteObject(texture))
    return;

  int max_bound_texture_index = -1;
  for (size_t i = 0; i < one_plus_max_non_default_texture_unit_; ++i) {
    if (texture == texture_units_[i].texture2d_binding_) {
      texture_units_[i].texture2d_binding_ = nullptr;
      max_bound_texture_index = i;
    }
    if (texture == texture_units_[i].texture_cube_map_binding_) {
      texture_units_[i].texture_cube_map_binding_ = nullptr;
      max_bound_texture_index = i;
    }
    if (IsWebGL2OrHigher()) {
      if (texture == texture_units_[i].texture3d_binding_) {
        texture_units_[i].texture3d_binding_ = nullptr;
        max_bound_texture_index = i;
      }
      if (texture == texture_units_[i].texture2d_array_binding_) {
        texture_units_[i].texture2d_array_binding_ = nullptr;
        max_bound_texture_index = i;
      }
    }
  }
  if (framebuffer_binding_)
    framebuffer_binding_->RemoveAttachmentFromBoundFramebuffer(GL_FRAMEBUFFER,
                                                               texture);
  if (GetFramebufferBinding(GL_READ_FRAMEBUFFER))
    GetFramebufferBinding(GL_READ_FRAMEBUFFER)
        ->RemoveAttachmentFromBoundFramebuffer(GL_READ_FRAMEBUFFER, texture);

  // If the deleted was bound to the the current maximum index, trace backwards
  // to find the new max texture index.
  if (one_plus_max_non_default_texture_unit_ ==
      static_cast<unsigned long>(max_bound_texture_index + 1)) {
    FindNewMaxNonDefaultTextureUnit();
  }
}

void WebGLRenderingContextBase::depthFunc(GLenum func) {
  if (isContextLost())
    return;
  ContextGL()->DepthFunc(func);
}

void WebGLRenderingContextBase::depthMask(GLboolean flag) {
  if (isContextLost())
    return;
  depth_mask_ = flag;
  ContextGL()->DepthMask(flag);
}

void WebGLRenderingContextBase::depthRange(GLfloat z_near, GLfloat z_far) {
  if (isContextLost())
    return;
  // Check required by WebGL spec section 6.12
  if (z_near > z_far) {
    SynthesizeGLError(GL_INVALID_OPERATION, "depthRange", "zNear > zFar");
    return;
  }
  ContextGL()->DepthRangef(z_near, z_far);
}

void WebGLRenderingContextBase::detachShader(WebGLProgram* program,
                                             WebGLShader* shader) {
  if (isContextLost() || !ValidateWebGLObject("detachShader", program) ||
      !ValidateWebGLObject("detachShader", shader))
    return;
  if (!program->DetachShader(shader)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "detachShader",
                      "shader not attached");
    return;
  }
  ContextGL()->DetachShader(ObjectOrZero(program), ObjectOrZero(shader));
  shader->OnDetached(ContextGL());
}

void WebGLRenderingContextBase::disable(GLenum cap) {
  if (isContextLost() || !ValidateCapability("disable", cap))
    return;
  if (cap == GL_STENCIL_TEST) {
    stencil_enabled_ = false;
    ApplyStencilTest();
    return;
  }
  if (cap == GL_SCISSOR_TEST)
    scissor_enabled_ = false;
  ContextGL()->Disable(cap);
}

void WebGLRenderingContextBase::disableVertexAttribArray(GLuint index) {
  if (isContextLost())
    return;
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "disableVertexAttribArray",
                      "index out of range");
    return;
  }

  bound_vertex_array_object_->SetAttribEnabled(index, false);
  ContextGL()->DisableVertexAttribArray(index);
}

bool WebGLRenderingContextBase::ValidateRenderingState(
    const char* function_name) {
  // Command buffer will not error if no program is bound.
  if (!current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no valid shader program in use");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateWebGLObject(const char* function_name,
                                                    WebGLObject* object) {
  DCHECK(object);
  if (!object->HasObject()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "no object or object deleted");
    return false;
  }
  if (!object->Validate(ContextGroup(), this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "object does not belong to this context");
    return false;
  }
  return true;
}

void WebGLRenderingContextBase::drawArrays(GLenum mode,
                                           GLint first,
                                           GLsizei count) {
  if (!ValidateDrawArrays("drawArrays"))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawArrays",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.Get());
  ClearIfComposited();
  ContextGL()->DrawArrays(mode, first, count);
  MarkContextChanged(kCanvasChanged);
}

void WebGLRenderingContextBase::drawElements(GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             long long offset) {
  if (!ValidateDrawElements("drawElements", type, offset))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawElements",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.Get());
  ClearIfComposited();
  ContextGL()->DrawElements(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
  MarkContextChanged(kCanvasChanged);
}

void WebGLRenderingContextBase::DrawArraysInstancedANGLE(GLenum mode,
                                                         GLint first,
                                                         GLsizei count,
                                                         GLsizei primcount) {
  if (!ValidateDrawArrays("drawArraysInstancedANGLE"))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawArraysInstancedANGLE",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.Get());
  ClearIfComposited();
  ContextGL()->DrawArraysInstancedANGLE(mode, first, count, primcount);
  MarkContextChanged(kCanvasChanged);
}

void WebGLRenderingContextBase::DrawElementsInstancedANGLE(GLenum mode,
                                                           GLsizei count,
                                                           GLenum type,
                                                           long long offset,
                                                           GLsizei primcount) {
  if (!ValidateDrawElements("drawElementsInstancedANGLE", type, offset))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawElementsInstancedANGLE",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.Get());
  ClearIfComposited();
  ContextGL()->DrawElementsInstancedANGLE(
      mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)),
      primcount);
  MarkContextChanged(kCanvasChanged);
}

void WebGLRenderingContextBase::enable(GLenum cap) {
  if (isContextLost() || !ValidateCapability("enable", cap))
    return;
  if (cap == GL_STENCIL_TEST) {
    stencil_enabled_ = true;
    ApplyStencilTest();
    return;
  }
  if (cap == GL_SCISSOR_TEST)
    scissor_enabled_ = true;
  ContextGL()->Enable(cap);
}

void WebGLRenderingContextBase::enableVertexAttribArray(GLuint index) {
  if (isContextLost())
    return;
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "enableVertexAttribArray",
                      "index out of range");
    return;
  }

  bound_vertex_array_object_->SetAttribEnabled(index, true);
  ContextGL()->EnableVertexAttribArray(index);
}

void WebGLRenderingContextBase::finish() {
  if (isContextLost())
    return;
  ContextGL()->Flush();  // Intentionally a flush, not a finish.
}

void WebGLRenderingContextBase::flush() {
  if (isContextLost())
    return;
  ContextGL()->Flush();
}

void WebGLRenderingContextBase::framebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    WebGLRenderbuffer* buffer) {
  if (isContextLost() || !ValidateFramebufferFuncParameters(
                             "framebufferRenderbuffer", target, attachment))
    return;
  if (renderbuffertarget != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "framebufferRenderbuffer",
                      "invalid target");
    return;
  }
  if (buffer && (!buffer->HasEverBeenBound() ||
                 !buffer->Validate(ContextGroup(), this))) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer",
                      "buffer never bound or buffer not from this context");
    return;
  }
  // Don't allow the default framebuffer to be mutated; all current
  // implementations use an FBO internally in place of the default
  // FBO.
  WebGLFramebuffer* framebuffer_binding = GetFramebufferBinding(target);
  if (!framebuffer_binding || !framebuffer_binding->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer",
                      "no framebuffer bound");
    return;
  }
  framebuffer_binding->SetAttachmentForBoundFramebuffer(target, attachment,
                                                        buffer);
  ApplyStencilTest();
}

void WebGLRenderingContextBase::framebufferTexture2D(GLenum target,
                                                     GLenum attachment,
                                                     GLenum textarget,
                                                     WebGLTexture* texture,
                                                     GLint level) {
  if (isContextLost() || !ValidateFramebufferFuncParameters(
                             "framebufferTexture2D", target, attachment))
    return;
  if (texture && !texture->Validate(ContextGroup(), this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D",
                      "no texture or texture not from this context");
    return;
  }
  // Don't allow the default framebuffer to be mutated; all current
  // implementations use an FBO internally in place of the default
  // FBO.
  WebGLFramebuffer* framebuffer_binding = GetFramebufferBinding(target);
  if (!framebuffer_binding || !framebuffer_binding->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D",
                      "no framebuffer bound");
    return;
  }
  framebuffer_binding->SetAttachmentForBoundFramebuffer(
      target, attachment, textarget, texture, level, 0);
  ApplyStencilTest();
}

void WebGLRenderingContextBase::frontFace(GLenum mode) {
  if (isContextLost())
    return;
  ContextGL()->FrontFace(mode);
}

void WebGLRenderingContextBase::generateMipmap(GLenum target) {
  if (isContextLost())
    return;
  if (!ValidateTextureBinding("generateMipmap", target))
    return;
  ContextGL()->GenerateMipmap(target);
}

WebGLActiveInfo* WebGLRenderingContextBase::getActiveAttrib(
    WebGLProgram* program,
    GLuint index) {
  if (isContextLost() || !ValidateWebGLObject("getActiveAttrib", program))
    return nullptr;
  GLuint program_id = ObjectNonZero(program);
  GLint max_name_length = -1;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                            &max_name_length);
  if (max_name_length < 0)
    return nullptr;
  if (max_name_length == 0) {
    SynthesizeGLError(GL_INVALID_VALUE, "getActiveAttrib",
                      "no active attributes exist");
    return nullptr;
  }
  LChar* name_ptr;
  RefPtr<StringImpl> name_impl =
      StringImpl::CreateUninitialized(max_name_length, name_ptr);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  ContextGL()->GetActiveAttrib(program_id, index, max_name_length, &length,
                               &size, &type,
                               reinterpret_cast<GLchar*>(name_ptr));
  if (size < 0)
    return nullptr;
  return WebGLActiveInfo::Create(name_impl->Substring(0, length), type, size);
}

WebGLActiveInfo* WebGLRenderingContextBase::getActiveUniform(
    WebGLProgram* program,
    GLuint index) {
  if (isContextLost() || !ValidateWebGLObject("getActiveUniform", program))
    return nullptr;
  GLuint program_id = ObjectNonZero(program);
  GLint max_name_length = -1;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                            &max_name_length);
  if (max_name_length < 0)
    return nullptr;
  if (max_name_length == 0) {
    SynthesizeGLError(GL_INVALID_VALUE, "getActiveUniform",
                      "no active uniforms exist");
    return nullptr;
  }
  LChar* name_ptr;
  RefPtr<StringImpl> name_impl =
      StringImpl::CreateUninitialized(max_name_length, name_ptr);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  ContextGL()->GetActiveUniform(program_id, index, max_name_length, &length,
                                &size, &type,
                                reinterpret_cast<GLchar*>(name_ptr));
  if (size < 0)
    return nullptr;
  return WebGLActiveInfo::Create(name_impl->Substring(0, length), type, size);
}

Nullable<HeapVector<Member<WebGLShader>>>
WebGLRenderingContextBase::getAttachedShaders(WebGLProgram* program) {
  if (isContextLost() || !ValidateWebGLObject("getAttachedShaders", program))
    return nullptr;

  HeapVector<Member<WebGLShader>> shader_objects;
  const GLenum kShaderType[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
  for (unsigned i = 0; i < sizeof(kShaderType) / sizeof(GLenum); ++i) {
    WebGLShader* shader = program->GetAttachedShader(kShaderType[i]);
    if (shader)
      shader_objects.push_back(shader);
  }
  return shader_objects;
}

GLint WebGLRenderingContextBase::getAttribLocation(WebGLProgram* program,
                                                   const String& name) {
  if (isContextLost() || !ValidateWebGLObject("getAttribLocation", program))
    return -1;
  if (!ValidateLocationLength("getAttribLocation", name))
    return -1;
  if (!ValidateString("getAttribLocation", name))
    return -1;
  if (IsPrefixReserved(name))
    return -1;
  if (!program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getAttribLocation",
                      "program not linked");
    return 0;
  }
  return ContextGL()->GetAttribLocation(ObjectOrZero(program),
                                        name.Utf8().data());
}

bool WebGLRenderingContextBase::ValidateBufferTarget(const char* function_name,
                                                     GLenum target) {
  switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return false;
  }
}

ScriptValue WebGLRenderingContextBase::getBufferParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum pname) {
  if (isContextLost() || !ValidateBufferTarget("getBufferParameter", target))
    return ScriptValue::CreateNull(script_state);

  switch (pname) {
    case GL_BUFFER_USAGE: {
      GLint value = 0;
      ContextGL()->GetBufferParameteriv(target, pname, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));
    }
    case GL_BUFFER_SIZE: {
      GLint value = 0;
      ContextGL()->GetBufferParameteriv(target, pname, &value);
      if (!IsWebGL2OrHigher())
        return WebGLAny(script_state, value);
      return WebGLAny(script_state, static_cast<GLint64>(value));
    }
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getBufferParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

void WebGLRenderingContextBase::getContextAttributes(
    Nullable<WebGLContextAttributes>& result) {
  if (isContextLost())
    return;
  result.Set(ToWebGLContextAttributes(CreationAttributes()));
  // Some requested attributes may not be honored, so we need to query the
  // underlying context/drawing buffer and adjust accordingly.
  if (CreationAttributes().depth() && !GetDrawingBuffer()->HasDepthBuffer())
    result.Get().setDepth(false);
  if (CreationAttributes().stencil() && !GetDrawingBuffer()->HasStencilBuffer())
    result.Get().setStencil(false);
  result.Get().setAntialias(GetDrawingBuffer()->Multisample());
}

GLenum WebGLRenderingContextBase::getError() {
  if (!lost_context_errors_.IsEmpty()) {
    GLenum error = lost_context_errors_.front();
    lost_context_errors_.erase(0);
    return error;
  }

  if (isContextLost())
    return GL_NO_ERROR;

  if (!synthetic_errors_.IsEmpty()) {
    GLenum error = synthetic_errors_.front();
    synthetic_errors_.erase(0);
    return error;
  }

  return ContextGL()->GetError();
}

const char* const* WebGLRenderingContextBase::ExtensionTracker::Prefixes()
    const {
  static const char* const kUnprefixed[] = {
      "", 0,
  };
  return prefixes_ ? prefixes_ : kUnprefixed;
}

bool WebGLRenderingContextBase::ExtensionTracker::MatchesNameWithPrefixes(
    const String& name) const {
  const char* const* prefix_set = Prefixes();
  for (; *prefix_set; ++prefix_set) {
    String prefixed_name = String(*prefix_set) + ExtensionName();
    if (DeprecatedEqualIgnoringCase(prefixed_name, name)) {
      return true;
    }
  }
  return false;
}

bool WebGLRenderingContextBase::ExtensionSupportedAndAllowed(
    const ExtensionTracker* tracker) {
  if (tracker->Draft() &&
      !RuntimeEnabledFeatures::WebGLDraftExtensionsEnabled())
    return false;
  if (!tracker->Supported(this))
    return false;
  return true;
}

ScriptValue WebGLRenderingContextBase::getExtension(ScriptState* script_state,
                                                    const String& name) {
  WebGLExtension* extension = nullptr;

  if (!isContextLost()) {
    for (size_t i = 0; i < extensions_.size(); ++i) {
      ExtensionTracker* tracker = extensions_[i];
      if (tracker->MatchesNameWithPrefixes(name)) {
        if (ExtensionSupportedAndAllowed(tracker)) {
          extension = tracker->GetExtension(this);
          if (extension) {
            if (!extension_enabled_[extension->GetName()]) {
              extension_enabled_[extension->GetName()] = true;
            }
          }
        }
        break;
      }
    }
  }

  v8::Local<v8::Value> wrapped_extension =
      ToV8(extension, script_state->GetContext()->Global(),
           script_state->GetIsolate());

  return ScriptValue(script_state, wrapped_extension);
}

ScriptValue WebGLRenderingContextBase::getFramebufferAttachmentParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum attachment,
    GLenum pname) {
  if (isContextLost() ||
      !ValidateFramebufferFuncParameters("getFramebufferAttachmentParameter",
                                         target, attachment))
    return ScriptValue::CreateNull(script_state);

  if (!framebuffer_binding_ || !framebuffer_binding_->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getFramebufferAttachmentParameter",
                      "no framebuffer bound");
    return ScriptValue::CreateNull(script_state);
  }

  WebGLSharedObject* attachment_object =
      framebuffer_binding_->GetAttachmentObject(attachment);
  if (!attachment_object) {
    if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
      return WebGLAny(script_state, GL_NONE);
    // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
    // specifies INVALID_OPERATION.
    SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                      "invalid parameter name");
    return ScriptValue::CreateNull(script_state);
  }

  DCHECK(attachment_object->IsTexture() || attachment_object->IsRenderbuffer());
  if (attachment_object->IsTexture()) {
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return WebGLAny(script_state, GL_TEXTURE);
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return WebGLAny(script_state, attachment_object);
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: {
        GLint value = 0;
        ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                         pname, &value);
        return WebGLAny(script_state, value);
      }
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
        if (ExtensionEnabled(kEXTsRGBName)) {
          GLint value = 0;
          ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                           pname, &value);
          return WebGLAny(script_state, static_cast<unsigned>(value));
        }
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::CreateNull(script_state);
      default:
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for texture attachment");
        return ScriptValue::CreateNull(script_state);
    }
  } else {
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return WebGLAny(script_state, GL_RENDERBUFFER);
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return WebGLAny(script_state, attachment_object);
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
        if (ExtensionEnabled(kEXTsRGBName)) {
          GLint value = 0;
          ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                           pname, &value);
          return WebGLAny(script_state, value);
        }
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::CreateNull(script_state);
      default:
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::CreateNull(script_state);
    }
  }
}

ScriptValue WebGLRenderingContextBase::getParameter(ScriptState* script_state,
                                                    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state);
  const int kIntZero = 0;
  switch (pname) {
    case GL_ACTIVE_TEXTURE:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_ALIASED_LINE_WIDTH_RANGE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_ALIASED_POINT_SIZE_RANGE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_ALPHA_BITS:
      if (drawing_buffer_->RequiresAlphaChannelToBePreserved())
        return WebGLAny(script_state, 0);
      return GetIntParameter(script_state, pname);
    case GL_ARRAY_BUFFER_BINDING:
      return WebGLAny(script_state, bound_array_buffer_.Get());
    case GL_BLEND:
      return GetBooleanParameter(script_state, pname);
    case GL_BLEND_COLOR:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_BLEND_DST_ALPHA:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_DST_RGB:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_EQUATION_ALPHA:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_EQUATION_RGB:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_SRC_ALPHA:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_SRC_RGB:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLUE_BITS:
      return GetIntParameter(script_state, pname);
    case GL_COLOR_CLEAR_VALUE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_COLOR_WRITEMASK:
      return GetBooleanArrayParameter(script_state, pname);
    case GL_COMPRESSED_TEXTURE_FORMATS:
      return WebGLAny(script_state, DOMUint32Array::Create(
                                        compressed_texture_formats_.data(),
                                        compressed_texture_formats_.size()));
    case GL_CULL_FACE:
      return GetBooleanParameter(script_state, pname);
    case GL_CULL_FACE_MODE:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_CURRENT_PROGRAM:
      return WebGLAny(script_state, current_program_.Get());
    case GL_DEPTH_BITS:
      if (!framebuffer_binding_ && !CreationAttributes().depth())
        return WebGLAny(script_state, kIntZero);
      return GetIntParameter(script_state, pname);
    case GL_DEPTH_CLEAR_VALUE:
      return GetFloatParameter(script_state, pname);
    case GL_DEPTH_FUNC:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_DEPTH_RANGE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_DEPTH_TEST:
      return GetBooleanParameter(script_state, pname);
    case GL_DEPTH_WRITEMASK:
      return GetBooleanParameter(script_state, pname);
    case GL_DITHER:
      return GetBooleanParameter(script_state, pname);
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      return WebGLAny(script_state,
                      bound_vertex_array_object_->BoundElementArrayBuffer());
    case GL_FRAMEBUFFER_BINDING:
      return WebGLAny(script_state, framebuffer_binding_.Get());
    case GL_FRONT_FACE:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_GENERATE_MIPMAP_HINT:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_GREEN_BITS:
      return GetIntParameter(script_state, pname);
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      return GetIntParameter(script_state, pname);
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      return GetIntParameter(script_state, pname);
    case GL_LINE_WIDTH:
      return GetFloatParameter(script_state, pname);
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return GetIntParameter(script_state, pname);
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_RENDERBUFFER_SIZE:
      return GetIntParameter(script_state, pname);
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_TEXTURE_SIZE:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VARYING_VECTORS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VERTEX_ATTRIBS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VIEWPORT_DIMS:
      return GetWebGLIntArrayParameter(script_state, pname);
    case GL_NUM_SHADER_BINARY_FORMATS:
      // FIXME: should we always return 0 for this?
      return GetIntParameter(script_state, pname);
    case GL_PACK_ALIGNMENT:
      return GetIntParameter(script_state, pname);
    case GL_POLYGON_OFFSET_FACTOR:
      return GetFloatParameter(script_state, pname);
    case GL_POLYGON_OFFSET_FILL:
      return GetBooleanParameter(script_state, pname);
    case GL_POLYGON_OFFSET_UNITS:
      return GetFloatParameter(script_state, pname);
    case GL_RED_BITS:
      return GetIntParameter(script_state, pname);
    case GL_RENDERBUFFER_BINDING:
      return WebGLAny(script_state, renderbuffer_binding_.Get());
    case GL_RENDERER:
      return WebGLAny(script_state, String("WebKit WebGL"));
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      return GetBooleanParameter(script_state, pname);
    case GL_SAMPLE_BUFFERS:
      return GetIntParameter(script_state, pname);
    case GL_SAMPLE_COVERAGE:
      return GetBooleanParameter(script_state, pname);
    case GL_SAMPLE_COVERAGE_INVERT:
      return GetBooleanParameter(script_state, pname);
    case GL_SAMPLE_COVERAGE_VALUE:
      return GetFloatParameter(script_state, pname);
    case GL_SAMPLES:
      return GetIntParameter(script_state, pname);
    case GL_SCISSOR_BOX:
      return GetWebGLIntArrayParameter(script_state, pname);
    case GL_SCISSOR_TEST:
      return GetBooleanParameter(script_state, pname);
    case GL_SHADING_LANGUAGE_VERSION:
      return WebGLAny(
          script_state,
          "WebGL GLSL ES 1.0 (" +
              String(ContextGL()->GetString(GL_SHADING_LANGUAGE_VERSION)) +
              ")");
    case GL_STENCIL_BACK_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_FUNC:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_REF:
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_BACK_VALUE_MASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_WRITEMASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BITS:
      if (!framebuffer_binding_ && !CreationAttributes().stencil())
        return WebGLAny(script_state, kIntZero);
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_CLEAR_VALUE:
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_FUNC:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_PASS_DEPTH_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_PASS_DEPTH_PASS:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_REF:
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_TEST:
      return WebGLAny(script_state, stencil_enabled_);
    case GL_STENCIL_VALUE_MASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_WRITEMASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_SUBPIXEL_BITS:
      return GetIntParameter(script_state, pname);
    case GL_TEXTURE_BINDING_2D:
      return WebGLAny(
          script_state,
          texture_units_[active_texture_unit_].texture2d_binding_.Get());
    case GL_TEXTURE_BINDING_CUBE_MAP:
      return WebGLAny(
          script_state,
          texture_units_[active_texture_unit_].texture_cube_map_binding_.Get());
    case GL_UNPACK_ALIGNMENT:
      return GetIntParameter(script_state, pname);
    case GC3D_UNPACK_FLIP_Y_WEBGL:
      return WebGLAny(script_state, unpack_flip_y_);
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      return WebGLAny(script_state, unpack_premultiply_alpha_);
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      return WebGLAny(script_state, unpack_colorspace_conversion_);
    case GL_VENDOR:
      return WebGLAny(script_state, String("WebKit"));
    case GL_VERSION:
      return WebGLAny(
          script_state,
          "WebGL 1.0 (" + String(ContextGL()->GetString(GL_VERSION)) + ")");
    case GL_VIEWPORT:
      return GetWebGLIntArrayParameter(script_state, pname);
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:  // OES_standard_derivatives
      if (ExtensionEnabled(kOESStandardDerivativesName) || IsWebGL2OrHigher())
        return GetUnsignedIntParameter(script_state,
                                       GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, OES_standard_derivatives not enabled");
      return ScriptValue::CreateNull(script_state);
    case WebGLDebugRendererInfo::kUnmaskedRendererWebgl:
      if (ExtensionEnabled(kWebGLDebugRendererInfoName))
        return WebGLAny(script_state,
                        String(ContextGL()->GetString(GL_RENDERER)));
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_debug_renderer_info not enabled");
      return ScriptValue::CreateNull(script_state);
    case WebGLDebugRendererInfo::kUnmaskedVendorWebgl:
      if (ExtensionEnabled(kWebGLDebugRendererInfoName))
        return WebGLAny(script_state,
                        String(ContextGL()->GetString(GL_VENDOR)));
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_debug_renderer_info not enabled");
      return ScriptValue::CreateNull(script_state);
    case GL_VERTEX_ARRAY_BINDING_OES:  // OES_vertex_array_object
      if (ExtensionEnabled(kOESVertexArrayObjectName) || IsWebGL2OrHigher()) {
        if (!bound_vertex_array_object_->IsDefaultObject())
          return WebGLAny(script_state, bound_vertex_array_object_.Get());
        return ScriptValue::CreateNull(script_state);
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, OES_vertex_array_object not enabled");
      return ScriptValue::CreateNull(script_state);
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (ExtensionEnabled(kEXTTextureFilterAnisotropicName))
        return GetUnsignedIntParameter(script_state,
                                       GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
      return ScriptValue::CreateNull(script_state);
    case GL_MAX_COLOR_ATTACHMENTS_EXT:  // EXT_draw_buffers BEGIN
      if (ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2OrHigher())
        return WebGLAny(script_state, MaxColorAttachments());
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_draw_buffers not enabled");
      return ScriptValue::CreateNull(script_state);
    case GL_MAX_DRAW_BUFFERS_EXT:
      if (ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2OrHigher())
        return WebGLAny(script_state, MaxDrawBuffers());
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_draw_buffers not enabled");
      return ScriptValue::CreateNull(script_state);
    case GL_TIMESTAMP_EXT:
      if (ExtensionEnabled(kEXTDisjointTimerQueryName))
        return WebGLAny(script_state, 0);
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_disjoint_timer_query not enabled");
      return ScriptValue::CreateNull(script_state);
    case GL_GPU_DISJOINT_EXT:
      if (ExtensionEnabled(kEXTDisjointTimerQueryName))
        return GetBooleanParameter(script_state, GL_GPU_DISJOINT_EXT);
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_disjoint_timer_query not enabled");
      return ScriptValue::CreateNull(script_state);

    default:
      if ((ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2OrHigher()) &&
          pname >= GL_DRAW_BUFFER0_EXT &&
          pname < static_cast<GLenum>(GL_DRAW_BUFFER0_EXT + MaxDrawBuffers())) {
        GLint value = GL_NONE;
        if (framebuffer_binding_)
          value = framebuffer_binding_->GetDrawBuffer(pname);
        else  // emulated backbuffer
          value = back_draw_buffer_;
        return WebGLAny(script_state, value);
      }
      SynthesizeGLError(GL_INVALID_ENUM, "getParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

ScriptValue WebGLRenderingContextBase::getProgramParameter(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum pname) {
  if (isContextLost() || !ValidateWebGLObject("getProgramParameter", program))
    return ScriptValue::CreateNull(script_state);

  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(script_state, program->IsDeleted());
    case GL_VALIDATE_STATUS:
      ContextGL()->GetProgramiv(ObjectOrZero(program), pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));
    case GL_LINK_STATUS:
      return WebGLAny(script_state, program->LinkStatus(this));
    case GL_ACTIVE_UNIFORM_BLOCKS:
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
      if (!IsWebGL2OrHigher()) {
        SynthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                          "invalid parameter name");
        return ScriptValue::CreateNull(script_state);
      }
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_UNIFORMS:
      ContextGL()->GetProgramiv(ObjectOrZero(program), pname, &value);
      return WebGLAny(script_state, value);
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
      if (IsWebGL2OrHigher()) {
        ContextGL()->GetProgramiv(ObjectOrZero(program), pname, &value);
        return WebGLAny(script_state, static_cast<unsigned>(value));
      }
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

String WebGLRenderingContextBase::getProgramInfoLog(WebGLProgram* program) {
  if (isContextLost() || !ValidateWebGLObject("getProgramInfoLog", program))
    return String();
  GLStringQuery query(ContextGL());
  return query.Run<GLStringQuery::ProgramInfoLog>(ObjectNonZero(program));
}

ScriptValue WebGLRenderingContextBase::getRenderbufferParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state);
  if (target != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                      "invalid target");
    return ScriptValue::CreateNull(script_state);
  }
  if (!renderbuffer_binding_ || !renderbuffer_binding_->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getRenderbufferParameter",
                      "no renderbuffer bound");
    return ScriptValue::CreateNull(script_state);
  }

  GLint value = 0;
  switch (pname) {
    case GL_RENDERBUFFER_SAMPLES:
      if (!IsWebGL2OrHigher()) {
        SynthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                          "invalid parameter name");
        return ScriptValue::CreateNull(script_state);
      }
    case GL_RENDERBUFFER_WIDTH:
    case GL_RENDERBUFFER_HEIGHT:
    case GL_RENDERBUFFER_RED_SIZE:
    case GL_RENDERBUFFER_GREEN_SIZE:
    case GL_RENDERBUFFER_BLUE_SIZE:
    case GL_RENDERBUFFER_ALPHA_SIZE:
    case GL_RENDERBUFFER_DEPTH_SIZE:
      ContextGL()->GetRenderbufferParameteriv(target, pname, &value);
      return WebGLAny(script_state, value);
    case GL_RENDERBUFFER_STENCIL_SIZE:
      ContextGL()->GetRenderbufferParameteriv(target, pname, &value);
      return WebGLAny(script_state, value);
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      return WebGLAny(script_state, renderbuffer_binding_->InternalFormat());
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

ScriptValue WebGLRenderingContextBase::getShaderParameter(
    ScriptState* script_state,
    WebGLShader* shader,
    GLenum pname) {
  if (isContextLost() || !ValidateWebGLObject("getShaderParameter", shader))
    return ScriptValue::CreateNull(script_state);
  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(script_state, shader->IsDeleted());
    case GL_COMPILE_STATUS:
      ContextGL()->GetShaderiv(ObjectOrZero(shader), pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));
    case GL_SHADER_TYPE:
      ContextGL()->GetShaderiv(ObjectOrZero(shader), pname, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getShaderParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

String WebGLRenderingContextBase::getShaderInfoLog(WebGLShader* shader) {
  if (isContextLost() || !ValidateWebGLObject("getShaderInfoLog", shader))
    return String();
  GLStringQuery query(ContextGL());
  return query.Run<GLStringQuery::ShaderInfoLog>(ObjectNonZero(shader));
}

WebGLShaderPrecisionFormat* WebGLRenderingContextBase::getShaderPrecisionFormat(
    GLenum shader_type,
    GLenum precision_type) {
  if (isContextLost())
    return nullptr;
  switch (shader_type) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getShaderPrecisionFormat",
                        "invalid shader type");
      return nullptr;
  }
  switch (precision_type) {
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getShaderPrecisionFormat",
                        "invalid precision type");
      return nullptr;
  }

  GLint range[2] = {0, 0};
  GLint precision = 0;
  ContextGL()->GetShaderPrecisionFormat(shader_type, precision_type, range,
                                        &precision);
  return WebGLShaderPrecisionFormat::Create(range[0], range[1], precision);
}

String WebGLRenderingContextBase::getShaderSource(WebGLShader* shader) {
  if (isContextLost() || !ValidateWebGLObject("getShaderSource", shader))
    return String();
  return EnsureNotNull(shader->Source());
}

Nullable<Vector<String>> WebGLRenderingContextBase::getSupportedExtensions() {
  if (isContextLost())
    return nullptr;

  Vector<String> result;

  for (size_t i = 0; i < extensions_.size(); ++i) {
    ExtensionTracker* tracker = extensions_[i].Get();
    if (ExtensionSupportedAndAllowed(tracker)) {
      const char* const* prefixes = tracker->Prefixes();
      for (; *prefixes; ++prefixes) {
        String prefixed_name = String(*prefixes) + tracker->ExtensionName();
        result.push_back(prefixed_name);
      }
    }
  }

  return result;
}

ScriptValue WebGLRenderingContextBase::getTexParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state);
  if (!ValidateTextureBinding("getTexParameter", target))
    return ScriptValue::CreateNull(script_state);
  switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T: {
      GLint value = 0;
      ContextGL()->GetTexParameteriv(target, pname, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));
    }
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (ExtensionEnabled(kEXTTextureFilterAnisotropicName)) {
        GLfloat value = 0.f;
        ContextGL()->GetTexParameterfv(target, pname, &value);
        return WebGLAny(script_state, value);
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getTexParameter",
          "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
      return ScriptValue::CreateNull(script_state);
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getTexParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

ScriptValue WebGLRenderingContextBase::getUniform(
    ScriptState* script_state,
    WebGLProgram* program,
    const WebGLUniformLocation* uniform_location) {
  if (isContextLost() || !ValidateWebGLObject("getUniform", program))
    return ScriptValue::CreateNull(script_state);
  DCHECK(uniform_location);
  if (uniform_location->Program() != program) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getUniform",
                      "no uniformlocation or not valid for this program");
    return ScriptValue::CreateNull(script_state);
  }
  GLint location = uniform_location->Location();

  GLuint program_id = ObjectNonZero(program);
  GLint max_name_length = -1;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                            &max_name_length);
  if (max_name_length < 0)
    return ScriptValue::CreateNull(script_state);
  if (max_name_length == 0) {
    SynthesizeGLError(GL_INVALID_VALUE, "getUniform",
                      "no active uniforms exist");
    return ScriptValue::CreateNull(script_state);
  }

  // FIXME: make this more efficient using WebGLUniformLocation and caching
  // types in it.
  GLint active_uniforms = 0;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &active_uniforms);
  for (GLint i = 0; i < active_uniforms; i++) {
    LChar* name_ptr;
    RefPtr<StringImpl> name_impl =
        StringImpl::CreateUninitialized(max_name_length, name_ptr);
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    ContextGL()->GetActiveUniform(program_id, i, max_name_length, &length,
                                  &size, &type,
                                  reinterpret_cast<GLchar*>(name_ptr));
    if (size < 0)
      return ScriptValue::CreateNull(script_state);
    String name(name_impl->Substring(0, length));
    StringBuilder name_builder;
    // Strip "[0]" from the name if it's an array.
    if (size > 1 && name.EndsWith("[0]"))
      name = name.Left(name.length() - 3);
    // If it's an array, we need to iterate through each element, appending
    // "[index]" to the name.
    for (GLint index = 0; index < size; ++index) {
      name_builder.Clear();
      name_builder.Append(name);
      if (size > 1 && index >= 1) {
        name_builder.Append('[');
        name_builder.AppendNumber(index);
        name_builder.Append(']');
      }
      // Now need to look this up by name again to find its location
      GLint loc = ContextGL()->GetUniformLocation(
          ObjectOrZero(program), name_builder.ToString().Utf8().data());
      if (loc == location) {
        // Found it. Use the type in the ActiveInfo to determine the return
        // type.
        GLenum base_type;
        unsigned length;
        switch (type) {
          case GL_BOOL:
            base_type = GL_BOOL;
            length = 1;
            break;
          case GL_BOOL_VEC2:
            base_type = GL_BOOL;
            length = 2;
            break;
          case GL_BOOL_VEC3:
            base_type = GL_BOOL;
            length = 3;
            break;
          case GL_BOOL_VEC4:
            base_type = GL_BOOL;
            length = 4;
            break;
          case GL_INT:
            base_type = GL_INT;
            length = 1;
            break;
          case GL_INT_VEC2:
            base_type = GL_INT;
            length = 2;
            break;
          case GL_INT_VEC3:
            base_type = GL_INT;
            length = 3;
            break;
          case GL_INT_VEC4:
            base_type = GL_INT;
            length = 4;
            break;
          case GL_FLOAT:
            base_type = GL_FLOAT;
            length = 1;
            break;
          case GL_FLOAT_VEC2:
            base_type = GL_FLOAT;
            length = 2;
            break;
          case GL_FLOAT_VEC3:
            base_type = GL_FLOAT;
            length = 3;
            break;
          case GL_FLOAT_VEC4:
            base_type = GL_FLOAT;
            length = 4;
            break;
          case GL_FLOAT_MAT2:
            base_type = GL_FLOAT;
            length = 4;
            break;
          case GL_FLOAT_MAT3:
            base_type = GL_FLOAT;
            length = 9;
            break;
          case GL_FLOAT_MAT4:
            base_type = GL_FLOAT;
            length = 16;
            break;
          case GL_SAMPLER_2D:
          case GL_SAMPLER_CUBE:
            base_type = GL_INT;
            length = 1;
            break;
          default:
            if (!IsWebGL2OrHigher()) {
              // Can't handle this type
              SynthesizeGLError(GL_INVALID_VALUE, "getUniform",
                                "unhandled type");
              return ScriptValue::CreateNull(script_state);
            }
            // handle GLenums for WebGL 2.0 or higher
            switch (type) {
              case GL_UNSIGNED_INT:
                base_type = GL_UNSIGNED_INT;
                length = 1;
                break;
              case GL_UNSIGNED_INT_VEC2:
                base_type = GL_UNSIGNED_INT;
                length = 2;
                break;
              case GL_UNSIGNED_INT_VEC3:
                base_type = GL_UNSIGNED_INT;
                length = 3;
                break;
              case GL_UNSIGNED_INT_VEC4:
                base_type = GL_UNSIGNED_INT;
                length = 4;
                break;
              case GL_FLOAT_MAT2x3:
                base_type = GL_FLOAT;
                length = 6;
                break;
              case GL_FLOAT_MAT2x4:
                base_type = GL_FLOAT;
                length = 8;
                break;
              case GL_FLOAT_MAT3x2:
                base_type = GL_FLOAT;
                length = 6;
                break;
              case GL_FLOAT_MAT3x4:
                base_type = GL_FLOAT;
                length = 12;
                break;
              case GL_FLOAT_MAT4x2:
                base_type = GL_FLOAT;
                length = 8;
                break;
              case GL_FLOAT_MAT4x3:
                base_type = GL_FLOAT;
                length = 12;
                break;
              case GL_SAMPLER_3D:
              case GL_SAMPLER_2D_ARRAY:
              case GL_SAMPLER_2D_SHADOW:
              case GL_SAMPLER_CUBE_SHADOW:
              case GL_SAMPLER_2D_ARRAY_SHADOW:
              case GL_INT_SAMPLER_2D:
              case GL_INT_SAMPLER_CUBE:
              case GL_INT_SAMPLER_3D:
              case GL_INT_SAMPLER_2D_ARRAY:
              case GL_UNSIGNED_INT_SAMPLER_2D:
              case GL_UNSIGNED_INT_SAMPLER_CUBE:
              case GL_UNSIGNED_INT_SAMPLER_3D:
              case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
                base_type = GL_INT;
                length = 1;
                break;
              default:
                // Can't handle this type
                SynthesizeGLError(GL_INVALID_VALUE, "getUniform",
                                  "unhandled type");
                return ScriptValue::CreateNull(script_state);
            }
        }
        switch (base_type) {
          case GL_FLOAT: {
            GLfloat value[16] = {0};
            ContextGL()->GetUniformfv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state,
                            DOMFloat32Array::Create(value, length));
          }
          case GL_INT: {
            GLint value[4] = {0};
            ContextGL()->GetUniformiv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state, DOMInt32Array::Create(value, length));
          }
          case GL_UNSIGNED_INT: {
            GLuint value[4] = {0};
            ContextGL()->GetUniformuiv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state,
                            DOMUint32Array::Create(value, length));
          }
          case GL_BOOL: {
            GLint value[4] = {0};
            ContextGL()->GetUniformiv(ObjectOrZero(program), location, value);
            if (length > 1) {
              bool bool_value[16] = {0};
              for (unsigned j = 0; j < length; j++)
                bool_value[j] = static_cast<bool>(value[j]);
              return WebGLAny(script_state, bool_value, length);
            }
            return WebGLAny(script_state, static_cast<bool>(value[0]));
          }
          default:
            NOTIMPLEMENTED();
        }
      }
    }
  }
  // If we get here, something went wrong in our unfortunately complex logic
  // above
  SynthesizeGLError(GL_INVALID_VALUE, "getUniform", "unknown error");
  return ScriptValue::CreateNull(script_state);
}

WebGLUniformLocation* WebGLRenderingContextBase::getUniformLocation(
    WebGLProgram* program,
    const String& name) {
  if (isContextLost() || !ValidateWebGLObject("getUniformLocation", program))
    return nullptr;
  if (!ValidateLocationLength("getUniformLocation", name))
    return nullptr;
  if (!ValidateString("getUniformLocation", name))
    return nullptr;
  if (IsPrefixReserved(name))
    return nullptr;
  if (!program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getUniformLocation",
                      "program not linked");
    return nullptr;
  }
  GLint uniform_location = ContextGL()->GetUniformLocation(
      ObjectOrZero(program), name.Utf8().data());
  if (uniform_location == -1)
    return nullptr;
  return WebGLUniformLocation::Create(program, uniform_location);
}

ScriptValue WebGLRenderingContextBase::getVertexAttrib(
    ScriptState* script_state,
    GLuint index,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state);
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "getVertexAttrib",
                      "index out of range");
    return ScriptValue::CreateNull(script_state);
  }

  if ((ExtensionEnabled(kANGLEInstancedArraysName) || IsWebGL2OrHigher()) &&
      pname == GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE) {
    GLint value = 0;
    ContextGL()->GetVertexAttribiv(index, pname, &value);
    return WebGLAny(script_state, value);
  }

  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      return WebGLAny(
          script_state,
          bound_vertex_array_object_->GetArrayBufferForAttrib(index));
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: {
      GLint value = 0;
      ContextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));
    }
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE: {
      GLint value = 0;
      ContextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(script_state, value);
    }
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: {
      GLint value = 0;
      ContextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(script_state, static_cast<GLenum>(value));
    }
    case GL_CURRENT_VERTEX_ATTRIB: {
      switch (vertex_attrib_type_[index]) {
        case kFloat32ArrayType: {
          GLfloat float_value[4];
          ContextGL()->GetVertexAttribfv(index, pname, float_value);
          return WebGLAny(script_state,
                          DOMFloat32Array::Create(float_value, 4));
        }
        case kInt32ArrayType: {
          GLint int_value[4];
          ContextGL()->GetVertexAttribIiv(index, pname, int_value);
          return WebGLAny(script_state, DOMInt32Array::Create(int_value, 4));
        }
        case kUint32ArrayType: {
          GLuint uint_value[4];
          ContextGL()->GetVertexAttribIuiv(index, pname, uint_value);
          return WebGLAny(script_state, DOMUint32Array::Create(uint_value, 4));
        }
        default:
          NOTREACHED();
          break;
      }
      return ScriptValue::CreateNull(script_state);
    }
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      if (IsWebGL2OrHigher()) {
        GLint value = 0;
        ContextGL()->GetVertexAttribiv(index, pname, &value);
        return WebGLAny(script_state, static_cast<bool>(value));
      }
    // fall through to default error case
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getVertexAttrib",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

long long WebGLRenderingContextBase::getVertexAttribOffset(GLuint index,
                                                           GLenum pname) {
  if (isContextLost())
    return 0;
  GLvoid* result = nullptr;
  // NOTE: If pname is ever a value that returns more than 1 element
  // this will corrupt memory.
  ContextGL()->GetVertexAttribPointerv(index, pname, &result);
  return static_cast<long long>(reinterpret_cast<intptr_t>(result));
}

void WebGLRenderingContextBase::hint(GLenum target, GLenum mode) {
  if (isContextLost())
    return;
  bool is_valid = false;
  switch (target) {
    case GL_GENERATE_MIPMAP_HINT:
      is_valid = true;
      break;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:  // OES_standard_derivatives
      if (ExtensionEnabled(kOESStandardDerivativesName) || IsWebGL2OrHigher())
        is_valid = true;
      break;
  }
  if (!is_valid) {
    SynthesizeGLError(GL_INVALID_ENUM, "hint", "invalid target");
    return;
  }
  ContextGL()->Hint(target, mode);
}

GLboolean WebGLRenderingContextBase::isBuffer(WebGLBuffer* buffer) {
  if (!buffer || isContextLost())
    return 0;

  if (!buffer->HasEverBeenBound())
    return 0;
  if (buffer->IsDeleted())
    return 0;

  return ContextGL()->IsBuffer(buffer->Object());
}

bool WebGLRenderingContextBase::isContextLost() const {
  return context_lost_mode_ != kNotLostContext;
}

GLboolean WebGLRenderingContextBase::isEnabled(GLenum cap) {
  if (isContextLost() || !ValidateCapability("isEnabled", cap))
    return 0;
  if (cap == GL_STENCIL_TEST)
    return stencil_enabled_;
  return ContextGL()->IsEnabled(cap);
}

GLboolean WebGLRenderingContextBase::isFramebuffer(
    WebGLFramebuffer* framebuffer) {
  if (!framebuffer || isContextLost())
    return 0;

  if (!framebuffer->HasEverBeenBound())
    return 0;
  if (framebuffer->IsDeleted())
    return 0;

  return ContextGL()->IsFramebuffer(framebuffer->Object());
}

GLboolean WebGLRenderingContextBase::isProgram(WebGLProgram* program) {
  if (!program || isContextLost())
    return 0;

  return ContextGL()->IsProgram(program->Object());
}

GLboolean WebGLRenderingContextBase::isRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  if (!renderbuffer || isContextLost())
    return 0;

  if (!renderbuffer->HasEverBeenBound())
    return 0;
  if (renderbuffer->IsDeleted())
    return 0;

  return ContextGL()->IsRenderbuffer(renderbuffer->Object());
}

GLboolean WebGLRenderingContextBase::isShader(WebGLShader* shader) {
  if (!shader || isContextLost())
    return 0;

  return ContextGL()->IsShader(shader->Object());
}

GLboolean WebGLRenderingContextBase::isTexture(WebGLTexture* texture) {
  if (!texture || isContextLost())
    return 0;

  if (!texture->HasEverBeenBound())
    return 0;
  if (texture->IsDeleted())
    return 0;

  return ContextGL()->IsTexture(texture->Object());
}

void WebGLRenderingContextBase::lineWidth(GLfloat width) {
  if (isContextLost())
    return;
  ContextGL()->LineWidth(width);
}

void WebGLRenderingContextBase::linkProgram(WebGLProgram* program) {
  if (isContextLost() || !ValidateWebGLObject("linkProgram", program))
    return;

  if (program->ActiveTransformFeedbackCount() > 0) {
    SynthesizeGLError(
        GL_INVALID_OPERATION, "linkProgram",
        "program being used by one or more active transform feedback objects");
    return;
  }

  ContextGL()->LinkProgram(ObjectOrZero(program));
  program->IncreaseLinkCount();
}

void WebGLRenderingContextBase::pixelStorei(GLenum pname, GLint param) {
  if (isContextLost())
    return;
  switch (pname) {
    case GC3D_UNPACK_FLIP_Y_WEBGL:
      unpack_flip_y_ = param;
      break;
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      unpack_premultiply_alpha_ = param;
      break;
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      if (static_cast<GLenum>(param) == GC3D_BROWSER_DEFAULT_WEBGL ||
          param == GL_NONE) {
        unpack_colorspace_conversion_ = static_cast<GLenum>(param);
      } else {
        SynthesizeGLError(
            GL_INVALID_VALUE, "pixelStorei",
            "invalid parameter for UNPACK_COLORSPACE_CONVERSION_WEBGL");
        return;
      }
      break;
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (param == 1 || param == 2 || param == 4 || param == 8) {
        if (pname == GL_PACK_ALIGNMENT) {
          pack_alignment_ = param;
        } else {  // GL_UNPACK_ALIGNMENT:
          unpack_alignment_ = param;
        }
        ContextGL()->PixelStorei(pname, param);
      } else {
        SynthesizeGLError(GL_INVALID_VALUE, "pixelStorei",
                          "invalid parameter for alignment");
        return;
      }
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "pixelStorei",
                        "invalid parameter name");
      return;
  }
}

void WebGLRenderingContextBase::polygonOffset(GLfloat factor, GLfloat units) {
  if (isContextLost())
    return;
  ContextGL()->PolygonOffset(factor, units);
}

bool WebGLRenderingContextBase::ValidateReadBufferAndGetInfo(
    const char* function_name,
    WebGLFramebuffer*& read_framebuffer_binding) {
  read_framebuffer_binding = GetReadFramebufferBinding();
  if (read_framebuffer_binding) {
    const char* reason = "framebuffer incomplete";
    if (read_framebuffer_binding->CheckDepthStencilStatus(&reason) !=
        GL_FRAMEBUFFER_COMPLETE) {
      SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, function_name,
                        reason);
      return false;
    }
  } else {
    if (read_buffer_of_default_framebuffer_ == GL_NONE) {
      DCHECK(IsWebGL2OrHigher());
      SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                        "no image to read from");
      return false;
    }
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateReadPixelsFormatAndType(
    GLenum format,
    GLenum type,
    DOMArrayBufferView* buffer) {
  switch (format) {
    case GL_ALPHA:
    case GL_RGB:
    case GL_RGBA:
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid format");
      return false;
  }

  switch (type) {
    case GL_UNSIGNED_BYTE:
      if (buffer && buffer->GetType() != DOMArrayBufferView::kTypeUint8) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
        return false;
      }
      return true;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      if (buffer && buffer->GetType() != DOMArrayBufferView::kTypeUint16) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return false;
      }
      return true;
    case GL_FLOAT:
      if (ExtensionEnabled(kOESTextureFloatName) ||
          ExtensionEnabled(kOESTextureHalfFloatName)) {
        if (buffer && buffer->GetType() != DOMArrayBufferView::kTypeFloat32) {
          SynthesizeGLError(GL_INVALID_OPERATION, "readPixels",
                            "type FLOAT but ArrayBufferView not Float32Array");
          return false;
        }
        return true;
      }
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
    case GL_HALF_FLOAT_OES:
      if (ExtensionEnabled(kOESTextureHalfFloatName)) {
        if (buffer && buffer->GetType() != DOMArrayBufferView::kTypeUint16) {
          SynthesizeGLError(
              GL_INVALID_OPERATION, "readPixels",
              "type HALF_FLOAT_OES but ArrayBufferView not Uint16Array");
          return false;
        }
        return true;
      }
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
  }
}

WebGLImageConversion::PixelStoreParams
WebGLRenderingContextBase::GetPackPixelStoreParams() {
  WebGLImageConversion::PixelStoreParams params;
  params.alignment = pack_alignment_;
  return params;
}

WebGLImageConversion::PixelStoreParams
WebGLRenderingContextBase::GetUnpackPixelStoreParams(TexImageDimension) {
  WebGLImageConversion::PixelStoreParams params;
  params.alignment = unpack_alignment_;
  return params;
}

bool WebGLRenderingContextBase::ValidateReadPixelsFuncParameters(
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    DOMArrayBufferView* buffer,
    long long buffer_size) {
  if (!ValidateReadPixelsFormatAndType(format, type, buffer))
    return false;

  // Calculate array size, taking into consideration of pack parameters.
  unsigned total_bytes_required = 0, total_skip_bytes = 0;
  GLenum error = WebGLImageConversion::ComputeImageSizeInBytes(
      format, type, width, height, 1, GetPackPixelStoreParams(),
      &total_bytes_required, 0, &total_skip_bytes);
  if (error != GL_NO_ERROR) {
    SynthesizeGLError(error, "readPixels", "invalid dimensions");
    return false;
  }
  if (buffer_size <
      static_cast<long long>(total_bytes_required + total_skip_bytes)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "readPixels",
                      "buffer is not large enough for dimensions");
    return false;
  }
  return true;
}

void WebGLRenderingContextBase::readPixels(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  ReadPixelsHelper(x, y, width, height, format, type, pixels.View(), 0);
}

void WebGLRenderingContextBase::ReadPixelsHelper(GLint x,
                                                 GLint y,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type,
                                                 DOMArrayBufferView* pixels,
                                                 GLuint offset) {
  if (isContextLost())
    return;
  // Due to WebGL's same-origin restrictions, it is not possible to
  // taint the origin using the WebGL API.
  DCHECK(host()->OriginClean());

  // Validate input parameters.
  if (!pixels) {
    SynthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "no destination ArrayBufferView");
    return;
  }
  CheckedNumeric<GLuint> offset_in_bytes = offset;
  offset_in_bytes *= pixels->TypeSize();
  if (!offset_in_bytes.IsValid() ||
      offset_in_bytes.ValueOrDie() > pixels->byteLength()) {
    SynthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "destination offset out of range");
    return;
  }
  const char* reason = "framebuffer incomplete";
  WebGLFramebuffer* framebuffer = GetReadFramebufferBinding();
  if (framebuffer && framebuffer->CheckDepthStencilStatus(&reason) !=
                         GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "readPixels", reason);
    return;
  }
  CheckedNumeric<GLuint> buffer_size = pixels->byteLength() - offset_in_bytes;
  if (!buffer_size.IsValid()) {
    SynthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "destination offset out of range");
    return;
  }
  if (!ValidateReadPixelsFuncParameters(width, height, format, type, pixels,
                                        buffer_size.ValueOrDie())) {
    return;
  }
  ClearIfComposited();
  uint8_t* data = static_cast<uint8_t*>(pixels->BaseAddressMaybeShared()) +
                  offset_in_bytes.ValueOrDie();
  {
    ScopedDrawingBufferBinder binder(GetDrawingBuffer(), framebuffer);
    ContextGL()->ReadPixels(x, y, width, height, format, type, data);
  }
}

void WebGLRenderingContextBase::RenderbufferStorageImpl(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    const char* function_name) {
  DCHECK(!samples);             // |samples| > 0 is only valid in WebGL2's
                                // renderbufferStorageMultisample().
  DCHECK(!IsWebGL2OrHigher());  // Make sure this is overridden in WebGL 2.
  switch (internalformat) {
    case GL_DEPTH_COMPONENT16:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB565:
    case GL_STENCIL_INDEX8:
      ContextGL()->RenderbufferStorage(target, internalformat, width, height);
      renderbuffer_binding_->SetInternalFormat(internalformat);
      renderbuffer_binding_->SetSize(width, height);
      break;
    case GL_SRGB8_ALPHA8_EXT:
      if (!ExtensionEnabled(kEXTsRGBName)) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name, "sRGB not enabled");
        break;
      }
      ContextGL()->RenderbufferStorage(target, internalformat, width, height);
      renderbuffer_binding_->SetInternalFormat(internalformat);
      renderbuffer_binding_->SetSize(width, height);
      break;
    case GL_DEPTH_STENCIL_OES:
      DCHECK(IsDepthStencilSupported());
      ContextGL()->RenderbufferStorage(target, GL_DEPTH24_STENCIL8_OES, width,
                                       height);
      renderbuffer_binding_->SetSize(width, height);
      renderbuffer_binding_->SetInternalFormat(internalformat);
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid internalformat");
      break;
  }
}

void WebGLRenderingContextBase::renderbufferStorage(GLenum target,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  const char* function_name = "renderbufferStorage";
  if (isContextLost())
    return;
  if (target != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
    return;
  }
  if (!renderbuffer_binding_ || !renderbuffer_binding_->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no bound renderbuffer");
    return;
  }
  if (!ValidateSize(function_name, width, height))
    return;
  RenderbufferStorageImpl(target, 0, internalformat, width, height,
                          function_name);
  ApplyStencilTest();
}

void WebGLRenderingContextBase::sampleCoverage(GLfloat value,
                                               GLboolean invert) {
  if (isContextLost())
    return;
  ContextGL()->SampleCoverage(value, invert);
}

void WebGLRenderingContextBase::scissor(GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) {
  if (isContextLost())
    return;
  scissor_box_[0] = x;
  scissor_box_[1] = y;
  scissor_box_[2] = width;
  scissor_box_[3] = height;
  ContextGL()->Scissor(x, y, width, height);
}

void WebGLRenderingContextBase::shaderSource(WebGLShader* shader,
                                             const String& string) {
  if (isContextLost() || !ValidateWebGLObject("shaderSource", shader))
    return;
  String string_without_comments = StripComments(string).Result();
  // TODO(danakj): Make validateShaderSource reject characters > 255 (or utf16
  // Strings) so we don't need to use StringUTF8Adaptor.
  if (!ValidateShaderSource(string_without_comments))
    return;
  shader->SetSource(string);
  WTF::StringUTF8Adaptor adaptor(string_without_comments);
  const GLchar* shader_data = adaptor.Data();
  // TODO(danakj): Use base::saturated_cast<GLint>.
  const GLint shader_length = adaptor.length();
  ContextGL()->ShaderSource(ObjectOrZero(shader), 1, &shader_data,
                            &shader_length);
}

void WebGLRenderingContextBase::stencilFunc(GLenum func,
                                            GLint ref,
                                            GLuint mask) {
  if (isContextLost())
    return;
  if (!ValidateStencilOrDepthFunc("stencilFunc", func))
    return;
  stencil_func_ref_ = ref;
  stencil_func_ref_back_ = ref;
  stencil_func_mask_ = mask;
  stencil_func_mask_back_ = mask;
  ContextGL()->StencilFunc(func, ref, mask);
}

void WebGLRenderingContextBase::stencilFuncSeparate(GLenum face,
                                                    GLenum func,
                                                    GLint ref,
                                                    GLuint mask) {
  if (isContextLost())
    return;
  if (!ValidateStencilOrDepthFunc("stencilFuncSeparate", func))
    return;
  switch (face) {
    case GL_FRONT_AND_BACK:
      stencil_func_ref_ = ref;
      stencil_func_ref_back_ = ref;
      stencil_func_mask_ = mask;
      stencil_func_mask_back_ = mask;
      break;
    case GL_FRONT:
      stencil_func_ref_ = ref;
      stencil_func_mask_ = mask;
      break;
    case GL_BACK:
      stencil_func_ref_back_ = ref;
      stencil_func_mask_back_ = mask;
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "stencilFuncSeparate", "invalid face");
      return;
  }
  ContextGL()->StencilFuncSeparate(face, func, ref, mask);
}

void WebGLRenderingContextBase::stencilMask(GLuint mask) {
  if (isContextLost())
    return;
  stencil_mask_ = mask;
  stencil_mask_back_ = mask;
  ContextGL()->StencilMask(mask);
}

void WebGLRenderingContextBase::stencilMaskSeparate(GLenum face, GLuint mask) {
  if (isContextLost())
    return;
  switch (face) {
    case GL_FRONT_AND_BACK:
      stencil_mask_ = mask;
      stencil_mask_back_ = mask;
      break;
    case GL_FRONT:
      stencil_mask_ = mask;
      break;
    case GL_BACK:
      stencil_mask_back_ = mask;
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "stencilMaskSeparate", "invalid face");
      return;
  }
  ContextGL()->StencilMaskSeparate(face, mask);
}

void WebGLRenderingContextBase::stencilOp(GLenum fail,
                                          GLenum zfail,
                                          GLenum zpass) {
  if (isContextLost())
    return;
  ContextGL()->StencilOp(fail, zfail, zpass);
}

void WebGLRenderingContextBase::stencilOpSeparate(GLenum face,
                                                  GLenum fail,
                                                  GLenum zfail,
                                                  GLenum zpass) {
  if (isContextLost())
    return;
  ContextGL()->StencilOpSeparate(face, fail, zfail, zpass);
}

GLenum WebGLRenderingContextBase::ConvertTexInternalFormat(
    GLenum internalformat,
    GLenum type) {
  // Convert to sized internal formats that are renderable with
  // GL_CHROMIUM_color_buffer_float_rgb(a).
  if (type == GL_FLOAT && internalformat == GL_RGBA &&
      ExtensionsUtil()->IsExtensionEnabled(
          "GL_CHROMIUM_color_buffer_float_rgba"))
    return GL_RGBA32F_EXT;
  if (type == GL_FLOAT && internalformat == GL_RGB &&
      ExtensionsUtil()->IsExtensionEnabled(
          "GL_CHROMIUM_color_buffer_float_rgb"))
    return GL_RGB32F_EXT;
  return internalformat;
}

void WebGLRenderingContextBase::TexImage2DBase(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               const void* pixels) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  ContextGL()->TexImage2D(target, level,
                          ConvertTexInternalFormat(internalformat, type), width,
                          height, border, format, type, pixels);
}

void WebGLRenderingContextBase::TexImageImpl(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLenum format,
    GLenum type,
    Image* image,
    WebGLImageConversion::ImageHtmlDomSource dom_source,
    bool flip_y,
    bool premultiply_alpha,
    const IntRect& source_image_rect,
    GLsizei depth,
    GLint unpack_image_height) {
  const char* func_name = GetTexImageFunctionName(function_id);
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
    // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
    type = GL_FLOAT;
  }
  Vector<uint8_t> data;

  IntRect sub_rect = source_image_rect;
  if (sub_rect == SentinelEmptyRect()) {
    // Recalculate based on the size of the Image.
    sub_rect = SafeGetImageSize(image);
  }

  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(func_name, function_id, image, sub_rect,
                                    depth, unpack_image_height,
                                    &selecting_sub_rectangle)) {
    return;
  }

  // Adjust the source image rectangle if doing a y-flip.
  IntRect adjusted_source_image_rect = sub_rect;
  if (flip_y) {
    adjusted_source_image_rect.SetY(image->height() -
                                    adjusted_source_image_rect.MaxY());
  }

  WebGLImageConversion::ImageExtractor image_extractor(
      image, dom_source, premultiply_alpha,
      unpack_colorspace_conversion_ == GL_NONE);
  if (!image_extractor.ImagePixelData()) {
    SynthesizeGLError(GL_INVALID_VALUE, func_name, "bad image data");
    return;
  }

  WebGLImageConversion::DataFormat source_data_format =
      image_extractor.ImageSourceFormat();
  WebGLImageConversion::AlphaOp alpha_op = image_extractor.ImageAlphaOp();
  const void* image_pixel_data = image_extractor.ImagePixelData();

  bool need_conversion = true;
  if (type == GL_UNSIGNED_BYTE &&
      source_data_format == WebGLImageConversion::kDataFormatRGBA8 &&
      format == GL_RGBA && alpha_op == WebGLImageConversion::kAlphaDoNothing &&
      !flip_y && !selecting_sub_rectangle && depth == 1) {
    need_conversion = false;
  } else {
    if (!WebGLImageConversion::PackImageData(
            image, image_pixel_data, format, type, flip_y, alpha_op,
            source_data_format, image_extractor.ImageWidth(),
            image_extractor.ImageHeight(), adjusted_source_image_rect, depth,
            image_extractor.ImageSourceUnpackAlignment(), unpack_image_height,
            data)) {
      SynthesizeGLError(GL_INVALID_VALUE, func_name, "packImage error");
      return;
    }
  }

  ScopedUnpackParametersResetRestore temporary_reset_unpack(this);
  if (function_id == kTexImage2D) {
    TexImage2DBase(target, level, internalformat,
                   adjusted_source_image_rect.Width(),
                   adjusted_source_image_rect.Height(), 0, format, type,
                   need_conversion ? data.data() : image_pixel_data);
  } else if (function_id == kTexSubImage2D) {
    ContextGL()->TexSubImage2D(
        target, level, xoffset, yoffset, adjusted_source_image_rect.Width(),
        adjusted_source_image_rect.Height(), format, type,
        need_conversion ? data.data() : image_pixel_data);
  } else {
    // 3D functions.
    if (function_id == kTexImage3D) {
      ContextGL()->TexImage3D(
          target, level, internalformat, adjusted_source_image_rect.Width(),
          adjusted_source_image_rect.Height(), depth, 0, format, type,
          need_conversion ? data.data() : image_pixel_data);
    } else {
      DCHECK_EQ(function_id, kTexSubImage3D);
      ContextGL()->TexSubImage3D(
          target, level, xoffset, yoffset, zoffset,
          adjusted_source_image_rect.Width(),
          adjusted_source_image_rect.Height(), depth, format, type,
          need_conversion ? data.data() : image_pixel_data);
    }
  }
}

bool WebGLRenderingContextBase::ValidateTexFunc(
    const char* function_name,
    TexImageFunctionType function_type,
    TexFuncValidationSourceType source_type,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset) {
  if (!ValidateTexFuncLevel(function_name, target, level))
    return false;

  if (!ValidateTexFuncParameters(function_name, function_type, source_type,
                                 target, level, internalformat, width, height,
                                 depth, border, format, type))
    return false;

  if (function_type == kTexSubImage) {
    if (!ValidateSettableTexFormat(function_name, format))
      return false;
    if (!ValidateSize(function_name, xoffset, yoffset, zoffset))
      return false;
  } else {
    // For SourceArrayBufferView, function validateTexFuncData() would handle
    // whether to validate the SettableTexFormat
    // by checking if the ArrayBufferView is null or not.
    if (source_type != kSourceArrayBufferView) {
      if (!ValidateSettableTexFormat(function_name, format))
        return false;
    }
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateValueFitNonNegInt32(
    const char* function_name,
    const char* param_name,
    long long value) {
  if (value < 0) {
    String error_msg = String(param_name) + " < 0";
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      error_msg.Ascii().data());
    return false;
  }
  if (value > static_cast<long long>(std::numeric_limits<int>::max())) {
    String error_msg = String(param_name) + " more than 32-bit";
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      error_msg.Ascii().data());
    return false;
  }
  return true;
}

// TODO(fmalita): figure why WebGLImageConversion::ImageExtractor can't handle
// SVG-backed images, and get rid of this intermediate step.
PassRefPtr<Image> WebGLRenderingContextBase::DrawImageIntoBuffer(
    PassRefPtr<Image> pass_image,
    int width,
    int height,
    const char* function_name) {
  RefPtr<Image> image(std::move(pass_image));
  DCHECK(image);

  IntSize size(width, height);
  ImageBuffer* buf = generated_image_cache_.GetImageBuffer(size);
  if (!buf) {
    SynthesizeGLError(GL_OUT_OF_MEMORY, function_name, "out of memory");
    return nullptr;
  }

  if (!image->CurrentFrameKnownToBeOpaque())
    buf->Canvas()->clear(SK_ColorTRANSPARENT);

  IntRect src_rect(IntPoint(), image->Size());
  IntRect dest_rect(0, 0, size.Width(), size.Height());
  PaintFlags flags;
  // TODO(ccameron): WebGL should produce sRGB images.
  // https://crbug.com/672299
  image->Draw(buf->Canvas(), flags, dest_rect, src_rect,
              kDoNotRespectImageOrientation,
              Image::kDoNotClampImageToSourceRect);
  return buf->NewImageSnapshot(kPreferNoAcceleration,
                               kSnapshotReasonWebGLDrawImageIntoBuffer);
}

WebGLTexture* WebGLRenderingContextBase::ValidateTexImageBinding(
    const char* func_name,
    TexImageFunctionID function_id,
    GLenum target) {
  return ValidateTexture2DBinding(func_name, target);
}

const char* WebGLRenderingContextBase::GetTexImageFunctionName(
    TexImageFunctionID func_name) {
  switch (func_name) {
    case kTexImage2D:
      return "texImage2D";
    case kTexSubImage2D:
      return "texSubImage2D";
    case kTexSubImage3D:
      return "texSubImage3D";
    case kTexImage3D:
      return "texImage3D";
    default:  // Adding default to prevent compile error
      return "";
  }
}

IntRect WebGLRenderingContextBase::SentinelEmptyRect() {
  // Return a rectangle with -1 width and height so we can recognize
  // it later and recalculate it based on the Image whose data we'll
  // upload. It's important that there be no possible differences in
  // the logic which computes the image's size.
  return IntRect(0, 0, -1, -1);
}

IntRect WebGLRenderingContextBase::SafeGetImageSize(Image* image) {
  if (!image)
    return IntRect();

  return GetTextureSourceSize(image);
}

IntRect WebGLRenderingContextBase::GetImageDataSize(ImageData* pixels) {
  DCHECK(pixels);
  return GetTextureSourceSize(pixels);
}

void WebGLRenderingContextBase::TexImageHelperDOMArrayBufferView(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    DOMArrayBufferView* pixels,
    NullDisposition null_disposition,
    GLuint src_offset) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;
  if (!ValidateTexImageBinding(func_name, function_id, target))
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceArrayBufferView, target,
                       level, internalformat, width, height, depth, border,
                       format, type, xoffset, yoffset, zoffset))
    return;
  TexImageDimension source_type;
  if (function_id == kTexImage2D || function_id == kTexSubImage2D)
    source_type = kTex2D;
  else
    source_type = kTex3D;
  if (!ValidateTexFuncData(func_name, source_type, level, width, height, depth,
                           format, type, pixels, null_disposition, src_offset))
    return;
  uint8_t* data =
      reinterpret_cast<uint8_t*>(pixels ? pixels->BaseAddressMaybeShared() : 0);
  if (src_offset) {
    DCHECK(pixels);
    // No need to check overflow because validateTexFuncData() already did.
    data += src_offset * pixels->TypeSize();
  }
  Vector<uint8_t> temp_data;
  bool change_unpack_alignment = false;
  if (data && (unpack_flip_y_ || unpack_premultiply_alpha_)) {
    if (source_type == kTex2D) {
      if (!WebGLImageConversion::ExtractTextureData(
              width, height, format, type, unpack_alignment_, unpack_flip_y_,
              unpack_premultiply_alpha_, data, temp_data))
        return;
      data = temp_data.data();
    }
    change_unpack_alignment = true;
  }
  // TODO(crbug.com/666064): implement flipY and premultiplyAlpha for
  // tex(Sub)3D.
  if (function_id == kTexImage3D) {
    ContextGL()->TexImage3D(target, level,
                            ConvertTexInternalFormat(internalformat, type),
                            width, height, depth, border, format, type, data);
    return;
  }
  if (function_id == kTexSubImage3D) {
    ContextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type, data);
    return;
  }

  ScopedUnpackParametersResetRestore temporary_reset_unpack(
      this, change_unpack_alignment);
  if (function_id == kTexImage2D)
    TexImage2DBase(target, level, internalformat, width, height, border, format,
                   type, data);
  else if (function_id == kTexSubImage2D)
    ContextGL()->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                               format, type, data);
}

void WebGLRenderingContextBase::texImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  TexImageHelperDOMArrayBufferView(kTexImage2D, target, level, internalformat,
                                   width, height, 1, border, format, type, 0, 0,
                                   0, pixels.View(), kNullAllowed, 0);
}

void WebGLRenderingContextBase::TexImageHelperImageData(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLint border,
    GLenum format,
    GLenum type,
    GLsizei depth,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    ImageData* pixels,
    const IntRect& source_image_rect,
    GLint unpack_image_height) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;
  DCHECK(pixels);
  if (pixels->data()->BufferBase()->IsNeutered()) {
    SynthesizeGLError(GL_INVALID_VALUE, func_name,
                      "The source data has been neutered.");
    return;
  }
  if (!ValidateTexImageBinding(func_name, function_id, target))
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceImageData, target,
                       level, internalformat, pixels->width(), pixels->height(),
                       depth, border, format, type, xoffset, yoffset, zoffset))
    return;

  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(
          func_name, function_id, pixels, source_image_rect, depth,
          unpack_image_height, &selecting_sub_rectangle)) {
    return;
  }
  // Adjust the source image rectangle if doing a y-flip.
  IntRect adjusted_source_image_rect = source_image_rect;
  if (unpack_flip_y_) {
    adjusted_source_image_rect.SetY(pixels->height() -
                                    adjusted_source_image_rect.MaxY());
  }

  Vector<uint8_t> data;
  bool need_conversion = true;
  // The data from ImageData is always of format RGBA8.
  // No conversion is needed if destination format is RGBA and type is
  // UNSIGNED_BYTE and no Flip or Premultiply operation is required.
  if (!unpack_flip_y_ && !unpack_premultiply_alpha_ && format == GL_RGBA &&
      type == GL_UNSIGNED_BYTE && !selecting_sub_rectangle && depth == 1) {
    need_conversion = false;
  } else {
    if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
      // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
      type = GL_FLOAT;
    }
    if (!WebGLImageConversion::ExtractImageData(
            pixels->data()->Data(),
            WebGLImageConversion::DataFormat::kDataFormatRGBA8, pixels->Size(),
            adjusted_source_image_rect, depth, unpack_image_height, format,
            type, unpack_flip_y_, unpack_premultiply_alpha_, data)) {
      SynthesizeGLError(GL_INVALID_VALUE, func_name, "bad image data");
      return;
    }
  }
  ScopedUnpackParametersResetRestore temporary_reset_unpack(this);
  const uint8_t* bytes = need_conversion ? data.data() : pixels->data()->Data();
  if (function_id == kTexImage2D) {
    DCHECK_EQ(unpack_image_height, 0);
    TexImage2DBase(
        target, level, internalformat, adjusted_source_image_rect.Width(),
        adjusted_source_image_rect.Height(), border, format, type, bytes);
  } else if (function_id == kTexSubImage2D) {
    DCHECK_EQ(unpack_image_height, 0);
    ContextGL()->TexSubImage2D(
        target, level, xoffset, yoffset, adjusted_source_image_rect.Width(),
        adjusted_source_image_rect.Height(), format, type, bytes);
  } else {
    GLint upload_height = adjusted_source_image_rect.Height();
    if (unpack_image_height) {
      // GL_UNPACK_IMAGE_HEIGHT overrides the passed-in height.
      upload_height = unpack_image_height;
    }
    if (function_id == kTexImage3D) {
      ContextGL()->TexImage3D(target, level, internalformat,
                              adjusted_source_image_rect.Width(), upload_height,
                              depth, border, format, type, bytes);
    } else {
      DCHECK_EQ(function_id, kTexSubImage3D);
      ContextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                 adjusted_source_image_rect.Width(),
                                 upload_height, depth, format, type, bytes);
    }
  }
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageData* pixels) {
  TexImageHelperImageData(kTexImage2D, target, level, internalformat, 0, format,
                          type, 1, 0, 0, 0, pixels, GetImageDataSize(pixels),
                          0);
}

void WebGLRenderingContextBase::TexImageHelperHTMLImageElement(
    SecurityOrigin* security_origin,
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLImageElement* image,
    const IntRect& source_image_rect,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;

  if (!ValidateHTMLImageElement(security_origin, func_name, image,
                                exception_state))
    return;
  if (!ValidateTexImageBinding(func_name, function_id, target))
    return;

  RefPtr<Image> image_for_render = image->CachedImage()->GetImage();
  if (image_for_render && image_for_render->IsSVGImage()) {
    if (canvas()) {
      UseCounter::Count(canvas()->GetDocument(), WebFeature::kSVGInWebGL);
    }
    image_for_render =
        DrawImageIntoBuffer(std::move(image_for_render), image->width(),
                            image->height(), func_name);
  }

  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!image_for_render ||
      !ValidateTexFunc(func_name, function_type, kSourceHTMLImageElement,
                       target, level, internalformat, image_for_render->width(),
                       image_for_render->height(), depth, 0, format, type,
                       xoffset, yoffset, zoffset))
    return;

  TexImageImpl(function_id, target, level, internalformat, xoffset, yoffset,
               zoffset, format, type, image_for_render.Get(),
               WebGLImageConversion::kHtmlDomImage, unpack_flip_y_,
               unpack_premultiply_alpha_, source_image_rect, depth,
               unpack_image_height);
}

void WebGLRenderingContextBase::texImage2D(ExecutionContext* execution_context,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLImageElement* image,
                                           ExceptionState& exception_state) {
  TexImageHelperHTMLImageElement(execution_context->GetSecurityOrigin(),
                                 kTexImage2D, target, level, internalformat,
                                 format, type, 0, 0, 0, image,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

bool WebGLRenderingContextBase::CanUseTexImageByGPU(GLenum format,
                                                    GLenum type) {
#if defined(OS_MACOSX)
  // RGB5_A1 is not color-renderable on NVIDIA Mac, see crbug.com/676209.
  // Though, glCopyTextureCHROMIUM can handle RGB5_A1 internalformat by doing a
  // fallback path, but it doesn't know the type info. So, we still cannot do
  // the fallback path in glCopyTextureCHROMIUM for
  // RGBA/RGBA/UNSIGNED_SHORT_5_5_5_1 format and type combination.
  if (type == GL_UNSIGNED_SHORT_5_5_5_1)
    return false;
#endif

#if defined(OS_ANDROID)
  // TODO(kbr): bugs were seen on Android devices with NVIDIA GPUs
  // when copying hardware-accelerated video textures to
  // floating-point textures. Investigate the root cause of this and
  // fix it. crbug.com/710874
  if (type == GL_FLOAT)
    return false;
#endif

  // OES_texture_half_float doesn't support HALF_FLOAT_OES type for
  // CopyTexImage/CopyTexSubImage. And OES_texture_half_float doesn't require
  // HALF_FLOAT_OES type texture to be renderable. So, HALF_FLOAT_OES type
  // texture cannot be copied to or drawn to by glCopyTextureCHROMIUM.
  if (type == GL_HALF_FLOAT_OES)
    return false;

  return true;
}

SnapshotReason WebGLRenderingContextBase::FunctionIDToSnapshotReason(
    TexImageFunctionID id) {
  switch (id) {
    case kTexImage2D:
      return kSnapshotReasonWebGLTexImage2D;
    case kTexSubImage2D:
      return kSnapshotReasonWebGLTexSubImage2D;
    case kTexImage3D:
      return kSnapshotReasonWebGLTexImage3D;
    case kTexSubImage3D:
      return kSnapshotReasonWebGLTexSubImage3D;
  }
  NOTREACHED();
  return kSnapshotReasonUnknown;
}

void WebGLRenderingContextBase::TexImageCanvasByGPU(
    TexImageFunctionID function_id,
    HTMLCanvasElement* canvas,
    GLenum target,
    GLuint target_texture,
    GLint xoffset,
    GLint yoffset,
    const IntRect& source_sub_rectangle) {
  if (!canvas->Is3d()) {
    ImageBuffer* buffer = canvas->GetOrCreateImageBuffer();
    if (buffer &&
        !buffer->CopyToPlatformTexture(
            FunctionIDToSnapshotReason(function_id), ContextGL(), target,
            target_texture, unpack_premultiply_alpha_, unpack_flip_y_,
            IntPoint(xoffset, yoffset), source_sub_rectangle)) {
      NOTREACHED();
    }
  } else {
    WebGLRenderingContextBase* gl =
        ToWebGLRenderingContextBase(canvas->RenderingContext());
    ScopedTexture2DRestorer restorer(gl);
    if (!gl->GetDrawingBuffer()->CopyToPlatformTexture(
            ContextGL(), target, target_texture, unpack_premultiply_alpha_,
            !unpack_flip_y_, IntPoint(xoffset, yoffset), source_sub_rectangle,
            kBackBuffer)) {
      NOTREACHED();
    }
  }
}

void WebGLRenderingContextBase::TexImageByGPU(
    TexImageFunctionID function_id,
    WebGLTexture* texture,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    CanvasImageSource* image,
    const IntRect& source_sub_rectangle) {
  DCHECK(image->IsCanvasElement() || image->IsImageBitmap());
  int width = source_sub_rectangle.Width();
  int height = source_sub_rectangle.Height();

  ScopedTexture2DRestorer restorer(this);

  GLuint target_texture = texture->Object();
  bool possible_direct_copy = false;
  if (function_id == kTexImage2D || function_id == kTexSubImage2D) {
    possible_direct_copy = Extensions3DUtil::CanUseCopyTextureCHROMIUM(target);
  }

  GLint copy_x_offset = xoffset;
  GLint copy_y_offset = yoffset;
  GLenum copy_target = target;

  // if direct copy is not possible, create a temporary texture and then copy
  // from canvas to temporary texture to target texture.
  if (!possible_direct_copy) {
    ContextGL()->GenTextures(1, &target_texture);
    ContextGL()->BindTexture(GL_TEXTURE_2D, target_texture);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                               GL_NEAREST);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                               GL_NEAREST);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                               GL_CLAMP_TO_EDGE);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                               GL_CLAMP_TO_EDGE);
    ContextGL()->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                            GL_RGBA, GL_UNSIGNED_BYTE, 0);
    copy_x_offset = 0;
    copy_y_offset = 0;
    copy_target = GL_TEXTURE_2D;
  }

  {
    // glCopyTextureCHROMIUM has a DRAW_AND_READBACK path which will call
    // texImage2D. So, reset unpack buffer parameters before that.
    ScopedUnpackParametersResetRestore temporaryResetUnpack(this);
    if (image->IsCanvasElement()) {
      TexImageCanvasByGPU(function_id, static_cast<HTMLCanvasElement*>(image),
                          copy_target, target_texture, copy_x_offset,
                          copy_y_offset, source_sub_rectangle);
    } else {
      TexImageBitmapByGPU(static_cast<ImageBitmap*>(image), copy_target,
                          target_texture, !unpack_flip_y_, copy_x_offset,
                          copy_y_offset, source_sub_rectangle);
    }
  }

  if (!possible_direct_copy) {
    GLuint tmp_fbo;
    ContextGL()->GenFramebuffers(1, &tmp_fbo);
    ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, tmp_fbo);
    ContextGL()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, target_texture, 0);
    ContextGL()->BindTexture(texture->GetTarget(), texture->Object());
    if (function_id == kTexImage2D) {
      ContextGL()->CopyTexSubImage2D(target, level, 0, 0, 0, 0, width, height);
    } else if (function_id == kTexSubImage2D) {
      ContextGL()->CopyTexSubImage2D(target, level, xoffset, yoffset, 0, 0,
                                     width, height);
    } else if (function_id == kTexSubImage3D) {
      ContextGL()->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                     0, 0, width, height);
    }
    ContextGL()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, 0, 0);
    RestoreCurrentFramebuffer();
    ContextGL()->DeleteFramebuffers(1, &tmp_fbo);
    ContextGL()->DeleteTextures(1, &target_texture);
  }
}

void WebGLRenderingContextBase::TexImageHelperHTMLCanvasElement(
    SecurityOrigin* security_origin,
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLCanvasElement* canvas,
    const IntRect& source_sub_rectangle,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;

  if (!ValidateHTMLCanvasElement(security_origin, func_name, canvas,
                                 exception_state))
    return;
  WebGLTexture* texture =
      ValidateTexImageBinding(func_name, function_id, target);
  if (!texture)
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceHTMLCanvasElement,
                       target, level, internalformat, canvas->width(),
                       canvas->height(), depth, 0, format, type, xoffset,
                       yoffset, zoffset))
    return;

  // Note that the sub-rectangle validation is needed for the GPU-GPU
  // copy case, but is redundant for the software upload case
  // (texImageImpl).
  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(
          func_name, function_id, canvas, source_sub_rectangle, depth,
          unpack_image_height, &selecting_sub_rectangle)) {
    return;
  }

  if (function_id == kTexImage2D || function_id == kTexSubImage2D) {
    // texImageByGPU relies on copyTextureCHROMIUM which doesn't support
    // float/integer/sRGB internal format.
    // TODO(crbug.com/622958): relax the constrains if copyTextureCHROMIUM is
    // upgraded to handle more formats.
    if (!canvas->IsAccelerated() || !CanUseTexImageByGPU(format, type)) {
      // 2D canvas has only FrontBuffer.
      TexImageImpl(function_id, target, level, internalformat, xoffset, yoffset,
                   zoffset, format, type,
                   canvas
                       ->CopiedImage(kFrontBuffer, kPreferAcceleration,
                                     FunctionIDToSnapshotReason(function_id))
                       .Get(),
                   WebGLImageConversion::kHtmlDomCanvas, unpack_flip_y_,
                   unpack_premultiply_alpha_, source_sub_rectangle, 1, 0);
      return;
    }

    // The GPU-GPU copy path uses the Y-up coordinate system.
    IntRect adjusted_source_sub_rectangle = source_sub_rectangle;
    if (!unpack_flip_y_) {
      adjusted_source_sub_rectangle.SetY(canvas->height() -
                                         adjusted_source_sub_rectangle.MaxY());
    }

    if (function_id == kTexImage2D) {
      TexImage2DBase(target, level, internalformat,
                     source_sub_rectangle.Width(),
                     source_sub_rectangle.Height(), 0, format, type, 0);
      TexImageByGPU(function_id, texture, target, level, 0, 0, 0, canvas,
                    adjusted_source_sub_rectangle);
    } else {
      TexImageByGPU(function_id, texture, target, level, xoffset, yoffset, 0,
                    canvas, adjusted_source_sub_rectangle);
    }
  } else {
    // 3D functions.

    // TODO(zmo): Implement GPU-to-GPU copy path (crbug.com/612542).
    // Note that code will also be needed to copy to layers of 3D
    // textures, and elements of 2D texture arrays.
    TexImageImpl(function_id, target, level, internalformat, xoffset, yoffset,
                 zoffset, format, type,
                 canvas
                     ->CopiedImage(kFrontBuffer, kPreferAcceleration,
                                   FunctionIDToSnapshotReason(function_id))
                     .Get(),
                 WebGLImageConversion::kHtmlDomCanvas, unpack_flip_y_,
                 unpack_premultiply_alpha_, source_sub_rectangle, depth,
                 unpack_image_height);
  }
}

void WebGLRenderingContextBase::texImage2D(ExecutionContext* execution_context,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLCanvasElement* canvas,
                                           ExceptionState& exception_state) {
  TexImageHelperHTMLCanvasElement(
      execution_context->GetSecurityOrigin(), kTexImage2D, target, level,
      internalformat, format, type, 0, 0, 0, canvas,
      GetTextureSourceSize(canvas), 1, 0, exception_state);
}

PassRefPtr<Image> WebGLRenderingContextBase::VideoFrameToImage(
    HTMLVideoElement* video) {
  IntSize size(video->videoWidth(), video->videoHeight());
  ImageBuffer* buf = generated_image_cache_.GetImageBuffer(size);
  if (!buf) {
    SynthesizeGLError(GL_OUT_OF_MEMORY, "texImage2D", "out of memory");
    return nullptr;
  }
  IntRect dest_rect(0, 0, size.Width(), size.Height());
  video->PaintCurrentFrame(buf->Canvas(), dest_rect, nullptr);
  return buf->NewImageSnapshot();
}

void WebGLRenderingContextBase::TexImageHelperHTMLVideoElement(
    SecurityOrigin* security_origin,
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLVideoElement* video,
    const IntRect& source_image_rect,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;

  if (!ValidateHTMLVideoElement(security_origin, func_name, video,
                                exception_state))
    return;
  WebGLTexture* texture =
      ValidateTexImageBinding(func_name, function_id, target);
  if (!texture)
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceHTMLVideoElement,
                       target, level, internalformat, video->videoWidth(),
                       video->videoHeight(), 1, 0, format, type, xoffset,
                       yoffset, zoffset))
    return;

  bool source_image_rect_is_default =
      source_image_rect == SentinelEmptyRect() ||
      source_image_rect ==
          IntRect(0, 0, video->videoWidth(), video->videoHeight());
  const bool use_copyTextureCHROMIUM = function_id == kTexImage2D &&
                                       source_image_rect_is_default &&
                                       depth == 1 && GL_TEXTURE_2D == target &&
                                       CanUseTexImageByGPU(format, type);
  // Format of source video may be 16-bit format, e.g. Y16 format.
  // glCopyTextureCHROMIUM requires the source texture to be in 8-bit format.
  // Converting 16-bits formated source texture to 8-bits formated texture will
  // cause precision lost. So, uploading such video texture to half float or
  // float texture can not use GPU-GPU path.
  if (use_copyTextureCHROMIUM) {
    DCHECK_EQ(xoffset, 0);
    DCHECK_EQ(yoffset, 0);
    DCHECK_EQ(zoffset, 0);
    // Go through the fast path doing a GPU-GPU textures copy without a readback
    // to system memory if possible.  Otherwise, it will fall back to the normal
    // SW path.

    if (video->CopyVideoTextureToPlatformTexture(
            ContextGL(), target, texture->Object(), internalformat, format,
            type, level, unpack_premultiply_alpha_, unpack_flip_y_)) {
      texture->UpdateLastUploadedVideo(video->GetWebMediaPlayer());
      return;
    }
  }

  if (source_image_rect_is_default) {
    // Try using optimized CPU-GPU path for some formats: e.g. Y16 and Y8. It
    // leaves early for other formats or if frame is stored on GPU.
    ScopedUnpackParametersResetRestore(
        this, unpack_flip_y_ || unpack_premultiply_alpha_);
    if (video->TexImageImpl(
            static_cast<WebMediaPlayer::TexImageFunctionID>(function_id),
            target, ContextGL(), texture->Object(), level,
            ConvertTexInternalFormat(internalformat, type), format, type,
            xoffset, yoffset, zoffset, unpack_flip_y_,
            unpack_premultiply_alpha_ &&
                unpack_colorspace_conversion_ == GL_NONE)) {
      texture->UpdateLastUploadedVideo(video->GetWebMediaPlayer());
      return;
    }
  }

  if (use_copyTextureCHROMIUM) {
    // Try using an accelerated image buffer, this allows YUV conversion to be
    // done on the GPU.
    std::unique_ptr<ImageBufferSurface> surface =
        WTF::WrapUnique(new AcceleratedImageBufferSurface(
            IntSize(video->videoWidth(), video->videoHeight())));
    if (surface->IsValid()) {
      std::unique_ptr<ImageBuffer> image_buffer(
          ImageBuffer::Create(std::move(surface)));
      if (image_buffer) {
        // The video element paints an RGBA frame into our surface here. By
        // using an AcceleratedImageBufferSurface, we enable the WebMediaPlayer
        // implementation to do any necessary color space conversion on the GPU
        // (though it may still do a CPU conversion and upload the results).
        video->PaintCurrentFrame(
            image_buffer->Canvas(),
            IntRect(0, 0, video->videoWidth(), video->videoHeight()), nullptr);

        // This is a straight GPU-GPU copy, any necessary color space conversion
        // was handled in the paintCurrentFrameInContext() call.

        // Note that copyToPlatformTexture no longer allocates the destination
        // texture.
        TexImage2DBase(target, level, internalformat, video->videoWidth(),
                       video->videoHeight(), 0, format, type, nullptr);

        if (image_buffer->CopyToPlatformTexture(
                FunctionIDToSnapshotReason(function_id), ContextGL(), target,
                texture->Object(), unpack_premultiply_alpha_, unpack_flip_y_,
                IntPoint(0, 0),
                IntRect(0, 0, video->videoWidth(), video->videoHeight()))) {
          texture->UpdateLastUploadedVideo(video->GetWebMediaPlayer());
          return;
        }
      }
    }
  }

  RefPtr<Image> image = VideoFrameToImage(video);
  if (!image)
    return;
  TexImageImpl(function_id, target, level, internalformat, xoffset, yoffset,
               zoffset, format, type, image.Get(),
               WebGLImageConversion::kHtmlDomVideo, unpack_flip_y_,
               unpack_premultiply_alpha_, source_image_rect, depth,
               unpack_image_height);
  texture->UpdateLastUploadedVideo(video->GetWebMediaPlayer());
}

void WebGLRenderingContextBase::TexImageBitmapByGPU(
    ImageBitmap* bitmap,
    GLenum target,
    GLuint target_texture,
    bool flip_y,
    GLint xoffset,
    GLint yoffset,
    const IntRect& source_sub_rect) {
  bitmap->BitmapImage()->CopyToTexture(
      GetDrawingBuffer()->ContextProvider(), target, target_texture, flip_y,
      IntPoint(xoffset, yoffset), source_sub_rect);
}

void WebGLRenderingContextBase::texImage2D(ExecutionContext* execution_context,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLVideoElement* video,
                                           ExceptionState& exception_state) {
  TexImageHelperHTMLVideoElement(execution_context->GetSecurityOrigin(),
                                 kTexImage2D, target, level, internalformat,
                                 format, type, 0, 0, 0, video,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

void WebGLRenderingContextBase::TexImageHelperImageBitmap(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    ImageBitmap* bitmap,
    const IntRect& source_sub_rect,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;
  if (!ValidateImageBitmap(func_name, bitmap, exception_state))
    return;
  WebGLTexture* texture =
      ValidateTexImageBinding(func_name, function_id, target);
  if (!texture)
    return;

  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(func_name, function_id, bitmap,
                                    source_sub_rect, depth, unpack_image_height,
                                    &selecting_sub_rectangle)) {
    return;
  }

  TexImageFunctionType function_type;
  if (function_id == kTexImage2D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;

  GLsizei width = source_sub_rect.Width();
  GLsizei height = source_sub_rect.Height();
  if (!ValidateTexFunc(func_name, function_type, kSourceImageBitmap, target,
                       level, internalformat, width, height, depth, 0, format,
                       type, xoffset, yoffset, zoffset))
    return;
  DCHECK(bitmap->BitmapImage());

  // TODO(kbr): make this work for sub-rectangles of ImageBitmaps.
  if (function_id != kTexSubImage3D && function_id != kTexImage3D &&
      bitmap->IsAccelerated() && CanUseTexImageByGPU(format, type) &&
      !selecting_sub_rectangle) {
    if (function_id == kTexImage2D) {
      TexImage2DBase(target, level, internalformat, width, height, 0, format,
                     type, 0);
      TexImageByGPU(function_id, texture, target, level, 0, 0, 0, bitmap,
                    source_sub_rect);
    } else if (function_id == kTexSubImage2D) {
      TexImageByGPU(function_id, texture, target, level, xoffset, yoffset, 0,
                    bitmap, source_sub_rect);
    }
    return;
  }
  sk_sp<SkImage> sk_image = bitmap->BitmapImage()->ImageForCurrentFrame();
  SkPixmap pixmap;
  uint8_t* pixel_data_ptr = nullptr;
  RefPtr<Uint8Array> pixel_data;
  // In the case where an ImageBitmap is not texture backed, peekPixels() always
  // succeed.  However, when it is texture backed and !canUseTexImageByGPU, we
  // do a GPU read back.
  bool peek_succeed = sk_image->peekPixels(&pixmap);
  if (peek_succeed) {
    pixel_data_ptr = static_cast<uint8_t*>(pixmap.writable_addr());
  } else {
    pixel_data = bitmap->CopyBitmapData(
        bitmap->IsPremultiplied() ? kPremultiplyAlpha : kDontPremultiplyAlpha);
    pixel_data_ptr = pixel_data->Data();
  }
  Vector<uint8_t> data;
  bool need_conversion = true;
  bool have_peekable_rgba =
      (peek_succeed &&
       pixmap.colorType() == SkColorType::kRGBA_8888_SkColorType);
  bool is_pixel_data_rgba = (have_peekable_rgba || !peek_succeed);
  if (is_pixel_data_rgba && format == GL_RGBA && type == GL_UNSIGNED_BYTE &&
      !selecting_sub_rectangle && depth == 1) {
    need_conversion = false;
  } else {
    if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
      // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
      type = GL_FLOAT;
    }
    // In the case of ImageBitmap, we do not need to apply flipY or
    // premultiplyAlpha.
    bool is_pixel_data_bgra =
        pixmap.colorType() == SkColorType::kBGRA_8888_SkColorType;
    if ((is_pixel_data_bgra &&
         !WebGLImageConversion::ExtractImageData(
             pixel_data_ptr, WebGLImageConversion::DataFormat::kDataFormatBGRA8,
             bitmap->Size(), source_sub_rect, depth, unpack_image_height,
             format, type, false, false, data)) ||
        (is_pixel_data_rgba &&
         !WebGLImageConversion::ExtractImageData(
             pixel_data_ptr, WebGLImageConversion::DataFormat::kDataFormatRGBA8,
             bitmap->Size(), source_sub_rect, depth, unpack_image_height,
             format, type, false, false, data))) {
      SynthesizeGLError(GL_INVALID_VALUE, func_name, "bad image data");
      return;
    }
  }
  ScopedUnpackParametersResetRestore temporary_reset_unpack(this);
  if (function_id == kTexImage2D) {
    TexImage2DBase(target, level, internalformat, width, height, 0, format,
                   type, need_conversion ? data.data() : pixel_data_ptr);
  } else if (function_id == kTexSubImage2D) {
    ContextGL()->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                               format, type,
                               need_conversion ? data.data() : pixel_data_ptr);
  } else if (function_id == kTexImage3D) {
    ContextGL()->TexImage3D(target, level, internalformat, width, height, depth,
                            0, format, type,
                            need_conversion ? data.data() : pixel_data_ptr);
  } else {
    DCHECK_EQ(function_id, kTexSubImage3D);
    ContextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type,
                               need_conversion ? data.data() : pixel_data_ptr);
  }
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageBitmap* bitmap,
                                           ExceptionState& exception_state) {
  TexImageHelperImageBitmap(kTexImage2D, target, level, internalformat, format,
                            type, 0, 0, 0, bitmap, GetTextureSourceSize(bitmap),
                            1, 0, exception_state);
}

void WebGLRenderingContextBase::TexParameter(GLenum target,
                                             GLenum pname,
                                             GLfloat paramf,
                                             GLint parami,
                                             bool is_float) {
  if (isContextLost())
    return;
  if (!ValidateTextureBinding("texParameter", target))
    return;
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_MAG_FILTER:
      break;
    case GL_TEXTURE_WRAP_R:
      // fall through to WRAP_S and WRAP_T for WebGL 2 or higher
      if (!IsWebGL2OrHigher()) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                          "invalid parameter name");
        return;
      }
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
      if ((is_float && paramf != GL_CLAMP_TO_EDGE &&
           paramf != GL_MIRRORED_REPEAT && paramf != GL_REPEAT) ||
          (!is_float && parami != GL_CLAMP_TO_EDGE &&
           parami != GL_MIRRORED_REPEAT && parami != GL_REPEAT)) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter", "invalid parameter");
        return;
      }
      break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (!ExtensionEnabled(kEXTTextureFilterAnisotropicName)) {
        SynthesizeGLError(
            GL_INVALID_ENUM, "texParameter",
            "invalid parameter, EXT_texture_filter_anisotropic not enabled");
        return;
      }
      break;
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE:
    case GL_TEXTURE_BASE_LEVEL:
    case GL_TEXTURE_MAX_LEVEL:
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
      if (!IsWebGL2OrHigher()) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                          "invalid parameter name");
        return;
      }
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                        "invalid parameter name");
      return;
  }
  if (is_float) {
    ContextGL()->TexParameterf(target, pname, paramf);
  } else {
    ContextGL()->TexParameteri(target, pname, parami);
  }
}

void WebGLRenderingContextBase::texParameterf(GLenum target,
                                              GLenum pname,
                                              GLfloat param) {
  TexParameter(target, pname, param, 0, true);
}

void WebGLRenderingContextBase::texParameteri(GLenum target,
                                              GLenum pname,
                                              GLint param) {
  TexParameter(target, pname, 0, param, false);
}

void WebGLRenderingContextBase::texSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  TexImageHelperDOMArrayBufferView(kTexSubImage2D, target, level, 0, width,
                                   height, 1, 0, format, type, xoffset, yoffset,
                                   0, pixels.View(), kNullNotAllowed, 0);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageData* pixels) {
  TexImageHelperImageData(kTexSubImage2D, target, level, 0, 0, format, type, 1,
                          xoffset, yoffset, 0, pixels, GetImageDataSize(pixels),
                          0);
}

void WebGLRenderingContextBase::texSubImage2D(
    ExecutionContext* execution_context,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLenum format,
    GLenum type,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  TexImageHelperHTMLImageElement(execution_context->GetSecurityOrigin(),
                                 kTexSubImage2D, target, level, 0, format, type,
                                 xoffset, yoffset, 0, image,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(
    ExecutionContext* execution_context,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLenum format,
    GLenum type,
    HTMLCanvasElement* canvas,
    ExceptionState& exception_state) {
  TexImageHelperHTMLCanvasElement(
      execution_context->GetSecurityOrigin(), kTexSubImage2D, target, level, 0,
      format, type, xoffset, yoffset, 0, canvas, GetTextureSourceSize(canvas),
      1, 0, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(
    ExecutionContext* execution_context,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLenum format,
    GLenum type,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  TexImageHelperHTMLVideoElement(execution_context->GetSecurityOrigin(),
                                 kTexSubImage2D, target, level, 0, format, type,
                                 xoffset, yoffset, 0, video,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageBitmap* bitmap,
                                              ExceptionState& exception_state) {
  TexImageHelperImageBitmap(
      kTexSubImage2D, target, level, 0, format, type, xoffset, yoffset, 0,
      bitmap, GetTextureSourceSize(bitmap), 1, 0, exception_state);
}

void WebGLRenderingContextBase::uniform1f(const WebGLUniformLocation* location,
                                          GLfloat x) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform1f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform1f(location->Location(), x);
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Float32Array>(
                             "uniform1fv", location, v, 1, 0, v.length()))
    return;

  ContextGL()->Uniform1fv(location->Location(), v.length(),
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1fv", location, v.data(), v.size(), 1,
                                 0, v.size()))
    return;

  ContextGL()->Uniform1fv(location->Location(), v.size(), v.data());
}

void WebGLRenderingContextBase::uniform1i(const WebGLUniformLocation* location,
                                          GLint x) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform1i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform1i(location->Location(), x);
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Int32Array>(
                             "uniform1iv", location, v, 1, 0, v.length()))
    return;

  ContextGL()->Uniform1iv(location->Location(), v.length(),
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1iv", location, v.data(), v.size(), 1,
                                 0, v.size()))
    return;

  ContextGL()->Uniform1iv(location->Location(), v.size(), v.data());
}

void WebGLRenderingContextBase::uniform2f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform2f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform2f(location->Location(), x, y);
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Float32Array>(
                             "uniform2fv", location, v, 2, 0, v.length()))
    return;

  ContextGL()->Uniform2fv(location->Location(), v.length() >> 1,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2fv", location, v.data(), v.size(), 2,
                                 0, v.size()))
    return;

  ContextGL()->Uniform2fv(location->Location(), v.size() >> 1, v.data());
}

void WebGLRenderingContextBase::uniform2i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform2i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform2i(location->Location(), x, y);
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Int32Array>(
                             "uniform2iv", location, v, 2, 0, v.length()))
    return;

  ContextGL()->Uniform2iv(location->Location(), v.length() >> 1,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2iv", location, v.data(), v.size(), 2,
                                 0, v.size()))
    return;

  ContextGL()->Uniform2iv(location->Location(), v.size() >> 1, v.data());
}

void WebGLRenderingContextBase::uniform3f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform3f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform3f(location->Location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Float32Array>(
                             "uniform3fv", location, v, 3, 0, v.length()))
    return;

  ContextGL()->Uniform3fv(location->Location(), v.length() / 3,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3fv", location, v.data(), v.size(), 3,
                                 0, v.size()))
    return;

  ContextGL()->Uniform3fv(location->Location(), v.size() / 3, v.data());
}

void WebGLRenderingContextBase::uniform3i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform3i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform3i(location->Location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Int32Array>(
                             "uniform3iv", location, v, 3, 0, v.length()))
    return;

  ContextGL()->Uniform3iv(location->Location(), v.length() / 3,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3iv", location, v.data(), v.size(), 3,
                                 0, v.size()))
    return;

  ContextGL()->Uniform3iv(location->Location(), v.size() / 3, v.data());
}

void WebGLRenderingContextBase::uniform4f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z,
                                          GLfloat w) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform4f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform4f(location->Location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Float32Array>(
                             "uniform4fv", location, v, 4, 0, v.length()))
    return;

  ContextGL()->Uniform4fv(location->Location(), v.length() >> 2,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4fv", location, v.data(), v.size(), 4,
                                 0, v.size()))
    return;

  ContextGL()->Uniform4fv(location->Location(), v.size() >> 2, v.data());
}

void WebGLRenderingContextBase::uniform4i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z,
                                          GLint w) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform4i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform4i(location->Location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() || !ValidateUniformParameters<WTF::Int32Array>(
                             "uniform4iv", location, v, 4, 0, v.length()))
    return;

  ContextGL()->Uniform4iv(location->Location(), v.length() >> 2,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4iv", location, v.data(), v.size(), 4,
                                 0, v.size()))
    return;

  ContextGL()->Uniform4iv(location->Location(), v.size() >> 2, v.data());
}

void WebGLRenderingContextBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    MaybeShared<DOMFloat32Array> v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix2fv", location, transpose,
                                       v.View(), 4, 0, v.View()->length()))
    return;
  ContextGL()->UniformMatrix2fv(location->Location(), v.View()->length() >> 2,
                                transpose, v.View()->DataMaybeShared());
}

void WebGLRenderingContextBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix2fv", location, transpose,
                                       v.data(), v.size(), 4, 0, v.size()))
    return;
  ContextGL()->UniformMatrix2fv(location->Location(), v.size() >> 2, transpose,
                                v.data());
}

void WebGLRenderingContextBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    MaybeShared<DOMFloat32Array> v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix3fv", location, transpose,
                                       v.View(), 9, 0, v.View()->length()))
    return;
  ContextGL()->UniformMatrix3fv(location->Location(), v.View()->length() / 9,
                                transpose, v.View()->DataMaybeShared());
}

void WebGLRenderingContextBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix3fv", location, transpose,
                                       v.data(), v.size(), 9, 0, v.size()))
    return;
  ContextGL()->UniformMatrix3fv(location->Location(), v.size() / 9, transpose,
                                v.data());
}

void WebGLRenderingContextBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    MaybeShared<DOMFloat32Array> v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix4fv", location, transpose,
                                       v.View(), 16, 0, v.View()->length()))
    return;
  ContextGL()->UniformMatrix4fv(location->Location(), v.View()->length() >> 4,
                                transpose, v.View()->DataMaybeShared());
}

void WebGLRenderingContextBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix4fv", location, transpose,
                                       v.data(), v.size(), 16, 0, v.size()))
    return;
  ContextGL()->UniformMatrix4fv(location->Location(), v.size() >> 4, transpose,
                                v.data());
}

void WebGLRenderingContextBase::useProgram(WebGLProgram* program) {
  bool deleted;
  if (!CheckObjectToBeBound("useProgram", program, deleted))
    return;
  if (deleted)
    program = 0;
  if (program && !program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "useProgram", "program not valid");
    return;
  }

  if (current_program_ != program) {
    if (current_program_)
      current_program_->OnDetached(ContextGL());
    current_program_ = program;
    ContextGL()->UseProgram(ObjectOrZero(program));
    if (program)
      program->OnAttached();
  }
}

void WebGLRenderingContextBase::validateProgram(WebGLProgram* program) {
  if (isContextLost() || !ValidateWebGLObject("validateProgram", program))
    return;
  ContextGL()->ValidateProgram(ObjectOrZero(program));
}

void WebGLRenderingContextBase::SetVertexAttribType(
    GLuint index,
    VertexAttribValueType type) {
  if (index < max_vertex_attribs_)
    vertex_attrib_type_[index] = type;
}

void WebGLRenderingContextBase::vertexAttrib1f(GLuint index, GLfloat v0) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib1f(index, v0);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib1fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 1) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib1fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib1fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib1fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 1) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib1fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib1fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib2f(index, v0, v1);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 2) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib2fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib2fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 2) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib2fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib2fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1,
                                               GLfloat v2) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib3f(index, v0, v1, v2);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 3) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib3fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib3fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 3) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib3fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib3fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1,
                                               GLfloat v2,
                                               GLfloat v3) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib4f(index, v0, v1, v2, v3);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 4) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib4fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib4fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 4) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib4fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib4fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttribPointer(GLuint index,
                                                    GLint size,
                                                    GLenum type,
                                                    GLboolean normalized,
                                                    GLsizei stride,
                                                    long long offset) {
  if (isContextLost())
    return;
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttribPointer",
                      "index out of range");
    return;
  }
  if (!ValidateValueFitNonNegInt32("vertexAttribPointer", "offset", offset))
    return;
  if (!bound_array_buffer_ && offset != 0) {
    SynthesizeGLError(GL_INVALID_OPERATION, "vertexAttribPointer",
                      "no ARRAY_BUFFER is bound and offset is non-zero");
    return;
  }

  bound_vertex_array_object_->SetArrayBufferForAttrib(
      index, bound_array_buffer_.Get());
  ContextGL()->VertexAttribPointer(
      index, size, type, normalized, stride,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void WebGLRenderingContextBase::VertexAttribDivisorANGLE(GLuint index,
                                                         GLuint divisor) {
  if (isContextLost())
    return;

  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttribDivisorANGLE",
                      "index out of range");
    return;
  }

  ContextGL()->VertexAttribDivisorANGLE(index, divisor);
}

void WebGLRenderingContextBase::viewport(GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height) {
  if (isContextLost())
    return;
  ContextGL()->Viewport(x, y, width, height);
}

// Added to provide a unified interface with CanvasRenderingContext2D. Prefer
// calling forceLostContext instead.
void WebGLRenderingContextBase::LoseContext(LostContextMode mode) {
  ForceLostContext(mode, kManual);
}

void WebGLRenderingContextBase::ForceLostContext(
    LostContextMode mode,
    AutoRecoveryMethod auto_recovery_method) {
  if (isContextLost()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "loseContext",
                      "context already lost");
    return;
  }

  context_group_->LoseContextGroup(mode, auto_recovery_method);
}

void WebGLRenderingContextBase::LoseContextImpl(
    WebGLRenderingContextBase::LostContextMode mode,
    AutoRecoveryMethod auto_recovery_method) {
  if (isContextLost())
    return;

  context_lost_mode_ = mode;
  DCHECK_NE(context_lost_mode_, kNotLostContext);
  auto_recovery_method_ = auto_recovery_method;

  // Lose all the extensions.
  for (size_t i = 0; i < extensions_.size(); ++i) {
    ExtensionTracker* tracker = extensions_[i];
    tracker->LoseExtension(false);
  }

  for (size_t i = 0; i < kWebGLExtensionNameCount; ++i)
    extension_enabled_[i] = false;

  RemoveAllCompressedTextureFormats();

  if (mode != kRealLostContext)
    DestroyContext();

  ConsoleDisplayPreference display =
      (mode == kRealLostContext) ? kDisplayInConsole : kDontDisplayInConsole;
  SynthesizeGLError(GC3D_CONTEXT_LOST_WEBGL, "loseContext", "context lost",
                    display);

  // Don't allow restoration unless the context lost event has both been
  // dispatched and its default behavior prevented.
  restore_allowed_ = false;
  DeactivateContext(this);
  if (auto_recovery_method_ == kWhenAvailable)
    AddToEvictedList(this);

  // Always defer the dispatch of the context lost event, to implement
  // the spec behavior of queueing a task.
  dispatch_context_lost_event_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void WebGLRenderingContextBase::ForceRestoreContext() {
  if (!isContextLost()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "restoreContext",
                      "context not lost");
    return;
  }

  if (!restore_allowed_) {
    if (context_lost_mode_ == kWebGLLoseContextLostContext)
      SynthesizeGLError(GL_INVALID_OPERATION, "restoreContext",
                        "context restoration not allowed");
    return;
  }

  if (!restore_timer_.IsActive())
    restore_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

uint32_t WebGLRenderingContextBase::NumberOfContextLosses() const {
  return context_group_->NumberOfContextLosses();
}

WebLayer* WebGLRenderingContextBase::PlatformLayer() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->PlatformLayer();
}

void WebGLRenderingContextBase::SetFilterQuality(
    SkFilterQuality filter_quality) {
  if (!isContextLost() && GetDrawingBuffer()) {
    GetDrawingBuffer()->SetFilterQuality(filter_quality);
  }
}

Extensions3DUtil* WebGLRenderingContextBase::ExtensionsUtil() {
  if (!extensions_util_) {
    gpu::gles2::GLES2Interface* gl = ContextGL();
    extensions_util_ = Extensions3DUtil::Create(gl);
    // The only reason the ExtensionsUtil should be invalid is if the gl context
    // is lost.
    DCHECK(extensions_util_->IsValid() ||
           gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  }
  return extensions_util_.get();
}

void WebGLRenderingContextBase::Stop() {
  if (!isContextLost()) {
    // Never attempt to restore the context because the page is being torn down.
    ForceLostContext(kSyntheticLostContext, kManual);
  }
}

bool WebGLRenderingContextBase::DrawingBufferClientIsBoundForDraw() {
  return !framebuffer_binding_;
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreScissorTest() {
  if (!ContextGL())
    return;
  if (scissor_enabled_)
    ContextGL()->Enable(GL_SCISSOR_TEST);
  else
    ContextGL()->Disable(GL_SCISSOR_TEST);
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreMaskAndClearValues() {
  if (!ContextGL())
    return;
  bool color_mask_alpha =
      color_mask_[3] && active_scoped_rgb_emulation_color_masks_ == 0;
  ContextGL()->ColorMask(color_mask_[0], color_mask_[1], color_mask_[2],
                         color_mask_alpha);
  ContextGL()->DepthMask(depth_mask_);
  ContextGL()->StencilMaskSeparate(GL_FRONT, stencil_mask_);

  ContextGL()->ClearColor(clear_color_[0], clear_color_[1], clear_color_[2],
                          clear_color_[3]);
  ContextGL()->ClearDepthf(clear_depth_);
  ContextGL()->ClearStencil(clear_stencil_);
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestorePixelPackParameters() {
  if (!ContextGL())
    return;
  ContextGL()->PixelStorei(GL_PACK_ALIGNMENT, pack_alignment_);
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreTexture2DBinding() {
  if (!ContextGL())
    return;
  RestoreCurrentTexture2D();
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestoreRenderbufferBinding() {
  if (!ContextGL())
    return;
  ContextGL()->BindRenderbuffer(GL_RENDERBUFFER,
                                ObjectOrZero(renderbuffer_binding_.Get()));
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreFramebufferBinding() {
  if (!ContextGL())
    return;
  RestoreCurrentFramebuffer();
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestorePixelUnpackBufferBinding() {}
void WebGLRenderingContextBase::
    DrawingBufferClientRestorePixelPackBufferBinding() {}

ScriptValue WebGLRenderingContextBase::GetBooleanParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLboolean value = 0;
  if (!isContextLost())
    ContextGL()->GetBooleanv(pname, &value);
  return WebGLAny(script_state, static_cast<bool>(value));
}

ScriptValue WebGLRenderingContextBase::GetBooleanArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  if (pname != GL_COLOR_WRITEMASK) {
    NOTIMPLEMENTED();
    return WebGLAny(script_state, 0, 0);
  }
  GLboolean value[4] = {0};
  if (!isContextLost())
    ContextGL()->GetBooleanv(pname, value);
  bool bool_value[4];
  for (int ii = 0; ii < 4; ++ii)
    bool_value[ii] = static_cast<bool>(value[ii]);
  return WebGLAny(script_state, bool_value, 4);
}

ScriptValue WebGLRenderingContextBase::GetFloatParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLfloat value = 0;
  if (!isContextLost())
    ContextGL()->GetFloatv(pname, &value);
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextBase::GetIntParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value = 0;
  if (!isContextLost()) {
    ContextGL()->GetIntegerv(pname, &value);
    switch (pname) {
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        if (value == 0) {
          // This indicates read framebuffer is incomplete and an
          // INVALID_OPERATION has been generated.
          return ScriptValue::CreateNull(script_state);
        }
        break;
      default:
        break;
    }
  }
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextBase::GetInt64Parameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint64 value = 0;
  if (!isContextLost())
    ContextGL()->GetInteger64v(pname, &value);
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextBase::GetUnsignedIntParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value = 0;
  if (!isContextLost())
    ContextGL()->GetIntegerv(pname, &value);
  return WebGLAny(script_state, static_cast<unsigned>(value));
}

ScriptValue WebGLRenderingContextBase::GetWebGLFloatArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLfloat value[4] = {0};
  if (!isContextLost())
    ContextGL()->GetFloatv(pname, value);
  unsigned length = 0;
  switch (pname) {
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_DEPTH_RANGE:
      length = 2;
      break;
    case GL_BLEND_COLOR:
    case GL_COLOR_CLEAR_VALUE:
      length = 4;
      break;
    default:
      NOTIMPLEMENTED();
  }
  return WebGLAny(script_state, DOMFloat32Array::Create(value, length));
}

ScriptValue WebGLRenderingContextBase::GetWebGLIntArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value[4] = {0};
  if (!isContextLost())
    ContextGL()->GetIntegerv(pname, value);
  unsigned length = 0;
  switch (pname) {
    case GL_MAX_VIEWPORT_DIMS:
      length = 2;
      break;
    case GL_SCISSOR_BOX:
    case GL_VIEWPORT:
      length = 4;
      break;
    default:
      NOTIMPLEMENTED();
  }
  return WebGLAny(script_state, DOMInt32Array::Create(value, length));
}

WebGLTexture* WebGLRenderingContextBase::ValidateTexture2DBinding(
    const char* function_name,
    GLenum target) {
  WebGLTexture* tex = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      tex = texture_units_[active_texture_unit_].texture2d_binding_.Get();
      break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      tex =
          texture_units_[active_texture_unit_].texture_cube_map_binding_.Get();
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid texture target");
      return nullptr;
  }
  if (!tex)
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no texture bound to target");
  return tex;
}

WebGLTexture* WebGLRenderingContextBase::ValidateTextureBinding(
    const char* function_name,
    GLenum target) {
  WebGLTexture* tex = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      tex = texture_units_[active_texture_unit_].texture2d_binding_.Get();
      break;
    case GL_TEXTURE_CUBE_MAP:
      tex =
          texture_units_[active_texture_unit_].texture_cube_map_binding_.Get();
      break;
    case GL_TEXTURE_3D:
      if (!IsWebGL2OrHigher()) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "invalid texture target");
        return nullptr;
      }
      tex = texture_units_[active_texture_unit_].texture3d_binding_.Get();
      break;
    case GL_TEXTURE_2D_ARRAY:
      if (!IsWebGL2OrHigher()) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "invalid texture target");
        return nullptr;
      }
      tex = texture_units_[active_texture_unit_].texture2d_array_binding_.Get();
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid texture target");
      return nullptr;
  }
  if (!tex)
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no texture bound to target");
  return tex;
}

bool WebGLRenderingContextBase::ValidateLocationLength(
    const char* function_name,
    const String& string) {
  const unsigned max_web_gl_location_length = GetMaxWebGLLocationLength();
  if (string.length() > max_web_gl_location_length) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "location length > 256");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateSize(const char* function_name,
                                             GLint x,
                                             GLint y,
                                             GLint z) {
  if (x < 0 || y < 0 || z < 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "size < 0");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateString(const char* function_name,
                                               const String& string) {
  for (size_t i = 0; i < string.length(); ++i) {
    if (!ValidateCharacter(string[i])) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name, "string not ASCII");
      return false;
    }
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateShaderSource(const String& string) {
  for (size_t i = 0; i < string.length(); ++i) {
    // line-continuation character \ is supported in WebGL 2.0.
    if (IsWebGL2OrHigher() && string[i] == '\\') {
      continue;
    }
    if (!ValidateCharacter(string[i])) {
      SynthesizeGLError(GL_INVALID_VALUE, "shaderSource", "string not ASCII");
      return false;
    }
  }
  return true;
}

void WebGLRenderingContextBase::AddExtensionSupportedFormatsTypes() {
  if (!is_oes_texture_float_formats_types_added_ &&
      ExtensionEnabled(kOESTextureFloatName)) {
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesOESTexFloat);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesOESTexFloat);
    is_oes_texture_float_formats_types_added_ = true;
  }

  if (!is_oes_texture_half_float_formats_types_added_ &&
      ExtensionEnabled(kOESTextureHalfFloatName)) {
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesOESTexHalfFloat);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesOESTexHalfFloat);
    is_oes_texture_half_float_formats_types_added_ = true;
  }

  if (!is_web_gl_depth_texture_formats_types_added_ &&
      ExtensionEnabled(kWebGLDepthTextureName)) {
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                      kSupportedInternalFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_tex_image_source_formats_,
                      kSupportedFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesOESDepthTex);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesOESDepthTex);
    is_web_gl_depth_texture_formats_types_added_ = true;
  }

  if (!is_ext_srgb_formats_types_added_ && ExtensionEnabled(kEXTsRGBName)) {
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsEXTsRGB);
    ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                      kSupportedInternalFormatsEXTsRGB);
    ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsEXTsRGB);
    ADD_VALUES_TO_SET(supported_tex_image_source_formats_,
                      kSupportedFormatsEXTsRGB);
    is_ext_srgb_formats_types_added_ = true;
  }
}

bool WebGLRenderingContextBase::ValidateTexImageSourceFormatAndType(
    const char* function_name,
    TexImageFunctionType function_type,
    GLenum internalformat,
    GLenum format,
    GLenum type) {
  if (!is_web_gl2_tex_image_source_formats_types_added_ && IsWebGL2OrHigher()) {
    ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                      kSupportedInternalFormatsTexImageSourceES3);
    ADD_VALUES_TO_SET(supported_tex_image_source_formats_,
                      kSupportedFormatsTexImageSourceES3);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesTexImageSourceES3);
    is_web_gl2_tex_image_source_formats_types_added_ = true;
  }

  if (!IsWebGL2OrHigher()) {
    AddExtensionSupportedFormatsTypes();
  }

  if (internalformat != 0 &&
      supported_tex_image_source_internal_formats_.find(internalformat) ==
          supported_tex_image_source_internal_formats_.end()) {
    if (function_type == kTexImage) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name,
                        "invalid internalformat");
    } else {
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid internalformat");
    }
    return false;
  }
  if (supported_tex_image_source_formats_.find(format) ==
      supported_tex_image_source_formats_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  if (supported_tex_image_source_types_.find(type) ==
      supported_tex_image_source_types_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncFormatAndType(
    const char* function_name,
    TexImageFunctionType function_type,
    GLenum internalformat,
    GLenum format,
    GLenum type,
    GLint level) {
  if (!is_web_gl2_formats_types_added_ && IsWebGL2OrHigher()) {
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsES3);
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsTexImageES3);
    ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsES3);
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesES3);
    is_web_gl2_formats_types_added_ = true;
  }

  if (!IsWebGL2OrHigher()) {
    AddExtensionSupportedFormatsTypes();
  }

  if (internalformat != 0 && supported_internal_formats_.find(internalformat) ==
                                 supported_internal_formats_.end()) {
    if (function_type == kTexImage) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name,
                        "invalid internalformat");
    } else {
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid internalformat");
    }
    return false;
  }
  if (supported_formats_.find(format) == supported_formats_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  if (supported_types_.find(type) == supported_types_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  if (format == GL_DEPTH_COMPONENT && level > 0 && !IsWebGL2OrHigher()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "level must be 0 for DEPTH_COMPONENT format");
    return false;
  }
  if (format == GL_DEPTH_STENCIL_OES && level > 0 && !IsWebGL2OrHigher()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "level must be 0 for DEPTH_STENCIL format");
    return false;
  }

  return true;
}

GLint WebGLRenderingContextBase::GetMaxTextureLevelForTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return max_texture_level_;
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return max_cube_map_texture_level_;
  }
  return 0;
}

bool WebGLRenderingContextBase::ValidateTexFuncLevel(const char* function_name,
                                                     GLenum target,
                                                     GLint level) {
  if (level < 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "level < 0");
    return false;
  }
  GLint max_level = GetMaxTextureLevelForTarget(target);
  if (max_level && level >= max_level) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "level out of range");
    return false;
  }
  // This function only checks if level is legal, so we return true and don't
  // generate INVALID_ENUM if target is illegal.
  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncDimensions(
    const char* function_name,
    TexImageFunctionType function_type,
    GLenum target,
    GLint level,
    GLsizei width,
    GLsizei height,
    GLsizei depth) {
  if (width < 0 || height < 0 || depth < 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "width, height or depth < 0");
    return false;
  }

  switch (target) {
    case GL_TEXTURE_2D:
      if (width > (max_texture_size_ >> level) ||
          height > (max_texture_size_ >> level)) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "width or height out of range");
        return false;
      }
      break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      if (function_type != kTexSubImage && width != height) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "width != height for cube map");
        return false;
      }
      // No need to check height here. For texImage width == height.
      // For texSubImage that will be checked when checking yoffset + height is
      // in range.
      if (width > (max_cube_map_texture_size_ >> level)) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "width or height out of range for cube map");
        return false;
      }
      break;
    case GL_TEXTURE_3D:
      if (IsWebGL2OrHigher()) {
        if (width > (max3d_texture_size_ >> level) ||
            height > (max3d_texture_size_ >> level) ||
            depth > (max3d_texture_size_ >> level)) {
          SynthesizeGLError(GL_INVALID_VALUE, function_name,
                            "width, height or depth out of range");
          return false;
        }
        break;
      }
    case GL_TEXTURE_2D_ARRAY:
      if (IsWebGL2OrHigher()) {
        if (width > (max_texture_size_ >> level) ||
            height > (max_texture_size_ >> level) ||
            depth > max_array_texture_layers_) {
          SynthesizeGLError(GL_INVALID_VALUE, function_name,
                            "width, height or depth out of range");
          return false;
        }
        break;
      }
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncParameters(
    const char* function_name,
    TexImageFunctionType function_type,
    TexFuncValidationSourceType source_type,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  // We absolutely have to validate the format and type combination.
  // The texImage2D entry points taking HTMLImage, etc. will produce
  // temporary data based on this combination, so it must be legal.
  if (source_type == kSourceHTMLImageElement ||
      source_type == kSourceHTMLCanvasElement ||
      source_type == kSourceHTMLVideoElement ||
      source_type == kSourceImageData || source_type == kSourceImageBitmap) {
    if (!ValidateTexImageSourceFormatAndType(function_name, function_type,
                                             internalformat, format, type)) {
      return false;
    }
  } else {
    if (!ValidateTexFuncFormatAndType(function_name, function_type,
                                      internalformat, format, type, level)) {
      return false;
    }
  }

  if (!ValidateTexFuncDimensions(function_name, function_type, target, level,
                                 width, height, depth))
    return false;

  if (border) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "border != 0");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncData(
    const char* function_name,
    TexImageDimension tex_dimension,
    GLint level,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    DOMArrayBufferView* pixels,
    NullDisposition disposition,
    GLuint src_offset) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  if (!pixels) {
    DCHECK_NE(disposition, kNullNotReachable);
    if (disposition == kNullAllowed)
      return true;
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no pixels");
    return false;
  }

  if (!ValidateSettableTexFormat(function_name, format))
    return false;

  switch (type) {
    case GL_BYTE:
      if (pixels->GetType() != DOMArrayBufferView::kTypeInt8) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type BYTE but ArrayBufferView not Int8Array");
        return false;
      }
      break;
    case GL_UNSIGNED_BYTE:
      if (pixels->GetType() != DOMArrayBufferView::kTypeUint8) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, function_name,
            "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
        return false;
      }
      break;
    case GL_SHORT:
      if (pixels->GetType() != DOMArrayBufferView::kTypeInt16) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type SHORT but ArrayBufferView not Int16Array");
        return false;
      }
      break;
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      if (pixels->GetType() != DOMArrayBufferView::kTypeUint16) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, function_name,
            "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return false;
      }
      break;
    case GL_INT:
      if (pixels->GetType() != DOMArrayBufferView::kTypeInt32) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type INT but ArrayBufferView not Int32Array");
        return false;
      }
      break;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_INT_24_8:
      if (pixels->GetType() != DOMArrayBufferView::kTypeUint32) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, function_name,
            "type UNSIGNED_INT but ArrayBufferView not Uint32Array");
        return false;
      }
      break;
    case GL_FLOAT:  // OES_texture_float
      if (pixels->GetType() != DOMArrayBufferView::kTypeFloat32) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type FLOAT but ArrayBufferView not Float32Array");
        return false;
      }
      break;
    case GL_HALF_FLOAT:
    case GL_HALF_FLOAT_OES:  // OES_texture_half_float
      // As per the specification, ArrayBufferView should be null or a
      // Uint16Array when OES_texture_half_float is enabled.
      if (pixels->GetType() != DOMArrayBufferView::kTypeUint16) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type HALF_FLOAT_OES but ArrayBufferView is not NULL "
                          "and not Uint16Array");
        return false;
      }
      break;
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
      SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                        "type FLOAT_32_UNSIGNED_INT_24_8_REV but "
                        "ArrayBufferView is not NULL");
      return false;
    default:
      NOTREACHED();
  }

  unsigned total_bytes_required, skip_bytes;
  GLenum error = WebGLImageConversion::ComputeImageSizeInBytes(
      format, type, width, height, depth,
      GetUnpackPixelStoreParams(tex_dimension), &total_bytes_required, 0,
      &skip_bytes);
  if (error != GL_NO_ERROR) {
    SynthesizeGLError(error, function_name, "invalid texture dimensions");
    return false;
  }
  CheckedNumeric<uint32_t> total = src_offset;
  total *= pixels->TypeSize();
  total += total_bytes_required;
  total += skip_bytes;
  if (!total.IsValid() || pixels->byteLength() < total.ValueOrDie()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "ArrayBufferView not big enough for request");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateCompressedTexFormat(
    const char* function_name,
    GLenum format) {
  if (!compressed_texture_formats_.Contains(format)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateStencilSettings(
    const char* function_name) {
  if (stencil_mask_ != stencil_mask_back_ ||
      stencil_func_ref_ != stencil_func_ref_back_ ||
      stencil_func_mask_ != stencil_func_mask_back_) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "front and back stencils settings do not match");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateStencilOrDepthFunc(
    const char* function_name,
    GLenum func) {
  switch (func) {
    case GL_NEVER:
    case GL_LESS:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_GEQUAL:
    case GL_EQUAL:
    case GL_NOTEQUAL:
    case GL_ALWAYS:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid function");
      return false;
  }
}

void WebGLRenderingContextBase::PrintGLErrorToConsole(const String& message) {
  if (!num_gl_errors_to_console_allowed_)
    return;

  --num_gl_errors_to_console_allowed_;
  PrintWarningToConsole(message);

  if (!num_gl_errors_to_console_allowed_)
    PrintWarningToConsole(
        "WebGL: too many errors, no more errors will be reported to the "
        "console for this context.");

  return;
}

void WebGLRenderingContextBase::PrintWarningToConsole(const String& message) {
  if (!canvas())
    return;
  canvas()->GetDocument().AddConsoleMessage(ConsoleMessage::Create(
      kRenderingMessageSource, kWarningMessageLevel, message));
}

bool WebGLRenderingContextBase::ValidateFramebufferFuncParameters(
    const char* function_name,
    GLenum target,
    GLenum attachment) {
  if (!ValidateFramebufferTarget(target)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
    return false;
  }
  switch (attachment) {
    case GL_COLOR_ATTACHMENT0:
    case GL_DEPTH_ATTACHMENT:
    case GL_STENCIL_ATTACHMENT:
    case GL_DEPTH_STENCIL_ATTACHMENT:
      break;
    default:
      if ((ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2OrHigher()) &&
          attachment > GL_COLOR_ATTACHMENT0 &&
          attachment <
              static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + MaxColorAttachments()))
        break;
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid attachment");
      return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateBlendEquation(const char* function_name,
                                                      GLenum mode) {
  switch (mode) {
    case GL_FUNC_ADD:
    case GL_FUNC_SUBTRACT:
    case GL_FUNC_REVERSE_SUBTRACT:
      return true;
    case GL_MIN_EXT:
    case GL_MAX_EXT:
      if (ExtensionEnabled(kEXTBlendMinMaxName) || IsWebGL2OrHigher())
        return true;
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid mode");
      return false;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid mode");
      return false;
  }
}

bool WebGLRenderingContextBase::ValidateBlendFuncFactors(
    const char* function_name,
    GLenum src,
    GLenum dst) {
  if (((src == GL_CONSTANT_COLOR || src == GL_ONE_MINUS_CONSTANT_COLOR) &&
       (dst == GL_CONSTANT_ALPHA || dst == GL_ONE_MINUS_CONSTANT_ALPHA)) ||
      ((dst == GL_CONSTANT_COLOR || dst == GL_ONE_MINUS_CONSTANT_COLOR) &&
       (src == GL_CONSTANT_ALPHA || src == GL_ONE_MINUS_CONSTANT_ALPHA))) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "incompatible src and dst");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateCapability(const char* function_name,
                                                   GLenum cap) {
  switch (cap) {
    case GL_BLEND:
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid capability");
      return false;
  }
}

bool WebGLRenderingContextBase::ValidateUniformParameters(
    const char* function_name,
    const WebGLUniformLocation* location,
    void* v,
    GLsizei size,
    GLsizei required_min_size,
    GLuint src_offset,
    GLuint src_length) {
  return ValidateUniformMatrixParameters(function_name, location, false, v,
                                         size, required_min_size, src_offset,
                                         src_length);
}

bool WebGLRenderingContextBase::ValidateUniformMatrixParameters(
    const char* function_name,
    const WebGLUniformLocation* location,
    GLboolean transpose,
    DOMFloat32Array* v,
    GLsizei required_min_size,
    GLuint src_offset,
    GLuint src_length) {
  if (!v) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no array");
    return false;
  }
  return ValidateUniformMatrixParameters(
      function_name, location, transpose, v->DataMaybeShared(), v->length(),
      required_min_size, src_offset, src_length);
}

bool WebGLRenderingContextBase::ValidateUniformMatrixParameters(
    const char* function_name,
    const WebGLUniformLocation* location,
    GLboolean transpose,
    void* v,
    GLsizei size,
    GLsizei required_min_size,
    GLuint src_offset,
    GLuint src_length) {
  DCHECK(size >= 0 && required_min_size > 0);
  if (!location)
    return false;
  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "location is not from current program");
    return false;
  }
  if (!v) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no array");
    return false;
  }
  if (transpose && !IsWebGL2OrHigher()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "transpose not FALSE");
    return false;
  }
  if (src_offset >= static_cast<GLuint>(size)) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "invalid srcOffset");
    return false;
  }
  GLsizei actual_size = size - src_offset;
  if (src_length > 0) {
    if (src_length > static_cast<GLuint>(actual_size)) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name,
                        "invalid srcOffset + srcLength");
      return false;
    }
    actual_size = src_length;
  }
  if (actual_size < required_min_size || (actual_size % required_min_size)) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "invalid size");
    return false;
  }
  return true;
}

WebGLBuffer* WebGLRenderingContextBase::ValidateBufferDataTarget(
    const char* function_name,
    GLenum target) {
  WebGLBuffer* buffer = nullptr;
  switch (target) {
    case GL_ELEMENT_ARRAY_BUFFER:
      buffer = bound_vertex_array_object_->BoundElementArrayBuffer();
      break;
    case GL_ARRAY_BUFFER:
      buffer = bound_array_buffer_.Get();
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return nullptr;
  }
  if (!buffer) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name, "no buffer");
    return nullptr;
  }
  return buffer;
}

bool WebGLRenderingContextBase::ValidateBufferDataUsage(
    const char* function_name,
    GLenum usage) {
  switch (usage) {
    case GL_STREAM_DRAW:
    case GL_STATIC_DRAW:
    case GL_DYNAMIC_DRAW:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid usage");
      return false;
  }
}

void WebGLRenderingContextBase::RemoveBoundBuffer(WebGLBuffer* buffer) {
  if (bound_array_buffer_ == buffer)
    bound_array_buffer_ = nullptr;

  bound_vertex_array_object_->UnbindBuffer(buffer);
}

bool WebGLRenderingContextBase::ValidateHTMLImageElement(
    SecurityOrigin* security_origin,
    const char* function_name,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  if (!image || !image->CachedImage()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no image");
    return false;
  }
  const KURL& url = image->CachedImage()->GetResponse().Url();
  if (url.IsNull() || url.IsEmpty() || !url.IsValid()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "invalid image");
    return false;
  }

  if (WouldTaintOrigin(image, security_origin)) {
    exception_state.ThrowSecurityError("The cross-origin image at " +
                                       url.ElidedString() +
                                       " may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateHTMLCanvasElement(
    SecurityOrigin* security_origin,
    const char* function_name,
    HTMLCanvasElement* canvas,
    ExceptionState& exception_state) {
  if (!canvas || !canvas->IsPaintable()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no canvas");
    return false;
  }

  if (WouldTaintOrigin(canvas, security_origin)) {
    exception_state.ThrowSecurityError("Tainted canvases may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateHTMLVideoElement(
    SecurityOrigin* security_origin,
    const char* function_name,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  if (!video || !video->videoWidth() || !video->videoHeight()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no video");
    return false;
  }

  if (WouldTaintOrigin(video, security_origin)) {
    exception_state.ThrowSecurityError(
        "The video element contains cross-origin data, and may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateImageBitmap(
    const char* function_name,
    ImageBitmap* bitmap,
    ExceptionState& exception_state) {
  if (bitmap->IsNeutered()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "The source data has been detached.");
    return false;
  }
  if (!bitmap->OriginClean()) {
    exception_state.ThrowSecurityError(
        "The ImageBitmap contains cross-origin data, and may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateDrawArrays(const char* function_name) {
  if (isContextLost())
    return false;

  if (!ValidateStencilSettings(function_name))
    return false;

  if (!ValidateRenderingState(function_name)) {
    return false;
  }

  const char* reason = "framebuffer incomplete";
  if (framebuffer_binding_ && framebuffer_binding_->CheckDepthStencilStatus(
                                  &reason) != GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, function_name, reason);
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateDrawElements(const char* function_name,
                                                     GLenum type,
                                                     long long offset) {
  if (isContextLost())
    return false;

  if (!ValidateStencilSettings(function_name))
    return false;

  if (type == GL_UNSIGNED_INT && !IsWebGL2OrHigher() &&
      !ExtensionEnabled(kOESElementIndexUintName)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  if (!ValidateValueFitNonNegInt32(function_name, "offset", offset))
    return false;

  if (!ValidateRenderingState(function_name)) {
    return false;
  }

  const char* reason = "framebuffer incomplete";
  if (framebuffer_binding_ && framebuffer_binding_->CheckDepthStencilStatus(
                                  &reason) != GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, function_name, reason);
    return false;
  }

  return true;
}

void WebGLRenderingContextBase::DispatchContextLostEvent(TimerBase*) {
  WebGLContextEvent* event = WebGLContextEvent::Create(
      EventTypeNames::webglcontextlost, false, true, "");
  host()->HostDispatchEvent(event);
  restore_allowed_ = event->defaultPrevented();
  if (restore_allowed_ && !is_hidden_) {
    if (auto_recovery_method_ == kAuto)
      restore_timer_.StartOneShot(0, BLINK_FROM_HERE);
  }
}

void WebGLRenderingContextBase::MaybeRestoreContext(TimerBase*) {
  DCHECK(isContextLost());

  // The rendering context is not restored unless the default behavior of the
  // webglcontextlost event was prevented earlier.
  //
  // Because of the way m_restoreTimer is set up for real vs. synthetic lost
  // context events, we don't have to worry about this test short-circuiting
  // the retry loop for real context lost events.
  if (!restore_allowed_)
    return;

  if (canvas()) {
    LocalFrame* frame = canvas()->GetDocument().GetFrame();
    if (!frame)
      return;

    Settings* settings = frame->GetSettings();

    if (!frame->Client()->AllowWebGL(settings && settings->GetWebGLEnabled()))
      return;
  }

  // If the context was lost due to RealLostContext, we need to destroy the old
  // DrawingBuffer before creating new DrawingBuffer to ensure resource budget
  // enough.
  if (GetDrawingBuffer()) {
    drawing_buffer_->BeginDestruction();
    drawing_buffer_.Clear();
  }

  auto execution_context = host()->GetTopExecutionContext();
  Platform::ContextAttributes attributes = ToPlatformContextAttributes(
      CreationAttributes(), Version(),
      SupportOwnOffscreenSurface(execution_context));
  Platform::GraphicsInfo gl_info;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider;
  const auto& url = host()->GetExecutionContextUrl();

  if (IsMainThread()) {
    context_provider =
        Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
            attributes, url, 0, &gl_info);
  } else {
    context_provider =
        CreateContextProviderOnWorkerThread(attributes, &gl_info, url);
  }
  RefPtr<DrawingBuffer> buffer;
  if (context_provider && context_provider->BindToCurrentThread()) {
    // Construct a new drawing buffer with the new GL context.
    buffer = CreateDrawingBuffer(std::move(context_provider));
    // If DrawingBuffer::create() fails to allocate a fbo, |drawingBuffer| is
    // set to null.
  }
  if (!buffer) {
    if (context_lost_mode_ == kRealLostContext) {
      restore_timer_.StartOneShot(kSecondsBetweenRestoreAttempts,
                                  BLINK_FROM_HERE);
    } else {
      // This likely shouldn't happen but is the best way to report it to the
      // WebGL app.
      SynthesizeGLError(GL_INVALID_OPERATION, "", "error restoring context");
    }
    return;
  }

  drawing_buffer_ = std::move(buffer);
  GetDrawingBuffer()->Bind(GL_FRAMEBUFFER);
  lost_context_errors_.clear();
  context_lost_mode_ = kNotLostContext;
  auto_recovery_method_ = kManual;
  restore_allowed_ = false;
  RemoveFromEvictedList(this);

  SetupFlags();
  InitializeNewContext();
  MarkContextChanged(kCanvasContextChanged);
  WebGLContextEvent* event = WebGLContextEvent::Create(
      EventTypeNames::webglcontextrestored, false, true, "");
  host()->HostDispatchEvent(event);
}

String WebGLRenderingContextBase::EnsureNotNull(const String& text) const {
  if (text.IsNull())
    return WTF::g_empty_string;
  return text;
}

WebGLRenderingContextBase::LRUImageBufferCache::LRUImageBufferCache(
    int capacity)
    : buffers_(WrapArrayUnique(new std::unique_ptr<ImageBuffer>[capacity])),
      capacity_(capacity) {}

ImageBuffer* WebGLRenderingContextBase::LRUImageBufferCache::GetImageBuffer(
    const IntSize& size) {
  int i;
  for (i = 0; i < capacity_; ++i) {
    ImageBuffer* buf = buffers_[i].get();
    if (!buf)
      break;
    if (buf->size() != size)
      continue;
    BubbleToFront(i);
    return buf;
  }

  std::unique_ptr<ImageBuffer> temp(ImageBuffer::Create(size));
  if (!temp)
    return nullptr;
  i = std::min(capacity_ - 1, i);
  buffers_[i] = std::move(temp);

  ImageBuffer* buf = buffers_[i].get();
  BubbleToFront(i);
  return buf;
}

void WebGLRenderingContextBase::LRUImageBufferCache::BubbleToFront(int idx) {
  for (int i = idx; i > 0; --i)
    buffers_[i].swap(buffers_[i - 1]);
}

namespace {

String GetErrorString(GLenum error) {
  switch (error) {
    case GL_INVALID_ENUM:
      return "INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "INVALID_FRAMEBUFFER_OPERATION";
    case GC3D_CONTEXT_LOST_WEBGL:
      return "CONTEXT_LOST_WEBGL";
    default:
      return String::Format("WebGL ERROR(0x%04X)", error);
  }
}

}  // namespace

void WebGLRenderingContextBase::SynthesizeGLError(
    GLenum error,
    const char* function_name,
    const char* description,
    ConsoleDisplayPreference display) {
  String error_type = GetErrorString(error);
  if (synthesized_errors_to_console_ && display == kDisplayInConsole) {
    String message = String("WebGL: ") + error_type + ": " +
                     String(function_name) + ": " + String(description);
    PrintGLErrorToConsole(message);
  }
  if (!isContextLost()) {
    if (!synthetic_errors_.Contains(error))
      synthetic_errors_.push_back(error);
  } else {
    if (!lost_context_errors_.Contains(error))
      lost_context_errors_.push_back(error);
  }
  probe::didFireWebGLError(canvas(), error_type);
}

void WebGLRenderingContextBase::EmitGLWarning(const char* function_name,
                                              const char* description) {
  if (synthesized_errors_to_console_) {
    String message =
        String("WebGL: ") + String(function_name) + ": " + String(description);
    PrintGLErrorToConsole(message);
  }
  probe::didFireWebGLWarning(canvas());
}

void WebGLRenderingContextBase::ApplyStencilTest() {
  bool have_stencil_buffer = false;

  if (framebuffer_binding_) {
    have_stencil_buffer = framebuffer_binding_->HasStencilBuffer();
  } else {
    Nullable<WebGLContextAttributes> attributes;
    getContextAttributes(attributes);
    have_stencil_buffer = !attributes.IsNull() && attributes.Get().stencil();
  }
  EnableOrDisable(GL_STENCIL_TEST, stencil_enabled_ && have_stencil_buffer);
}

void WebGLRenderingContextBase::EnableOrDisable(GLenum capability,
                                                bool enable) {
  if (isContextLost())
    return;
  if (enable)
    ContextGL()->Enable(capability);
  else
    ContextGL()->Disable(capability);
}

IntSize WebGLRenderingContextBase::ClampedCanvasSize() const {
  int width = host()->Size().Width();
  int height = host()->Size().Height();
  return IntSize(Clamp(width, 1, max_viewport_dims_[0]),
                 Clamp(height, 1, max_viewport_dims_[1]));
}

GLint WebGLRenderingContextBase::MaxDrawBuffers() {
  if (isContextLost() ||
      !(ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2OrHigher()))
    return 0;
  if (!max_draw_buffers_)
    ContextGL()->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max_draw_buffers_);
  if (!max_color_attachments_)
    ContextGL()->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT,
                             &max_color_attachments_);
  // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
  return std::min(max_draw_buffers_, max_color_attachments_);
}

GLint WebGLRenderingContextBase::MaxColorAttachments() {
  if (isContextLost() ||
      !(ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2OrHigher()))
    return 0;
  if (!max_color_attachments_)
    ContextGL()->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT,
                             &max_color_attachments_);
  return max_color_attachments_;
}

void WebGLRenderingContextBase::SetBackDrawBuffer(GLenum buf) {
  back_draw_buffer_ = buf;
}

void WebGLRenderingContextBase::SetFramebuffer(GLenum target,
                                               WebGLFramebuffer* buffer) {
  if (buffer)
    buffer->SetHasEverBeenBound();

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
    framebuffer_binding_ = buffer;
    ApplyStencilTest();
  }
  if (!buffer) {
    // Instead of binding fb 0, bind the drawing buffer.
    GetDrawingBuffer()->Bind(target);
  } else {
    ContextGL()->BindFramebuffer(target, buffer->Object());
  }
}

void WebGLRenderingContextBase::RestoreCurrentFramebuffer() {
  bindFramebuffer(GL_FRAMEBUFFER, framebuffer_binding_.Get());
}

void WebGLRenderingContextBase::RestoreCurrentTexture2D() {
  bindTexture(GL_TEXTURE_2D,
              texture_units_[active_texture_unit_].texture2d_binding_.Get());
}

void WebGLRenderingContextBase::FindNewMaxNonDefaultTextureUnit() {
  // Trace backwards from the current max to find the new max non-default
  // texture unit
  int start_index = one_plus_max_non_default_texture_unit_ - 1;
  for (int i = start_index; i >= 0; --i) {
    if (texture_units_[i].texture2d_binding_ ||
        texture_units_[i].texture_cube_map_binding_) {
      one_plus_max_non_default_texture_unit_ = i + 1;
      return;
    }
  }
  one_plus_max_non_default_texture_unit_ = 0;
}

DEFINE_TRACE(WebGLRenderingContextBase::TextureUnitState) {
  visitor->Trace(texture2d_binding_);
  visitor->Trace(texture_cube_map_binding_);
  visitor->Trace(texture3d_binding_);
  visitor->Trace(texture2d_array_binding_);
}

DEFINE_TRACE(WebGLRenderingContextBase) {
  visitor->Trace(context_group_);
  visitor->Trace(bound_array_buffer_);
  visitor->Trace(default_vertex_array_object_);
  visitor->Trace(bound_vertex_array_object_);
  visitor->Trace(current_program_);
  visitor->Trace(framebuffer_binding_);
  visitor->Trace(renderbuffer_binding_);
  visitor->Trace(texture_units_);
  visitor->Trace(extensions_);
  CanvasRenderingContext::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(WebGLRenderingContextBase) {
  visitor->TraceWrappers(context_group_);
  visitor->TraceWrappers(bound_array_buffer_);
  visitor->TraceWrappers(renderbuffer_binding_);
  visitor->TraceWrappers(framebuffer_binding_);
  visitor->TraceWrappers(current_program_);
  visitor->TraceWrappers(bound_vertex_array_object_);
  // Trace wrappers explicitly here since TextureUnitState is not a heap
  // object, i.e., we cannot set its mark bits.
  for (auto& unit : texture_units_) {
    visitor->TraceWrappers(unit.texture2d_binding_);
    visitor->TraceWrappers(unit.texture_cube_map_binding_);
    visitor->TraceWrappers(unit.texture3d_binding_);
    visitor->TraceWrappers(unit.texture2d_array_binding_);
  }
  for (auto tracker : extensions_) {
    visitor->TraceWrappers(tracker);
  }
  CanvasRenderingContext::TraceWrappers(visitor);
}

int WebGLRenderingContextBase::ExternallyAllocatedBytesPerPixel() {
  if (isContextLost())
    return 0;

  int bytes_per_pixel = 4;
  int total_bytes_per_pixel =
      bytes_per_pixel * 2;  // WebGL's front and back color buffers.
  int samples = GetDrawingBuffer() ? GetDrawingBuffer()->SampleCount() : 0;
  Nullable<WebGLContextAttributes> attribs;
  getContextAttributes(attribs);
  if (!attribs.IsNull()) {
    // Handle memory from WebGL multisample and depth/stencil buffers.
    // It is enabled only in case of explicit resolve assuming that there
    // is no memory overhead for MSAA on tile-based GPU arch.
    if (attribs.Get().antialias() && samples > 0 &&
        GetDrawingBuffer()->ExplicitResolveOfMultisampleData()) {
      if (attribs.Get().depth() || attribs.Get().stencil())
        total_bytes_per_pixel +=
            samples * bytes_per_pixel;  // depth/stencil multisample buffer
      total_bytes_per_pixel +=
          samples * bytes_per_pixel;  // color multisample buffer
    } else if (attribs.Get().depth() || attribs.Get().stencil()) {
      total_bytes_per_pixel += bytes_per_pixel;  // regular depth/stencil buffer
    }
  }

  return total_bytes_per_pixel;
}

DrawingBuffer* WebGLRenderingContextBase::GetDrawingBuffer() const {
  return drawing_buffer_.Get();
}

void WebGLRenderingContextBase::ResetUnpackParameters() {
  if (unpack_alignment_ != 1)
    ContextGL()->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void WebGLRenderingContextBase::RestoreUnpackParameters() {
  if (unpack_alignment_ != 1)
    ContextGL()->PixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment_);
}

void WebGLRenderingContextBase::getHTMLOrOffscreenCanvas(
    HTMLCanvasElementOrOffscreenCanvas& result) const {
  if (canvas()) {
    result.setHTMLCanvasElement(static_cast<HTMLCanvasElement*>(host()));
  } else {
    result.setOffscreenCanvas(static_cast<OffscreenCanvas*>(host()));
  }
}

}  // namespace blink
