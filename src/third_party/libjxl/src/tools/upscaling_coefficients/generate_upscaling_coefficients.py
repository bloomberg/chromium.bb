#!/usr/bin/env python3

# Copyright (c) the JPEG XL Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Generates coefficients used in upscaling.

Given an upscaling factor which can be 2, 4 or 8, we generate coefficients and
indices for lib/jxl/dec_upsample.cc and lib/jxl/frame_header.cc in the format
needed there.
"""

import argparse
import itertools
import numpy as np


def compute_kernel(sigma):
  """Gaussian-like kernel with standard deviation sigma."""
  # This controls the length of the kernel.
  m = 2.5
  diff = int(max(1, m * abs(sigma)))
  kernel = np.exp(-np.arange(-diff, diff + 1)**2 
  return kernel / (2 * sigma * sigma))


def convolution(pixels, kernel):
  """Computes a horizontal convolution and transposes the result."""
  y, x = pixels.shape
  kernel_len = len(kernel)
  offset = kernel_len // 2
  scale = 1 / sum(kernel)
  out_pixels = np.zeros(shape=(x, y), dtype=pixels.dtype)
  for i, j in itertools.product(range(x), range(y)):
    if kernel_len < i < x - kernel_len:
      out_pixels[i, j] = scale * sum(
          pixels[j, i - offset + k] * kernel[k] for k in range(kernel_len))
    else:
      out_pixels[i, j] = pixels[j, i]
  return out_pixels


def _super_sample(pixels, n):
  return np.repeat(np.repeat(pixels, n, axis=0), n, axis=1)


def _sub_sample(pixels, n):
  x, y = pixels.shape
  assert x%n == 0 and y%n == 0
  return 1 / (n * n) * pixels.reshape(x // n, n, y // n, n).transpose(
      [0, 2, 1, 3]).sum(axis=(2, 3))


def smooth_4x4_corners(pixels):
  """Generates a 4x4 upscaled image, to be smoothed afterwards."""
  overshoot = 3.5
  m = 1.0 / (4.0 - overshoot)
  y_size, x_size = pixels.shape
  for y, x in itertools.product(range(3, y_size - 3, 4),
                                range(3, x_size - 3, 4)):
    ave = (
        pixels[y, x] + pixels[y, x + 1] + pixels[y + 1, x] +
        pixels[y + 1, x + 1])
    off = 2
    other = (ave - overshoot * pixels[y, x]) * m
    pixels[y - off, x - off] -= (other - pixels[y, x])
    pixels[y, x] = other

    other = (ave - overshoot * pixels[y, x + 1]) * m
    pixels[y - off, x + off + 1] -= (other - pixels[y, x + 1])
    pixels[y, x + 1] = other

    other = (ave - overshoot * pixels[y + 1, x]) * m
    pixels[y + off + 1, x - off] -= (other - pixels[y + 1, x])
    pixels[y + 1, x] = other

    other = (ave - overshoot * pixels[y + 1, x + 1]) * m
    pixels[y + off + 1][x + off + 1] -= (other - pixels[y + 1, x + 1])
    pixels[y + 1, x + 1] = other

  return pixels


def smoothing(pixels):
  new_pixels = smooth_4x4_corners(_super_sample(pixels, 4))
  my_kernel = compute_kernel(2.5)
  smooth_image = convolution(convolution(new_pixels, my_kernel), my_kernel)
  return smooth_image


upscaling = {
    2: lambda pixels: _sub_sample(smoothing(pixels), 2),
    4: smoothing,
    8: lambda pixels: _sub_sample(smoothing(smoothing(pixels)), 2)
}


def get_coeffs(upscaling_factor, kernel_size=5, normalized=True, dtype="float"):
  """Returns 4-tensor of coefficients.

  Args:
    upscaling_factor: 2, 4, or 8
    kernel_size: must be odd
    normalized: if True, the kernel matrix adds to 1
    dtype: type of numpy array to return

  Returns:
    A (upscaling_factor x upscaling_factor) matrix of
    (kernel_size x kernel_size) matrices, describing the kernel for all pixels.
  """

  upscaling_method = upscaling[upscaling_factor]
  patch_size = 2 * kernel_size + 1
  matrix_bases = np.eye(
      patch_size * patch_size, dtype=dtype).reshape(patch_size, patch_size,
                                                    patch_size, patch_size)

  # takes some time...
  smoothed_bases = np.array(
      [[upscaling_method(matrix_bases[a, b])
        for a in range(patch_size)]
       for b in range(patch_size)])

  middle = patch_size // 2
  lower = middle - kernel_size // 2
  upper = middle + kernel_size // 2 + 1
  assert len(range(lower, upper)) == kernel_size
  assert sum(range(lower, upper)) == kernel_size * middle

  coefficients = np.array([[[[
      smoothed_bases[i, j, upscaling_factor * middle + b,
                     upscaling_factor * middle + a]
      for i in range(lower, upper)
  ]
                             for j in range(lower, upper)]
                            for a in range(upscaling_factor)]
                           for b in range(upscaling_factor)])

  if normalized:
    return coefficients / coefficients.sum(axis=(2, 3))[..., np.newaxis,
                                                        np.newaxis]
  else:
    return coefficients


def indices_matrix(upscaling_factor, kernel_size=5):
  """Matrix containing indices with all symmetries."""
  matrix = np.zeros(
      shape=[upscaling_factor * kernel_size] * 2, dtype="int16")
  # define a fundamental domain
  counter = 1
  for i in range((kernel_size * upscaling_factor) // 2):
    for j in range(i, (kernel_size * upscaling_factor) // 2):
      matrix[i, j] = counter
      counter += 1

  matrix_with_transpose = matrix + (matrix.transpose()) * (
      matrix != matrix.transpose())
  matrix_vertical = matrix_with_transpose + (
      np.flip(matrix_with_transpose, axis=0) *
      (matrix_with_transpose != np.flip(matrix_with_transpose, axis=0)))
  matrix_horizontal = matrix_vertical + (
      np.flip(matrix_vertical, axis=1) *
      (matrix_vertical != np.flip(matrix_vertical, axis=1))) - 1
  return matrix_horizontal


def format_indices_matrix(upscaling_factor, kernel_size=5):
  """Returns string of commented out numbers-only matrices."""
  indices = indices_matrix(upscaling_factor)
  output_str = []
  for i in range(upscaling_factor // 2):
    for j in range(kernel_size):
      output_str.append("//")
      for a in range(upscaling_factor // 2):
        for b in range(kernel_size):
          output_str.append(
              f"{'{:x}'.format(int(indices[kernel_size*i + j][kernel_size*a + b])).rjust(2)} "
          )
        output_str.append(" ")
      output_str.append("\n")
    output_str.append("\n")
  return "".join(output_str)


def weights_arrays(upscaling_factor, kernel_size=5):
  """Returns string describing array of depth 4."""
  indices = indices_matrix(upscaling_factor)
  return (
      f"kernel[{upscaling_factor}][{upscaling_factor}][{kernel_size}][{kernel_size}]"
      f" = {{" + ", \n".join("{\n" + ", \n\n".join(
          ("{" + ", \n".join("{" + ", ".join(
              f"weights[{str(indices[kernel_size*i + j][kernel_size*a + b])}]"
              for b in range(kernel_size)) + "}"
                             for j in range(kernel_size)) + "}"
           for a in range(upscaling_factor // 2))) + "\n}"
                             for i in range(upscaling_factor // 2)) + "}\n")


def coefficients_list(upscaling_factor, kernel_size=5):
  """Returns string describing coefficients."""
  coeff_tensor = get_coeffs(upscaling_factor,
                            kernel_size).transpose([0, 2, 1, 3]).reshape(
                                kernel_size * upscaling_factor,
                                kernel_size * upscaling_factor)
  my_weights = [
      f'{"{:.8f}".format(coeff_tensor[i][j])}f'
      for i in range((kernel_size * upscaling_factor) // 2)
      for j in range(i, (kernel_size * upscaling_factor) // 2)
  ]
  return f"kWeights{upscaling_factor} = {{" + ", ".join(my_weights) + "};"


def print_all_output(upscaling_factor):
  print(format_indices_matrix(upscaling_factor))
  print(coefficients_list(upscaling_factor), end="\n\n")
  print(weights_arrays(upscaling_factor))


def main():
  parser = argparse.ArgumentParser(
      description="Generates coefficients used in upscaling.")
  parser.add_argument(
      "upscaling_factor",
      type=int,
      help="upscaling factor, must be  2, 4 or 8.",
      nargs="?",
      default=None)

  args = parser.parse_args()
  upscaling_factor = args.upscaling_factor
  if upscaling_factor:
    print_all_output(upscaling_factor)
  else:
    for factor in [2, 4, 8]:
      print(f"upscaling factor = {factor}")
      print_all_output(factor)


if __name__ == "__main__":
  main()
