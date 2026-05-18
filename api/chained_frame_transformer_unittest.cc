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

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/chained_frame_transformer.h"
#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_transformable_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

class PassThroughFrameTransformer : public FrameTransformerInterface {
 public:
  PassThroughFrameTransformer(std::string name, std::vector<std::string>* events)
      : name_(std::move(name)), events_(events) {}

  void Transform(std::unique_ptr<TransformableFrameInterface> frame) override {
    events_->push_back(name_);

    auto sink_it = sink_callbacks_.find(frame->GetSsrc());
    if (sink_it != sink_callbacks_.end()) {
      sink_it->second->OnTransformedFrame(std::move(frame));
      return;
    }

    if (callback_) {
      callback_->OnTransformedFrame(std::move(frame));
    }
  }

  void RegisterTransformedFrameCallback(scoped_refptr<TransformedFrameCallback> callback) override {
    callback_ = std::move(callback);
  }

  void RegisterTransformedFrameSinkCallback(scoped_refptr<TransformedFrameCallback> callback,
                                            uint32_t ssrc) override {
    sink_callbacks_[ssrc] = std::move(callback);
  }

  void UnregisterTransformedFrameCallback() override { callback_ = nullptr; }

  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override {
    sink_callbacks_.erase(ssrc);
  }

  bool has_callback() const { return callback_ != nullptr; }
  bool has_sink_callback(uint32_t ssrc) const {
    return sink_callbacks_.find(ssrc) != sink_callbacks_.end();
  }

 protected:
  ~PassThroughFrameTransformer() override = default;

 private:
  std::string name_;
  std::vector<std::string>* events_;
  scoped_refptr<TransformedFrameCallback> callback_;
  std::map<uint32_t, scoped_refptr<TransformedFrameCallback>> sink_callbacks_;
};

class RecordingTransformedFrameCallback : public TransformedFrameCallback {
 public:
  explicit RecordingTransformedFrameCallback(std::vector<std::string>* events) : events_(events) {}

  void OnTransformedFrame(std::unique_ptr<TransformableFrameInterface> frame) override {
    events_->push_back("callback");
    frames_.push_back(std::move(frame));
  }

  size_t frame_count() const { return frames_.size(); }

 protected:
  ~RecordingTransformedFrameCallback() override = default;

 private:
  std::vector<std::string>* events_;
  std::vector<std::unique_ptr<TransformableFrameInterface>> frames_;
};

std::unique_ptr<NiceMock<MockTransformableFrame>> CreateFrameWithSsrc(uint32_t ssrc) {
  auto frame = std::make_unique<NiceMock<MockTransformableFrame>>();
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(ssrc));
  return frame;
}

TEST(ChainedFrameTransformerTest, RunsFirstThenSecondThenCallback) {
  std::vector<std::string> events;
  auto first = make_ref_counted<PassThroughFrameTransformer>("first", &events);
  auto second = make_ref_counted<PassThroughFrameTransformer>("second", &events);
  auto callback = make_ref_counted<RecordingTransformedFrameCallback>(&events);
  auto chained = CreateChainedFrameTransformer(first, second);

  chained->RegisterTransformedFrameCallback(callback);
  chained->Transform(CreateFrameWithSsrc(17));

  EXPECT_THAT(events, testing::ElementsAre("first", "second", "callback"));
  EXPECT_EQ(callback->frame_count(), 1u);
  EXPECT_TRUE(first->has_callback());
  EXPECT_TRUE(second->has_callback());
}

TEST(ChainedFrameTransformerTest, RegistersAndUnregistersSinkCallbacks) {
  constexpr uint32_t kSsrc = 42;
  std::vector<std::string> events;
  auto first = make_ref_counted<PassThroughFrameTransformer>("first", &events);
  auto second = make_ref_counted<PassThroughFrameTransformer>("second", &events);
  auto callback = make_ref_counted<RecordingTransformedFrameCallback>(&events);
  auto chained = CreateChainedFrameTransformer(first, second);

  chained->RegisterTransformedFrameSinkCallback(callback, kSsrc);
  EXPECT_TRUE(first->has_sink_callback(kSsrc));
  EXPECT_TRUE(second->has_sink_callback(kSsrc));

  chained->Transform(CreateFrameWithSsrc(kSsrc));
  EXPECT_THAT(events, testing::ElementsAre("first", "second", "callback"));
  EXPECT_EQ(callback->frame_count(), 1u);

  chained->UnregisterTransformedFrameSinkCallback(kSsrc);
  EXPECT_FALSE(first->has_sink_callback(kSsrc));
  EXPECT_FALSE(second->has_sink_callback(kSsrc));
}

}  // namespace
}  // namespace webrtc
