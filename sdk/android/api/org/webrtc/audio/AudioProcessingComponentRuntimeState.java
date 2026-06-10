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

import androidx.annotation.Nullable;

/**
 * Runtime state of a single audio processing component (echo cancellation,
 * noise suppression, auto gain control or high-pass filter). Nullable fields
 * are tri-state: {@code null} means the value is unknown.
 */
public final class AudioProcessingComponentRuntimeState {
  public final @Nullable Boolean isRequestedEnabled;
  public final @Nullable AudioProcessingMode requestedMode;
  public final @Nullable Boolean isResolvedSoftwareEnabled;
  public final @Nullable Boolean isSoftwareEnabled;
  public final boolean isPlatformAvailable;
  public final @Nullable Boolean isPlatformRequested;
  public final @Nullable Boolean isPlatformActive;
  public final AudioProcessingImplementation effective;

  public AudioProcessingComponentRuntimeState(@Nullable Boolean isRequestedEnabled,
      @Nullable AudioProcessingMode requestedMode, @Nullable Boolean isResolvedSoftwareEnabled,
      @Nullable Boolean isSoftwareEnabled, boolean isPlatformAvailable,
      @Nullable Boolean isPlatformRequested, @Nullable Boolean isPlatformActive,
      AudioProcessingImplementation effective) {
    this.isRequestedEnabled = isRequestedEnabled;
    this.requestedMode = requestedMode;
    this.isResolvedSoftwareEnabled = isResolvedSoftwareEnabled;
    this.isSoftwareEnabled = isSoftwareEnabled;
    this.isPlatformAvailable = isPlatformAvailable;
    this.isPlatformRequested = isPlatformRequested;
    this.isPlatformActive = isPlatformActive;
    this.effective = effective;
  }
}
