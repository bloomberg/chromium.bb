import { unreachable } from '../../common/util/util.js';

const kPlainTypeInfo = {
  i32: {
    suffix: '',
    fractionDigits: 0,
  },
  u32: {
    suffix: 'u',
    fractionDigits: 0,
  },
  f32: {
    suffix: '',
    fractionDigits: 4,
  },
};

/**
 *
 * @param sampleType sampleType of texture format
 * @returns plain type compatible of the sampleType
 */
export function getPlainTypeInfo(sampleType: GPUTextureSampleType): keyof typeof kPlainTypeInfo {
  switch (sampleType) {
    case 'sint':
      return 'i32';
    case 'uint':
      return 'u32';
    case 'float':
    case 'unfilterable-float':
    case 'depth':
      return 'f32';
    default:
      unreachable();
  }
}

/**
 * Build a fragment shader based on output value and types
 * e.g. write to color target 0 a vec4<f32>(1.0, 0.0, 1.0, 1.0) and color target 2 a vec2<u32>(1, 2)
 * outputs: [
 *   {
 *     values: [1, 0, 1, 1],,
 *     plainType: 'f32',
 *     componentCount: 4,
 *   },
 *   null,
 *   {
 *     values: [1, 2],
 *     plainType: 'u32',
 *     componentCount: 2,
 *   },
 * ]
 *
 * return:
 * struct Outputs {
 *     @location(0) o1 : vec4<f32>;
 *     @location(2) o3 : vec2<u32>;
 * }
 * @stage(fragment) fn main() -> Outputs {
 *     return Outputs(vec4<f32>(1.0, 0.0, 1.0, 1.0), vec4<u32>(1, 2));
 * }
 * @param outputs the shader outputs for each location attribute
 * @returns the fragment shader string
 */
export function getFragmentShaderCodeWithOutput(
  outputs: ({
    values: readonly number[];
    plainType: 'i32' | 'u32' | 'f32';
    componentCount: number;
  } | null)[]
): string {
  if (outputs.length === 0) {
    return `
        @stage(fragment) fn main() {
        }`;
  }

  const resultStrings = [] as string[];
  let outputStructString = '';

  for (let i = 0; i < outputs.length; i++) {
    const o = outputs[i];
    if (o === null) {
      continue;
    }

    const plainType = o.plainType;
    const { suffix, fractionDigits } = kPlainTypeInfo[plainType];

    let outputType;
    const v = o.values.map(n => n.toFixed(fractionDigits));
    switch (o.componentCount) {
      case 1:
        outputType = plainType;
        resultStrings.push(`${v[0]}${suffix}`);
        break;
      case 2:
        outputType = `vec2<${plainType}>`;
        resultStrings.push(`${outputType}(${v[0]}${suffix}, ${v[1]}${suffix})`);
        break;
      case 3:
        outputType = `vec3<${plainType}>`;
        resultStrings.push(`${outputType}(${v[0]}${suffix}, ${v[1]}${suffix}, ${v[2]}${suffix})`);
        break;
      case 4:
        outputType = `vec4<${plainType}>`;
        resultStrings.push(
          `${outputType}(${v[0]}${suffix}, ${v[1]}${suffix}, ${v[2]}${suffix}, ${v[3]}${suffix})`
        );
        break;
      default:
        unreachable();
    }

    outputStructString += `@location(${i}) o${i} : ${outputType},\n`;
  }

  return `
    struct Outputs {
      ${outputStructString}
    }

    @stage(fragment) fn main() -> Outputs {
        return Outputs(${resultStrings.join(',')});
    }`;
}
