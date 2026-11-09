/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_
#define API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_

#include <jni.h>

#include <atomic>
#include <optional>
#include <vector>

#include "api/environment/environment.h"
#include "api/frame_metadata/frame_metadata_transformer.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "media/base/adapted_video_track_source.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/timestamp_aligner.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace jni {

// This class needs to be used in conjunction with the Java corresponding class
// NativeAndroidVideoTrackSource. This class is thred safe and methods can be
// called from any thread, but if frames A, B, ..., are sent to adaptFrame(),
// the adapted frames adaptedA, adaptedB, ..., needs to be passed in the same
// order to onFrameCaptured().
class AndroidVideoTrackSource : public AdaptedVideoTrackSource {
 public:
  [[deprecated("Provide a webrtc::Environment")]]
  AndroidVideoTrackSource(Thread* signaling_thread,
                          JNIEnv* jni,
                          bool is_screencast,
                          bool align_timestamps);
  AndroidVideoTrackSource(Thread* signaling_thread,
                          JNIEnv* jni,
                          bool is_screencast,
                          bool align_timestamps,
                          std::optional<Environment> env);
  ~AndroidVideoTrackSource() override;

  bool is_screencast() const override;

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  std::optional<bool> needs_denoising() const override;

  void SetState(SourceState state);

  SourceState state() const override;

  bool remote() const override;

  // This function should be called before delivering any frame to determine if
  // the frame should be dropped or what the cropping and scaling parameters
  // should be. This function is thread safe and can be called from any thread.
  // This function returns
  // NativeAndroidVideoTrackSource.FrameAdaptationParameters, or null if the
  // frame should be dropped.
  ScopedJavaLocalRef<jobject> AdaptFrame(JNIEnv* env,
                                         jint j_width,
                                         jint j_height,
                                         jint j_rotation,
                                         jlong j_timestamp_ns);

  // This function converts and passes the frame on to the rest of the C++
  // WebRTC layer. Note that GetFrameAdaptationParameters() is expected to be
  // called first and that the delivered frame conforms to those parameters.
  // This function is thread safe and can be called from any thread.
  void OnFrameCaptured(JNIEnv* env,
                       jint j_rotation,
                       jlong j_timestamp_ns,
                       const JavaRef<jobject>& j_video_frame_buffer,
                       const JavaRef<jobject>& j_frame_metadata);

  // Links a send transformer to this source. The FrameMetadata on frames
  // delivered to OnFrameCaptured() is stored in every linked transformer.
  void AddFrameMetadataTransformer(JNIEnv* env, jlong j_transformer);
  void RemoveFrameMetadataTransformer(JNIEnv* env, jlong j_transformer);

  void SetState(JNIEnv* env, jboolean j_is_live);

  void AdaptOutputFormat(JNIEnv* env,
                         jint j_landscape_width,
                         jint j_landscape_height,
                         const JavaRef<jobject>& j_max_landscape_pixel_count,
                         jint j_portrait_width,
                         jint j_portrait_height,
                         const JavaRef<jobject>& j_max_portrait_pixel_count,
                         const JavaRef<jobject>& j_max_fps);

  void SetIsScreencast(JNIEnv* env, jboolean j_is_screencast);

 private:
  std::optional<Environment> env_;
  Clock& clock_;
  Thread* signaling_thread_;
  std::atomic<SourceState> state_;
  std::atomic<bool> is_screencast_;
  TimestampAligner timestamp_aligner_;
  const bool align_timestamps_;
  scoped_refptr<PendingTaskSafetyFlag> safety_;
  Mutex frame_metadata_mutex_;
  std::vector<scoped_refptr<FrameMetadataTransformer>>
      frame_metadata_transformers_ RTC_GUARDED_BY(frame_metadata_mutex_);
};

}  // namespace jni
}  // namespace webrtc

#endif  // API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_
