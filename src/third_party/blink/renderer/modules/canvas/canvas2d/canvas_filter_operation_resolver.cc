// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_fe_convolve_matrix_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_turbulence_element.h"

namespace blink {

namespace {
BlurFilterOperation* ResolveBlur(const Dictionary& blur_dict,
                                 ExceptionState& exception_state) {
  absl::optional<double> std_deviation =
      blur_dict.Get<IDLDouble>("stdDeviation", exception_state);
  if (!std_deviation) {
    exception_state.ThrowTypeError(
        "Failed to construct blur filter, 'stdDeviation' required and must be "
        "a number.");
    return nullptr;
  }

  return MakeGarbageCollected<BlurFilterOperation>(
      Length::Fixed(*std_deviation));
}

ColorMatrixFilterOperation* ResolveColorMatrix(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  absl::optional<Vector<float>> values =
      dict.Get<IDLSequence<IDLFloat>>("values", exception_state);

  if (!values) {
    exception_state.ThrowTypeError(
        "Failed to construct color matrix filter, 'values' array required.");
    return nullptr;
  }

  if (values->size() != 20) {
    exception_state.ThrowTypeError(
        "Failed to construct color matrix filter, 'values' must be an array "
        "of 20 numbers.");
    return nullptr;
  }

  return MakeGarbageCollected<ColorMatrixFilterOperation>(
      *values, FilterOperation::kColorMatrix);
}

struct KernelMatrix {
  Vector<float> values;
  uint32_t width;
  uint32_t height;
};

// For resolving feConvolveMatrix type filters
absl::optional<KernelMatrix> GetKernelMatrix(const Dictionary& dict,
                                             ExceptionState& exception_state) {
  absl::optional<Vector<Vector<float>>> km_input =
      dict.Get<IDLSequence<IDLSequence<IDLFloat>>>("kernelMatrix",
                                                   exception_state);
  if (!km_input || km_input->size() == 0) {
    exception_state.ThrowTypeError(
        "Failed to construct convolve matrix filter. 'kernelMatrix' must be an "
        "array of arrays of numbers representing an n by m matrix.");
    return absl::nullopt;
  }
  KernelMatrix result;
  result.height = km_input->size();
  result.width = km_input.value()[0].size();

  for (uint32_t y = 0; y < result.height; ++y) {
    if (km_input.value()[y].size() != result.width) {
      exception_state.ThrowTypeError(
          "Failed to construct convolve matrix filter. All rows of the "
          "'kernelMatrix' must be the same length.");
      return absl::nullopt;
    }

    result.values.AppendVector(km_input.value()[y]);
  }

  return absl::optional<KernelMatrix>(result);
}

ConvolveMatrixFilterOperation* ResolveConvolveMatrix(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  absl::optional<KernelMatrix> kernel_matrix =
      GetKernelMatrix(dict, exception_state);

  if (!kernel_matrix)
    return nullptr;

  gfx::Size kernel_size(kernel_matrix->width, kernel_matrix->height);
  double divisor = dict.Get<IDLDouble>("divisor", exception_state).value_or(1);
  double bias = dict.Get<IDLDouble>("bias", exception_state).value_or(0);
  gfx::Point target_offset =
      gfx::Point(dict.Get<IDLShort>("targetX", exception_state)
                     .value_or(kernel_matrix->width / 2),
                 dict.Get<IDLShort>("targetY", exception_state)
                     .value_or(kernel_matrix->height / 2));

  String edge_mode_string =
      dict.Get<IDLString>("edgeMode", exception_state).value_or("duplicate");
  FEConvolveMatrix::EdgeModeType edge_mode =
      static_cast<FEConvolveMatrix::EdgeModeType>(
          GetEnumerationMap<FEConvolveMatrix::EdgeModeType>().ValueFromName(
              edge_mode_string));

  bool preserve_alpha =
      dict.Get<IDLBoolean>("preserveAlpha", exception_state).value_or(false);

  return MakeGarbageCollected<ConvolveMatrixFilterOperation>(
      kernel_size, divisor, bias, target_offset, edge_mode, preserve_alpha,
      kernel_matrix->values);
}

ComponentTransferFunction GetComponentTransferFunction(
    const StringView& key,
    const Dictionary& filter,
    ExceptionState& exception_state) {
  ComponentTransferFunction result;
  // An earlier stage threw an error
  if (exception_state.HadException())
    return result;
  Dictionary transfer_dict;
  filter.Get(key, transfer_dict);

  result.slope =
      transfer_dict.Get<IDLDouble>("slope", exception_state).value_or(1);
  result.intercept =
      transfer_dict.Get<IDLDouble>("intercept", exception_state).value_or(0);
  result.amplitude =
      transfer_dict.Get<IDLDouble>("amplitude", exception_state).value_or(1);
  result.exponent =
      transfer_dict.Get<IDLDouble>("exponent", exception_state).value_or(1);
  result.offset =
      transfer_dict.Get<IDLDouble>("offset", exception_state).value_or(0);

  String type = transfer_dict.Get<IDLString>("type", exception_state)
                    .value_or("identity");
  if (type == "identity")
    result.type = FECOMPONENTTRANSFER_TYPE_IDENTITY;
  else if (type == "linear")
    result.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
  else if (type == "gamma")
    result.type = FECOMPONENTTRANSFER_TYPE_GAMMA;
  else if (type == "table")
    result.type = FECOMPONENTTRANSFER_TYPE_TABLE;
  else if (type == "discrete")
    result.type = FECOMPONENTTRANSFER_TYPE_DISCRETE;

  absl::optional<Vector<float>> table_values =
      transfer_dict.Get<IDLSequence<IDLFloat>>("tableValues", exception_state);
  if (table_values)
    result.table_values.AppendVector(table_values.value());

  return result;
}

ComponentTransferFilterOperation* ResolveComponentTransfer(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<ComponentTransferFilterOperation>(
      GetComponentTransferFunction("funcR", dict, exception_state),
      GetComponentTransferFunction("funcG", dict, exception_state),
      GetComponentTransferFunction("funcB", dict, exception_state),
      GetComponentTransferFunction("funcA", dict, exception_state));
}

// https://drafts.fxtf.org/filter-effects/#feTurbulenceElement
TurbulenceFilterOperation* ResolveTurbulence(const Dictionary& dict,
                                             ExceptionState& exception_state) {
  // Default values for all parameters per spec.
  float base_frequency_x = 0;
  float base_frequency_y = 0;
  float seed = 1;
  int num_octaves = 1;
  SVGStitchOptions stitch_tiles = kSvgStitchtypeNostitch;
  TurbulenceType type = FETURBULENCE_TYPE_TURBULENCE;

  // For checking the presence of keys.
  NonThrowableExceptionState no_throw;

  // baseFrequency can be either a number or a list of numbers.
  if (dict.HasProperty("baseFrequency", no_throw)) {
    // Try first to get baseFrequency as an array.
    absl::optional<Vector<float>> base_frequency_array =
        dict.Get<IDLSequence<IDLFloat>>("baseFrequency", exception_state);
    // Clear the exception if it exists in order to try again as a float
    exception_state.ClearException();
    // An array size of one is parse-able as a float.
    if (base_frequency_array.has_value() && base_frequency_array->size() == 2) {
      base_frequency_x = base_frequency_array.value()[0];
      base_frequency_y = base_frequency_array.value()[1];
    } else {
      // Otherwise, see if it the input can be interpreted as a float.
      absl::optional<float> base_frequency_float =
          dict.Get<IDLFloat>("baseFrequency", exception_state);
      if (exception_state.HadException() || !base_frequency_float.has_value()) {
        exception_state.ThrowTypeError(
            "Failed to construct turbulence filter, \"baseFrequency\" must be "
            "a number or list of two numbers.");
        return nullptr;
      }
      base_frequency_x = base_frequency_float.value();
      base_frequency_y = base_frequency_float.value();
    }
    if (base_frequency_x < 0 || base_frequency_y < 0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, negative values for "
          "\"baseFrequency\" are unsupported.");
      return nullptr;
    }
  }

