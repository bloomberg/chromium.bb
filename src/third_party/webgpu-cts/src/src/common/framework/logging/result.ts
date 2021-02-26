import { LogMessageWithStack } from './log_message.js';

export type Status = 'running' | 'pass' | 'skip' | 'warn' | 'fail';

export interface TestCaseResult {
  status: Status;
  timems: number;
}

export interface LiveTestCaseResult extends TestCaseResult {
  logs?: LogMessageWithStack[];
}

export interface TransferredTestCaseResult extends TestCaseResult {
  // When transferred from a worker, a LogMessageWithStack turns into a generic Error
  // (its prototype gets lost and replaced with Error).
  logs?: Error[];
}
