// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/window_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/auto_reset.h"
#include "base/observer_list.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/begin_frame_source.h"
#include "jni/WindowAndroid_jni.h"
#include "ui/android/window_android_compositor.h"
#include "ui/android/window_android_observer.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

class WindowAndroid::WindowBeginFrameSource : public cc::BeginFrameSource {
 public:
  explicit WindowBeginFrameSource(WindowAndroid* window)
      : window_(window),
        observer_count_(0),
        in_on_vsync_(false) {}
  ~WindowBeginFrameSource() override {}

  // cc::BeginFrameSource implementation.
  void AddObserver(cc::BeginFrameObserver* obs) override;
  void RemoveObserver(cc::BeginFrameObserver* obs) override;
  void DidFinishFrame(cc::BeginFrameObserver* obs,
                      size_t remaining_frames) override {}
  bool IsThrottled() const override { return true; }

  void OnVSync(base::TimeTicks frame_time,
               base::TimeDelta vsync_period);

 private:
  WindowAndroid* const window_;
  base::ObserverList<cc::BeginFrameObserver> observers_;
  int observer_count_;
  cc::BeginFrameArgs last_begin_frame_args_;
  bool in_on_vsync_;
};

void WindowAndroid::WindowBeginFrameSource::AddObserver(
    cc::BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!observers_.HasObserver(obs));

  observers_.AddObserver(obs);
  observer_count_++;
  obs->OnBeginFrameSourcePausedChanged(false);
  window_->SetNeedsBeginFrames(true);

  // Send a MISSED BeginFrame if possible and necessary. If an observer is added
  // during OnVSync(), it will get a NORMAL BeginFrame from OnVSync() instead.
  if (!in_on_vsync_ && last_begin_frame_args_.IsValid()) {
    cc::BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        last_args.frame_time < last_begin_frame_args_.frame_time) {
      last_begin_frame_args_.type = cc::BeginFrameArgs::MISSED;
      // TODO(crbug.com/602485): A deadline doesn't make too much sense
      // for a missed BeginFrame (the intention rather is 'immediately'),
      // but currently the retro frame logic is very strict in discarding
      // BeginFrames.
      last_begin_frame_args_.deadline =
          base::TimeTicks::Now() + last_begin_frame_args_.interval;
      obs->OnBeginFrame(last_begin_frame_args_);
    }
  }
}

void WindowAndroid::WindowBeginFrameSource::RemoveObserver(
    cc::BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.HasObserver(obs));

  observers_.RemoveObserver(obs);
  observer_count_--;
  if (observer_count_ <= 0)
    window_->SetNeedsBeginFrames(false);
}

void WindowAndroid::WindowBeginFrameSource::OnVSync(
    base::TimeTicks frame_time,
    base::TimeDelta vsync_period) {
  // frame time is in the past, so give the next vsync period as the deadline.
  base::TimeTicks deadline = frame_time + vsync_period;
  last_begin_frame_args_ =
      cc::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, frame_time, deadline,
                                 vsync_period, cc::BeginFrameArgs::NORMAL);
  DCHECK(last_begin_frame_args_.IsValid());

  // We support adding/removing observers during observer iteration through
  // base::ObserverList. We also prevent observers that are added during
  // observer iteration from receiving a MISSED BeginFrame by means of
  // |in_on_vsync_| - they will receive the current NORMAL one during the
  // iteration instead. Note that SynchronousCompositorBrowserFilter relies on
  // this behavior.
  base::AutoReset<bool> auto_reset(&in_on_vsync_, true);
  for (auto& obs : observers_)
    obs.OnBeginFrame(last_begin_frame_args_);
}

WindowAndroid::WindowAndroid(JNIEnv* env, jobject obj, int display_id)
    : display_id_(display_id),
      compositor_(NULL),
      begin_frame_source_(new WindowBeginFrameSource(this)),
      needs_begin_frames_(false) {
  java_window_.Reset(env, obj);
}

void WindowAndroid::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> WindowAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_window_);
}

bool WindowAndroid::RegisterWindowAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WindowAndroid::~WindowAndroid() {
  DCHECK(parent_ == nullptr) << "WindowAndroid must be a root view.";
  DCHECK(!compositor_);
  Java_WindowAndroid_clearNativePointer(AttachCurrentThread(), GetJavaObject());
}

WindowAndroid* WindowAndroid::CreateForTesting() {
  JNIEnv* env = AttachCurrentThread();
  const JavaRef<jobject>& context = base::android::GetApplicationContext();
  long native_pointer = Java_WindowAndroid_createForTesting(env, context);
  return reinterpret_cast<WindowAndroid*>(native_pointer);
}

void WindowAndroid::DestroyForTesting() {
  delete this;
}

void WindowAndroid::OnCompositingDidCommit() {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnCompositingDidCommit();
}

void WindowAndroid::AddObserver(WindowAndroidObserver* observer) {
  if (!observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WindowAndroid::RemoveObserver(WindowAndroidObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

cc::BeginFrameSource* WindowAndroid::GetBeginFrameSource() {
  return begin_frame_source_.get();
}

void WindowAndroid::AttachCompositor(WindowAndroidCompositor* compositor) {
  if (compositor_ && compositor != compositor_)
    DetachCompositor();

  compositor_ = compositor;
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnAttachCompositor();
}

void WindowAndroid::DetachCompositor() {
  compositor_ = NULL;
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnDetachCompositor();
  observer_list_.Clear();
}

void WindowAndroid::RequestVSyncUpdate() {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_requestVSyncUpdate(env, GetJavaObject());
}

void WindowAndroid::SetNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames)
    return;

  needs_begin_frames_ = needs_begin_frames;
  if (needs_begin_frames_)
    RequestVSyncUpdate();
}

void WindowAndroid::SetNeedsAnimate() {
  if (compositor_)
    compositor_->SetNeedsAnimate();
}

void WindowAndroid::Animate(base::TimeTicks begin_frame_time) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnAnimate(begin_frame_time);
}

void WindowAndroid::OnVSync(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            jlong time_micros,
                            jlong period_micros) {
  base::TimeTicks frame_time(base::TimeTicks::FromInternalValue(time_micros));
  base::TimeDelta vsync_period(
      base::TimeDelta::FromMicroseconds(period_micros));
  begin_frame_source_->OnVSync(frame_time, vsync_period);
  if (needs_begin_frames_)
    RequestVSyncUpdate();
}

void WindowAndroid::OnVisibilityChanged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        bool visible) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnRootWindowVisibilityChanged(visible);
}

void WindowAndroid::OnActivityStopped(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnActivityStopped();
}

void WindowAndroid::OnActivityStarted(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnActivityStarted();
}

bool WindowAndroid::HasPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_hasPermission(
      env, GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, permission));
}

bool WindowAndroid::CanRequestPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_canRequestPermission(
      env, GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, permission));
}

WindowAndroid* WindowAndroid::GetWindowAndroid() const {
  DCHECK(parent_ == nullptr);
  return const_cast<WindowAndroid*>(this);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj, int sdk_display_id) {
  WindowAndroid* window = new WindowAndroid(env, obj, sdk_display_id);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui
