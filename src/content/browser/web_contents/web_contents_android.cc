// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_android.h"

#include <stdint.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/android/content_jni_headers/WebContentsImpl_jni.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;

namespace content {

namespace {

// Track all WebContentsAndroid objects here so that we don't deserialize a
// destroyed WebContents object.
base::LazyInstance<std::unordered_set<WebContentsAndroid*>>::Leaky
    g_allocated_web_contents_androids = LAZY_INSTANCE_INITIALIZER;

void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              base::Value result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_WebContentsImpl_onEvaluateJavaScriptResult(env, j_json, callback);
}

void SmartClipCallback(const ScopedJavaGlobalRef<jobject>& callback,
                       const base::string16& text,
                       const base::string16& html,
                       const gfx::Rect& clip_rect) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_text = ConvertUTF16ToJavaString(env, text);
  ScopedJavaLocalRef<jstring> j_html = ConvertUTF16ToJavaString(env, html);
  Java_WebContentsImpl_onSmartClipDataExtracted(
      env, j_text, j_html, clip_rect.x(), clip_rect.y(), clip_rect.right(),
      clip_rect.bottom(), callback);
}

ScopedJavaLocalRef<jobject> JNI_WebContentsImpl_CreateJavaAXSnapshot(
    JNIEnv* env,
    const ui::AssistantTree* tree,
    const ui::AssistantNode* node,
    bool is_root) {
  ScopedJavaLocalRef<jstring> j_text =
      ConvertUTF16ToJavaString(env, node->text);
  ScopedJavaLocalRef<jstring> j_class =
      ConvertUTF8ToJavaString(env, node->class_name);
  ScopedJavaLocalRef<jobject> j_node =
      Java_WebContentsImpl_createAccessibilitySnapshotNode(
          env, node->rect.x(), node->rect.y(), node->rect.width(),
          node->rect.height(), is_root, j_text, node->color, node->bgcolor,
          node->text_size, node->bold, node->italic, node->underline,
          node->line_through, j_class);

  if (node->selection.has_value()) {
    Java_WebContentsImpl_setAccessibilitySnapshotSelection(
        env, j_node, node->selection->start(), node->selection->end());
  }

  for (int child : node->children_indices) {
    Java_WebContentsImpl_addAccessibilityNodeAsChild(
        env, j_node,
        JNI_WebContentsImpl_CreateJavaAXSnapshot(
            env, tree, tree->nodes[child].get(), false));
  }
  return j_node;
}

// Walks over the AXTreeUpdate and creates a light weight snapshot.
void AXTreeSnapshotCallback(const ScopedJavaGlobalRef<jobject>& callback,
                            const ui::AXTreeUpdate& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (result.nodes.empty()) {
    Java_WebContentsImpl_onAccessibilitySnapshot(env, nullptr, callback);
    return;
  }
  std::unique_ptr<BrowserAccessibilityManagerAndroid> manager(
      static_cast<BrowserAccessibilityManagerAndroid*>(
          BrowserAccessibilityManager::Create(result, nullptr)));
  std::unique_ptr<ui::AssistantTree> assistant_tree =
      ui::CreateAssistantTree(result, manager->ShouldExposePasswordText());
  ScopedJavaLocalRef<jobject> j_root = JNI_WebContentsImpl_CreateJavaAXSnapshot(
      env, assistant_tree.get(), assistant_tree->nodes.front().get(), true);
  Java_WebContentsImpl_onAccessibilitySnapshot(env, j_root, callback);
}

}  // namespace

// static
WebContents* WebContents::FromJavaWebContents(
    const JavaRef<jobject>& jweb_contents_android) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (jweb_contents_android.is_null())
    return NULL;

  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(
          Java_WebContentsImpl_getNativePointer(AttachCurrentThread(),
                                                jweb_contents_android));
  if (!web_contents_android)
    return NULL;
  return web_contents_android->web_contents();
}

// static
static void JNI_WebContentsImpl_DestroyWebContents(
    JNIEnv* env,
    jlong jweb_contents_android_ptr) {
  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(jweb_contents_android_ptr);
  if (!web_contents_android)
    return;

  WebContents* web_contents = web_contents_android->web_contents();
  if (!web_contents)
    return;

  delete web_contents;
}

