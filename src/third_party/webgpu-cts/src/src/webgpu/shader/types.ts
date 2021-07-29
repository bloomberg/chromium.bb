import { keysOf } from '../../common/util/data_tables.js';

/** WGSL plain scalar type names. */
export type ScalarType = 'i32' | 'u32' | 'f32' | 'bool';

/** Info for each plain scalar type. */
export const kScalarTypeInfo = /* prettier-ignore */ {
  'i32':    { layout: { alignment:  4, size:  4 }, supportsAtomics:  true },
  'u32':    { layout: { alignment:  4, size:  4 }, supportsAtomics:  true },
  'f32':    { layout: { alignment:  4, size:  4 }, supportsAtomics: false },
  'bool':   { layout:                   undefined, supportsAtomics: false },
} as const;
/** List of all plain scalar types. */
export const kScalarTypes = keysOf(kScalarTypeInfo);

/** Info for each vecN<> container type. */
export const kVectorContainerTypeInfo = /* prettier-ignore */ {
  'vec2':   { layout: { alignment:  8, size:  8 }, arrayLength: 2 },
  'vec3':   { layout: { alignment: 16, size: 12 }, arrayLength: 3 },
  'vec4':   { layout: { alignment: 16, size: 16 }, arrayLength: 4 },
} as const;
/** List of all vecN<> container types. */
export const kVectorContainerTypes = keysOf(kVectorContainerTypeInfo);

/** Info for each matNxN<> container type. */
export const kMatrixContainerTypeInfo = /* prettier-ignore */ {
  'mat2x2': { layout: { alignment:  8, size: 16 }, arrayLength: 2, innerLength: 2 },
  'mat3x2': { layout: { alignment:  8, size: 24 }, arrayLength: 3, innerLength: 2 },
  'mat4x2': { layout: { alignment:  8, size: 32 }, arrayLength: 4, innerLength: 2 },
  'mat2x3': { layout: { alignment: 16, size: 32 }, arrayLength: 2, innerLength: 3 },
  'mat3x3': { layout: { alignment: 16, size: 48 }, arrayLength: 3, innerLength: 3 },
  'mat4x3': { layout: { alignment: 16, size: 64 }, arrayLength: 4, innerLength: 3 },
  'mat2x4': { layout: { alignment: 16, size: 32 }, arrayLength: 2, innerLength: 4 },
  'mat3x4': { layout: { alignment: 16, size: 48 }, arrayLength: 3, innerLength: 4 },
  'mat4x4': { layout: { alignment: 16, size: 64 }, arrayLength: 4, innerLength: 4 },
} as const;
/** List of all matNxN<> container types. */
export const kMatrixContainerTypes = keysOf(kMatrixContainerTypeInfo);
