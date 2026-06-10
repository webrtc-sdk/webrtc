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
 * Diagnostic snapshot of the requested vs. resolved audio processing state for
 * the shared audio processing module. The module is owned by the peer
 * connection factory and shared across every peer connection it creates, so
 * this reflects factory-scoped state.
 */
public final class AudioProcessingRuntimeState {
  private static final int OPTIONAL_UNKNOWN = -1;
  private static final int TOP_LEVEL_FIELD_COUNT = 5;
  private static final int COMPONENT_FIELD_COUNT = 8;
  private static final int BUILT_IN_COMPONENT_FIELD_COUNT = 3;
  private static final int FIELD_COUNT =
      TOP_LEVEL_FIELD_COUNT + COMPONENT_FIELD_COUNT * 4 + 1 + BUILT_IN_COMPONENT_FIELD_COUNT * 3;

  public final JavaAudioDeviceModule.BuiltInAudioProcessingTopology topology;
  public final boolean hasAudioProcessingModule;
  public final boolean hasAudioProcessingConfig;
  public final boolean hasRequestedAudioProcessingOptions;
  public final boolean hasResolvedAudioProcessingOptions;
  public final AudioProcessingComponentRuntimeState echoCancellation;
  public final AudioProcessingComponentRuntimeState noiseSuppression;
  public final AudioProcessingComponentRuntimeState autoGainControl;
  public final AudioProcessingComponentRuntimeState highPassFilter;
  public final JavaAudioDeviceModule.BuiltInAudioProcessingState builtIn;

  public AudioProcessingRuntimeState(
      JavaAudioDeviceModule.BuiltInAudioProcessingTopology topology,
      boolean hasAudioProcessingModule,
      boolean hasAudioProcessingConfig,
      boolean hasRequestedAudioProcessingOptions,
      boolean hasResolvedAudioProcessingOptions,
      AudioProcessingComponentRuntimeState echoCancellation,
      AudioProcessingComponentRuntimeState noiseSuppression,
      AudioProcessingComponentRuntimeState autoGainControl,
      AudioProcessingComponentRuntimeState highPassFilter,
      JavaAudioDeviceModule.BuiltInAudioProcessingState builtIn) {
    this.topology = topology;
    this.hasAudioProcessingModule = hasAudioProcessingModule;
    this.hasAudioProcessingConfig = hasAudioProcessingConfig;
    this.hasRequestedAudioProcessingOptions = hasRequestedAudioProcessingOptions;
    this.hasResolvedAudioProcessingOptions = hasResolvedAudioProcessingOptions;
    this.echoCancellation = echoCancellation;
    this.noiseSuppression = noiseSuppression;
    this.autoGainControl = autoGainControl;
    this.highPassFilter = highPassFilter;
    this.builtIn = builtIn;
  }

  /**
   * Deserializes the value layout produced by the JNI serializer
   * ({@code AudioProcessingRuntimeStateToJavaValues} in
   * {@code sdk/android/src/jni/pc/peer_connection_factory.cc}). Called by
   * {@code PeerConnectionFactory#getAudioProcessingRuntimeState()}.
   */
  public static AudioProcessingRuntimeState fromNative(int[] values) {
    if (values.length != FIELD_COUNT) {
      throw new IllegalStateException(
          "Unexpected audio processing runtime state field count: " + values.length);
    }
    int offset = 0;
    JavaAudioDeviceModule.BuiltInAudioProcessingTopology topology =
        topologyFromNative(values[offset++]);
    boolean hasAudioProcessingModule = values[offset++] != 0;
    boolean hasAudioProcessingConfig = values[offset++] != 0;
    boolean hasRequestedAudioProcessingOptions = values[offset++] != 0;
    boolean hasResolvedAudioProcessingOptions = values[offset++] != 0;
    AudioProcessingComponentRuntimeState echoCancellation = componentFromNative(values, offset);
    offset += COMPONENT_FIELD_COUNT;
    AudioProcessingComponentRuntimeState noiseSuppression = componentFromNative(values, offset);
    offset += COMPONENT_FIELD_COUNT;
    AudioProcessingComponentRuntimeState autoGainControl = componentFromNative(values, offset);
    offset += COMPONENT_FIELD_COUNT;
    AudioProcessingComponentRuntimeState highPassFilter = componentFromNative(values, offset);
    offset += COMPONENT_FIELD_COUNT;
    JavaAudioDeviceModule.BuiltInAudioProcessingState builtIn =
        builtInStateFromNative(values, offset);
    return new AudioProcessingRuntimeState(
        topology, hasAudioProcessingModule, hasAudioProcessingConfig,
        hasRequestedAudioProcessingOptions, hasResolvedAudioProcessingOptions, echoCancellation,
        noiseSuppression, autoGainControl, highPassFilter, builtIn);
  }

  private static AudioProcessingComponentRuntimeState componentFromNative(
      int[] values, int offset) {
    return new AudioProcessingComponentRuntimeState(optionalBoolFromNative(values[offset]),
        audioProcessingModeFromNative(values[offset + 1]),
        optionalBoolFromNative(values[offset + 2]), optionalBoolFromNative(values[offset + 3]),
        values[offset + 4] != 0, optionalBoolFromNative(values[offset + 5]),
        optionalBoolFromNative(values[offset + 6]),
        audioProcessingImplementationFromNative(values[offset + 7]));
  }

  private static JavaAudioDeviceModule.BuiltInAudioProcessingState builtInStateFromNative(
      int[] values, int offset) {
    JavaAudioDeviceModule.BuiltInAudioProcessingTopology topology =
        topologyFromNative(values[offset++]);
    JavaAudioDeviceModule.BuiltInAudioProcessingComponentState echoCancellation =
        builtInComponentFromNative(values, offset);
    offset += BUILT_IN_COMPONENT_FIELD_COUNT;
    JavaAudioDeviceModule.BuiltInAudioProcessingComponentState noiseSuppression =
        builtInComponentFromNative(values, offset);
    offset += BUILT_IN_COMPONENT_FIELD_COUNT;
    JavaAudioDeviceModule.BuiltInAudioProcessingComponentState autoGainControl =
        builtInComponentFromNative(values, offset);
    return new JavaAudioDeviceModule.BuiltInAudioProcessingState(
        topology, echoCancellation, noiseSuppression, autoGainControl);
  }

  private static JavaAudioDeviceModule.BuiltInAudioProcessingComponentState
  builtInComponentFromNative(int[] values, int offset) {
    return new JavaAudioDeviceModule.BuiltInAudioProcessingComponentState(values[offset] != 0,
        optionalBoolFromNative(values[offset + 1]), optionalBoolFromNative(values[offset + 2]));
  }

  private static JavaAudioDeviceModule.BuiltInAudioProcessingTopology topologyFromNative(
      int value) {
    return JavaAudioDeviceModule.BuiltInAudioProcessingTopology.values()[value];
  }

  private static @Nullable AudioProcessingMode audioProcessingModeFromNative(int value) {
    return value == OPTIONAL_UNKNOWN ? null : AudioProcessingMode.values()[value];
  }

  private static AudioProcessingImplementation audioProcessingImplementationFromNative(
      int value) {
    return AudioProcessingImplementation.values()[value];
  }

  private static @Nullable Boolean optionalBoolFromNative(int value) {
    return value == OPTIONAL_UNKNOWN ? null : value != 0;
  }
}