// static
ScopedJavaLocalRef<jobject> JNI_WebContentsImpl_FromNativePtr(
    JNIEnv* env,
    jlong web_contents_ptr) {
  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(web_contents_ptr);

  if (!web_contents_android)
    return ScopedJavaLocalRef<jobject>();

  // Check to make sure this object hasn't been destroyed.
  if (g_allocated_web_contents_androids.Get().find(web_contents_android) ==
      g_allocated_web_contents_androids.Get().end()) {
    return ScopedJavaLocalRef<jobject>();
  }

  return web_contents_android->GetJavaObject();
}

WebContentsAndroid::WebContentsAndroid(WebContentsImpl* web_contents)
    : web_contents_(web_contents),
      navigation_controller_(&(web_contents->GetController())) {
  g_allocated_web_contents_androids.Get().insert(this);
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_WebContentsImpl_create(env, reinterpret_cast<intptr_t>(this),
                                         navigation_controller_.GetJavaObject())
                 .obj());
}

WebContentsAndroid::~WebContentsAndroid() {
  DCHECK(g_allocated_web_contents_androids.Get().find(this) !=
      g_allocated_web_contents_androids.Get().end());
  g_allocated_web_contents_androids.Get().erase(this);
  for (auto& observer : destruction_observers_)
    observer.WebContentsAndroidDestroyed(this);
  Java_WebContentsImpl_clearNativePtr(AttachCurrentThread(), obj_);
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

void WebContentsAndroid::ClearNativeReference(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->ClearWebContentsAndroid();
}

void WebContentsAndroid::AddDestructionObserver(DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void WebContentsAndroid::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetTopLevelNativeWindow(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  ui::WindowAndroid* window_android = web_contents_->GetTopLevelNativeWindow();
  if (!window_android)
    return nullptr;
  return window_android->GetJavaObject();
}

void WebContentsAndroid::SetTopLevelNativeWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jwindow_android) {
  ui::WindowAndroid* window =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  auto* old_window = web_contents_->GetTopLevelNativeWindow();
  if (window == old_window)
    return;

  auto* view = web_contents_->GetNativeView();
  if (old_window)
    view->RemoveFromParent();
  if (window)
    window->AddChild(view);
}

void WebContentsAndroid::SetViewAndroidDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jview_delegate) {
  ui::ViewAndroid* view_android = web_contents_->GetView()->GetNativeView();
  view_android->SetDelegate(jview_delegate);
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetMainFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return web_contents_->GetMainFrame()->GetJavaRenderFrameHost();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetFocusedFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  RenderFrameHostImpl* rfh = web_contents_->GetFocusedFrame();
  if (!rfh)
    return nullptr;
  return rfh->GetJavaRenderFrameHost();
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetTitle(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF16ToJavaString(env,
                                                 web_contents_->GetTitle());
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetVisibleURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return url::GURLAndroid::FromNativeGURL(env, web_contents_->GetVisibleURL());
}

bool WebContentsAndroid::IsLoading(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) const {
  return web_contents_->IsLoading();
}

bool WebContentsAndroid::IsLoadingToDifferentDocument(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return web_contents_->IsLoadingToDifferentDocument();
}

void WebContentsAndroid::DispatchBeforeUnload(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              bool auto_cancel) {
  web_contents_->DispatchBeforeUnload(auto_cancel);
}

void WebContentsAndroid::Stop(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Stop();
}

void WebContentsAndroid::Cut(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Cut();
}

void WebContentsAndroid::Copy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Copy();
}

void WebContentsAndroid::Paste(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Paste();
}

void WebContentsAndroid::PasteAsPlainText(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  // Paste as if user typed the characters, which should match current style of
  // the caret location.
  web_contents_->PasteAndMatchStyle();
}

void WebContentsAndroid::Replace(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jstring>& jstr) {
  web_contents_->Replace(base::android::ConvertJavaStringToUTF16(env, jstr));
}

void WebContentsAndroid::SelectAll(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  web_contents_->SelectAll();
}

