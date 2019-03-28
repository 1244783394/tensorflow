/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/delegates/gpu/metal/kernels/depth_wise_conv3x3_stride1x1.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/substitute.h"
#include "tensorflow/lite/delegates/gpu/common/model.h"
#include "tensorflow/lite/delegates/gpu/common/shape.h"
#include "tensorflow/lite/delegates/gpu/common/util.h"
#include "tensorflow/lite/delegates/gpu/metal/compute_task_descriptor.h"
#include "tensorflow/lite/delegates/gpu/metal/runtime_options.h"

namespace tflite {
namespace gpu {
namespace metal {
namespace {
std::string GetKernelDepthWiseConv() {
  std::string code = R"(
#include <metal_stdlib>
using namespace metal;

struct uniforms {
  int4 src_size;
  int4 dst_size;
  int2 padding;
  int2 dummy0;  // for alignment
  int4 dummy1;  // for alignment
};
$0

kernel void ComputeFunction(
                            $1
                            uint3 ugid[[thread_position_in_grid]])
{
  int gid_x = ugid.x * 2;
  int gid_y = ugid.y * 2;
  int gid_z = ugid.z;

  if (gid_x >= params.dst_size.x || gid_y >= params.dst_size.y) {
      return;
  }

  ACCUM_FLT4 r0 = ACCUM_FLT4(0.0f, 0.0f, 0.0f, 0.0f);
  ACCUM_FLT4 l0 = ACCUM_FLT4(0.0f, 0.0f, 0.0f, 0.0f);
  ACCUM_FLT4 t0 = ACCUM_FLT4(0.0f, 0.0f, 0.0f, 0.0f);
  ACCUM_FLT4 b0 = ACCUM_FLT4(0.0f, 0.0f, 0.0f, 0.0f);

  int x0 = gid_x + params.padding.x;
  int x1 = gid_x + params.padding.x + 1;
  int x2 = gid_x + params.padding.x + 2;
  int x3 = gid_x + params.padding.x + 3;
  int y0 = gid_y + params.padding.y;
  int y1 = gid_y + params.padding.y + 1;
  int y2 = gid_y + params.padding.y + 2;
  int y3 = gid_y + params.padding.y + 3;

  bool x0_out = x0 < 0 || x0 >= params.src_size.x;
  bool x1_out = x1 < 0 || x1 >= params.src_size.x;
  bool x2_out = x2 < 0 || x2 >= params.src_size.x;
  bool x3_out = x3 < 0 || x3 >= params.src_size.x;
  bool y0_out = y0 < 0 || y0 >= params.src_size.y;
  bool y1_out = y1 < 0 || y1 >= params.src_size.y;
  bool y2_out = y2 < 0 || y2 >= params.src_size.y;
  bool y3_out = y3 < 0 || y3 >= params.src_size.y;

  x0 = clamp(x0, 0, params.src_size.x - 1);
  x1 = clamp(x1, 0, params.src_size.x - 1);
  x2 = clamp(x2, 0, params.src_size.x - 1);
  x3 = clamp(x3, 0, params.src_size.x - 1);
  y0 = clamp(y0, 0, params.src_size.y - 1);
  y1 = clamp(y1, 0, params.src_size.y - 1);
  y2 = clamp(y2, 0, params.src_size.y - 1);
  y3 = clamp(y3, 0, params.src_size.y - 1);

  device FLT4* src_loc = src_buffer + gid_z * params.src_size.z;
  device FLT4* filters_loc = filters + gid_z * 10;

  FLT4 s0 = src_loc[y0 * params.src_size.x + x0] * FLT(!(x0_out || y0_out));
  FLT4 s1 = src_loc[y1 * params.src_size.x + x0] * FLT(!(x0_out || y1_out));
  FLT4 s2 = src_loc[y2 * params.src_size.x + x0] * FLT(!(x0_out || y2_out));
  FLT4 s3 = src_loc[y3 * params.src_size.x + x0] * FLT(!(x0_out || y3_out));

  r0 += ACCUM_FLT4(s0 * filters_loc[0]);
  r0 += ACCUM_FLT4(s1 * filters_loc[1]);
  r0 += ACCUM_FLT4(s2 * filters_loc[2]);
  l0 += ACCUM_FLT4(s1 * filters_loc[0]);
  l0 += ACCUM_FLT4(s2 * filters_loc[1]);
  l0 += ACCUM_FLT4(s3 * filters_loc[2]);

  s0 = src_loc[y0 * params.src_size.x + x1] * FLT(!(x1_out || y0_out));
  s1 = src_loc[y1 * params.src_size.x + x1] * FLT(!(x1_out || y1_out));
  s2 = src_loc[y2 * params.src_size.x + x1] * FLT(!(x1_out || y2_out));
  s3 = src_loc[y3 * params.src_size.x + x1] * FLT(!(x1_out || y3_out));

  r0 += ACCUM_FLT4(s0 * filters_loc[3]);
  r0 += ACCUM_FLT4(s1 * filters_loc[4]);
  r0 += ACCUM_FLT4(s2 * filters_loc[5]);
  l0 += ACCUM_FLT4(s1 * filters_loc[3]);
  l0 += ACCUM_FLT4(s2 * filters_loc[4]);
  l0 += ACCUM_FLT4(s3 * filters_loc[5]);
  t0 += ACCUM_FLT4(s0 * filters_loc[0]);
  t0 += ACCUM_FLT4(s1 * filters_loc[1]);
  t0 += ACCUM_FLT4(s2 * filters_loc[2]);
  b0 += ACCUM_FLT4(s1 * filters_loc[0]);
  b0 += ACCUM_FLT4(s2 * filters_loc[1]);
  b0 += ACCUM_FLT4(s3 * filters_loc[2]);

  s0 = src_loc[y0 * params.src_size.x + x2] * FLT(!(x2_out || y0_out));
  s1 = src_loc[y1 * params.src_size.x + x2] * FLT(!(x2_out || y1_out));
  s2 = src_loc[y2 * params.src_size.x + x2] * FLT(!(x2_out || y2_out));
  s3 = src_loc[y3 * params.src_size.x + x2] * FLT(!(x2_out || y3_out));

  r0 += ACCUM_FLT4(s0 * filters_loc[6]);
  r0 += ACCUM_FLT4(s1 * filters_loc[7]);
  r0 += ACCUM_FLT4(s2 * filters_loc[8]);
  l0 += ACCUM_FLT4(s1 * filters_loc[6]);
  l0 += ACCUM_FLT4(s2 * filters_loc[7]);
  l0 += ACCUM_FLT4(s3 * filters_loc[8]);
  t0 += ACCUM_FLT4(s0 * filters_loc[3]);
  t0 += ACCUM_FLT4(s1 * filters_loc[4]);
  t0 += ACCUM_FLT4(s2 * filters_loc[5]);
  b0 += ACCUM_FLT4(s1 * filters_loc[3]);
  b0 += ACCUM_FLT4(s2 * filters_loc[4]);
  b0 += ACCUM_FLT4(s3 * filters_loc[5]);

  s0 = src_loc[y0 * params.src_size.x + x3] * FLT(!(x3_out || y0_out));
  s1 = src_loc[y1 * params.src_size.x + x3] * FLT(!(x3_out || y1_out));
  s2 = src_loc[y2 * params.src_size.x + x3] * FLT(!(x3_out || y2_out));
  s3 = src_loc[y3 * params.src_size.x + x3] * FLT(!(x3_out || y3_out));

  t0 += ACCUM_FLT4(s0 * filters_loc[6]);
  t0 += ACCUM_FLT4(s1 * filters_loc[7]);
  t0 += ACCUM_FLT4(s2 * filters_loc[8]);
  b0 += ACCUM_FLT4(s1 * filters_loc[6]);
  b0 += ACCUM_FLT4(s2 * filters_loc[7]);
  b0 += ACCUM_FLT4(s3 * filters_loc[8]);

  r0 += ACCUM_FLT4(filters_loc[9]);
  l0 += ACCUM_FLT4(filters_loc[9]);
  t0 += ACCUM_FLT4(filters_loc[9]);
  b0 += ACCUM_FLT4(filters_loc[9]);

  const int offset_0 = gid_z * params.dst_size.z + gid_y * params.dst_size.x + gid_x;
  const int offset_1 = offset_0 + params.dst_size.x;
  const int offset_2 = offset_0 + 1;
  const int offset_3 = offset_0 + params.dst_size.x + 1;
  bool x0_in = gid_x < params.dst_size.x;
  bool x1_in = gid_x + 1 < params.dst_size.x;
  bool y0_in = gid_y < params.dst_size.y;
  bool y1_in = gid_y + 1 < params.dst_size.y;

  if (y0_in && x0_in) {
      int linear_index = offset_0;
      FLT4 value = FLT4(r0);
      uint3 gid = uint3(gid_x, gid_y, gid_z);
      $2
      dst_buffer[linear_index] = value;
  }
  if (y1_in && x0_in) {
      int linear_index = offset_1;
      FLT4 value = FLT4(l0);
      uint3 gid = uint3(gid_x, gid_y + 1, gid_z);
      $2
      dst_buffer[linear_index] = value;
  }
  if (y0_in && x1_in) {
      int linear_index = offset_2;
      FLT4 value = FLT4(t0);
      uint3 gid = uint3(gid_x + 1, gid_y, gid_z);
      $2
      dst_buffer[linear_index] = value;
  }
  if (y1_in && x1_in) {
      int linear_index = offset_3;
      FLT4 value = FLT4(b0);
      uint3 gid = uint3(gid_x + 1, gid_y + 1, gid_z);
      $2
      dst_buffer[linear_index] = value;
  }
}
  )";

