export const description = `
Basic command buffer compute tests.
`;

import { params, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('memcpy').fn(async t => {
  const data = new Uint32Array([0x01020304]);

  const src = t.device.createBuffer({
    mappedAtCreation: true,
    size: 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE,
  });
  new Uint32Array(src.getMappedRange()).set(data);
  src.unmap();

  const dst = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE,
  });

  const pipeline = t.device.createComputePipeline({
    compute: {
      module: t.device.createShaderModule({
        code: `
          [[block]] struct Data {
              [[offset(0)]] value : u32;
          };

          [[group(0), binding(0)]] var<storage> src : [[access(read)]] Data;
          [[group(0), binding(1)]] var<storage> dst : [[access(read_write)]] Data;

          [[stage(compute)]] fn main() {
            dst.value = src.value;
            return;
          }
        `,
      }),
      entryPoint: 'main',
    },
  });

  const bg = t.device.createBindGroup({
    entries: [
      { binding: 0, resource: { buffer: src, offset: 0, size: 4 } },
      { binding: 1, resource: { buffer: dst, offset: 0, size: 4 } },
    ],
    layout: pipeline.getBindGroupLayout(0),
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bg);
  pass.dispatch(1);
  pass.endPass();
  t.device.queue.submit([encoder.finish()]);

  t.expectContents(dst, data);
});

g.test('large_dispatch')
  .desc(
    `
TODO: add query for the maximum dispatch size and test closer to those limits.

Test reasonably-sized large dispatches (see also stress tests).
`
  )
  .cases(
    params()
      .combine(
        // Reasonably-sized powers of two, and some stranger larger sizes.
        poptions('dispatchSize', [256, 512, 1024, 2048, 315, 628, 1053, 2179] as const)
      )
      .combine(
        // Test some reasonable workgroup sizes.
        poptions('workgroupSize', [1, 2, 4, 8, 16, 32, 64] as const)
      )
  )
  .subcases(() =>
    // 0 == x axis; 1 == y axis; 2 == z axis.
    poptions('largeDimension', [0, 1, 2] as const)
  )
  .fn(async t => {
    // The output storage buffer is filled with this value.
    const val = 0x01020304;
    const badVal = 0xbaadf00d;
    const data = new Uint32Array([val]);

    const wgSize = t.params.workgroupSize;
    const bufferSize = Uint32Array.BYTES_PER_ELEMENT * t.params.dispatchSize * wgSize;
    const dst = t.device.createBuffer({
      size: bufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE,
    });

    // Only use one large dimension and workgroup size in the dispatch
    // call to keep the size of the test reasonable.
    const dims = [1, 1, 1];
    dims[t.params.largeDimension] = t.params.dispatchSize;
    const wgSizes = [1, 1, 1];
    wgSizes[t.params.largeDimension] = t.params.workgroupSize;
    const pipeline = t.device.createComputePipeline({
      compute: {
        module: t.device.createShaderModule({
          code: `
            [[block]] struct OutputBuffer {
              value : array<u32>;
            };

            [[group(0), binding(0)]] var<storage> dst : [[access(read_write)]] OutputBuffer;

            [[stage(compute), workgroup_size(${wgSizes[0]}, ${wgSizes[1]}, ${wgSizes[2]})]]
            fn main(
              [[builtin(global_invocation_id)]] GlobalInvocationID : vec3<u32>
            ) {
              var xExtent : u32 = ${dims[0]}u * ${wgSizes[0]}u;
              var yExtent : u32 = ${dims[1]}u * ${wgSizes[1]}u;
              var zExtent : u32 = ${dims[2]}u * ${wgSizes[2]}u;
              var index : u32 = (
                GlobalInvocationID.z * xExtent * yExtent +
                GlobalInvocationID.y * xExtent +
                GlobalInvocationID.x);
              var val : u32 = ${val}u;
              // Trivial error checking in the indexing and invocation.
              if (GlobalInvocationID.x > xExtent ||
                  GlobalInvocationID.y > yExtent ||
                  GlobalInvocationID.z > zExtent) {
                val = ${badVal}u;
              }
              dst.value[index] = val;
            }
          `,
        }),
        entryPoint: 'main',
      },
    });

    const bg = t.device.createBindGroup({
      entries: [{ binding: 0, resource: { buffer: dst, offset: 0, size: bufferSize } }],
      layout: pipeline.getBindGroupLayout(0),
    });

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bg);
    pass.dispatch(dims[0], dims[1], dims[2]);
    pass.endPass();
    t.device.queue.submit([encoder.finish()]);

    t.expectSingleValueContents(dst, data, bufferSize);

    dst.destroy();
  });
