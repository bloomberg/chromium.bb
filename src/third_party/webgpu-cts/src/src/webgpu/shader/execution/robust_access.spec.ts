export const description = `
Tests to check datatype clamping in shaders is correctly implemented for all indexable types
(vectors, matrices, sized/unsized arrays) visible to shaders in various ways.

TODO: add tests to check that textureLoad operations stay in-bounds.
`;

import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert } from '../../../common/util/util.js';
import { GPUTest } from '../../gpu_test.js';
import { align } from '../../util/math.js';
import {
  ScalarType,
  kScalarTypeInfo,
  kVectorContainerTypes,
  kVectorContainerTypeInfo,
  kMatrixContainerTypes,
  kMatrixContainerTypeInfo,
  kScalarTypes,
} from '../types.js';

export const g = makeTestGroup(GPUTest);

const kMaxU32 = 0xffff_ffff;
const kMaxI32 = 0x7fff_ffff;
const kMinI32 = -0x8000_0000;

/**
 * Wraps the provided source into a harness that checks calling `runTest()` returns 0.
 *
 * Non-test bindings are in bind group 1, including:
 * - `constants.zero`: a dynamically-uniform `0u` value.
 */
function runShaderTest(
  t: GPUTest,
  stage: GPUShaderStageFlags,
  testSource: string,
  testBindings: GPUBindGroupEntry[]
): void {
  assert(stage === GPUShaderStage.COMPUTE, 'Only know how to deal with compute for now');

  // Contains just zero (for now).
  const constantsBuffer = t.device.createBuffer({ size: 4, usage: GPUBufferUsage.UNIFORM });

  const resultBuffer = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE,
  });

  const source = `
    [[block]] struct Constants {
      zero: u32;
    };
    [[group(1), binding(0)]] var<uniform> constants: Constants;

    [[block]] struct Result {
      value: u32;
    };
    [[group(1), binding(1)]] var<storage, write> result: Result;

    ${testSource}

    [[stage(compute), workgroup_size(1)]]
    fn main() {
      ignore(constants.zero); // Ensure constants buffer is statically-accessed
      result.value = runTest();
    }`;

  t.debug(source);
  const module = t.device.createShaderModule({ code: source });
  const pipeline = t.device.createComputePipeline({
    compute: { module, entryPoint: 'main' },
  });

  const group = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(1),
    entries: [
      { binding: 0, resource: { buffer: constantsBuffer } },
      { binding: 1, resource: { buffer: resultBuffer } },
    ],
  });

  const testGroup = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: testBindings,
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, testGroup);
  pass.setBindGroup(1, group);
  pass.dispatch(1);
  pass.endPass();

  t.queue.submit([encoder.finish()]);

  t.expectGPUBufferValuesEqual(resultBuffer, new Uint32Array([0]));
}

/** Fill an ArrayBuffer with sentinel values, except clear a region to zero. */
function testFillArrayBuffer(
  array: ArrayBuffer,
  type: 'u32' | 'i32' | 'f32',
  { zeroByteStart, zeroByteCount }: { zeroByteStart: number; zeroByteCount: number }
) {
  const constructor = { u32: Uint32Array, i32: Int32Array, f32: Float32Array }[type];
  assert(zeroByteCount % constructor.BYTES_PER_ELEMENT === 0);
  new constructor(array).fill(42);
  new constructor(array, zeroByteStart, zeroByteCount / constructor.BYTES_PER_ELEMENT).fill(0);
}

const kArrayLength = 3;

/**
 * Generate a bunch of indexable types (vec, mat, sized/unsized array) for testing.
 */
function* generateIndexableTypes({
  storageClass,
  baseType,
  atomic = false,
}: {
  /** Unsized array will only be generated for storageClass "storage". */
  storageClass: string;
  /** Base scalar type (i32/u32/f32/bool). */
  baseType: ScalarType;
  /** Whether to wrap the baseType in `atomic<>`. */
  atomic?: boolean;
}) {
  const scalarInfo = kScalarTypeInfo[baseType];
  if (atomic) assert(scalarInfo.supportsAtomics, 'type does not support atomics');
  const scalarType = atomic ? `atomic<${baseType}>` : baseType;

  // Don't generate vec2<atomic<i32>> etc.
  if (!atomic) {
    // Vector types
    for (const vectorType of kVectorContainerTypes) {
      yield {
        type: `${vectorType}<${scalarType}>`,
        _typeInfo: { elementBaseType: baseType, ...kVectorContainerTypeInfo[vectorType] },
      };
    }

    // Matrices can only be f32.
    if (baseType === 'f32') {
      for (const matrixType of kMatrixContainerTypes) {
        const matrixInfo = kMatrixContainerTypeInfo[matrixType];
        yield {
          type: `${matrixType}<${scalarType}>`,
          _typeInfo: {
            elementBaseType: `vec${matrixInfo.innerLength}<${scalarType}>`,
            ...matrixInfo,
          },
        };
      }
    }
  }

  const arrayTypeInfo = {
    elementBaseType: baseType,
    arrayLength: kArrayLength,
    layout: scalarInfo.layout
      ? {
          alignment: scalarInfo.layout.alignment,
          size: kArrayLength * arrayStride(scalarInfo.layout),
        }
      : undefined,
  };
  // Sized array
  yield { type: `array<${scalarType},${kArrayLength}>`, _typeInfo: arrayTypeInfo };
  // Unsized array
  if (storageClass === 'storage') {
    yield { type: `array<${scalarType}>`, _typeInfo: arrayTypeInfo };
  }
}

