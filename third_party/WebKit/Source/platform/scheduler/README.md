# Blink Scheduler

This directory contains the Blink Scheduler, which coordinates task execution
in renderer processes. The main subdirectories are:

- `base/` -- basic scheduling primitives such as `TaskQueue` and
   `TaskQueueManager`.
- `child/` -- contains the `ChildScheduler` which is the base class for all
   thread schedulers, as well as a `WorkerScheduler` for worker threads.
- `utility/` -- a small scheduler for utility processes.
- `renderer/` -- `RendererScheduler` for the renderer process.

The scheduler exposes an API at `public/platform/scheduler`.
