import { extractImportantStackTrace } from '../util/stack.js';

export class LogMessageWithStack extends Error {
  private stackHidden: boolean = false;
  private timesSeen: number = 1;

  constructor(name: string, ex: Error) {
    super(ex.message);

    this.name = name;
    this.stack = ex.stack;
  }

  /** Set a flag so the stack is not printed in toJSON(). */
  setStackHidden() {
    this.stackHidden = true;
  }

  /** Increment the "seen x times" counter. */
  incrementTimesSeen() {
    this.timesSeen++;
  }

  toJSON(): string {
    let m = this.name;
    if (this.message) m += ': ' + this.message;
    if (!this.stackHidden && this.stack) {
      m += '\n' + extractImportantStackTrace(this);
    }
    if (this.timesSeen > 1) {
      m += `\n(seen ${this.timesSeen} times with identical stack, not necessarily in a row; enable ?debug=1 to stop)`;
    }
    return m;
  }
}

/**
 * Returns a string, nicely indented, for debug logs.
 * This is used in the cmdline and wpt runtimes. In WPT, it shows up in the `*-actual.txt` file.
 */
export function prettyPrintLog(log: LogMessageWithStack): string {
  return '  - ' + log.toJSON().replace(/\n/g, '\n    ');
}