function arrayStride(elementLayout: { size: number; alignment: number }) {
  return align(elementLayout.size, elementLayout.alignment);
}

/** Generates scalarTypes (i32/u32/f32/bool) that support the specified usage. */
function* supportedScalarTypes(p: { atomic: boolean; storageClass: string }) {
  for (const scalarType of kScalarTypes) {
    const info = kScalarTypeInfo[scalarType];

    // Test atomics only on supported scalar types.
    if (p.atomic && !info.supportsAtomics) continue;

    // Storage and uniform require host-sharable types.
    const isHostShared = p.storageClass === 'storage' || p.storageClass === 'uniform';
    if (isHostShared && info.layout === undefined) continue;

    yield scalarType;
  }
}

/** Atomic access requires atomic type and storage/workgroup memory. */
function supportsAtomics(p: { storageClass: string; storageMode: string | undefined }) {
  return (
    (p.storageClass === 'storage' && p.storageMode === 'read_write') ||
    p.storageClass === 'workgroup'
  );
}

g.test('linear_memory')
  .desc(
    `For each indexable data type (vec, mat, sized/unsized array, of various scalar types), attempts
    to access (read, write, atomic load/store) a region of memory (buffer or internal) at various
    (signed/unsigned) indices. Checks that the accesses conform to robust access (OOB reads only
    return bound memory, OOB writes don't write OOB).

    TODO: Test in/out storage classes.
    TODO: Test vertex and fragment stages.
    TODO: Test using a dynamic offset instead of a static offset into uniform/storage bindings.
    TODO: Test types like vec2<atomic<i32>>, if that's allowed.
    TODO: Test exprIndexAddon as constexpr.
    TODO: Test exprIndexAddon as pipeline-overridable constant expression.
  `
  )
  .params(u =>
    u
      .combineWithParams([
        { storageClass: 'storage', storageMode: 'read', access: 'read' },
        { storageClass: 'storage', storageMode: 'write', access: 'write' },
        { storageClass: 'storage', storageMode: 'read_write', access: 'read' },
        { storageClass: 'storage', storageMode: 'read_write', access: 'write' },
        { storageClass: 'uniform', access: 'read' },
        { storageClass: 'private', access: 'read' },
        { storageClass: 'private', access: 'write' },
        { storageClass: 'function', access: 'read' },
        { storageClass: 'function', access: 'write' },
        { storageClass: 'workgroup', access: 'read' },
        { storageClass: 'workgroup', access: 'write' },
      ] as const)
      .expand('atomic', p => (supportsAtomics(p) ? [false, true] : [false]))
      .expand('baseType', supportedScalarTypes)
      .beginSubcases()
      .expandWithParams(generateIndexableTypes)
  )
  .fn(async t => {
    const { storageClass, storageMode, access, atomic, baseType, type, _typeInfo } = t.params;

    let usesCanary = false;
    let globalSource = '';
    let testFunctionSource = '';
    const testBufferSize = 512;
    const bufferBindingOffset = 256;
    /** Undefined if no buffer binding is needed */
    let bufferBindingSize: number | undefined = undefined;

    // Declare the data that will be accessed to check robust access, as a buffer or a struct
    // in the global scope or inside the test function itself.
    const structDecl = `
      struct S {
        startCanary: array<u32, 10>;
        data: ${type};
        endCanary: array<u32, 10>;
      };`;

    switch (storageClass) {
      case 'uniform':
      case 'storage':
        {
          assert(_typeInfo.layout !== undefined);
          const layout = _typeInfo.layout;
          bufferBindingSize = layout.size;
          const qualifiers = storageClass === 'storage' ? `storage, ${storageMode}` : storageClass;
          globalSource += `
          [[block]] struct TestData {
            data: ${type};
          };
          [[group(0), binding(0)]] var<${qualifiers}> s: TestData;`;
        }
        break;

      case 'private':
      case 'workgroup':
        usesCanary = true;
        globalSource += structDecl;
        globalSource += `var<${storageClass}> s: S;`;
        break;

      case 'function':
        usesCanary = true;
        globalSource += structDecl;
        testFunctionSource += 'var s: S;';
        break;
    }

    // Build the test function that will do the tests.

    // If we use a local canary declared in the shader, initialize it.
    if (usesCanary) {
      testFunctionSource += `
        for (var i = 0u; i < 10u; i = i + 1u) {
          s.startCanary[i] = 0xFFFFFFFFu;
          s.endCanary[i] = 0xFFFFFFFFu;
        }`;
    }

    /** Returns a different number each time, kind of like a `__LINE__` to ID the failing check. */
    const nextErrorReturnValue = (() => {
      let errorReturnValue = 0x1000;
      return () => {
        ++errorReturnValue;
        return `0x${errorReturnValue.toString(16)}u`;
      };
    })();

    // This is here, instead of in subcases, so only a single shader is needed to test many modes.
    for (const indexSigned of [false, true]) {
      const indicesToTest = indexSigned
        ? [
            // Exactly in bounds (should be OK)
            '0',
            `${_typeInfo.arrayLength} - 1`,
            // Exactly out of bounds
            '-1',
            `${_typeInfo.arrayLength}`,
            // Far out of bounds
            '-1000000',
            '1000000',
            `${kMinI32}`,
            `${kMaxI32}`,
          ]
        : [
            // Exactly in bounds (should be OK)
            '0u',
            `${_typeInfo.arrayLength}u - 1u`,
            // Exactly out of bounds
            `${_typeInfo.arrayLength}u`,
            // Far out of bounds
            '1000000u',
            `${kMaxU32}u`,
            `${kMaxI32}u`,
          ];

      const indexTypeLiteral = indexSigned ? '0' : '0u';
      const indexTypeCast = indexSigned ? 'i32' : 'u32';
      for (const exprIndexAddon of [
        '', // No addon
        ` + ${indexTypeLiteral}`, // Add a literal 0
        ` + ${indexTypeCast}(constants.zero)`, // Add a uniform 0
      ]) {
        // Produce the accesses to the variable.
        for (const indexToTest of indicesToTest) {
          const exprIndex = `(${indexToTest})${exprIndexAddon}`;
          const exprZeroElement = `${_typeInfo.elementBaseType}()`;
          const exprElement = `s.data[${exprIndex}]`;

          switch (access) {
            case 'read':
              {
                const exprLoadElement = atomic ? `atomicLoad(&${exprElement})` : exprElement;
                let condition = `${exprLoadElement} != ${exprZeroElement}`;
                if ('innerLength' in _typeInfo) condition = `any(${condition})`;
                testFunctionSource += `
                  if (${condition}) { return ${nextErrorReturnValue()}; }`;
              }
              break;

            case 'write':
              if (atomic) {
                testFunctionSource += `
                  atomicStore(&s.data[${exprIndex}], ${exprZeroElement});`;
              } else {
                testFunctionSource += `
                  s.data[${exprIndex}] = ${exprZeroElement};`;
              }
              break;
          }
        }
        testFunctionSource += '\n';
      }
    }

    // Check that the canaries haven't been modified
    if (usesCanary) {
      testFunctionSource += `
        for (var i = 0u; i < 10u; i = i + 1u) {
          if (s.startCanary[i] != 0xFFFFFFFFu) {
            return ${nextErrorReturnValue()};
          }
          if (s.endCanary[i] != 0xFFFFFFFFu) {
            return ${nextErrorReturnValue()};
          }
        }`;
    }

    // Run the test

    // First aggregate the test source
    const testSource = `
      ${globalSource}

      fn runTest() -> u32 {
        ${testFunctionSource}
        return 0u;
      }`;

    // Run it.
    if (bufferBindingSize !== undefined) {
      assert(baseType !== 'bool', 'case should have been filtered out');

      const expectedData = new ArrayBuffer(testBufferSize);
      const bufferBindingEnd = bufferBindingOffset + bufferBindingSize;
      testFillArrayBuffer(expectedData, baseType, {
        zeroByteStart: bufferBindingOffset,
        zeroByteCount: bufferBindingSize,
      });

      // Create a buffer that contains zeroes in the allowed access area, and 42s everywhere else.
      const testBuffer = t.makeBufferWithContents(
        new Uint8Array(expectedData),
        GPUBufferUsage.COPY_SRC |
          GPUBufferUsage.UNIFORM |
          GPUBufferUsage.STORAGE |
          GPUBufferUsage.COPY_DST
      );

      // Run the shader, accessing the buffer.
      runShaderTest(t, GPUShaderStage.COMPUTE, testSource, [
        {
          binding: 0,
          resource: { buffer: testBuffer, offset: bufferBindingOffset, size: bufferBindingSize },
        },
      ]);

      // Check that content of the buffer outside of the allowed area didn't change.
      const expectedBytes = new Uint8Array(expectedData);
      t.expectGPUBufferValuesEqual(testBuffer, expectedBytes.subarray(0, bufferBindingOffset), 0);
      t.expectGPUBufferValuesEqual(
        testBuffer,
        expectedBytes.subarray(bufferBindingEnd, testBufferSize),
        bufferBindingEnd
      );

      testBuffer.destroy();
    } else {
      runShaderTest(t, GPUShaderStage.COMPUTE, testSource, []);
    }
  });
