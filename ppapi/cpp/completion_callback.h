// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_COMPLETION_CALLBACK_H_
#define PPAPI_CPP_COMPLETION_CALLBACK_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/non_thread_safe_ref_count.h"

/// @file
/// This file defines the API to create and run a callback.
namespace pp {

/// This API enables you to implement and receive callbacks when
/// Pepper operations complete asynchronously.
class CompletionCallback {
 public:
  /// The default constructor will create a blocking
  /// <code>CompletionCallback</code> that can be passed to a method to
  /// indicate that the calling thread should be blocked until the asynchronous
  /// operation corresponding to the method completes.
  ///
  /// <strong>Note:</strong> Blocking completion callbacks are only allowed from
  /// from background threads.
  CompletionCallback() {
    cc_ = PP_BlockUntilComplete();
  }

  /// A constructor for creating a <code>CompletionCallback</code>.
  ///
  /// @param[in] func The function to be called on completion.
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  CompletionCallback(PP_CompletionCallback_Func func, void* user_data) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
  }

  /// A constructor for creating a <code>CompletionCallback</code> with
  /// specified flags.
  ///
  /// @param[in] func The function to be called on completion.
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  /// @param[in] flags Bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags used to control how
  /// non-NULL callbacks are scheduled by asynchronous methods.
  CompletionCallback(PP_CompletionCallback_Func func, void* user_data,
                     int32_t flags) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
    cc_.flags = flags;
  }

  /// The set_flags() function is used to set the flags used to control
  /// how non-NULL callbacks are scheduled by asynchronous methods.
  ///
  /// @param[in] flags Bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags used to control how
  /// non-NULL callbacks are scheduled by asynchronous methods.
  void set_flags(int32_t flags) { cc_.flags = flags; }

  /// Run() is used to run the <code>CompletionCallback</code>.
  /// Normally, the system runs a <code>CompletionCallback</code> after an
  /// asynchronous operation completes, but programs may wish to run the
  /// <code>CompletionCallback</code> manually in order to reuse the same code
  /// paths.
  ///
  /// @param[in] result The result of the operation to be passed to the
  /// callback function. Non-positive values correspond to the error codes
  /// from <code>pp_errors.h</code> (excluding
  /// <code>PP_OK_COMPLETIONPENDING</code>). Positive values indicate
  /// additional information such as bytes read.
  void Run(int32_t result) {
    PP_DCHECK(cc_.func);
    PP_RunCompletionCallback(&cc_, result);
  }

  /// IsOptional() is used to determine the setting of the
  /// <code>PP_COMPLETIONCALLBACK_FLAG_OPTIONAL</code> flag. This flag allows
  /// any method taking such callback to complete synchronously
  /// and not call the callback if the operation would not block. This is useful
  /// when performance is an issue, and the operation bandwidth should not be
  /// limited to the processing speed of the message loop.
  ///
  /// On synchronous method completion, the completion result will be returned
  /// by the method itself. Otherwise, the method will return
  /// PP_OK_COMPLETIONPENDING, and the callback will be invoked asynchronously
  /// on the main thread of Pepper execution.
  ///
  /// @return true if this callback is optional, otherwise false.
  bool IsOptional() const {
    return (cc_.func == NULL ||
            (cc_.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL) != 0);
  }

  /// The pp_completion_callback() function returns the underlying
  /// <code>PP_CompletionCallback</code>
  ///
  /// @return A <code>PP_CompletionCallback</code>.
  const PP_CompletionCallback& pp_completion_callback() const { return cc_; }

  /// The flags() function returns flags used to control how non-NULL callbacks
  /// are scheduled by asynchronous methods.
  ///
  /// @return An int32_t containing a bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags.
  int32_t flags() const { return cc_.flags; }

  /// MayForce() is used when implementing functions taking callbacks.
  /// If the callback is required and <code>result</code> indicates that it has
  /// not been scheduled, it will be forced on the main thread.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code
  ///
  /// int32_t OpenURL(pp::URLLoader* loader,
  ///                 pp::URLRequestInfo* url_request_info,
  ///                 const CompletionCallback& cc) {
  ///   if (loader == NULL || url_request_info == NULL)
  ///     return cc.MayForce(PP_ERROR_BADRESOURCE);
  ///   return loader->Open(*loader, *url_request_info, cc);
  /// }
  ///
  /// @endcode
  ///
  /// @param[in] result PP_OK_COMPLETIONPENDING or the result of the completed
  /// operation to be passed to the callback function. PP_OK_COMPLETIONPENDING
  /// indicates that the callback has already been scheduled. Other
  /// non-positive values correspond to error codes from
  /// <code>pp_errors.h</code>. Positive values indicate additional information
  /// such as bytes read.
  ///
  /// @return <code>PP_OK_COMPLETIONPENDING</code> if the callback has been
  /// forced, result parameter otherwise.
  int32_t MayForce(int32_t result) const {
    if (result == PP_OK_COMPLETIONPENDING || IsOptional())
      return result;
    Module::Get()->core()->CallOnMainThread(0, *this, result);
    return PP_OK_COMPLETIONPENDING;
  }

 protected:
  PP_CompletionCallback cc_;
};

