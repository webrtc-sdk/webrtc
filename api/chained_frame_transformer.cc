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

#include <memory>
#include <utility>

#include "api/chained_frame_transformer.h"
#include "api/make_ref_counted.h"
#include "rtc_base/checks.h"

namespace webrtc {

class ChainedFrameTransformer::ForwardingTransformedFrameCallback
    : public TransformedFrameCallback {
 public:
  explicit ForwardingTransformedFrameCallback(scoped_refptr<FrameTransformerInterface> next)
      : next_(std::move(next)) {
    RTC_DCHECK(next_);
  }

  void OnTransformedFrame(std::unique_ptr<TransformableFrameInterface> frame) override {
    next_->Transform(std::move(frame));
  }

 protected:
  ~ForwardingTransformedFrameCallback() override = default;

 private:
  scoped_refptr<FrameTransformerInterface> next_;
};

ChainedFrameTransformer::ChainedFrameTransformer(scoped_refptr<FrameTransformerInterface> first,
                                                 scoped_refptr<FrameTransformerInterface> second)
    : first_(std::move(first)), second_(std::move(second)) {
  RTC_DCHECK(first_);
  RTC_DCHECK(second_);
  forwarding_callback_ = make_ref_counted<ForwardingTransformedFrameCallback>(second_);
}

ChainedFrameTransformer::~ChainedFrameTransformer() = default;

void ChainedFrameTransformer::Transform(
    std::unique_ptr<TransformableFrameInterface> transformable_frame) {
  first_->Transform(std::move(transformable_frame));
}

void ChainedFrameTransformer::RegisterTransformedFrameCallback(
    scoped_refptr<TransformedFrameCallback> callback) {
  second_->RegisterTransformedFrameCallback(callback);
  first_->RegisterTransformedFrameCallback(forwarding_callback_);
}

void ChainedFrameTransformer::RegisterTransformedFrameSinkCallback(
    scoped_refptr<TransformedFrameCallback> callback, uint32_t ssrc) {
  second_->RegisterTransformedFrameSinkCallback(callback, ssrc);
  first_->RegisterTransformedFrameSinkCallback(forwarding_callback_, ssrc);
}

void ChainedFrameTransformer::UnregisterTransformedFrameCallback() {
  first_->UnregisterTransformedFrameCallback();
  second_->UnregisterTransformedFrameCallback();
}

void ChainedFrameTransformer::UnregisterTransformedFrameSinkCallback(uint32_t ssrc) {
  first_->UnregisterTransformedFrameSinkCallback(ssrc);
  second_->UnregisterTransformedFrameSinkCallback(ssrc);
}

scoped_refptr<FrameTransformerInterface> CreateChainedFrameTransformer(
    scoped_refptr<FrameTransformerInterface> first,
    scoped_refptr<FrameTransformerInterface> second) {
  return make_ref_counted<ChainedFrameTransformer>(std::move(first), std::move(second));
}

}  // namespace webrtc
