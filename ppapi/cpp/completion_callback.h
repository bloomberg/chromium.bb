// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_COMPLETION_CALLBACK_H_
#define PPAPI_CPP_COMPLETION_CALLBACK_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/non_thread_safe_ref_count.h"

namespace pp {

// A CompletionCallback provides a wrapper around PP_CompletionCallback.
class CompletionCallback {
 public:
  // The default constructor will create a 'blocking' CompletionCallback
  // that may be passed to a method to indicate that the calling thread should
  // be blocked until the asynchronous operation corresponding to the method
  // completes.
  CompletionCallback() {
    cc_ = PP_BlockUntilComplete();
  }

  CompletionCallback(PP_CompletionCallback_Func func, void* user_data) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
  }

  CompletionCallback(PP_CompletionCallback_Func func, void* user_data,
                     int32_t flags) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
    cc_.flags = flags;
  }

  void set_flags(int32_t flags) { cc_.flags = flags; }

  // Call this method to explicitly run the CompletionCallback.  Normally, the
  // system runs a CompletionCallback after an asynchronous operation
  // completes, but programs may wish to run the CompletionCallback manually
  // in order to reuse the same code paths.
  void Run(int32_t result) {
    PP_DCHECK(cc_.func);
    PP_RunCompletionCallback(&cc_, result);
  }

  bool IsOptional() const {
    return (cc_.func == NULL ||
            (cc_.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL) != 0);
  }

  const PP_CompletionCallback& pp_completion_callback() const { return cc_; }
  int32_t flags() const { return cc_.flags; }

  // Use this when implementing functions taking callbacks.
  // If the callback is required and |result| indicates that it has not
  // been scheduled, it will be forced on the main thread.
  //
  // EXAMPLE USAGE:
  //
  // int32_t OpenURL(pp::URLLoader* loader,
  //                 pp::URLRequestInfo* url_request_info,
  //                 const CompletionCallback& cc) {
  //   if (loader == NULL || url_request_info == NULL)
  //     return cc.MayForce(PP_ERROR_BADRESOURCE);
  //   return loader->Open(*loader, *url_request_info, cc);
  // }
  //
  int32_t MayForce(int32_t result) const {
    if (result == PP_OK_COMPLETIONPENDING || IsOptional())
      return result;
    Module::Get()->core()->CallOnMainThread(0, *this, result);
    return PP_OK_COMPLETIONPENDING;
  }

 protected:
  PP_CompletionCallback cc_;
};

// Create a 'blocking' CompletionCallback.
CompletionCallback BlockUntilComplete();

