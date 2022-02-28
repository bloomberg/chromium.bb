---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/coding-style
  - Coding Style
page_name: important-abstractions-and-data-structures
title: Important Abstractions and Data Structures
---

[TOC]

## [TaskRunner](https://code.google.com/p/chromium/codesearch#chromium/src/base/task_runner.h&q=TaskRunner&sq=package:chromium&type=cs) & [SequencedTaskRunner](https://code.google.com/p/chromium/codesearch#chromium/src/base/sequenced_task_runner.h&q=SequencedTaskRunner&sq=package:chromium&type=cs) & [SingleThreadTaskRunner](https://code.google.com/p/chromium/codesearch#chromium/src/base/single_thread_task_runner.h&q=SingleThreadTaskRunner&sq=package:chromium&type=cs)

Interfaces for posting base::Callbacks "tasks" to be run by the TaskRunner.
TaskRunner makes no guarantees about execution (order, concurrency, or if it's
even run at all). SequencedTaskRunner offers certain guarantees about the
sequence of execution (roughly speaking FIFO, but see the header for nitty
gritty details if interested) and SingleThreadTaskRunner offers the same
guarantees as SequencedTaskRunner except all tasks run on the same thread.
MessageLoopProxy is the canonical example of a SingleThreadTaskRunner. These
interfaces are also useful for testing via dependency injection. **NOTE:**
successfully posting to a TaskRunner does not necessarily mean the task will
run.

**NOTE:** A **very** useful member function of TaskRunner is PostTaskAndReply(),
which will post a task to a target TaskRunner and on completion post a "reply"
task to the origin TaskRunner.

## [MessageLoop](https://code.google.com/p/chromium/codesearch#chromium/src/base/message_loop/message_loop.h&q=MessageLoop&sq=package:chromium&type=cs&l=46) & [MessageLoopProxy](https://code.google.com/p/chromium/codesearch#chromium/src/base/message_loop/message_loop_proxy.h&q=MessageLoopProxy&sq=package:chromium&type=cs&l=17) & [BrowserThread](https://code.google.com/p/chromium/codesearch#chromium/src/content/public/browser/browser_thread.h&q=BrowserThread&sq=package:chromium&type=cs) & [RunLoop](https://code.google.com/p/chromium/codesearch#chromium/src/base/run_loop.h&q=run_loop.h&sq=package:chromium&type=cs&l=31)

These are various APIs for posting a task. MessageLoop is a concrete object used
by MessageLoopProxy (the most widely used task runner in Chromium code). You
should almost always use MessageLoopProxy instead of MessageLoop, or if you're
in chrome/ or content/, you can use BrowserThread. This is to avoid races on
MessageLoop destruction, since MessageLoopProxy and BrowserThread will delete
the task if the underlying MessageLoop is already destroyed. **NOTE:**
successfully posting to a MessageLoop(Proxy) does not necessarily mean the task
will run.

PS: There's some debate about when to use SequencedTaskRunner vs
MessageLoopProxy vs BrowserThread. Using an interface class like
SequencedTaskRunner makes the code more abstract/reusable/testable. On the other
hand, due to the extra layer of indirection, it makes the code less obvious.
Using a concrete BrowserThread ID makes it immediately obvious which thread it's
running on, although arguably you could name the SequencedTaskRunner variable
appropriately to make it more clear. The current decision is to only convert
code from BrowserThread to a TaskRunner subtype when necessary. MessageLoopProxy
should probably always be passed around as a SingleThreadTaskRunner or a parent
interface like SequencedTaskRunner.

## [base::SequencedWorkerPool](https://code.google.com/p/chromium/codesearch#chromium/src/base/threading/sequenced_worker_pool.h&q=base::SequencedWorkerPool&sq=package:chromium&l=72&type=cs) & [base::WorkerPool](https://code.google.com/p/chromium/codesearch#chromium/src/base/threading/worker_pool.h&q=base::WorkerPool&sq=package:chromium&l=30&type=cs)

These are the two primary worker pools in Chromium. SequencedWorkerPool is a
more complicated worker pool that inherits from TaskRunner and provides ways to
order tasks in a sequence (by sharing a SequenceToken) and also specifies
shutdown behavior (block shutdown on task execution, do not run the task if the
browser is shutting down and it hasn't started yet but if it has then block on
it, or allow the task to run irrespective of browser shutdown and don't block
shutdown on it). SequencedWorkerPool also provides a facility to return a
SequencedTaskRunner based on a SequenceToken. The Chromium browser process will
shutdown base::SequencedWorkerPool after all main browser threads (other than
the main thread) have stopped. base::WorkerPool is a global object that is not
shutdown on browser process shutdown, so all the tasks running on it will not be
joined. It's generally unadvisable to use base::WorkerPool since tasks may have
dependencies on other objects that may be in the process of being destroyed
during browser shutdown.

