export const description = `
Positive and negative validation tests for variable and const.
`;

import { makeTestGroup } from '../../../common/framework/test_group.js';

import { ShaderValidationTest } from './shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kScalarType = ['i32', 'f32', 'u32', 'bool'] as const;
type ScalarType = 'i32' | 'f32' | 'u32' | 'bool';

const kContainerTypes = [
  undefined,
  'vec2',
  'vec3',
  'vec4',
  'mat2x2',
  'mat2x3',
  'mat2x4',
  'mat3x2',
  'mat3x3',
  'mat3x4',
  'mat4x2',
  'mat4x3',
  'mat4x4',
  'array',
] as const;
type ContainerType =
  | undefined
  | 'vec2'
  | 'vec3'
  | 'vec4'
  | 'mat2x2'
  | 'mat2x3'
  | 'mat2x4'
  | 'mat3x2'
  | 'mat3x3'
  | 'mat3x4'
  | 'mat4x2'
  | 'mat4x3'
  | 'mat4x4'
  | 'array';

function getType(scalarType: ScalarType, containerType: ContainerType) {
  let type = '';
  switch (containerType) {
    case undefined: {
      type = scalarType;
      break;
    }
    case 'array': {
      // TODO(sarahM0): 12 is a random number here. find a solution to replace it.
      type = `array<${scalarType}, 12>`;
      break;
    }
    default: {
      type = `${containerType}<${scalarType}>`;
      break;
    }
  }
  return type;
}

g.test('initializer_type')
  .desc(
    `
  If present, the initializer's type must match the store type of the variable.
  Testing scalars, vectors, and matrices of every dimension and type.
  TODO: add test for: structs - arrays of vectors and matrices - arrays of different length
`
  )
  .params(u =>
    u
      .combine('variableOrConstant', ['var', 'const'])
      .combine('lhsContainerType', kContainerTypes)
      .combine('lhsScalarType', kScalarType)
      .combine('rhsContainerType', kContainerTypes)
      .combine('rhsScalarType', kScalarType)
  )
  .fn(t => {
    const {
      variableOrConstant,
      lhsContainerType,
      lhsScalarType,
      rhsContainerType,
      rhsScalarType,
    } = t.params;

    const lhsType = getType(lhsScalarType, lhsContainerType);
    const rhsType = getType(rhsScalarType, rhsContainerType);

    const code = `
      [[stage(fragment)]]
      fn main() {
        ${variableOrConstant} a : ${lhsType} = ${rhsType}();
      }
    `;

    const expectation = lhsScalarType === rhsScalarType && lhsContainerType === rhsContainerType;
    t.expectCompileResult(expectation, code);
  });

g.test('io_shareable_type')
  .desc(
    `
  The following types are IO-shareable:
  - numeric scalar types
  - numeric vector types
  - Matrix Types
  - Array Types if its element type is IO-shareable, and the array is not runtime-sized
  - Structure Types if all its members are IO-shareable

  As a result these are not IO-shareable:
  - boolean
  - vector of booleans
  - array of booleans
  - matrix of booleans
  - array runtime sized -> cannot be used outside of a struct, so no cts for this
  - struct with bool component
  - struct with runtime array

  Control case: 'private' is used to make sure when only the storage class changes, the shader
  becomes invalid and nothing else is wrong.
  TODO: add test for: struct - struct with bool component - struct with runtime array`
  )
  .params(u =>
    u
      .combine('storageClass', ['in', 'out', 'private'])
      .combine('containerType', kContainerTypes)
      .combine('scalarType', kScalarType)
  )
  .fn(t => {
    const { storageClass, containerType, scalarType } = t.params;
    const type = containerType ? `${containerType}<${scalarType}>` : scalarType;

    let code;
    if (`${storageClass}` === 'in') {
      code = `
        struct MyInputs {
          [[location(0)]] a : ${type};
        };

        [[stage(fragment)]]
        fn main(inputs : MyInputs) {
        }
      `;
    } else if (`${storageClass}` === 'out') {
      code = `
        struct MyOutputs {
          [[location(0)]] a : ${type};
        };

        [[stage(fragment)]]
        fn main() -> MyOutputs {
          return MyOutputs();
        }
      `;
    } else {
      code = `
      var<${storageClass}> a : ${type} = ${type}();

      [[stage(fragment)]]
      fn main() {
      }
      `;
    }

    const expectation = storageClass === 'private' || scalarType !== 'bool';
    t.expectCompileResult(expectation, code);
  });
