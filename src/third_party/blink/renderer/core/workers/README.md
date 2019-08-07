This directory contains the base implementation of all worker and worklet types. Also, this contains the implementation of the [Web Workers API](https://html.spec.whatwg.org/C/#workers) (Dedicated Worker and Shared Worker) and [Worklets API](https://drafts.css-houdini.org/worklets/).

# Worker / Worklet types

- Workers are divided into 2 types:
  - **In-process workers (Dedicated Workers)**: A worker of this type always runs in the same renderer process with a parent document that starts the worker.
  - **Out-of-process workers (Shared Workers and Service Workers)**: A worker of this type may run in a different renderer process with a parent document that starts the worker.
- Worklets are divided into 2 types:
  - **Main thread worklets (Paint Worklets and Layout Worklets)**: A worklet of this type runs on the main thread.
  - **Threaded worklets (Audio Worklets and Animation Worklets)**: A worklet of this type runs on a worker thread.

Worklets always run in the same renderer process with a parent document that starts the worklet like the in-process-workers.

# Naming conventions

Classes in this directory are named with the following conventions, there're still some exceptions though.

- `WorkerOrWorklet` prefix: Classes commonly used for workers and worklets (e.g., `WorkerOrWorkletGlobalScope`).
- `Worker` / `Worklet` prefix: Classes used for workers or worklets (e.g., `WorkerGlobalScope`).
- `Threaded` prefix: Classes used for workers and threaded worklets (e.g., `ThreadedMessagingProxyBase`).
- `MainThreadWorklet` prefix: Classes used for main thread worklets (e.g., `MainThreadWorkletReportingProxy`).

Thread hopping between the main (parent) thread and a worker thread is handled by proxy classes.

- `MessagingProxy` is the main (parent) thread side proxy that communicates to the worker thread.
- `ObjectProxy` is the worker thread side proxy that communicates to the main (parent) thread. `Object` indicates a worker/worklet JavaScript object on the parent execution context.

# Off-the-main-thread fetch

All worker subresources, and some of worker/worklet top-level scripts,
are fetched off-the-main-thread (i.e. on the worker/worklet thread).
See following docs for more details.

- [off-the-main-thread subresource fetch](https://docs.google.com/document/d/1829D6zllR1qfwvwDXHb9pjhIcbM4EZ70EiaFAaPj9YQ/edit?usp=sharing), Done.
- [off-the-main-thread top-level script fetch](https://docs.google.com/document/d/1cI6UJGdeWvlavCzfxGh3hfWy7N62Yfsu_A98RxsCyys/edit?usp=sharing), ongoing.

There are two types of network fetch on the worker/worklet thread:
insideSettings and outsideSettings fetch.
The terms insideSettings and outsideSettings are originated from
[HTML spec](https://html.spec.whatwg.org/C/#worker-processing-model) and
[Worklet spec](https://drafts.css-houdini.org/worklets/).

## insideSettings fetch

insideSettings fetch is subresource fetch.

In the spec, insideSettings corresponds to the
[worker environment settings object](https://html.spec.whatwg.org/multipage/workers.html#set-up-a-worker-environment-settings-object)
of `WorkerOrWorkletGlobalScope`.

In the implementation, insideSettings roughly corresponds to
`WorkerOrWorkletGlobalScope`.
`WorkerOrWorkletGlobalScope::Fetcher()`,
its corresponding `WorkerFetchContext`, and
`WorkerOrWorkletGlobalScope::GetContentSecurityPolicy()` are used.

Currently, all subresource fetches are already off-the-main-thread.

## outsideSettings fetch

outsideSettings fetch is off-the-main-thread top-level worker/worklet
script fetch.

In the spec, an outsideSettings is the environment settings object of
worker's parent context.

In the implementation, outsideSettings should correspond to
`Document` (or `WorkerOrWorkletGlobalScope` for nested workers), but
the worker thread can't access these objects due to threading restriction.
Therefore, we pass `FetchClientSettingsObjectSnapshot` that contains
information of these objects across threads, and create
`ResourceFetcher`, `WorkerFetchContext` and `ContentSecurityPolicy`
(separate objects from those used for insideSettings fetch)
on the worker thread.
They work as if the parent context, and are used via
`WorkerOrWorkletGlobalScope::CreateOutsideSettingsFetcher()`.

Note that, where off-the-main-thread top-level fetch is NOT enabled
(e.g. classic workers), the worker scripts are fetched on the main thread and
thus WorkerOrWorkletGlobalScope and the worker thread are not involved.

# References

- [WorkerGlobalScope Initialization](https://docs.google.com/document/d/1JCv8TD2nPLNC2iRCp_D1OM4I3uTS0HoEobuTymaMqgw/edit?usp=sharing) (April 1, 2019)
- [Worker / Worklet Internals](https://docs.google.com/presentation/d/1GZJ3VnLIO_Pw0jr9nRw6_-trg68ol-AkliMxJ6jo6Bo/edit?usp=sharing) (April 19, 2018)