## [base::Callback](https://code.google.com/p/chromium/codesearch#chromium/src/base/callback.h) and [base::Bind()](https://code.google.com/p/chromium/codesearch#chromium/src/base/bind.h&q=Base::bind&sq=package:chromium&type=cs)

base::Callback is a set of internally refcounted templated callback classes with
different arities and return values (including void). Note that these callbacks
are copyable, but share (via refcounting) internal storage for the function
pointer and the bound arguments. base::Bind() will bind arguments to a function
pointer (under the hood, it copies the function pointer and all arguments into
an internal refcounted storage object) and returns a base::Callback.

base::Bind() will automagically AddRef()/Release() the first argument if the
function is a member function and will complain if the type is not refcounted
(avoid this problem with base::WeakPtr or base::Unretained()). Also, for the
function arguments, it will use a COMPILE_ASSERT to try to verify they are not
raw pointers to a refcounted type (only possible with full type information, not
forward declarations). Instead, use scoped_refptrs or call make_scoped_refptr()
to prevent [bugs](http://code.google.com/p/chromium/issues/detail?id=28083). In
addition, base::Bind() understands base::WeakPtr. If the function is a member
function and the first argument is a base::WeakPtr to the object, base::Bind()
will inject a wrapper function that only invokes the function pointer if the
base::WeakPtr is non-NULL. base::Bind() also has the following helper wrappers
for arguments.

*   base::Unretained() - disables the refcounting of member function
            receiver objects (which may not be of refcounted types) and the
            COMPILE_ASSERT on function arguments. Use with care, since it
            implies you need to make sure the lifetime of the object lasts
            beyond when the callback can be invoked. For the member function
            receiver object, it's probably better to use a base::WeakPtr
            instead.
*   base::Owned() - transfer ownership of a raw pointer to the returned
            base::Callback storage. Very useful because TaskRunners are not
            guaranteed to run callbacks (which may want to delete the object) on
            shutdown, so by making the callback take ownership, this prevents
            annoying shutdown leaks when the callback is not run.
*   base::Passed() - useful for passing a scoped object
            (scoped_ptr/ScopedVector/etc) to a callback. The primary difference
            between base::Owned() and base::Passed() is base::Passed() requires
            the function signature take the scoped type as a parameter, and thus
            allows for **transferring** ownership via .release(). **NOTE:**
            since the scope of the scoped type is the function scope, that means
            the base::Callback **must only be called once**. Otherwise, it would
            be a potential use after free and a definite double delete. Given
            the complexity of base::Passed()'s semantics in comparison to
            base::Owned(), you should **prefer base::Owned() to base::Passed()**
            in general.
*   base::ConstRef() - passes an argument as a const reference instead
            of copying it into the internal callback storage. Useful for obvious
            performance reasons, but generally should not be used, since it
            requires that the lifetime of the referent must live beyond when the
            callback can be invoked.
*   base::IgnoreResult() - use this with the function pointer passed to
            base::Bind() to ignore the result. Useful to make the callback
            usable with a TaskRunner which only takes Closures (callbacks with
            no parameters nor return values).

## [scoped_refptr&lt;T&gt; & base::RefCounted & base::RefCountedThreadSafe](https://code.google.com/p/chromium/codesearch#chromium/src/base/memory/ref_counted.h)

Reference counting is occasionally useful but is more often a sign that someone
isn't thinking carefully about ownership. Use it when ownership is truly shared
(for example, multiple tabs sharing the same renderer process), **not** for when
lifetime management is difficult to reason about.

## [Singleton](https://code.google.com/p/chromium/codesearch#chromium/src/base/memory/singleton.h&q=singleton&sq=package:chromium&type=cs) & [base::LazyInstance](https://code.google.com/p/chromium/codesearch#chromium/src/base/lazy_instance.h&q=base::LazyInstance&sq=package:chromium&type=cs)

They're globals, so you generally should [avoid using
them](http://www.object-oriented-security.org/lets-argue/singletons), as per the
[style
guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml?showone=Static_and_Global_Variables#Static_and_Global_Variables).
That said, when you use globals in Chromium code, it's often good to use one of
these, and in general, prefer base::LazyInstance over Singleton. The reason to
use these classes is construction is lazy (thereby preventing startup slowdown
due to static initializers) and destruction order is well-defined. They are all
destroyed in opposite order as construction when the AtExitManager is destroyed.
In the Chromium browser process, the AtExitManager is instantiated early on in
the main thread (the UI thread), so all of these objects will be destroyed on
the main thread, even if constructed on a different thread. The reason to prefer
base::LazyInstance over base::Singleton is base::LazyInstance reduces heap
fragmentation by reserving space in the data segment and using placement new to
construct the object in that memory location. **NOTE:** Both Singleton and
base::LazyInstance provide "leaky" traits to leak the global on shutdown. This
is often advisable (except potentially in library code where the code may be
dynamically loaded into another process's address space or when data needs to be
flushed on process shutdown) in order to not to slow down shutdown. There are
valgrind suppressions for these "leaky" traits.

## [base::Thread](https://code.google.com/p/chromium/codesearch#chromium/src/base/threading/thread.h&q=base::Thread&sq=package:chromium&type=cs) & [base::PlatformThread](https://code.google.com/p/chromium/codesearch#chromium/src/base/threading/platform_thread.h&q=base::PlatformThread&sq=package:chromium&type=cs)

Generally you shouldn't use these, since you should usually post tasks to an
existing TaskRunner. PlatformThread is a platform-specific thread. base::Thread
contains a MessageLoop running on a PlatformThread.

## [base::WeakPtr](https://code.google.com/p/chromium/codesearch#chromium/src/base/memory/weak_ptr.h&q=base::WeakPtr&sq=package:chromium&type=cs) & [base::WeakPtrFactory](https://code.google.com/p/chromium/codesearch#chromium/src/base/memory/weak_ptr.h&q=base::WeakPtrFactory&sq=package:chromium&type=cs&l=246)

Mostly thread-unsafe weak pointer that returns NULL if the referent has been
destroyed. It's safe to pass across threads (and to destroy on other threads),
but it should only be used on the original thread it was created on.
base::WeakPtrFactory is useful for automatically canceling base::Callbacks when
the referent of the base::WeakPtr gets destroyed.

## [FilePath](https://code.google.com/p/chromium/codesearch#chromium/src/base/files/file_path.h&q=FilePath&sq=package:chromium&type=cs&l=132)

A cross-platform representation of a file path. You should generally use this
instead of platform-specific representations.

## [ObserverList](https://code.google.com/p/chromium/codesearch#chromium/src/base/observer_list.h&q=ObserverList&sq=package:chromium&l=54) & [ObserverListThreadSafe](https://code.google.com/p/chromium/codesearch#chromium/src/base/observer_list_threadsafe.h&q=ObserverListThreadSafe&sq=package:chromium&type=cs&l=94)

ObserverList is a thread-unsafe object that is intended to be used as a member
variable of a class. It provides a simple interface for iterating on a bunch of
Observer objects and invoking a notification method.

ObserverListThreadSafe similar. It contains multiple ObserverLists, and observer
notifications are invoked on the same PlatformThreadId that the observer was
registered on, thereby allowing proxying notifications across threads and
allowing the individual observers to receive notifications in a single threaded
manner.

## [Pickle](https://code.google.com/p/chromium/codesearch#chromium/src/base/pickle.h&q=Pickle&sq=package:chromium&type=cs)

Pickle provides a basic facility for object serialization and deserialization in
binary form.

## [Value](https://code.google.com/p/chromium/codesearch#chromium/src/base/values.h&sq=package:chromium&rcl=1367374264&l=55)

Values allow for specifying recursive data classes (lists and dictionaries)
containing simple values (bool/int/string/etc). These values can also be
serialized to JSON and back.

## [LOG](https://code.google.com/p/chromium/codesearch#chromium/src/base/logging.h&sq=package:chromium)

This is the basic interface for logging in Chromium.

## [FileUtilProxy](https://code.google.com/p/chromium/codesearch#chromium/src/base/files/file_util_proxy.h&q=FileUtilProxy&sq=package:chromium&l=25&type=cs)

Generally you should not do file I/O on jank-sensitive threads
(BrowserThread::UI and BrowserThread::IO), so you can proxy them to another
thread (such as BrowserThread::FILE) via these utilities.

## [Time](https://code.google.com/p/chromium/codesearch#chromium/src/base/time/time.h&sq=package:chromium), [TimeDelta](https://code.google.com/p/chromium/codesearch#chromium/src/base/time/time.h&sq=package:chromium), [TimeTicks](https://code.google.com/p/chromium/codesearch#chromium/src/base/time/time.h&sq=package:chromium), [Timer](https://code.google.com/p/chromium/codesearch#chromium/src/base/timer/timer.h&sq=package:chromium)

Generally use TimeTicks instead of Time to keep a stable tick counter (Time may
change if the user changes the computer clock).

## [PrefService](https://code.google.com/p/chromium/codesearch#chromium/src/components/prefs/pref_service.h&q=PrefService&sq=package:chromium&type=cs), [ExtensionPrefs](https://code.google.com/p/chromium/codesearch#chromium/src/extensions/browser/extension_prefs.h)

Containers for persistent state associated with a user
[Profile](http://code.google.com/searchframe#OAMlx_jo-ck/src/chrome/browser/profiles/profile.h).