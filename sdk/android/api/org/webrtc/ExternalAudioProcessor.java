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

import org.webrtc.AudioProcessingFactory;

public class ExternalAudioProcessor implements AudioProcessingFactory {
  private long nativeAudioProcessor;

  public ExternalAudioProcessor() {
    nativeAudioProcessor = nativeGetDefaultApm();
  }

  @Override
  public long createNative() {
    if(nativeAudioProcessor == 0) {
      nativeAudioProcessor = nativeGetDefaultApm();
    }
    return nativeAudioProcessor;
  }

  public void SetCapturePostProcessing(long extProcessor) {
    nativeSetCapturePostProcessing(nativeAudioProcessor, extProcessor);
  }

  public void SetRenderPreProcessing(long extProcessor) {
    nativeSetRenderPreProcessing(nativeAudioProcessor, extProcessor);
  }

  public void SetBypassFlagForCapturePostProcessing(boolean bypass) {
    nativeSetBypassFlagForCapturePostProcessing(nativeAudioProcessor, bypass);
  }

  public void SetBypassFlagForRenderPreProcessing(boolean bypass) {
    nativeSetBypassFlagForRenderPreProcessing(nativeAudioProcessor, bypass);
  }

  public void Destroy() {
    nativeDestroy(nativeAudioProcessor);
    nativeAudioProcessor = 0;
  }

  private static native long nativeGetDefaultApm();
  private static native boolean nativeSetCapturePostProcessing(long nativeAudioProcessor, long nativeProcessor);
  private static native boolean nativeSetRenderPreProcessing(long nativeAudioProcessor, long nativeProcessor);
  private static native boolean nativeSetBypassFlagForCapturePostProcessing(long nativeAudioProcessor, boolean bypass);
  private static native boolean nativeSetBypassFlagForRenderPreProcessing(long nativeAudioProcessor, boolean bypass);
  private static native void nativeDestroy(long nativeAudioProcessor);
}
