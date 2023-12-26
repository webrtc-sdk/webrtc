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


public class ExternalAudioProcessingFactory implements AudioProcessingFactory {

  public static interface AudioProcessing {
    @CalledByNative("AudioProcessing")
    void initialize(int sampleRateHz, int numChannels);
    /** Called when the processor should be reset with a new sample rate. */  
    @CalledByNative("AudioProcessing")
    void reset(int newRate);
    /**  
     * Processes the given capture or render signal. NOTE: `buffer.data` will be  
     * freed once this function returns so callers who want to use the data  
     * asynchronously must make sure to copy it first.  
     */
    @CalledByNative("AudioProcessing")
    void process(int numBands, int numFrames, ByteBuffer buffer);
  }

  private long apmPtr;
  private long capturePostProcessingPtr;
  private long renderPreProcessingPtr;

  public ExternalAudioProcessingFactory() {
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

  public void setCapturePostProcessing(@Nullable AudioProcessing processing) {
    checkExternalAudioProcessorExists();
    long newPtr = nativeSetCapturePostProcessing(processing);
    if (capturePostProcessingPtr != 0) {
      JniCommon.nativeReleaseRef(capturePostProcessingPtr);
      capturePostProcessingPtr = 0;
    }
    capturePostProcessingPtr = newPtr;
  }

  public void setRenderPreProcessing(@Nullable AudioProcessing processing) {
    checkExternalAudioProcessorExists();
    long newPtr = nativeSetRenderPreProcessing(processing);
    if (renderPreProcessingPtr != 0) {
      JniCommon.nativeReleaseRef(renderPreProcessingPtr);
      renderPreProcessingPtr = 0;
    }
    renderPreProcessingPtr = newPtr;
  }
  
  public void setBypassFlagForCapturePost( boolean bypass) {
    checkExternalAudioProcessorExists();
    nativeSetBypassFlagForCapturePost(bypass);
  }

  public void setBypassFlagForRenderPre( boolean bypass) {
    checkExternalAudioProcessorExists();
    nativeSetBypassFlagForRenderPre(bypass);
  }

  public void destroy() {
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
