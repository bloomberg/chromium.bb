// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_METHOD_INVOCATION_HELPER_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_METHOD_INVOCATION_HELPER_H_

#include <stddef.h>

#include <map>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/browser/android/java/java_type.h"
#include "content/common/android/gin_java_bridge_errors.h"
#include "content/common/content_export.h"

namespace content {

class JavaMethod;

// Instances of this class are created and used on the background thread.
class CONTENT_EXPORT GinJavaMethodInvocationHelper
    : public base::RefCountedThreadSafe<GinJavaMethodInvocationHelper> {
 public:
  class DispatcherDelegate {
   public:
    DispatcherDelegate() {}
    virtual ~DispatcherDelegate() {}
    virtual JavaObjectWeakGlobalRef GetObjectWeakRef(
        GinJavaBoundObject::ObjectID object_id) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(DispatcherDelegate);
  };

  class ObjectDelegate {
   public:
    ObjectDelegate() {}
    virtual ~ObjectDelegate() {}
    virtual base::android::ScopedJavaLocalRef<jobject> GetLocalRef(
        JNIEnv* env) = 0;
    virtual base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(
        JNIEnv* env) = 0;
    virtual const JavaMethod* FindMethod(const std::string& method_name,
                                         size_t num_parameters) = 0;
    virtual bool IsObjectGetClassMethod(const JavaMethod* method) = 0;
    virtual const base::android::JavaRef<jclass>& GetSafeAnnotationClass() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ObjectDelegate);
  };

  GinJavaMethodInvocationHelper(std::unique_ptr<ObjectDelegate> object,
                                const std::string& method_name,
                                const base::ListValue& arguments);
  void Init(DispatcherDelegate* dispatcher);

  void Invoke();

  bool HoldsPrimitiveResult();
  const base::ListValue& GetPrimitiveResult();
  const base::android::JavaRef<jobject>& GetObjectResult();
  const base::android::JavaRef<jclass>& GetSafeAnnotationClass();
  GinJavaBridgeError GetInvocationError();

 private:
  friend class base::RefCountedThreadSafe<GinJavaMethodInvocationHelper>;
  ~GinJavaMethodInvocationHelper();

  void BuildObjectRefsFromListValue(DispatcherDelegate* dispatcher,
                                    const base::Value& list_value);
  void BuildObjectRefsFromDictionaryValue(DispatcherDelegate* dispatcher,
                                          const base::Value& dict_value);

  bool AppendObjectRef(DispatcherDelegate* dispatcher,
                       const base::Value& raw_value);

  void InvokeMethod(jobject object,
                    jclass clazz,
                    const JavaType& return_type,
                    jmethodID id,
                    jvalue* parameters);
  void SetInvocationError(GinJavaBridgeError error);
  void SetPrimitiveResult(const base::ListValue& result_wrapper);
  void SetObjectResult(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);

  typedef std::map<GinJavaBoundObject::ObjectID,
                   JavaObjectWeakGlobalRef> ObjectRefs;

  std::unique_ptr<ObjectDelegate> object_;
  const std::string method_name_;
  std::unique_ptr<base::ListValue> arguments_;
  ObjectRefs object_refs_;
  bool holds_primitive_result_;
  std::unique_ptr<base::ListValue> primitive_result_;
  GinJavaBridgeError invocation_error_;
  base::android::ScopedJavaGlobalRef<jobject> object_result_;
  base::android::ScopedJavaGlobalRef<jclass> safe_annotation_clazz_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaMethodInvocationHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_METHOD_INVOCATION_HELPER_H_