  if (dict.HasProperty("seed", no_throw)) {
    absl::optional<float> seed_input =
        dict.Get<IDLFloat>("seed", exception_state);
    if (exception_state.HadException() || !seed_input.has_value()) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"seed\" must be a number.");
      return nullptr;
    }
    seed = seed_input.value();
  }

  if (dict.HasProperty("numOctaves", no_throw)) {
    // Get numOctaves as a float and then cast to int so that we throw for
    // inputs like undefined, NaN and Infinity.
    absl::optional<float> num_octaves_input =
        dict.Get<IDLFloat>("numOctaves", exception_state);
    if (exception_state.HadException() || !num_octaves_input.has_value() ||
        num_octaves_input.value() < 0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"numOctaves\" must be a "
          "positive number.");
      return nullptr;
    }
    num_octaves = static_cast<int>(num_octaves_input.value());
  }

  if (dict.HasProperty("stitchTiles", no_throw)) {
    absl::optional<String> stitch_tiles_input =
        dict.Get<IDLString>("stitchTiles", exception_state);
    if (exception_state.HadException() || !stitch_tiles_input.has_value() ||
        (stitch_tiles = static_cast<SVGStitchOptions>(
             GetEnumerationMap<SVGStitchOptions>().ValueFromName(
                 stitch_tiles_input.value()))) == 0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"stitchTiles\" must be "
          "either \"stitch\" or \"noStitch\".");
      return nullptr;
    }
  }

  if (dict.HasProperty("type", no_throw)) {
    absl::optional<String> type_input =
        dict.Get<IDLString>("type", exception_state);
    if (exception_state.HadException() || !type_input.has_value() ||
        (type = static_cast<TurbulenceType>(
             GetEnumerationMap<TurbulenceType>().ValueFromName(
                 type_input.value()))) == 0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"type\" must be either "
          "\"turbulence\" or \"fractalNoise\".");
      return nullptr;
    }
  }

  return MakeGarbageCollected<TurbulenceFilterOperation>(
      type, base_frequency_x, base_frequency_y, num_octaves, seed,
      stitch_tiles == kSvgStitchtypeStitch ? true : false);
}
}  // namespace

