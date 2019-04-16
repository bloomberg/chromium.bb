export const description = `
copy{Buffer,Texture}To{Buffer,Texture} tests.
`;

import { TestGroup, FixtureCreate } from "../../framework/index.js";
import { GPUTest, makeGPUTestCreate } from "../gpu_test.js";

export const group = new TestGroup();

class CopyTest extends GPUTest {
  public static create: FixtureCreate<CopyTest> = makeGPUTestCreate(CopyTest);
}

group.test("b2b", CopyTest, async (t) => {
  const data = new Uint32Array([0x01020304]);
  const src = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  const dst = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  src.setSubData(0, data);

  const encoder = t.device.createCommandEncoder({})
  encoder.copyBufferToBuffer(src, 0, dst, 0, 4);
  t.device.getQueue().submit([encoder.finish()]);

  await t.expectContents(dst, data);
});

group.test("b2t2b", CopyTest, async (t) => {
  const data = new Uint32Array([0x01020304]);
  const src = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  const dst = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  src.setSubData(0, data);

  const mid = t.device.createTexture({
    size: { width: 1, height: 1, depth: 1 },
    format: "rgba8uint",
    usage: 1 | 2,
  });

  const encoder = t.device.createCommandEncoder({})
  encoder.copyBufferToTexture(
    { buffer: src, rowPitch: 256, imageHeight: 1 },
    { texture: mid, mipLevel: 0, origin: { x: 0, y: 0, z: 0} },
    { width: 1, height: 1, depth: 1 });
  encoder.copyTextureToBuffer(
    { texture: mid, mipLevel: 0, origin: { x: 0, y: 0, z: 0} },
    { buffer: dst, rowPitch: 256, imageHeight: 1 },
    { width: 1, height: 1, depth: 1 });
  t.device.getQueue().submit([encoder.finish()]);

  await t.expectContents(dst, data);
});

group.test("b2t2t2b", CopyTest, async (t) => {
  const data = new Uint32Array([0x01020304]);
  const src = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  const dst = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  src.setSubData(0, data);

  const midDesc: GPUTextureDescriptor = {
    size: { width: 1, height: 1, depth: 1 },
    format: "rgba8uint",
    usage: 1 | 2,
  };
  const mid1 = t.device.createTexture(midDesc);
  const mid2 = t.device.createTexture(midDesc);

  const encoder = t.device.createCommandEncoder({})
  encoder.copyBufferToTexture(
    { buffer: src, rowPitch: 256, imageHeight: 1 },
    { texture: mid1, mipLevel: 0, origin: { x: 0, y: 0, z: 0} },
    { width: 1, height: 1, depth: 1 });
  encoder.copyTextureToTexture(
    { texture: mid1, mipLevel: 0, origin: { x: 0, y: 0, z: 0} },
    { texture: mid2, mipLevel: 0, origin: { x: 0, y: 0, z: 0} },
    { width: 1, height: 1, depth: 1 });
  encoder.copyTextureToBuffer(
    { texture: mid2, mipLevel: 0, origin: { x: 0, y: 0, z: 0} },
    { buffer: dst, rowPitch: 256, imageHeight: 1 },
    { width: 1, height: 1, depth: 1 });
  t.device.getQueue().submit([encoder.finish()]);

  await t.expectContents(dst, data);
});
