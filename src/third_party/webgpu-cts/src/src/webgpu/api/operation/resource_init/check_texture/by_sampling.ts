import { assert, unreachable } from '../../../../../common/util/util.js';
import { EncodableTextureFormat, kTextureFormatInfo } from '../../../../capability_info.js';
import { virtualMipSize } from '../../../../util/texture/base.js';
import {
  kTexelRepresentationInfo,
  getSingleDataType,
  getComponentReadbackTraits,
} from '../../../../util/texture/texel_data.js';
import { CheckContents } from '../texture_zero.spec.js';

export const checkContentsBySampling: CheckContents = (
  t,
  params,
  texture,
  state,
  subresourceRange
) => {
  assert(params.dimension !== '1d');
  assert(params.format in kTextureFormatInfo);
  const format = params.format as EncodableTextureFormat;
  const rep = kTexelRepresentationInfo[format];

  for (const { level, layers } of subresourceRange.mipLevels()) {
    const [width, height, depth] = virtualMipSize(
      params.dimension,
      [t.textureWidth, t.textureHeight, t.textureDepth],
      level
    );

    const { ReadbackTypedArray, shaderType } = getComponentReadbackTraits(
      getSingleDataType(format)
    );

    const componentOrder = rep.componentOrder;
    const componentCount = componentOrder.length;

    // For single-component textures, generates .r
    // For multi-component textures, generates ex.)
    //  .rgba[i], .bgra[i], .rgb[i]
    const indexExpression =
      componentCount === 1
        ? componentOrder[0].toLowerCase()
        : componentOrder.map(c => c.toLowerCase()).join('') + '[i]';

    const _xd = '_' + params.dimension;
    const _multisampled = params.sampleCount > 1 ? '_multisampled' : '';
    const texelIndexExpresion =
      params.dimension === '2d'
        ? 'vec2<i32>(GlobalInvocationID.xy)'
        : params.dimension === '3d'
        ? 'vec3<i32>(GlobalInvocationID.xyz)'
        : unreachable();
    const computePipeline = t.device.createComputePipeline({
      compute: {
        entryPoint: 'main',
        module: t.device.createShaderModule({
          code: `
            [[block]] struct Constants {
              level : i32;
            };

            [[group(0), binding(0)]] var<uniform> constants : Constants;
            [[group(0), binding(1)]] var myTexture : texture${_multisampled}${_xd}<${shaderType}>;

            [[block]] struct Result {
              values : [[stride(4)]] array<${shaderType}>;
            };
            [[group(0), binding(3)]] var<storage, read_write> result : Result;

            [[stage(compute), workgroup_size(1)]]
            fn main([[builtin(global_invocation_id)]] GlobalInvocationID : vec3<u32>) {
              let flatIndex : u32 = ${componentCount}u * (
                ${width}u * ${height}u * GlobalInvocationID.z +
                ${width}u * GlobalInvocationID.y +
                GlobalInvocationID.x
              );
              let texel : vec4<${shaderType}> = textureLoad(
                myTexture, ${texelIndexExpresion}, constants.level);

              for (var i : u32 = 0u; i < ${componentCount}u; i = i + 1u) {
                result.values[flatIndex + i] = texel.${indexExpression};
              }
            }`,
        }),
      },
    });

    for (const layer of layers) {
      const ubo = t.device.createBuffer({
        mappedAtCreation: true,
        size: 4,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      new Int32Array(ubo.getMappedRange(), 0, 1)[0] = level;
      ubo.unmap();

      const byteLength =
        width * height * depth * ReadbackTypedArray.BYTES_PER_ELEMENT * rep.componentOrder.length;
      const resultBuffer = t.device.createBuffer({
        size: byteLength,
        usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
      });
      t.trackForCleanup(resultBuffer);

      const bindGroup = t.device.createBindGroup({
        layout: computePipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: { buffer: ubo },
          },
          {
            binding: 1,
            resource: texture.createView({
              baseArrayLayer: layer,
              arrayLayerCount: 1,
            }),
          },
          {
            binding: 3,
            resource: {
              buffer: resultBuffer,
            },
          },
        ],
      });

      const commandEncoder = t.device.createCommandEncoder();
      const pass = commandEncoder.beginComputePass();
      pass.setPipeline(computePipeline);
      pass.setBindGroup(0, bindGroup);
      pass.dispatch(width, height, depth);
      pass.endPass();
      t.queue.submit([commandEncoder.finish()]);
      ubo.destroy();

      const expectedValues = new ReadbackTypedArray(new ArrayBuffer(byteLength));
      const expectedState = t.stateToTexelComponents[state];
      let i = 0;
      for (let d = 0; d < depth; ++d) {
        for (let h = 0; h < height; ++h) {
          for (let w = 0; w < width; ++w) {
            for (const c of rep.componentOrder) {
              const value = expectedState[c];
              assert(value !== undefined);
              expectedValues[i++] = value;
            }
          }
        }
      }
      t.expectGPUBufferValuesEqual(resultBuffer, expectedValues);
    }
  }
};
