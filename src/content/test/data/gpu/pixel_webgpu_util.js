// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const webGpuUtils = function() {
  const outputFormat = 'bgra8unorm';

  const wgslShaders = {
    vertex: `
var<private> quadPos : array<vec4<f32>, 4> = array<vec4<f32>, 4>(
    vec4<f32>(-1.0,  1.0, 0.0, 1.0),
    vec4<f32>(-1.0, -1.0, 0.0, 1.0),
    vec4<f32>( 1.0,  1.0, 0.0, 1.0),
    vec4<f32>( 1.0, -1.0, 0.0, 1.0));

var<private> quadUV : array<vec2<f32>, 4> = array<vec2<f32>, 4>(
    vec2<f32>(0.0, 0.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(1.0, 1.0));

struct VertexOutput {
  [[builtin(position)]] Position : vec4<f32>;
  [[location(0)]] fragUV : vec2<f32>;
};

[[stage(vertex)]]
fn main([[builtin(vertex_index)]] VertexIndex : u32) -> VertexOutput {
  var output: VertexOutput;
  output.Position = quadPos[VertexIndex];
  output.fragUV = quadUV[VertexIndex];
  return output;
}
`,

    fragmentBlit: `
[[binding(0), group(0)]] var mySampler: sampler;
[[binding(1), group(0)]] var myTexture: texture_2d<f32>;

[[stage(fragment)]]
fn main([[location(0)]] fragUV : vec2<f32>) -> [[location(0)]] vec4<f32> {
  return textureSample(myTexture, mySampler, fragUV);
}
`,

    fragmentClear: `
[[stage(fragment)]]
fn main([[location(0)]] fragUV : vec2<f32>) -> [[location(0)]] vec4<f32> {
  return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
`,
  };

  return {
    init: async function(gpuCanvas) {
      const adapter = navigator.gpu && await navigator.gpu.requestAdapter();
      if (!adapter) {
        console.error('navigator.gpu && navigator.gpu.requestAdapter failed');
        return null;
      }

      const device = await adapter.requestDevice();
      if (!device) {
        console.error('adapter.requestDevice() failed');
        return null;
      }

      const context = gpuCanvas.getContext('gpupresent');
      if (!context) {
        console.error('getContext(gpupresent) failed');
        return null;
      }

      context.configure({
        device: device,
        format: outputFormat,
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      });

      return [device, context];
    },

    importCanvasTest: function(device, context, sourceCanvas) {
      const blitPipeline = device.createRenderPipeline({
        vertex: {
          module: device.createShaderModule({
            code: wgslShaders.vertex,
          }),
          entryPoint: 'main',
        },
        fragment: {
          module: device.createShaderModule({
            code: wgslShaders.fragmentBlit,
          }),
          entryPoint: 'main',
          targets: [
            {
              format: outputFormat,
            },
          ],
        },
        primitive: {
          topology: 'triangle-strip',
          stripIndexFormat: 'uint16',
        },
      });

      const sampler = device.createSampler({
        magFilter: 'linear',
        minFilter: 'linear',
      });

      const texture = device.experimentalImportTexture(
          sourceCanvas, GPUTextureUsage.SAMPLED);

      const bindGroup = device.createBindGroup({
        layout: blitPipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: sampler,
          },
          {
            binding: 1,
            resource: texture.createView(),
          },
        ],
      });

      const renderPassDescriptor = {
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            loadValue: {r: 0.0, g: 0.0, b: 0.0, a: 1.0},
          },
        ],
      };

      const commandEncoder = device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(blitPipeline);
      passEncoder.setBindGroup(0, bindGroup);
      passEncoder.draw(4, 1, 0, 0);
      passEncoder.endPass();

      device.queue.submit([commandEncoder.finish()]);
    },

    fourColorsTest: function(device, context, width, height) {
      const clearPipeline = device.createRenderPipeline({
        vertex: {
          module: device.createShaderModule({
            code: wgslShaders.vertex,
          }),
          entryPoint: 'main',
        },
        fragment: {
          module: device.createShaderModule({
            code: wgslShaders.fragmentClear,
          }),
          entryPoint: 'main',
          targets: [{
            format: outputFormat,
            blend: {
              color: {
                srcFactor: 'constant',
                dstFactor: 'zero'
              },
              alpha: {
                srcFactor: 'constant',
                dstFactor: 'zero'
              },
            }
          }],
        },
        primitive: {
          topology: 'triangle-strip',
          stripIndexFormat: 'uint16',
        },
      });

      const renderPassDescriptor = {
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            loadValue: {r: 0.0, g: 0.0, b: 0.0, a: 1.0},
          },
        ],
      };

      const commandEncoder = device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(clearPipeline);

      passEncoder.setBlendConstant([0.0, 1.0, 0.0, 1.0]);
      passEncoder.setScissorRect(0, 0, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.setBlendConstant([1.0, 0.0, 0.0, 1.0]);
      passEncoder.setScissorRect(0, height / 2, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.setBlendConstant([1.0, 1.0, 0.0, 1.0]);
      passEncoder.setScissorRect(width / 2, height / 2, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.setBlendConstant([0.0, 0.0, 1.0, 1.0]);
      passEncoder.setScissorRect(width / 2, 0, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.endPass();
      device.queue.submit([commandEncoder.finish()]);
    },
  };
}();