  return code;
}

// Reorder weights to make the weights memory access pattern cache friendly for
// DepthWiseConv3x3Stride1x1
std::vector<float> ReorderWeights(
    const DepthwiseConvolution2DAttributes& attr) {
  const int src_depth = IntegralDivideRoundUp(attr.weights.shape.i, 4);
  const int kernel_x = 3;
  const int kernel_y = 3;
  std::vector<float> weights_reordered((kernel_x * kernel_y + 1) * src_depth *
                                       4);

  int counter = 0;
  for (int s = 0; s < src_depth; ++s) {
    for (int x = 0; x < kernel_x; ++x) {
      for (int y = 0; y < kernel_y; ++y) {
        for (int i = 0; i < 4; ++i) {
          const int s_ch = s * 4 + i;
          if (s_ch < attr.weights.shape.i) {
            const int f_index = attr.weights.shape.LinearIndex({0, y, x, s_ch});
            weights_reordered[counter++] = attr.weights.data[f_index];
          } else {
            weights_reordered[counter++] = 0.0f;
          }
        }
      }
    }

    for (int i = 0; i < 4; ++i) {
      const int dst_ch = s * 4 + i;
      if (dst_ch < attr.bias.shape.v) {
        weights_reordered[counter++] = attr.bias.data[dst_ch];
      } else {
        weights_reordered[counter++] = 0.0f;
      }
    }
  }

  return weights_reordered;
}

static std::vector<uint8_t> GetUniformBuffer(
    const BHWC& src_size, const BHWC& dst_size,
    const DepthwiseConvolution2DAttributes& params) {
  std::vector<int> uniform_params = {
      src_size.w,
      src_size.h,
      src_size.w * src_size.h,
      IntegralDivideRoundUp(src_size.c, 4),
      dst_size.w,
      dst_size.h,
      dst_size.w * dst_size.h,
      IntegralDivideRoundUp(dst_size.c, 4),
      -params.padding.prepended.w,
      -params.padding.prepended.h,
      0,  // dummy, for alignment
      0,  // dummy, for alignment
      0,  // dummy, for alignment
      0,  // dummy, for alignment
      0,  // dummy, for alignment
      0,  // dummy, for alignment
  };
  return VectorToUint8Vector(uniform_params);
}
}  // namespace

std::vector<ComputeTaskDescriptorPtr> DepthWiseConv3x3Stride1x1(
    int id, ValueId input_id, ValueId output_id,
    const DepthwiseConvolution2DAttributes& attr,
    const RuntimeOptions& options) {
  auto desc = std::make_shared<ComputeTaskDescriptor>();
  desc->id = id;
  desc->is_linkable = false;
  desc->shader_source = GetKernelDepthWiseConv();

  desc->input_buffers = {
      {input_id, "device FLT4* const src_buffer"},
  };

  desc->output_buffer = {
      output_id, "device FLT4* dst_buffer",
      [input_id, attr](const std::map<ValueId, BHWC>& buffers) {
        auto out_shape =
            CalculateOutputShape(buffers.find(input_id)->second, attr);
        return out_shape;
      }};

  // For this operation we keep weights and biases in one buffer
  auto weights_reordered = ReorderWeights(attr);
  auto weights =
      options.storage_precision == metal::RuntimeOptions::Precision::FP32
          ? VectorToUint8Vector(weights_reordered)
          : VectorFloatToHalf(weights_reordered);
  desc->immutable_buffers = {
      {"device FLT4* const filters", weights},
  };

  desc->uniform_buffers = {
      {"constant uniforms& params",
       [input_id, output_id, attr](const std::map<ValueId, BHWC>& buffers) {
         const auto& input_dimensions = buffers.find(input_id)->second;
         const auto& output_dimensions = buffers.find(output_id)->second;
         return GetUniformBuffer(input_dimensions, output_dimensions, attr);
       }},
  };

  desc->resize_function = [output_id](const std::map<ValueId, BHWC>& buffers) {
    const auto& dimension = buffers.find(output_id)->second;
    const int grid_x = IntegralDivideRoundUp(dimension.w, 2);
    const int grid_y = IntegralDivideRoundUp(dimension.h, 2);
    const int grid_z = IntegralDivideRoundUp(dimension.c, 4);
    uint3 group_size{8, 4, 1};
    if (grid_x <= 4) {
      group_size.x = 4;
      group_size.z = grid_z % 2 == 0 ? 2 : 1;
    }
    const int groups_x = IntegralDivideRoundUp(grid_x, group_size.x);
    const int groups_y = IntegralDivideRoundUp(grid_y, group_size.y);
    const int groups_z = IntegralDivideRoundUp(grid_z, group_size.z);
    return std::make_pair(group_size, uint3(groups_x, groups_y, groups_z));
  };

  return {desc};
}

bool CheckDepthWiseConv3x3Stride1x1Support(
    const DepthwiseConvolution2DAttributes& attr) {
  return attr.weights.shape.o == 1 && attr.weights.shape.h == 3 &&
         attr.weights.shape.w == 3 && attr.strides.h == 1 &&
         attr.strides.w == 1 && attr.dilations.h == 1 && attr.dilations.w == 1;
}

}  // namespace metal
}  // namespace gpu
}  // namespace tflite