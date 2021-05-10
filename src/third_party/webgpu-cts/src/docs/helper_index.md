# Index of Test Helpers

This index is a quick-reference of helper functions in the test suite.
Use it to determine whether you can reuse a helper, instead of writing new code,
to improve readability and reviewability.

Whenever a new generally-useful helper is added, it should be indexed here.

Generally, see:

- [`src/webgpu/gpu_test.ts`](../src/webgpu/gpu_test.ts) (for all tests)
- [`src/webgpu/api/validation/validation_test.ts`](../src/webgpu/api/validation/validation_test.ts)
  (for validation tests).
- Structured information about texture formats, binding types, etc. can be found in
  [`src/webgpu/capability_info.ts`](../src/webgpu/capability_info.ts).
- Constant values (needed anytime a WebGPU constant is needed outside of a test function)
  can be found in [`src/webgpu/constants.ts`](../src/webgpu/constants.ts).

## Index

- TODO: Index existing helpers.
