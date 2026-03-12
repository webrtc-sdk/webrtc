/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoFrameReorderQueue.h"
#include <algorithm>

namespace webrtc {

bool RTCVideoFrameReorderQueue::isEmpty() {
  webrtc::MutexLock lock(&_reorderQueueLock);
  return _reorderQueue.empty();
}

uint8_t RTCVideoFrameReorderQueue::reorderSize() const {
  webrtc::MutexLock lock(&_reorderQueueLock);
  return _reorderSize;
}

void RTCVideoFrameReorderQueue::setReorderSize(uint8_t size) {
  webrtc::MutexLock lock(&_reorderQueueLock);
  _reorderSize = size;
}

void RTCVideoFrameReorderQueue::append(RTC_OBJC_TYPE(RTCVideoFrame) * frame, uint8_t reorderSize) {
  webrtc::MutexLock lock(&_reorderQueueLock);
  auto newEntry = std::make_unique<RTC_OBJC_TYPE(RTCVideoFrameWithOrder)>(frame, reorderSize);
  const uint64_t ts = newEntry->timeStamp;

  // Keep queue sorted by timestamp with O(n) insertion instead of sorting
  // the entire container each time.
  auto it = std::upper_bound(
      _reorderQueue.begin(), _reorderQueue.end(), ts,
      [](const uint64_t value, const std::unique_ptr<RTC_OBJC_TYPE(RTCVideoFrameWithOrder)> &elem) {
        return value < elem->timeStamp;
      });
  _reorderQueue.insert(it, std::move(newEntry));
}

RTC_OBJC_TYPE(RTCVideoFrame) * RTCVideoFrameReorderQueue::takeIfAvailable() {
  webrtc::MutexLock lock(&_reorderQueueLock);
  if (_reorderQueue.size() && _reorderQueue.size() > _reorderQueue.front()->reorderSize) {
    auto *frame = _reorderQueue.front()->take();
    _reorderQueue.pop_front();
    return frame;
  }
  return nil;
}

RTC_OBJC_TYPE(RTCVideoFrame) * RTCVideoFrameReorderQueue::takeIfAny() {
  webrtc::MutexLock lock(&_reorderQueueLock);
  if (_reorderQueue.size()) {
    auto *frame = _reorderQueue.front()->take();
    _reorderQueue.pop_front();
    return frame;
  }
  return nil;
}

}  // namespace webrtc