This directory contains experimental APIs for farming tasks out to a pool of worker threads. Everything in this directory is still highly in flux, and is not anywhere near ready to ship.

thread_pool.{h,cc} contain a class that manages a pool of worker threads and can distribute work to them.

worker_task_queue.{h,cc,idl} exposes two APIs:
* postFunction - a simple API for posting a task to a worker.
* postTask - an API for posting tasks that can specify other tasks as prerequisites and coordinates the transfer of return values of prerequisite tasks to dependent tasks

task.{h,cc,idl} exposes the simple wrapper object returned by postTask and provides the backend that runs tasks on the worker thread (for both postFunction and postTask) and tracks the relationships between tasks (for postTask).