void WebContentsAndroid::CollapseSelection(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  web_contents_->CollapseSelection();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetRenderWidgetHostView(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return nullptr;
  return rwhva->GetJavaObject();
}

ScopedJavaLocalRef<jobjectArray> WebContentsAndroid::GetInnerWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::vector<WebContents*> inner_web_contents =
      web_contents_->GetInnerWebContents();
  jclass clazz =
      org_chromium_content_browser_webcontents_WebContentsImpl_clazz(env);
  jobjectArray array =
      env->NewObjectArray(inner_web_contents.size(), clazz, nullptr);
  for (size_t i = 0; i < inner_web_contents.size(); i++) {
    ScopedJavaLocalRef<jobject> contents_java =
        inner_web_contents[i]->GetJavaWebContents();
    env->SetObjectArrayElement(array, i, contents_java.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, array);
}

jint WebContentsAndroid::GetVisibility(JNIEnv* env) {
  return static_cast<jint>(web_contents_->GetVisibility());
}

RenderWidgetHostViewAndroid*
    WebContentsAndroid::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  rwhv = web_contents_->GetRenderWidgetHostView();
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

jint WebContentsAndroid::GetBackgroundColor(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();

  // Return transparent as an indicator that the web content background color
  // is not specified, and a default background color will be used on the Java
  // side.
  if (!rwhva || !rwhva->GetCachedBackgroundColor())
    return SK_ColorTRANSPARENT;
  return *rwhva->GetCachedBackgroundColor();
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetLastCommittedURL(
    JNIEnv* env,
    const JavaParamRef<jobject>&) const {
  return ConvertUTF8ToJavaString(env,
                                 web_contents_->GetLastCommittedURL().spec());
}

jboolean WebContentsAndroid::IsIncognito(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsAndroid::ResumeLoadingCreatedWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  web_contents_->ResumeLoadingCreatedWebContents();
}

void WebContentsAndroid::OnHide(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->WasHidden();
}

void WebContentsAndroid::OnShow(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->WasShown();
}

void WebContentsAndroid::SetImportance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint main_frame_importance) {
  web_contents_->SetMainFrameImportance(
      static_cast<ChildProcessImportance>(main_frame_importance));
}

void WebContentsAndroid::SuspendAllMediaPlayers(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  web_contents_->media_web_contents_observer()->SuspendAllMediaPlayers();
}

void WebContentsAndroid::SetAudioMuted(JNIEnv* env,
                                       const JavaParamRef<jobject>& jobj,
                                       jboolean mute) {
  web_contents_->SetAudioMuted(mute);
}

jboolean WebContentsAndroid::IsShowingInterstitialPage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return web_contents_->ShowingInterstitialPage();
}

jboolean WebContentsAndroid::FocusLocationBarByDefault(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return web_contents_->FocusLocationBarByDefault();
}

bool WebContentsAndroid::IsFullscreenForCurrentTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return web_contents_->IsFullscreenForCurrentTab();
}

void WebContentsAndroid::ExitFullscreen(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  web_contents_->ExitFullscreen(/*will_cause_resize=*/false);
}

void WebContentsAndroid::ScrollFocusedEditableNodeIntoView(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  auto* input_handler = web_contents_->GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->ScrollFocusedEditableNodeIntoRect(gfx::Rect());
}

void WebContentsAndroid::SelectWordAroundCaretAck(bool did_select,
                                                  int start_adjust,
                                                  int end_adjust) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva)
    rwhva->SelectWordAroundCaretAck(did_select, start_adjust, end_adjust);
}

void WebContentsAndroid::SelectWordAroundCaret(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  auto* input_handler = web_contents_->GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->SelectWordAroundCaret(
      base::BindOnce(&WebContentsAndroid::SelectWordAroundCaretAck,
                     weak_factory_.GetWeakPtr()));
}

void WebContentsAndroid::AdjustSelectionByCharacterOffset(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint start_adjust,
    jint end_adjust,
    jboolean show_selection_menu) {
  web_contents_->AdjustSelectionByCharacterOffset(start_adjust, end_adjust,
                                                  show_selection_menu);
}

