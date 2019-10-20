import { ParamSpec } from './params/index.js';

// Identifies a test spec file.
export interface TestSpecID {
  // The spec's suite, e.g. 'cts'.
  readonly suite: string;
  // The spec's path within the suite, e.g. 'command_buffer/compute/basic'.
  readonly path: string;
}

// Identifies a test case (a specific parameterization of a test), within its spec file.
export interface TestCaseID {
  readonly test: string;
  readonly params: ParamSpec | null;
}
