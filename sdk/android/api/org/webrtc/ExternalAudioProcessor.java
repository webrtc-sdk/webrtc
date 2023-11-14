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

package org.webrtc;

import java.nio.ByteBuffer;

import androidx.annotation.Nullable;
import org.webrtc.AudioProcessingFactory;


public class ExternalAudioProcessor implements AudioProcessingFactory {

  public static interface AudioProcessing {
    @CalledByNative("AudioProcessing")
    void Initialize(int sampleRateHz, int numChannels);
    @CalledByNative("AudioProcessing")
    void Reset(int newRate);
    @CalledByNative("AudioProcessing")
    void Process(int numBans, int numFrames, ByteBuffer buffer);
  }

  private long apmPtr;
  private long capturePostProcessingPtr;
  private long renderPreProcessingPtr;

  public ExternalAudioProcessor() {
    apmPtr = nativeGetDefaultApm();
    capturePostProcessingPtr = 0;
    renderPreProcessingPtr = 0;
  }

  @Override
  public long createNative() {
    if(apmPtr == 0) {
      apmPtr = nativeGetDefaultApm();
    }
    return apmPtr;
  }

  public void SetCapturePostProcessing(@Nullable AudioProcessing processing) {
    checkExternalAudioProcessorExists();
    long newPtr = nativeSetCapturePostProcessing(processing);
    if (capturePostProcessingPtr != 0) {
      JniCommon.nativeReleaseRef(capturePostProcessingPtr);
      capturePostProcessingPtr = 0;
    }
    capturePostProcessingPtr = newPtr;
  }

  public void SetRenderPreProcessing(@Nullable AudioProcessing processing) {
    checkExternalAudioProcessorExists();
    long newPtr = nativeSetRenderPreProcessing(processing);
    if (renderPreProcessingPtr != 0) {
      JniCommon.nativeReleaseRef(renderPreProcessingPtr);
      renderPreProcessingPtr = 0;
    }
    renderPreProcessingPtr = newPtr;
  }
  
  public void SetBypassFlagForCapturePost( boolean bypass) {
    checkExternalAudioProcessorExists();
    nativeSetBypassFlagForCapturePost(bypass);
  }

  public void SetBypassFlagForRenderPre( boolean bypass) {
    checkExternalAudioProcessorExists();
    nativeSetBypassFlagForRenderPre(bypass);
  }

  public void Destroy() {
    checkExternalAudioProcessorExists();
    if (renderPreProcessingPtr != 0) {
      JniCommon.nativeReleaseRef(renderPreProcessingPtr);
      renderPreProcessingPtr = 0;
    }
    if (capturePostProcessingPtr != 0) {
      JniCommon.nativeReleaseRef(capturePostProcessingPtr);
      capturePostProcessingPtr = 0;
    }
    nativeDestroy();
    apmPtr = 0;
  }

  private void checkExternalAudioProcessorExists() {
    if (apmPtr == 0) {
      throw new IllegalStateException("ExternalAudioProcessor has been disposed.");
    }
  }

  private static native long nativeGetDefaultApm();
  private static native long nativeSetCapturePostProcessing(AudioProcessing processing);
  private static native long nativeSetRenderPreProcessing(AudioProcessing processing);
  private static native void nativeSetBypassFlagForCapturePost(boolean bypass);
  private static native void nativeSetBypassFlagForRenderPre(boolean bypass);
  private static native void nativeDestroy();
}