bool WebContentsAndroid::InitializeRenderFrameForJavaScript() {
  if (!web_contents_->GetFrameTree()
           ->root()
           ->render_manager()
           ->InitializeRenderFrameForImmediateUse()) {
    LOG(ERROR) << "Failed to initialize RenderFrame to evaluate javascript";
    return false;
  }
  return true;
}

void WebContentsAndroid::EvaluateJavaScript(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!InitializeRenderFrameForJavaScript())
    return;

  if (!callback) {
    // No callback requested.
    web_contents_->GetMainFrame()->ExecuteJavaScript(
        ConvertJavaStringToUTF16(env, script), base::NullCallback());
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents_->GetMainFrame()->ExecuteJavaScript(
      ConvertJavaStringToUTF16(env, script),
      base::BindOnce(&JavaScriptResultCallback, j_callback));
}

void WebContentsAndroid::EvaluateJavaScriptForTests(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!InitializeRenderFrameForJavaScript())
    return;

  if (!callback) {
    // No callback requested.
    web_contents_->GetMainFrame()->ExecuteJavaScriptForTests(
        ConvertJavaStringToUTF16(env, script), base::NullCallback());
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents_->GetMainFrame()->ExecuteJavaScriptForTests(
      ConvertJavaStringToUTF16(env, script),
      base::BindOnce(&JavaScriptResultCallback, j_callback));
}

void WebContentsAndroid::AddMessageToDevToolsConsole(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint level,
    const JavaParamRef<jstring>& message) {
  DCHECK_GE(level, 0);
  DCHECK_LE(level, static_cast<int>(blink::mojom::ConsoleMessageLevel::kError));

  web_contents_->GetMainFrame()->AddMessageToConsole(
      static_cast<blink::mojom::ConsoleMessageLevel>(level),
      ConvertJavaStringToUTF8(env, message));
}

void WebContentsAndroid::PostMessageToMainFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jmessage,
    const JavaParamRef<jstring>& jsource_origin,
    const JavaParamRef<jstring>& jtarget_origin,
    const JavaParamRef<jobjectArray>& jports) {
  content::MessagePortProvider::PostMessageToFrame(
      web_contents_, env, jsource_origin, jtarget_origin, jmessage, jports);
}

jboolean WebContentsAndroid::HasAccessedInitialDocument(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  return static_cast<WebContentsImpl*>(web_contents_)->
      HasAccessedInitialDocument();
}

jint WebContentsAndroid::GetThemeColor(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return web_contents_->GetThemeColor().value_or(SK_ColorTRANSPARENT);
}

jfloat WebContentsAndroid::GetLoadProgress(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  return web_contents_->GetLoadProgress();
}

void WebContentsAndroid::RequestSmartClipExtract(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback,
    jint x,
    jint y,
    jint width,
    jint height) {
  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents_->GetMainFrame()->RequestSmartClipExtract(
      base::BindOnce(&SmartClipCallback, j_callback),
      gfx::Rect(x, y, width, height));
}

void WebContentsAndroid::RequestAccessibilitySnapshot(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  static_cast<WebContentsImpl*>(web_contents_)
      ->RequestAXTreeSnapshot(
          base::BindOnce(&AXTreeSnapshotCallback, j_callback),
          ui::kAXModeComplete);
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetEncoding(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF8ToJavaString(env,
                                                web_contents_->GetEncoding());
}

void WebContentsAndroid::SetOverscrollRefreshHandler(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& overscroll_refresh_handler) {
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->SetOverscrollRefreshHandler(
      std::make_unique<ui::OverscrollRefreshHandler>(
          overscroll_refresh_handler));
}

void WebContentsAndroid::SetSpatialNavigationDisabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool disabled) {
  web_contents_->SetSpatialNavigationDisabled(disabled);
}

