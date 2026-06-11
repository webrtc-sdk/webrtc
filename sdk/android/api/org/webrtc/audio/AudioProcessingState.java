/*
 *  Copyright 2026 LiveKit
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.webrtc.audio;

import org.webrtc.CalledByNative;

/**
 * Diagnostic snapshot of the resolved audio processing state for the shared audio processing
 * module. The module is owned by the peer connection factory and shared engine-wide, so this
 * reflects factory-scoped state rather than the state of a single track or peer connection.
 *
 * <p>Device-level platform processing detail - topology, raw per-effect availability - is
 * intentionally not embedded here; read
 * {@code JavaAudioDeviceModule.getPlatformAudioProcessingState()} instead.
 */
public final class AudioProcessingState {
  public final boolean hasAudioProcessingModule;

  public final AudioProcessingComponentState echoCancellation;
  public final AudioProcessingComponentState noiseSuppression;
  public final AudioProcessingComponentState autoGainControl;
  public final AudioProcessingComponentState highPassFilter;

  @CalledByNative
  AudioProcessingState(boolean hasAudioProcessingModule,
      AudioProcessingComponentState echoCancellation,
      AudioProcessingComponentState noiseSuppression,
      AudioProcessingComponentState autoGainControl,
      AudioProcessingComponentState highPassFilter) {
    this.hasAudioProcessingModule = hasAudioProcessingModule;
    this.echoCancellation = echoCancellation;
    this.noiseSuppression = noiseSuppression;
    this.autoGainControl = autoGainControl;
    this.highPassFilter = highPassFilter;
  }
}
