// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_H_
#define PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_H_

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/utility/non_thread_safe_ref_count.h"

namespace pp {

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
///     // If an compiler warns on following using |this| in the initializer
///     // list, use PP_ALLOW_THIS_IN_INITIALIZER_LIST macro.
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

  /// NewCallback() allocates a new, single-use
  /// <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  /// NewCallback() is equivalent to NewRequiredCallback() below.
  ///
  /// @param method The method taking the callback. Method should be of type:
  /// <code>
  /// void (T::*)(int32_t result, const A& a, const B& b, const C& c)
  /// </code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] c Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B, typename C>
  CompletionCallback NewCallback(Method method, const A& a, const B& b,
                                 const C& c) {
    PP_DCHECK(object_);
    return NewCallbackHelper(Dispatcher3<Method, A, B, C>(method, a, b, c));
  }

  /// NewRequiredCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that will always run. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param method The method taking the callback. Method should be of type:
  /// <code>
  /// void (T::*)(int32_t result, const A& a, const B& b, const C& c)
  /// </code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] c Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B, typename C>
  CompletionCallback NewRequiredCallback(Method method, const A& a,
                                         const B& b, const C& c) {
    CompletionCallback cc = NewCallback(method, a, b, c);
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
  /// <code>
  /// void (T::*)(int32_t result, const A& a, const B& b, const C& c)
  /// </code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] c Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B, typename C>
  CompletionCallback NewOptionalCallback(Method method, const A& a,
                                         const B& b, const C& c) {
    CompletionCallback cc = NewCallback(method, a, b, c);
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

  template <typename Method, typename A, typename B, typename C>
  class Dispatcher3 {
   public:
    Dispatcher3(Method method, const A& a, const B& b, const C& c)
        : method_(method),
          a_(a),
          b_(b),
          c_(c) {
    }
    void operator()(T* object, int32_t result) {
      (object->*method_)(result, a_, b_, c_);
    }
   private:
    Method method_;
    A a_;
    B b_;
    C c_;
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

#endif  // PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_H_