int WebContentsAndroid::DownloadImage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jurl,
    jboolean is_fav_icon,
    jint max_bitmap_size,
    jboolean bypass_cache,
    const base::android::JavaParamRef<jobject>& jcallback) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  const uint32_t preferred_size = 0;
  return web_contents_->DownloadImage(
      url, is_fav_icon, preferred_size, max_bitmap_size, bypass_cache,
      base::BindOnce(&WebContentsAndroid::OnFinishDownloadImage,
                     weak_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(env, obj),
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

void WebContentsAndroid::SetHasPersistentVideo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean value) {
  web_contents_->SetHasPersistentVideo(value);
}

bool WebContentsAndroid::HasActiveEffectivelyFullscreenVideo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->HasActiveEffectivelyFullscreenVideo();
}

bool WebContentsAndroid::IsPictureInPictureAllowedForFullscreenVideo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->IsPictureInPictureAllowedForFullscreenVideo();
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetFullscreenVideoSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!web_contents_->GetFullscreenVideoSize())
    return ScopedJavaLocalRef<jobject>();  // Return null.

  gfx::Size size = web_contents_->GetFullscreenVideoSize().value();
  return Java_WebContentsImpl_createSize(env, size.width(), size.height());
}

void WebContentsAndroid::SetSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint width,
    jint height) {
  web_contents_->GetNativeView()->OnSizeChanged(width, height);
}

int WebContentsAndroid::GetWidth(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->GetNativeView()->GetSize().width();
}

int WebContentsAndroid::GetHeight(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->GetNativeView()->GetSize().height();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetOrCreateEventForwarder(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  gfx::NativeView native_view = web_contents_->GetView()->GetNativeView();
  return native_view->GetEventForwarder();
}

void WebContentsAndroid::OnFinishDownloadImage(
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& callback,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbitmaps =
      Java_WebContentsImpl_createBitmapList(env);
  ScopedJavaLocalRef<jobject> jsizes =
      Java_WebContentsImpl_createSizeList(env);
  ScopedJavaLocalRef<jstring> jurl =
      base::android::ConvertUTF8ToJavaString(env, url.spec());

  for (const SkBitmap& bitmap : bitmaps) {
    // WARNING: convering to java bitmaps results in duplicate memory
    // allocations, which increases the chance of OOMs if DownloadImage() is
    // misused.
    ScopedJavaLocalRef<jobject> jbitmap = gfx::ConvertToJavaBitmap(&bitmap);
    Java_WebContentsImpl_addToBitmapList(env, jbitmaps, jbitmap);
  }
  for (const gfx::Size& size : sizes) {
    Java_WebContentsImpl_createSizeAndAddToList(env, jsizes, size.width(),
                                                size.height());
  }
  Java_WebContentsImpl_onDownloadImageFinished(
      env, obj, callback, id, http_status_code, jurl, jbitmaps, jsizes);
}

void WebContentsAndroid::SetMediaSession(
    const ScopedJavaLocalRef<jobject>& j_media_session) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebContentsImpl_setMediaSession(env, obj_, j_media_session);
}

void WebContentsAndroid::SendOrientationChangeEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint orientation) {
  base::RecordAction(base::UserMetricsAction("ScreenOrientationChange"));
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->set_device_orientation(orientation);
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva)
    rwhva->UpdateScreenInfo(web_contents_->GetView()->GetNativeView());

  web_contents_->OnScreenOrientationChange();
}

void WebContentsAndroid::OnScaleFactorChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva) {
    // |SendScreenRects()| indirectly calls GetViewSize() that asks Java layer.
    web_contents_->SendScreenRects();
    rwhva->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                       base::nullopt);
  }
}

void WebContentsAndroid::SetFocus(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jboolean focused) {
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->SetFocus(focused);
}

bool WebContentsAndroid::IsBeingDestroyed(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  return web_contents_->IsBeingDestroyed();
}

void WebContentsAndroid::SetDisplayCutoutSafeArea(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int top,
    int left,
    int bottom,
    int right) {
  web_contents()->SetDisplayCutoutSafeArea(
      gfx::Insets(top, left, bottom, right));
}

void WebContentsAndroid::NotifyRendererPreferenceUpdate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);
  rvh->OnWebkitPreferencesChanged();
}

void WebContentsAndroid::NotifyBrowserControlsHeightChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  web_contents_->GetNativeView()->OnBrowserControlsHeightChanged();
}

}  // namespace content
