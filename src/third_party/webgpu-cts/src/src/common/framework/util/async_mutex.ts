export class AsyncMutex {
  // The newest item currently waiting for the mutex. This promise is chained so
  // that it implicitly defines a FIFO queue where this is the "last-in" item.
  private newestQueueItem: Promise<unknown> | undefined;

  // Run an async function with a lock on this mutex.
  // Waits until the mutex is available, locks it, runs the function, then releases it.
  async with<T>(fn: () => Promise<T>): Promise<T> {
    const p = (async () => {
      // If the mutex is locked, wait for the last thing in the queue before running.
      // (Everything in the queue runs in order, so this is after everything currently enqueued.)
      if (this.newestQueueItem) {
        await this.newestQueueItem;
      }
      return fn();
    })();

    // Push the newly-created Promise onto the queue by replacing the old "newest" item.
    this.newestQueueItem = p;
    // And return so the caller can wait on the result.
    return p;
  }
}