/// BlockUntilComplete() is used in place of an actual completion callback
/// to request blocking behavior. If specified, the calling thread will block
/// until the function completes. Blocking completion callbacks are only
/// allowed from background threads.
///
/// @return A <code>CompletionCallback</code> corresponding to a NULL callback.
CompletionCallback BlockUntilComplete();

/// CompletionCallbackFactory<T> may be used to create CompletionCallback
/// objects that are bound to member functions.
///
/// If a factory is destroyed, then any pending callbacks will be cancelled
/// preventing any bound member functions from being called.  The CancelAll()
/// method allows pending callbacks to be cancelled without destroying the
/// factory.
///
/// <strong>Note: </strong><code>CompletionCallbackFactory<T></code> isn't
/// thread safe, but you can make it more thread-friendly by passing a
/// thread-safe refcounting class as the second template element. However, it
/// only guarantees safety for creating a callback from another thread, the
/// callback itself needs to execute on the same thread as the thread that
/// creates/destroys the factory. With this restriction, it is safe to create
/// the <code>CompletionCallbackFactory</code> on the main thread, create
/// callbacks from any thread and pass them to CallOnMainThread().
///
/// <strong>Example: </strong>
///
/// @code
///
///   class MyHandler {
///    public:
///     MyHandler() : factory_(this), offset_(0) {
///     }
///
///    void ProcessFile(const FileRef& file) {
///       CompletionCallback cc = factory_.NewRequiredCallback(
///           &MyHandler::DidOpen);
///       int32_t rv = fio_.Open(file, PP_FileOpenFlag_Read, cc);
///       CHECK(rv == PP_OK_COMPLETIONPENDING);
///     }
///
///    private:
///     CompletionCallback NewCallback() {
///       return factory_.NewCallback(&MyHandler::DidCompleteIO);
///     }
///
///     void DidOpen(int32_t result) {
///       if (result == PP_OK) {
///         // The file is open, and we can begin reading.
///         offset_ = 0;
///         ReadMore();
///       } else {
///         // Failed to open the file with error given by 'result'.
///       }
///     }
///
///     void DidRead(int32_t result) {
///       if (result > 0) {
///         // buf_ now contains 'result' number of bytes from the file.
///         ProcessBytes(buf_, result);
///         offset_ += result;
///         ReadMore();
///       } else {
///         // Done reading (possibly with an error given by 'result').
///       }
///     }
///
///     void ReadMore() {
///       CompletionCallback cc =
///           factory_.NewOptionalCallback(&MyHandler::DidRead);
///       int32_t rv = fio_.Read(offset_, buf_, sizeof(buf_),
///                              cc.pp_completion_callback());
///       if (rv != PP_OK_COMPLETIONPENDING)
///         cc.Run(rv);
///     }
///
///     void ProcessBytes(const char* bytes, int32_t length) {
///       // Do work ...
///     }
///
///     pp::CompletionCallbackFactory<MyHandler> factory_;
///     pp::FileIO fio_;
///     char buf_[4096];
///     int64_t offset_;
///   };
///
/// @endcode
///
template <typename T, typename RefCount = NonThreadSafeRefCount>
class CompletionCallbackFactory {
 public:

  /// This constructor creates a <code>CompletionCallbackFactory</code>
  /// bound to an object. If the constructor is called without an argument,
  /// the default value of <code>NULL</code> is used. The user then must call
  /// Initialize() to initialize the object.
  ///
  /// param[in] object Optional parameter. An object whose member functions
  /// are to be bound to CompletionCallbacks created by this
  /// <code>CompletionCallbackFactory</code>. The default value of this
  /// parameter is <code>NULL</code>.
  explicit CompletionCallbackFactory(T* object = NULL)
      : object_(object) {
    InitBackPointer();
  }

