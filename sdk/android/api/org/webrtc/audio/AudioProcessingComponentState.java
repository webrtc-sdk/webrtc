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
import org.webrtc.CalledByNative;

/**
 * Diagnostic state of one audio processing component (echo cancellation, noise suppression, auto
 * gain control or high-pass filter), observed at three stages of one pipeline: requested (caller
 * intent) -> resolved (the engine's per-path decision) -> active (live truth), with
 * {@link #effective} as the merged verdict.
 *
 * <p>Example: echo cancellation enabled with automatic mode on a device with platform AEC reads
 * {@code requested = {enabled, AUTOMATIC}}, {@code isSoftwareResolved = false},
 * {@code isPlatformResolved = true}, {@code isPlatformActive = true},
 * {@code effective = PLATFORM}. On a device without platform AEC the same request reads
 * {@code isSoftwareResolved = true}, {@code isSoftwareActive = true},
 * {@code effective = SOFTWARE}. {@code isPlatformResolved = true} with
 * {@code isPlatformActive = false} means the OS has not finished applying the request or rejected
 * it.
 */
public final class AudioProcessingComponentState {
  /**
   * The caller's most recent request for this component, as passed to
   * {@code AudioTrack.setAudioProcessingOptions}. Null when no audio processing options have ever
   * been applied, which distinguishes "nobody asked" from "asked for disabled". The mode reads
   * {@code AUTOMATIC} when the request did not specify one.
   */
  @Nullable public final AudioProcessingComponentOptions requested;

  /**
   * Whether the resolver decided the WebRTC software (APM) implementation should run for this
   * component, after weighing the requested mode against platform availability, coupling, and
   * policy. Automatic mode resolves to software exactly when the platform path is unavailable or
   * disallowed. False also covers "the resolver has not run yet" - check {@link #requested} to
   * tell the two apart.
   */
  public final boolean isSoftwareResolved;

  /**
   * Whether APM's live configuration currently has this component enabled. Normally equals
   * {@link #isSoftwareResolved} once options are applied; differs while an apply is in flight, if
   * applying failed, or if something else has since reconfigured the shared APM.
   */
  public final boolean isSoftwareActive;

  /**
   * Whether this device/OS offers a built-in implementation of this component at all. Capability
   * only - says nothing about whether it is in use.
   */
  public final boolean isPlatformAvailable;

  /**
   * Whether the resolver decided the platform implementation should run, as submitted to the OS.
   * Unlike the software path, the OS owns the outcome: it can decline, defer, or couple this with
   * another component.
   */
  public final boolean isPlatformResolved;

  /**
   * Whether the device reports the platform implementation actually running right now. Lags
   * {@link #isPlatformResolved} during engine transitions; stays false if the OS rejected the
   * request or the input path is not configured.
   */
  public final boolean isPlatformActive;

  /**
   * The verdict: which implementation is in effect for this component right now. Derived from the
   * active states (active wins over resolved when they disagree). {@code UNKNOWN} when the
   * pipeline state cannot be determined.
   */
  public final AudioProcessingImplementation effective;

  @CalledByNative
  AudioProcessingComponentState(@Nullable AudioProcessingComponentOptions requested,
      boolean isSoftwareResolved, boolean isSoftwareActive, boolean isPlatformAvailable,
      boolean isPlatformResolved, boolean isPlatformActive,
      AudioProcessingImplementation effective) {
    this.requested = requested;
    this.isSoftwareResolved = isSoftwareResolved;
    this.isSoftwareActive = isSoftwareActive;
    this.isPlatformAvailable = isPlatformAvailable;
    this.isPlatformResolved = isPlatformResolved;
    this.isPlatformActive = isPlatformActive;
    this.effective = effective;
  }
}
