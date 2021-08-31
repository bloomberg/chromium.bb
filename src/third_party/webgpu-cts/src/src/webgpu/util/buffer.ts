import { TypedArrayBufferView, TypedArrayBufferViewConstructor } from '../gpu_test.js';

export function makeBufferWithContents(
  device: GPUDevice,
  dataArray: TypedArrayBufferView,
  usage: GPUBufferUsageFlags
): GPUBuffer {
  const buffer = device.createBuffer({
    mappedAtCreation: true,
    size: dataArray.byteLength,
    usage,
  });
  const mappedBuffer = buffer.getMappedRange();
  const constructor = dataArray.constructor as TypedArrayBufferViewConstructor;
  new constructor(mappedBuffer).set(dataArray);
  buffer.unmap();
  return buffer;
}