  /// Destructor.
  ~CompletionCallbackFactory() {
    ResetBackPointer();
  }

  /// CancelAll() cancels all <code>CompletionCallbacks</code> allocated from
  /// this factory.
  void CancelAll() {
    ResetBackPointer();
    InitBackPointer();
  }
  /// Initialize() binds the <code>CallbackFactory</code> to a particular
  /// object. Use this when the object is not available at
  /// <code>CallbackFactory</code> creation, and the <code>NULL</code> default
  /// is passed to the constructor. The object may only be initialized once,
  /// either by the constructor, or by a call to Initialize().
  ///
  /// @param[in] object The object whose member functions are to be bound to
  /// the <code>CompletionCallback</code> created by this
  /// <code>CompletionCallbackFactory</code>.
  void Initialize(T* object) {
    PP_DCHECK(object);
    PP_DCHECK(!object_);  // May only initialize once!
    object_ = object;
  }

  /// GetObject() returns the object that was passed at initialization to
  /// Intialize().
  ///
  /// @return the object passed to the constructor or Intialize().
  T* GetObject() {
    return object_;
  }

  /// NewCallback allocates a new, single-use <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  /// NewCallback() is equivalent to NewRequiredCallback() below.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method>
  CompletionCallback NewCallback(Method method) {
    PP_DCHECK(object_);
    return NewCallbackHelper(Dispatcher0<Method>(method));
  }

  /// NewRequiredCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that will always run. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method>
  CompletionCallback NewRequiredCallback(Method method) {
    CompletionCallback cc = NewCallback(method);
    cc.set_flags(cc.flags() & ~PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewOptionalCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that might not run if the method
  /// taking it can complete synchronously. Thus, if after passing the
  /// CompletionCallback to a Pepper method, the method does not return
  /// PP_OK_COMPLETIONPENDING, then you should manually call the
  /// CompletionCallback's Run method, or memory will be leaked.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method>
  CompletionCallback NewOptionalCallback(Method method) {
    CompletionCallback cc = NewCallback(method);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewCallback() allocates a new, single-use <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  /// NewCallback() is equivalent to NewRequiredCallback() below.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A>
  CompletionCallback NewCallback(Method method, const A& a) {
    PP_DCHECK(object_);
    return NewCallbackHelper(Dispatcher1<Method, A>(method, a));
  }

  /// NewRequiredCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that will always run. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A>
  CompletionCallback NewRequiredCallback(Method method, const A& a) {
    CompletionCallback cc = NewCallback(method, a);
    cc.set_flags(cc.flags() & ~PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewOptionalCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that might not run if the method
  /// taking it can complete synchronously. Thus, if after passing the
  /// CompletionCallback to a Pepper method, the method does not return
  /// PP_OK_COMPLETIONPENDING, then you should manually call the
  /// CompletionCallback's Run method, or memory will be leaked.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A>
  CompletionCallback NewOptionalCallback(Method method, const A& a) {
    CompletionCallback cc = NewCallback(method, a);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewCallback() allocates a new, single-use
  /// <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  /// NewCallback() is equivalent to NewRequiredCallback() below.
  ///
  /// @param method The method taking the callback. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a, const B& b)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B>
  CompletionCallback NewCallback(Method method, const A& a, const B& b) {
    PP_DCHECK(object_);
    return NewCallbackHelper(Dispatcher2<Method, A, B>(method, a, b));
  }

  /// NewRequiredCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that will always run. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param method The method taking the callback. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a, const B& b)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B>
  CompletionCallback NewRequiredCallback(Method method, const A& a,
                                         const B& b) {
    CompletionCallback cc = NewCallback(method, a, b);
    cc.set_flags(cc.flags() & ~PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewOptionalCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that might not run if the method
  /// taking it can complete synchronously. Thus, if after passing the
  /// CompletionCallback to a Pepper method, the method does not return
  /// PP_OK_COMPLETIONPENDING, then you should manually call the
  /// CompletionCallback's Run method, or memory will be leaked.
  ///
  /// @param[in] method The method taking the callback. Method should be of
  /// type:
  /// <code>void (T::*)(int32_t result, const A& a, const B& b)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
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
