// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asynchronous job queue.
 */
export class AsyncJobQueue {
  private promise: Promise<unknown> = Promise.resolve();
  private clearing = false;

  /**
   * Pushes the given job into queue.
   * @return Resolved with the job return value when the job is finished, or
   *     null if the job is cleared.
   */
  push<T>(job: () => Promise<T>): Promise<T|null> {
    const promise: Promise<T|null> = this.promise.then(() => {
      if (this.clearing) {
        return null;
      }
      return job();
    });
    this.promise = promise;
    return promise;
  }

  /**
   * Flushes the job queue.
   * @return Resolved when all jobs in the queue are finished.
   */
  async flush(): Promise<void> {
    await this.promise;
  }

  /**
   * Clears all not-yet-scheduled jobs and waits for current job finished.
   */
  async clear(): Promise<void> {
    this.clearing = true;
    await this.flush();
    this.clearing = false;
  }
}
