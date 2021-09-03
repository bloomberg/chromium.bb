/* eslint-disable no-sparse-arrays */
import {
  assertTypeTrue,
  ResolveType,
  TypeEqual,
  ZipKeysWithValues,
} from '../common/framework/util/types.js';
import { assert, unreachable } from '../common/framework/util/util.js';

import { GPUConst } from './constants.js';

type valueof<K> = K[keyof K];

function keysOf<T extends string>(obj: { [k in T]: unknown }): readonly T[] {
  return (Object.keys(obj) as unknown[]) as T[];
}

function numericKeysOf<T>(obj: object): readonly T[] {
  return (Object.keys(obj).map(n => Number(n)) as unknown[]) as T[];
}

/**
 * Creates an info lookup object from a more nicely-formatted table. See below for examples.
 *
 * Note: Using `as const` on the arguments to this function is necessary to infer the correct type.
 */
function makeTable<
  Members extends readonly string[],
  Defaults extends readonly unknown[],
  Table extends { readonly [k: string]: readonly unknown[] }
>(
  members: Members,
  defaults: Defaults,
  table: Table
): {
  readonly [k in keyof Table]: ResolveType<ZipKeysWithValues<Members, Table[k], Defaults>>;
} {
  const result: { [k: string]: { [m: string]: unknown } } = {};
  for (const [k, v] of Object.entries<readonly unknown[]>(table)) {
    const item: { [m: string]: unknown } = {};
    for (let i = 0; i < members.length; ++i) {
      item[members[i]] = v[i] ?? defaults[i];
    }
    result[k] = item;
  }
  /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  return result as any;
}

// Queries

export const kMaxQueryCount = 8192;
export const kQueryTypeInfo: {
  readonly [k in GPUQueryType]: {
    readonly feature: GPUFeatureName | undefined;
  };
} = /* prettier-ignore */ {
  // Occlusion query does not require any features.
  'occlusion':           { feature:  undefined },
  'pipeline-statistics': { feature: 'pipeline-statistics-query' },
  'timestamp':           { feature: 'timestamp-query' },
};
export const kQueryTypes = keysOf(kQueryTypeInfo);

// Buffers

export const kBufferSizeAlignment = 4;

export const kBufferUsageInfo: {
  readonly [k in valueof<typeof GPUConst.BufferUsage>]: {};
} = /* prettier-ignore */ {
  [GPUConst.BufferUsage.MAP_READ]:      {},
  [GPUConst.BufferUsage.MAP_WRITE]:     {},
  [GPUConst.BufferUsage.COPY_SRC]:      {},
  [GPUConst.BufferUsage.COPY_DST]:      {},
  [GPUConst.BufferUsage.INDEX]:         {},
  [GPUConst.BufferUsage.VERTEX]:        {},
  [GPUConst.BufferUsage.UNIFORM]:       {},
  [GPUConst.BufferUsage.STORAGE]:       {},
  [GPUConst.BufferUsage.INDIRECT]:      {},
  [GPUConst.BufferUsage.QUERY_RESOLVE]: {},
};
export const kBufferUsages = numericKeysOf<GPUBufferUsageFlags>(kBufferUsageInfo);

// Textures