// CompletionCallbackFactory<T> may be used to create CompletionCallback
// objects that are bound to member functions.
//
// If a factory is destroyed, then any pending callbacks will be cancelled
// preventing any bound member functions from being called.  The CancelAll
// method allows pending callbacks to be cancelled without destroying the
// factory.
//
// NOTE: by default, CompletionCallbackFactory<T> isn't thread safe, but you can
// make it more thread-friendly by passing a thread-safe refcounting class as
// the second template element. However, it only guarantees safety for
// *creating* a callback from another thread, the callback itself needs to
// execute on the same thread as the thread that creates/destroys the factory.
// With this restriction, it is safe to create the CompletionCallbackFactory on
// the main thread, create callbacks from any thread and pass them to
// CallOnMainThread.
//
// EXAMPLE USAGE:
//
//   class MyHandler {
//    public:
//     MyHandler() : factory_(this), offset_(0) {
//     }
//
//     void ProcessFile(const FileRef& file) {
//       CompletionCallback cc = factory_.NewRequiredCallback(
//           &MyHandler::DidOpen);
//       int32_t rv = fio_.Open(file, PP_FileOpenFlag_Read, cc);
//       CHECK(rv == PP_OK_COMPLETIONPENDING);
//     }
//
//    private:
//     CompletionCallback NewCallback() {
//       return factory_.NewCallback(&MyHandler::DidCompleteIO);
//     }
//
//     void DidOpen(int32_t result) {
//       if (result == PP_OK) {
//         // The file is open, and we can begin reading.
//         offset_ = 0;
//         ReadMore();
//       } else {
//         // Failed to open the file with error given by 'result'.
//       }
//     }
//
//     void DidRead(int32_t result) {
//       if (result > 0) {
//         // buf_ now contains 'result' number of bytes from the file.
//         ProcessBytes(buf_, result);
//         offset_ += result;
//         ReadMore();
//       } else {
//         // Done reading (possibly with an error given by 'result').
//       }
//     }
//
//     void ReadMore() {
//       CompletionCallback cc =
//           factory_.NewOptionalCallback(&MyHandler::DidRead);
//       int32_t rv = fio_.Read(offset_, buf_, sizeof(buf_),
//                              cc.pp_completion_callback());
//       if (rv != PP_OK_COMPLETIONPENDING)
//         cc.Run(rv);
//     }
//
//     void ProcessBytes(const char* bytes, int32_t length) {
//       // Do work ...
//     }
//
//     pp::CompletionCallbackFactory<MyHandler> factory_;
//     pp::FileIO fio_;
//     char buf_[4096];
//     int64_t offset_;
//   };
//
template <typename T, typename RefCount = NonThreadSafeRefCount>
class CompletionCallbackFactory {
 public:
  explicit CompletionCallbackFactory(T* object = NULL)
      : object_(object) {
    InitBackPointer();
  }

  ~CompletionCallbackFactory() {
    ResetBackPointer();
  }

  // Cancels all CompletionCallbacks allocated from this factory.
  void CancelAll() {
    ResetBackPointer();
    InitBackPointer();
  }

  void Initialize(T* object) {
    PP_DCHECK(object);
    PP_DCHECK(!object_);  // May only initialize once!
    object_ = object;
  }

  T* GetObject() {
    return object_;
  }

  // Methods for allocating new, single-use CompletionCallbacks.
  // The CompletionCallback must be run in order for the memory
  // allocated by the methods to be freed.
  // NewRequiredCallback() creates callbacks that will always run.
  // NewOptionalCallback() creates callbacks that might not run if the method
  // taking them can complete synchronously. Thus, if after passing the
  // CompletionCallback to a PPAPI method, the method does not return
  // PP_OK_COMPLETIONPENDING, then you should manually call the
  // CompletionCallback's Run method, or memory will be leaked.
  // NewCallback() is equivalent to NewRequiredCallback().

  template <typename Method>
  CompletionCallback NewCallback(Method method) {
    PP_DCHECK(object_);
    return NewCallbackHelper(Dispatcher0<Method>(method));
  }