FilterOperations CanvasFilterOperationResolver::CreateFilterOperations(
    HeapVector<ScriptValue> filters,
    ExceptionState& exception_state) {
  FilterOperations operations;

  for (auto filter : filters) {
    Dictionary filter_dict = Dictionary(filter);
    absl::optional<String> name =
        filter_dict.Get<IDLString>("filter", exception_state);
    if (!name)
      continue;

    if (name == "gaussianBlur") {
      if (auto* blur_operation = ResolveBlur(filter_dict, exception_state)) {
        operations.Operations().push_back(blur_operation);
      }
    }
    if (name == "colorMatrix") {
      String type = filter_dict.Get<IDLString>("type", exception_state)
                        .value_or("matrix");
      if (type == "hueRotate") {
        double amount =
            filter_dict.Get<IDLDouble>("values", exception_state).value_or(0);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::kHueRotate));
      } else if (type == "saturate") {
        double amount =
            filter_dict.Get<IDLDouble>("values", exception_state).value_or(0);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::kSaturate));
      } else if (type == "luminanceToAlpha") {
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                0, FilterOperation::kLuminanceToAlpha));
      } else if (auto* color_matrix_operation =
                     ResolveColorMatrix(filter_dict, exception_state)) {
        operations.Operations().push_back(color_matrix_operation);
      }
    }
    if (name == "convolveMatrix") {
      if (auto* convolve_operation =
              ResolveConvolveMatrix(filter_dict, exception_state)) {
        operations.Operations().push_back(convolve_operation);
      }
    }
    if (name == "componentTransfer") {
      if (auto* component_transfer_operation =
              ResolveComponentTransfer(filter_dict, exception_state)) {
        operations.Operations().push_back(component_transfer_operation);
      }
    }
    if (name == "turbulence") {
      if (auto* turbulence_operation =
              ResolveTurbulence(filter_dict, exception_state)) {
        operations.Operations().push_back(turbulence_operation);
      }
    }
  }

  return operations;
}

}  // namespace blink
