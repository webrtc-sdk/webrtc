/*
 * Copyright 2026 LiveKit
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

#ifndef API_CHAINED_FRAME_TRANSFORMER_H_
#define API_CHAINED_FRAME_TRANSFORMER_H_

#include <cstdint>
#include <memory>

#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Chains two FrameTransformerInterface implementations into one transformer.
//
// Frames passed to Transform() are transformed by `first`. When `first`
// completes, the intermediate callback forwards the frame into `second`.
// The final transformed-frame callback is registered on `second`.
class RTC_EXPORT ChainedFrameTransformer : public FrameTransformerInterface {
 public:
  ChainedFrameTransformer(scoped_refptr<FrameTransformerInterface> first,
                          scoped_refptr<FrameTransformerInterface> second);

  void Transform(std::unique_ptr<TransformableFrameInterface> transformable_frame) override;

  void RegisterTransformedFrameCallback(scoped_refptr<TransformedFrameCallback> callback) override;
  void RegisterTransformedFrameSinkCallback(scoped_refptr<TransformedFrameCallback> callback,
                                            uint32_t ssrc) override;
  void UnregisterTransformedFrameCallback() override;
  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;

 protected:
  ~ChainedFrameTransformer() override;

 private:
  class ForwardingTransformedFrameCallback;

  scoped_refptr<FrameTransformerInterface> first_;
  scoped_refptr<FrameTransformerInterface> second_;
  scoped_refptr<TransformedFrameCallback> forwarding_callback_;
};

RTC_EXPORT scoped_refptr<FrameTransformerInterface> CreateChainedFrameTransformer(
    scoped_refptr<FrameTransformerInterface> first,
    scoped_refptr<FrameTransformerInterface> second);

}  // namespace webrtc

#endif  // API_CHAINED_FRAME_TRANSFORMER_H_