  template <typename Method>
  CompletionCallback NewRequiredCallback(Method method) {
    CompletionCallback cc = NewCallback(method);
    cc.set_flags(cc.flags() & ~PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  template <typename Method>
  CompletionCallback NewOptionalCallback(Method method) {
    CompletionCallback cc = NewCallback(method);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  // A copy of "a" will be passed to "method" when the completion callback
  // runs.
  //
  // Method should be of type:
  //   void (T::*)(int32_t result, const A& a)
  //
  template <typename Method, typename A>
  CompletionCallback NewCallback(Method method, const A& a) {
    PP_DCHECK(object_);
    return NewCallbackHelper(Dispatcher1<Method, A>(method, a));
  }

  template <typename Method, typename A>
  CompletionCallback NewRequiredCallback(Method method, const A& a) {
    CompletionCallback cc = NewCallback(method, a);
    cc.set_flags(cc.flags() & ~PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  template <typename Method, typename A>
  CompletionCallback NewOptionalCallback(Method method, const A& a) {
    CompletionCallback cc = NewCallback(method, a);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  // A copy of "a" and "b" will be passed to "method" when the completion
  // callback runs.
  //
  // Method should be of type:
  //   void (T::*)(int32_t result, const A& a, const B& b)
  //
  template <typename Method, typename A, typename B>
  CompletionCallback NewCallback(Method method, const A& a, const B& b) {
    PP_DCHECK(object_);
    return NewCallbackHelper(Dispatcher2<Method, A, B>(method, a, b));
  }

  template <typename Method, typename A, typename B>
  CompletionCallback NewRequiredCallback(Method method, const A& a,
                                         const B& b) {
    CompletionCallback cc = NewCallback(method, a, b);
    cc.set_flags(cc.flags() & ~PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  template <typename Method, typename A, typename B>
  CompletionCallback NewOptionalCallback(Method method, const A& a,
                                         const B& b) {
    CompletionCallback cc = NewCallback(method, a, b);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

 private:
  class BackPointer {
   public:
    typedef CompletionCallbackFactory<T, RefCount> FactoryType;

    BackPointer(FactoryType* factory)
        : factory_(factory) {
    }

    void AddRef() {
      ref_.AddRef();
    }

    void Release() {
      if (ref_.Release() == 0)
        delete this;
    }

    void DropFactory() {
      factory_ = NULL;
    }

    T* GetObject() {
      return factory_ ? factory_->GetObject() : NULL;
    }

   private:
    RefCount ref_;
    FactoryType* factory_;
  };

  template <typename Dispatcher>
  class CallbackData {
   public:
    CallbackData(BackPointer* back_pointer, const Dispatcher& dispatcher)
        : back_pointer_(back_pointer),
          dispatcher_(dispatcher) {
      back_pointer_->AddRef();
    }

    ~CallbackData() {
      back_pointer_->Release();
    }

    static void Thunk(void* user_data, int32_t result) {
      Self* self = static_cast<Self*>(user_data);
      T* object = self->back_pointer_->GetObject();
      if (object)
        self->dispatcher_(object, result);
      delete self;
    }

   private:
    typedef CallbackData<Dispatcher> Self;
    BackPointer* back_pointer_;
    Dispatcher dispatcher_;
  };

  template <typename Method>
  class Dispatcher0 {
   public:
    Dispatcher0(Method method) : method_(method) {
    }
    void operator()(T* object, int32_t result) {
      (object->*method_)(result);
    }
   private:
    Method method_;
  };

  template <typename Method, typename A>
  class Dispatcher1 {
   public:
    Dispatcher1(Method method, const A& a)
        : method_(method),
          a_(a) {
    }
    void operator()(T* object, int32_t result) {
      (object->*method_)(result, a_);
    }
   private:
    Method method_;
    A a_;
  };

  template <typename Method, typename A, typename B>
  class Dispatcher2 {
   public:
    Dispatcher2(Method method, const A& a, const B& b)
        : method_(method),
          a_(a),
          b_(b) {
    }
    void operator()(T* object, int32_t result) {
      (object->*method_)(result, a_, b_);
    }
   private:
    Method method_;
    A a_;
    B b_;
  };

  void InitBackPointer() {
    back_pointer_ = new BackPointer(this);
    back_pointer_->AddRef();
  }

  void ResetBackPointer() {
    back_pointer_->DropFactory();
    back_pointer_->Release();
  }

  template <typename Dispatcher>
  CompletionCallback NewCallbackHelper(const Dispatcher& dispatcher) {
    PP_DCHECK(object_);  // Expects a non-null object!
    return CompletionCallback(
        &CallbackData<Dispatcher>::Thunk,
        new CallbackData<Dispatcher>(back_pointer_, dispatcher));
  }

  // Disallowed:
  CompletionCallbackFactory(const CompletionCallbackFactory&);
  CompletionCallbackFactory& operator=(const CompletionCallbackFactory&);

  T* object_;
  BackPointer* back_pointer_;
};

}  // namespace pp

#endif  // PPAPI_CPP_COMPLETION_CALLBACK_H_