// Note that we repeat the header multiple times in order to make it easier to read.
export const kRegularTextureFormatInfo = /* prettier-ignore */ makeTable(
                           ['renderable', 'multisample', 'color', 'depth', 'stencil', 'storage', 'copySrc', 'copyDst', 'bytesPerBlock', 'blockWidth', 'blockHeight',                'feature'] as const,
                           [            ,              ,    true,   false,     false,          ,      true,      true,                ,            1,             1,                         ] as const, {
  // 8-bit formats
  'r8unorm':               [        true,          true,        ,        ,          ,     false,          ,          ,               1],
  'r8snorm':               [       false,         false,        ,        ,          ,     false,          ,          ,               1],
  'r8uint':                [        true,          true,        ,        ,          ,     false,          ,          ,               1],
  'r8sint':                [        true,          true,        ,        ,          ,     false,          ,          ,               1],
  // 16-bit formats
  'r16uint':               [        true,          true,        ,        ,          ,     false,          ,          ,               2],
  'r16sint':               [        true,          true,        ,        ,          ,     false,          ,          ,               2],
  'r16float':              [        true,          true,        ,        ,          ,     false,          ,          ,               2],
  'rg8unorm':              [        true,          true,        ,        ,          ,     false,          ,          ,               2],
  'rg8snorm':              [       false,         false,        ,        ,          ,     false,          ,          ,               2],
  'rg8uint':               [        true,          true,        ,        ,          ,     false,          ,          ,               2],
  'rg8sint':               [        true,          true,        ,        ,          ,     false,          ,          ,               2],
  // 32-bit formats
  'r32uint':               [        true,          true,        ,        ,          ,      true,          ,          ,               4],
  'r32sint':               [        true,          true,        ,        ,          ,      true,          ,          ,               4],
  'r32float':              [        true,          true,        ,        ,          ,      true,          ,          ,               4],
  'rg16uint':              [        true,          true,        ,        ,          ,     false,          ,          ,               4],
  'rg16sint':              [        true,          true,        ,        ,          ,     false,          ,          ,               4],
  'rg16float':             [        true,          true,        ,        ,          ,     false,          ,          ,               4],
  'rgba8unorm':            [        true,          true,        ,        ,          ,      true,          ,          ,               4],
  'rgba8unorm-srgb':       [        true,          true,        ,        ,          ,     false,          ,          ,               4],
  'rgba8snorm':            [       false,         false,        ,        ,          ,      true,          ,          ,               4],
  'rgba8uint':             [        true,          true,        ,        ,          ,      true,          ,          ,               4],
  'rgba8sint':             [        true,          true,        ,        ,          ,      true,          ,          ,               4],
  'bgra8unorm':            [        true,          true,        ,        ,          ,     false,          ,          ,               4],
  'bgra8unorm-srgb':       [        true,          true,        ,        ,          ,     false,          ,          ,               4],
  // Packed 32-bit formats
  'rgb10a2unorm':          [        true,          true,        ,        ,          ,     false,          ,          ,               4],
  'rg11b10ufloat':         [       false,         false,        ,        ,          ,     false,          ,          ,               4],
  'rgb9e5ufloat':          [       false,         false,        ,        ,          ,     false,          ,          ,               4],
  // 64-bit formats
  'rg32uint':              [        true,          true,        ,        ,          ,      true,          ,          ,               8],
  'rg32sint':              [        true,          true,        ,        ,          ,      true,          ,          ,               8],
  'rg32float':             [        true,          true,        ,        ,          ,      true,          ,          ,               8],
  'rgba16uint':            [        true,          true,        ,        ,          ,      true,          ,          ,               8],
  'rgba16sint':            [        true,          true,        ,        ,          ,      true,          ,          ,               8],
  'rgba16float':           [        true,          true,        ,        ,          ,      true,          ,          ,               8],
  // 128-bit formats
  'rgba32uint':            [        true,          true,        ,        ,          ,      true,          ,          ,              16],
  'rgba32sint':            [        true,          true,        ,        ,          ,      true,          ,          ,              16],
  'rgba32float':           [        true,          true,        ,        ,          ,      true,          ,          ,              16],
} as const);
/* prettier-ignore */
const kTexFmtInfoHeader =  ['renderable', 'multisample', 'color', 'depth', 'stencil', 'storage', 'copySrc', 'copyDst', 'bytesPerBlock', 'blockWidth', 'blockHeight',                'feature'] as const;
export const kSizedDepthStencilFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [        true,          true,   false,        ,          ,     false,     false,     false,                ,            1,             1,                         ] as const, {
  'depth32float':          [            ,              ,        ,    true,     false,          ,          ,          ,               4],
  'depth16unorm':          [            ,              ,        ,    true,     false,          ,          ,          ,               2],
  'stencil8':              [            ,              ,        ,   false,      true,          ,          ,          ,               1],
} as const);
export const kUnsizedDepthStencilFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [        true,          true,   false,        ,          ,     false,     false,     false,       undefined,            1,             1,                         ] as const, {
  'depth24plus':           [            ,              ,        ,    true,     false,          ,          ,          ],
  'depth24plus-stencil8':  [            ,              ,        ,    true,      true,          ,          ,          ],
  // bytesPerBlock only makes sense on a per-aspect basis. But this table can't express that. So we put depth24unorm-stencil8 and depth32float-stencil8 to be unsized formats for now.
  'depth24unorm-stencil8': [            ,              ,        ,    true,      true,          ,          ,          ,                ,             ,              ,  'depth24unorm-stencil8'],
  'depth32float-stencil8': [            ,              ,        ,    true,      true,          ,          ,          ,                ,             ,              ,  'depth32float-stencil8'],
} as const);
export const kCompressedTextureFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [       false,         false,    true,   false,     false,     false,      true,      true,                ,            4,             4,                         ] as const, {
  'bc1-rgba-unorm':        [            ,              ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc1-rgba-unorm-srgb':   [            ,              ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc2-rgba-unorm':        [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc2-rgba-unorm-srgb':   [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc3-rgba-unorm':        [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc3-rgba-unorm-srgb':   [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc4-r-unorm':           [            ,              ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc4-r-snorm':           [            ,              ,        ,        ,          ,          ,          ,          ,               8,            4,             4, 'texture-compression-bc'],
  'bc5-rg-unorm':          [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc5-rg-snorm':          [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc6h-rgb-ufloat':       [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc6h-rgb-float':        [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc7-rgba-unorm':        [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
  'bc7-rgba-unorm-srgb':   [            ,              ,        ,        ,          ,          ,          ,          ,              16,            4,             4, 'texture-compression-bc'],
} as const);

export type RegularTextureFormat = keyof typeof kRegularTextureFormatInfo;
export type SizedDepthStencilFormat = keyof typeof kSizedDepthStencilFormatInfo;
export type UnsizedDepthStencilFormat = keyof typeof kUnsizedDepthStencilFormatInfo;
export type CompressedTextureFormat = keyof typeof kCompressedTextureFormatInfo;

export const kRegularTextureFormats = keysOf(kRegularTextureFormatInfo);
export const kSizedDepthStencilFormats = keysOf(kSizedDepthStencilFormatInfo);
export const kUnsizedDepthStencilFormats = keysOf(kUnsizedDepthStencilFormatInfo);
export const kCompressedTextureFormats = keysOf(kCompressedTextureFormatInfo);

export const kColorTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kCompressedTextureFormatInfo,
} as const;
export type ColorTextureFormat = keyof typeof kColorTextureFormatInfo;
export const kColorTextureFormats = keysOf(kColorTextureFormatInfo);

export const kEncodableTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
} as const;
export type EncodableTextureFormat = keyof typeof kEncodableTextureFormatInfo;
export const kEncodableTextureFormats = keysOf(kEncodableTextureFormatInfo);

export const kSizedTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
  ...kCompressedTextureFormatInfo,
} as const;
export type SizedTextureFormat = keyof typeof kSizedTextureFormatInfo;
export const kSizedTextureFormats = keysOf(kSizedTextureFormatInfo);

export const kDepthStencilFormatInfo = {
  ...kSizedDepthStencilFormatInfo,
  ...kUnsizedDepthStencilFormatInfo,
} as const;
export type DepthStencilFormat = keyof typeof kDepthStencilFormatInfo;
export const kDepthStencilFormats = keysOf(kDepthStencilFormatInfo);

export const kUncompressedTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
  ...kUnsizedDepthStencilFormatInfo,
} as const;
export type UncompressedTextureFormat = keyof typeof kUncompressedTextureFormatInfo;
export const kUncompressedTextureFormats = keysOf(kUncompressedTextureFormatInfo);

export const kAllTextureFormatInfo = {
  ...kUncompressedTextureFormatInfo,
  ...kCompressedTextureFormatInfo,
} as const;
export const kAllTextureFormats = keysOf(kAllTextureFormatInfo);
// Assert every GPUTextureFormat is covered by one of the tables.
((x: { readonly [k in GPUTextureFormat]: {} }) => x)(kAllTextureFormatInfo);

export const kTextureDimensionInfo: {
  readonly [k in GPUTextureDimension]: {
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  '1d': {},
  '2d': {},
  '3d': {},
};
export const kTextureDimensions = keysOf(kTextureDimensionInfo);

export const kTextureAspectInfo: {
  readonly [k in GPUTextureAspect]: {
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'all': {},
  'depth-only': {},
  'stencil-only': {},
};
export const kTextureAspects = keysOf(kTextureAspectInfo);

const kDepthStencilFormatCapabilityInBufferTextureCopy = {
  // kUnsizedDepthStencilFormats
  depth24plus: {
    CopyB2T: [],
    CopyT2B: [],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': -1 },
  },
  'depth24plus-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },

  // kSizedDepthStencilFormats
  depth16unorm: {
    CopyB2T: ['all', 'depth-only'],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 2, 'stencil-only': -1 },
  },
  depth32float: {
    CopyB2T: [],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': -1 },
  },
  'depth24unorm-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['depth-only', 'stencil-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': 1 },
  },
  'depth32float-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['depth-only', 'stencil-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': 1 },
  },
  stencil8: {
    CopyB2T: ['all', 'stencil-only'],
    CopyT2B: ['all', 'stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },
} as const;

export function depthStencilBufferTextureCopySupported(
  type: 'CopyB2T' | 'CopyT2B',
  format: DepthStencilFormat,
  aspect: GPUTextureAspect
): boolean {
  const supportedAspects: readonly GPUTextureAspect[] =
    kDepthStencilFormatCapabilityInBufferTextureCopy[format][type];
  return supportedAspects.includes(aspect);
}

export function depthStencilFormatAspectSize(
  format: DepthStencilFormat,
  aspect: 'depth-only' | 'stencil-only'
) {
  const texelAspectSize =
    kDepthStencilFormatCapabilityInBufferTextureCopy[format].texelAspectSize[aspect];
  assert(texelAspectSize > 0);
  return texelAspectSize;
}

export function textureDimensionAndFormatCompatible(
  dimension: undefined | GPUTextureDimension,
  format: GPUTextureFormat
): boolean {
  const info = kAllTextureFormatInfo[format];
  return !(
    (dimension === '1d' || dimension === '3d') &&
    (info.blockWidth > 1 || info.depth || info.stencil)
  );
}

export const kTextureUsageInfo: {
  readonly [k in valueof<typeof GPUConst.TextureUsage>]: {};
} = {
  [GPUConst.TextureUsage.COPY_SRC]: {},
  [GPUConst.TextureUsage.COPY_DST]: {},
  [GPUConst.TextureUsage.SAMPLED]: {},
  [GPUConst.TextureUsage.STORAGE]: {},
  [GPUConst.TextureUsage.RENDER_ATTACHMENT]: {},
};
export const kTextureUsages = numericKeysOf<GPUTextureUsageFlags>(kTextureUsageInfo);

// Texture View

export const kTextureViewDimensionInfo: {
  readonly [k in GPUTextureViewDimension]: {
    readonly storage: boolean;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  '1d':         { storage: true  },
  '2d':         { storage: true  },
  '2d-array':   { storage: true  },
  'cube':       { storage: false },
  'cube-array': { storage: false },
  '3d':         { storage: true  },
};
export const kTextureViewDimensions = keysOf(kTextureViewDimensionInfo);

// Vertex formats

export const kVertexFormatInfo = /* prettier-ignore */ makeTable(
               ['bytesPerComponent',  'type', 'componentCount'] as const,
               [                   ,        ,                 ] as const, {
  // 8 bit components
  'uint8x2':   [                  1,  'uint',                2],
  'uint8x4':   [                  1,  'uint',                4],
  'sint8x2':   [                  1,  'sint',                2],
  'sint8x4':   [                  1,  'sint',                4],
  'unorm8x2':  [                  1, 'unorm',                2],
  'unorm8x4':  [                  1, 'unorm',                4],
  'snorm8x2':  [                  1, 'snorm',                2],
  'snorm8x4':  [                  1, 'snorm',                4],
  // 16 bit components
  'uint16x2':  [                  2,  'uint',                2],
  'uint16x4':  [                  2,  'uint',                4],
  'sint16x2':  [                  2,  'sint',                2],
  'sint16x4':  [                  2,  'sint',                4],
  'unorm16x2': [                  2, 'unorm',                2],
  'unorm16x4': [                  2, 'unorm',                4],
  'snorm16x2': [                  2, 'snorm',                2],
  'snorm16x4': [                  2, 'snorm',                4],
  'float16x2': [                  2, 'float',                2],
  'float16x4': [                  2, 'float',                4],
  // 32 bit components
  'float32':   [                  4, 'float',                1],
  'float32x2': [                  4, 'float',                2],
  'float32x3': [                  4, 'float',                3],
  'float32x4': [                  4, 'float',                4],
  'uint32':    [                  4,  'uint',                1],
  'uint32x2':  [                  4,  'uint',                2],
  'uint32x3':  [                  4,  'uint',                3],
  'uint32x4':  [                  4,  'uint',                4],
  'sint32':    [                  4,  'sint',                1],
  'sint32x2':  [                  4,  'sint',                2],
  'sint32x3':  [                  4,  'sint',                3],
  'sint32x4':  [                  4,  'sint',                4]
} as const);
export type VertexFormat = keyof typeof kVertexFormatInfo;
export const kVertexFormats = keysOf(kVertexFormatInfo);

// Typedefs for bindings

export type PerStageBindingLimitClass =
  | 'uniformBuf'
  | 'storageBuf'
  | 'sampler'
  | 'sampledTex'
  | 'storageTex';
export type PerPipelineBindingLimitClass = PerStageBindingLimitClass;

type ValidBindableResource =
  | 'uniformBuf'
  | 'storageBuf'
  | 'filtSamp'
  | 'nonFiltSamp'
  | 'compareSamp'
  | 'sampledTex'
  | 'sampledTexMS'
  | 'storageTex';
type ErrorBindableResource = 'errorBuf' | 'errorSamp' | 'errorTex';

export type BindableResource = ValidBindableResource | ErrorBindableResource;
export const kBindableResources = [
  'uniformBuf',
  'storageBuf',
  'filtSamp',
  'nonFiltSamp',
  'compareSamp',
  'sampledTex',
  'sampledTexMS',
  'storageTex',
  'errorBuf',
  'errorSamp',
  'errorTex',
] as const;
assertTypeTrue<TypeEqual<BindableResource, typeof kBindableResources[number]>>();

// Bindings

// Dynamic buffer offsets require offset to be divisible by 256
export const kMinDynamicBufferOffsetAlignment = 256;

export const kMaxBindingsPerBindGroup = 16;

export const kPerStageBindingLimits: {
  readonly [k in PerStageBindingLimitClass]: {
    readonly class: k;
    readonly max: number;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'uniformBuf': { class: 'uniformBuf', max: 12, },
  'storageBuf': { class: 'storageBuf', max:  4, },
  'sampler':    { class: 'sampler',    max: 16, },
  'sampledTex': { class: 'sampledTex', max: 16, },
  'storageTex': { class: 'storageTex', max:  4, },
};

export const kPerPipelineBindingLimits: {
  readonly [k in PerPipelineBindingLimitClass]: {
    readonly class: k;
    readonly maxDynamic: number;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'uniformBuf': { class: 'uniformBuf', maxDynamic: 8, },
  'storageBuf': { class: 'storageBuf', maxDynamic: 4, },
  'sampler':    { class: 'sampler',    maxDynamic: 0, },
  'sampledTex': { class: 'sampledTex', maxDynamic: 0, },
  'storageTex': { class: 'storageTex', maxDynamic: 0, },
};

interface BindingKindInfo {
  readonly resource: ValidBindableResource;
  readonly perStageLimitClass: typeof kPerStageBindingLimits[PerStageBindingLimitClass];
  readonly perPipelineLimitClass: typeof kPerPipelineBindingLimits[PerPipelineBindingLimitClass];
  // Add fields as needed
}

const kBindingKind: {
  readonly [k in ValidBindableResource]: BindingKindInfo;
} = /* prettier-ignore */ {
  uniformBuf:   { resource: 'uniformBuf',   perStageLimitClass: kPerStageBindingLimits.uniformBuf, perPipelineLimitClass: kPerPipelineBindingLimits.uniformBuf, },
  storageBuf:   { resource: 'storageBuf',   perStageLimitClass: kPerStageBindingLimits.storageBuf, perPipelineLimitClass: kPerPipelineBindingLimits.storageBuf, },
  filtSamp:     { resource: 'filtSamp',     perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  nonFiltSamp:  { resource: 'nonFiltSamp',  perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  compareSamp:  { resource: 'compareSamp',  perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  sampledTex:   { resource: 'sampledTex',   perStageLimitClass: kPerStageBindingLimits.sampledTex, perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex, },
  sampledTexMS: { resource: 'sampledTexMS', perStageLimitClass: kPerStageBindingLimits.sampledTex, perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex, },
  storageTex:   { resource: 'storageTex',   perStageLimitClass: kPerStageBindingLimits.storageTex, perPipelineLimitClass: kPerPipelineBindingLimits.storageTex, },
};

// Binding type info

const kValidStagesAll = {
  validStages:
    GPUConst.ShaderStage.VERTEX | GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
} as const;
const kValidStagesStorageWrite = {
  validStages: GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
} as const;

export function bufferBindingTypeInfo(d: GPUBufferBindingLayout) {
  /* prettier-ignore */
  switch (d.type ?? 'uniform') {
    case 'uniform':           return { usage: GPUConst.BufferUsage.UNIFORM, ...kBindingKind.uniformBuf,  ...kValidStagesAll,          };
    case 'storage':           return { usage: GPUConst.BufferUsage.STORAGE, ...kBindingKind.storageBuf,  ...kValidStagesStorageWrite, };
    case 'read-only-storage': return { usage: GPUConst.BufferUsage.STORAGE, ...kBindingKind.storageBuf,  ...kValidStagesAll,          };
  }
}
export const kBufferBindingTypes = ['uniform', 'storage', 'read-only-storage'] as const;
assertTypeTrue<TypeEqual<GPUBufferBindingType, typeof kBufferBindingTypes[number]>>();

export function samplerBindingTypeInfo(d: GPUSamplerBindingLayout) {
  /* prettier-ignore */
  switch (d.type ?? 'filtering') {
    case 'filtering':     return { ...kBindingKind.filtSamp,    ...kValidStagesAll, };
    case 'non-filtering': return { ...kBindingKind.nonFiltSamp, ...kValidStagesAll, };
    case 'comparison':    return { ...kBindingKind.compareSamp, ...kValidStagesAll, };
  }
}
export const kSamplerBindingTypes = ['filtering', 'non-filtering', 'comparison'] as const;
assertTypeTrue<TypeEqual<GPUSamplerBindingType, typeof kSamplerBindingTypes[number]>>();

export function sampledTextureBindingTypeInfo(d: GPUTextureBindingLayout) {
  /* prettier-ignore */
  if (d.multisampled) {
    return { usage: GPUConst.TextureUsage.SAMPLED, ...kBindingKind.sampledTexMS, ...kValidStagesAll, };
  } else {
    return { usage: GPUConst.TextureUsage.SAMPLED, ...kBindingKind.sampledTex,   ...kValidStagesAll, };
  }
}
export const kTextureSampleTypes = [
  'float',
  'unfilterable-float',
  'depth',
  'sint',
  'uint',
] as const;
assertTypeTrue<TypeEqual<GPUTextureSampleType, typeof kTextureSampleTypes[number]>>();

export function storageTextureBindingTypeInfo(d: GPUStorageTextureBindingLayout) {
  /* prettier-ignore */
  switch (d.access) {
    case 'read-only':  return { usage: GPUConst.TextureUsage.STORAGE, ...kBindingKind.storageTex, ...kValidStagesAll,          };
    case 'write-only': return { usage: GPUConst.TextureUsage.STORAGE, ...kBindingKind.storageTex, ...kValidStagesStorageWrite, };
  }
}
export const kStorageTextureAccessValues = ['read-only', 'write-only'] as const;
assertTypeTrue<TypeEqual<GPUStorageTextureAccess, typeof kStorageTextureAccessValues[number]>>();

export type BGLEntry = Omit<GPUBindGroupLayoutEntry, 'binding' | 'visibility'>;
export function texBindingTypeInfo(e: BGLEntry) {
  if (e.texture !== undefined) return sampledTextureBindingTypeInfo(e.texture);
  if (e.storageTexture !== undefined) return storageTextureBindingTypeInfo(e.storageTexture);
  unreachable();
}
export function bindingTypeInfo(e: BGLEntry) {
  if (e.buffer !== undefined) return bufferBindingTypeInfo(e.buffer);
  if (e.texture !== undefined) return sampledTextureBindingTypeInfo(e.texture);
  if (e.sampler !== undefined) return samplerBindingTypeInfo(e.sampler);
  if (e.storageTexture !== undefined) return storageTextureBindingTypeInfo(e.storageTexture);
  unreachable('GPUBindGroupLayoutEntry has no BindingLayout');
}

export function bufferBindingEntries(includeUndefined: boolean): readonly BGLEntry[] {
  return [
    ...(includeUndefined ? [{ buffer: { type: undefined } }] : []),
    { buffer: { type: 'uniform' } },
    { buffer: { type: 'storage' } },
    { buffer: { type: 'read-only-storage' } },
  ] as const;
}
export function samplerBindingEntries(includeUndefined: boolean): readonly BGLEntry[] {
  return [
    ...(includeUndefined ? [{ sampler: { type: undefined } }] : []),
    { sampler: { type: 'comparison' } },
    { sampler: { type: 'filtering' } },
    { sampler: { type: 'non-filtering' } },
  ] as const;
}
export function textureBindingEntries(includeUndefined: boolean): readonly BGLEntry[] {
  return [
    ...(includeUndefined ? [{ texture: { multisampled: undefined } }] : []),
    { texture: { multisampled: false } },
    { texture: { multisampled: true } },
  ] as const;
}
export function storageTextureBindingEntries(format: GPUTextureFormat): readonly BGLEntry[] {
  return [
    { storageTexture: { access: 'read-only', format } },
    { storageTexture: { access: 'write-only', format } },
  ] as const;
}
export function sampledAndStorageBindingEntries(
  includeUndefined: boolean,
  storageTextureFormat: GPUTextureFormat = 'rgba8unorm'
): readonly BGLEntry[] {
  return [
    ...textureBindingEntries(includeUndefined),
    ...storageTextureBindingEntries(storageTextureFormat),
  ] as const;
}
/** Generates all different types of binding entries.
 * Does not generate variants with different:
 * - buffer.hasDynamicOffset
 * - texture.sampleType
 * - texture.viewDimension
 * - storageTexture.viewDimension
 */
export function allBindingEntries(
  includeUndefined: boolean,
  storageTextureFormat: GPUTextureFormat = 'rgba8unorm'
): readonly BGLEntry[] {
  return [
    ...bufferBindingEntries(includeUndefined),
    ...samplerBindingEntries(includeUndefined),
    ...sampledAndStorageBindingEntries(includeUndefined, storageTextureFormat),
  ] as const;
}

// Shader stages

export const kShaderStages: readonly GPUShaderStageFlags[] = [
  GPUConst.ShaderStage.VERTEX,
  GPUConst.ShaderStage.FRAGMENT,
  GPUConst.ShaderStage.COMPUTE,
];
export const kShaderStageCombinations: readonly GPUShaderStageFlags[] = [0, 1, 2, 3, 4, 5, 6, 7];

// TODO: Update with all possible sample counts when defined
// TODO: Switch existing tests to use kTextureSampleCounts
export const kTextureSampleCounts = [1, 4] as const;

// Pipeline limits

// TODO: Update maximum color attachments when defined
export const kMaxColorAttachments = 4;

export const kMaxVertexBuffers = 8;
export const kMaxVertexAttributes = 16;
export const kMaxVertexBufferArrayStride = 2048;
