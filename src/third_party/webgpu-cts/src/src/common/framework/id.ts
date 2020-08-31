import { ParamSpec } from './params_utils.js';

// Identifies a test spec file.
export interface TestSpecID {
  // The spec's suite name, e.g. 'webgpu'.
  readonly suite: string;
  // The spec's path within the suite, e.g. 'command_buffer/compute/basic'.
  readonly path: string;
}

export function testSpecEquals(x: TestSpecID, y: TestSpecID): boolean {
  return x.suite === y.suite && x.path === y.path;
}

// Identifies a test case (a specific parameterization of a test), within its spec file.
export interface TestCaseID {
  readonly test: string;
  readonly params: ParamSpec | null;
}

export interface TestSpecOrTestOrCaseID {
  readonly spec: TestSpecID;
  readonly test?: string;
  readonly params?: ParamSpec | null;
}
