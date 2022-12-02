/*
 * Copyright 2022 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "yuv_helper.h"

#include "libyuv/convert.h"
#include "third_party/libyuv/include/libyuv.h"
#include "video_rotation.h"

namespace webrtc {

int I420Rotate(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height,
               VideoRotation mode) {
  return libyuv::I420Rotate(src_y, src_stride_y, src_u, src_stride_u, src_v,
                            src_stride_v, dst_y, dst_stride_y, dst_u,
                            dst_stride_u, dst_v, dst_stride_v, width, height,
                            static_cast<libyuv::RotationMode>(mode));
}

int I420ToNV12(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return libyuv::I420ToNV12(src_y, src_stride_y, src_u, src_stride_u, src_v,
                            src_stride_v, dst_y, dst_stride_y, dst_uv,
                            dst_stride_uv, width, height);
}

int NV12ToI420(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return libyuv::NV12ToI420(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                            dst_stride_y, dst_u, dst_stride_u, dst_v,
                            dst_stride_v, width, height);
}

int I420ToARGB(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_argb,
               int dst_stride_argb,
               int width,
               int height) {
  return libyuv::I420ToARGB(src_y, src_stride_y, src_u, src_stride_u, src_v,
                            src_stride_v, dst_argb, dst_stride_argb, width,
                            height);
}

int I420ToBGRA(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_bgra,
               int dst_stride_bgra,
               int width,
               int height) {
  return libyuv::I420ToBGRA(src_y, src_stride_y, src_u, src_stride_u, src_v,
                            src_stride_v, dst_bgra, dst_stride_bgra, width,
                            height);
}

int I420ToABGR(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_abgr,
               int dst_stride_abgr,
               int width,
               int height) {
  return libyuv::I420ToABGR(src_y, src_stride_y, src_u, src_stride_u, src_v,
                            src_stride_v, dst_abgr, dst_stride_abgr, width,
                            height);
}

int I420ToRGBA(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_rgba,
               int dst_stride_rgba,
               int width,
               int height) {
  return libyuv::I420ToRGBA(src_y, src_stride_y, src_u, src_stride_u, src_v,
                            src_stride_v, dst_rgba, dst_stride_rgba, width,
                            height);
}

int I420ToRGB24(const uint8_t* src_y,
                int src_stride_y,
                const uint8_t* src_u,
                int src_stride_u,
                const uint8_t* src_v,
                int src_stride_v,
                uint8_t* dst_rgb24,
                int dst_stride_rgb24,
                int width,
                int height) {
  return libyuv::I420ToRGB24(src_y, src_stride_y, src_u, src_stride_u, src_v,
                             src_stride_v, dst_rgb24, dst_stride_rgb24, width,
                             height);
}

int I420Scale(const uint8_t* src_y,
              int src_stride_y,
              const uint8_t* src_u,
              int src_stride_u,
              const uint8_t* src_v,
              int src_stride_v,
              int src_width,
              int src_height,
              uint8_t* dst_y,
              int dst_stride_y,
              uint8_t* dst_u,
              int dst_stride_u,
              uint8_t* dst_v,
              int dst_stride_v,
              int dst_width,
              int dst_height,
              libyuv::FilterMode filtering) {
  return libyuv::I420Scale(src_y, src_stride_y, src_u, src_stride_u, src_v,
                           src_stride_v, src_width, src_height, dst_y,
                           dst_stride_y, dst_u, dst_stride_u, dst_v,
                           dst_stride_v, dst_width, dst_height, filtering);
}

int ARGBToI420(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return libyuv::ARGBToI420(src_argb, src_stride_argb, dst_y, dst_stride_y,
                            dst_u, dst_stride_u, dst_v, dst_stride_v, width,
                            height);
}

int ABGRToI420(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return libyuv::ABGRToI420(src_abgr, src_stride_abgr, dst_y, dst_stride_y,
                            dst_u, dst_stride_u, dst_v, dst_stride_v, width,
                            height);
}

int ARGBToRGB24(const uint8_t* src_argb,
                int src_stride_argb,
                uint8_t* dst_rgb24,
                int dst_stride_rgb24,
                int width,
                int height) {
  return libyuv::ARGBToRGB24(src_argb, src_stride_argb, dst_rgb24,
                             dst_stride_rgb24, width, height);
}

}  // namespace webrtc
