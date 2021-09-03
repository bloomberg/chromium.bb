export const description = `
Test the operation of buffer mapping, specifically the data contents written via
map-write/mappedAtCreation, and the contents of buffers returned by getMappedRange on
buffers which are mapped-read/mapped-write/mappedAtCreation.

TODO: Test that ranges not written preserve previous contents.
TODO: Test that mapping-for-write again shows the values previously written.
TODO: Some testing (probably minimal) of accessing with different TypedArray/DataView types.
`;

import { params, pbool, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/framework/util/util.js';

import { MappingTest } from './mapping_test.js';

export const g = makeTestGroup(MappingTest);

const kSubcases = [
  { size: 0, range: [] },
  { size: 0, range: [undefined] },
  { size: 0, range: [undefined, undefined] },
  { size: 0, range: [0] },
  { size: 0, range: [0, undefined] },
  { size: 0, range: [0, 0] },
  { size: 12, range: [] },
  { size: 12, range: [undefined] },
  { size: 12, range: [undefined, undefined] },
  { size: 12, range: [0] },
  { size: 12, range: [0, undefined] },
  { size: 12, range: [0, 12] },
  { size: 12, range: [0, 0] },
  { size: 12, range: [8] },
  { size: 12, range: [8, undefined] },
  { size: 12, range: [8, 4] },
  { size: 28, range: [8, 8] },
  { size: 28, range: [8, 12] },
  { size: 512 * 1024, range: [] },
] as const;

function reifyMapRange(bufferSize: number, range: readonly [number?, number?]): [number, number] {
  const offset = range[0] ?? 0;
  return [offset, range[1] ?? bufferSize - offset];
}

const mapRegionBoundModes = ['default-expand', 'explicit-expand', 'minimal'] as const;
type MapRegionBoundMode = typeof mapRegionBoundModes[number];

function getRegionForMap(
  bufferSize: number,
  range: [number, number],
  {
    mapAsyncRegionLeft,
    mapAsyncRegionRight,
  }: {
    mapAsyncRegionLeft: MapRegionBoundMode;
    mapAsyncRegionRight: MapRegionBoundMode;
  }
) {
  const regionLeft = mapAsyncRegionLeft === 'minimal' ? range[0] : 0;
  const regionRight = mapAsyncRegionRight === 'minimal' ? range[0] + range[1] : bufferSize;
  return [
    mapAsyncRegionLeft === 'default-expand' ? undefined : regionLeft,
    mapAsyncRegionRight === 'default-expand' ? undefined : regionRight - regionLeft,
  ] as const;
}

g.test('mapAsync,write')
  .desc(
    `Use map-write to write to various ranges of variously-sized buffers, then expectContents
(which does copyBufferToBuffer + map-read) to ensure the contents were written.`
  )
  .cases(
    params()
      .combine(poptions('mapAsyncRegionLeft', mapRegionBoundModes))
      .combine(poptions('mapAsyncRegionRight', mapRegionBoundModes))
  )
  .subcases(() => kSubcases)
  .fn(async t => {
    const { size, range } = t.params;
    const [rangeOffset, rangeSize] = reifyMapRange(size, range);

    const buffer = t.device.createBuffer({
      size,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.MAP_WRITE,
    });

    const mapRegion = getRegionForMap(size, [rangeOffset, rangeSize], t.params);
    await buffer.mapAsync(GPUMapMode.WRITE, ...mapRegion);
    const arrayBuffer = buffer.getMappedRange(...range);
    t.checkMapWrite(buffer, rangeOffset, arrayBuffer, rangeSize);
  });

g.test('mapAsync,read')
  .desc(
    `Use mappedAtCreation to initialize various ranges of variously-sized buffers, then
map-read and check the read-back result.`
  )
  .cases(
    params()
      .combine(poptions('mapAsyncRegionLeft', mapRegionBoundModes))
      .combine(poptions('mapAsyncRegionRight', mapRegionBoundModes))
  )
  .subcases(() => kSubcases)
  .fn(async t => {
    const { size, range } = t.params;
    const [rangeOffset, rangeSize] = reifyMapRange(size, range);

    const buffer = t.device.createBuffer({
      mappedAtCreation: true,
      size,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });
    const init = buffer.getMappedRange(...range);

    assert(init.byteLength === rangeSize);
    const expected = new Uint32Array(new ArrayBuffer(rangeSize));
    const data = new Uint32Array(init);
    for (let i = 0; i < data.length; ++i) {
      data[i] = expected[i] = i + 1;
    }
    buffer.unmap();

    const mapRegion = getRegionForMap(size, [rangeOffset, rangeSize], t.params);
    await buffer.mapAsync(GPUMapMode.READ, ...mapRegion);
    const actual = new Uint8Array(buffer.getMappedRange(...range));
    t.expectBuffer(actual, new Uint8Array(expected.buffer));
  });

g.test('mappedAtCreation')
  .desc(
    `Use mappedAtCreation to write to various ranges of variously-sized buffers created either
with or without the MAP_WRITE usage (since this could affect the mappedAtCreation upload path),
then expectContents (which does copyBufferToBuffer + map-read) to ensure the contents were written.`
  )
  .cases(pbool('mappable'))
  .subcases(() => kSubcases)
  .fn(async t => {
    const { size, range, mappable } = t.params;
    const [, rangeSize] = reifyMapRange(size, range);

    const buffer = t.device.createBuffer({
      mappedAtCreation: true,
      size,
      usage: GPUBufferUsage.COPY_SRC | (mappable ? GPUBufferUsage.MAP_WRITE : 0),
    });
    const arrayBuffer = buffer.getMappedRange(...range);
    t.checkMapWrite(buffer, range[0] ?? 0, arrayBuffer, rangeSize);
  });
