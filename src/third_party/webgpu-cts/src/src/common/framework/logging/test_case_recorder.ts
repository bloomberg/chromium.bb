import { SkipTestCase } from '../fixture.js';
import { now, assert } from '../util/util.js';

import { LogMessageWithStack } from './log_message.js';
import { LiveTestCaseResult } from './result.js';

enum LogSeverity {
  Pass = 0,
  Skip = 1,
  Warn = 2,
  ExpectFailed = 3,
  ValidationFailed = 4,
  ThrewException = 5,
}

const kMaxLogStacks = 2;

/** Holds onto a LiveTestCaseResult owned by the Logger, and writes the results into it. */
export class TestCaseRecorder {
  private result: LiveTestCaseResult;
  private maxLogSeverity = LogSeverity.Pass;
  private startTime = -1;
  private logs: LogMessageWithStack[] = [];
  private logLinesAtCurrentSeverity = 0;
  private debugging = false;
  /** Used to dedup log messages which have identical stacks. */
  private messagesForPreviouslySeenStacks = new Map<string, LogMessageWithStack>();

  constructor(result: LiveTestCaseResult, debugging: boolean) {
    this.result = result;
    this.debugging = debugging;
  }

  start(): void {
    assert(this.startTime < 0, 'TestCaseRecorder cannot be reused');
    this.startTime = now();
  }

  finish(): void {
    assert(this.startTime >= 0, 'finish() before start()');

    const timeMilliseconds = now() - this.startTime;
    // Round to next microsecond to avoid storing useless .xxxx00000000000002 in results.
    this.result.timems = Math.ceil(timeMilliseconds * 1000) / 1000;

    // Convert numeric enum back to string (but expose 'exception' as 'fail')
    this.result.status =
      this.maxLogSeverity === LogSeverity.Pass
        ? 'pass'
        : this.maxLogSeverity === LogSeverity.Skip
        ? 'skip'
        : this.maxLogSeverity === LogSeverity.Warn
        ? 'warn'
        : 'fail'; // Everything else is an error

    this.result.logs = this.logs;
  }

  injectResult(injectedResult: LiveTestCaseResult): void {
    Object.assign(this.result, injectedResult);
  }

  debug(ex: Error): void {
    if (!this.debugging) {
      return;
    }
    const logMessage = new LogMessageWithStack('DEBUG', ex);
    logMessage.setStackHidden();
    this.logImpl(LogSeverity.Pass, logMessage);
  }

  skipped(ex: SkipTestCase): void {
    const message = new LogMessageWithStack('SKIP', ex);
    if (!this.debugging) {
      message.setStackHidden();
    }
    this.logImpl(LogSeverity.Skip, message);
  }

  warn(ex: Error): void {
    this.logImpl(LogSeverity.Warn, new LogMessageWithStack('WARN', ex));
  }

  expectationFailed(ex: Error): void {
    this.logImpl(LogSeverity.ExpectFailed, new LogMessageWithStack('EXPECTATION FAILED', ex));
  }

  validationFailed(ex: Error): void {
    this.logImpl(LogSeverity.ValidationFailed, new LogMessageWithStack('VALIDATION FAILED', ex));
  }

  threw(ex: Error): void {
    if (ex instanceof SkipTestCase) {
      this.skipped(ex);
      return;
    }
    this.logImpl(LogSeverity.ThrewException, new LogMessageWithStack('EXCEPTION', ex));
  }

  private logImpl(level: LogSeverity, logMessage: LogMessageWithStack): void {
    // Deduplicate errors with the exact same stack
    if (logMessage.stack) {
      const seen = this.messagesForPreviouslySeenStacks.get(logMessage.stack);
      if (seen) {
        seen.incrementTimesSeen();
        return;
      }
      this.messagesForPreviouslySeenStacks.set(logMessage.stack, logMessage);
    }

    // Mark printStack=false for all logs except 2 at the highest severity
    if (level > this.maxLogSeverity) {
      this.logLinesAtCurrentSeverity = 0;
      this.maxLogSeverity = level;
      if (!this.debugging) {
        // Go back and turn off printStack for everything of a lower log level
        for (const log of this.logs) {
          log.setStackHidden();
        }
      }
    }
    if (level < this.maxLogSeverity || this.logLinesAtCurrentSeverity >= kMaxLogStacks) {
      if (!this.debugging) {
        logMessage.setStackHidden();
      }
    }
    this.logs.push(logMessage);
    this.logLinesAtCurrentSeverity++;
  }
}
