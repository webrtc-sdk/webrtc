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

/** Result returned when updating runtime audio processing options. */
public final class AudioProcessingOptionsResult {
  /** Order must match {@code webrtc::AudioProcessingOptionsResultCode}. */
  public enum Code {
    APPLIED,
    STORED,
    REJECTED_REMOTE_TRACK,
    REJECTED_INVALID_COMBINATION,
    REJECTED_PLATFORM_UNAVAILABLE,
    APPLY_FAILED
  }

  public final Code code;
  public final String message;

  public AudioProcessingOptionsResult(Code code, String message) {
    this.code = code;
    this.message = message;
  }

  public boolean isSuccess() {
    return code == Code.APPLIED || code == Code.STORED;
  }

  public static AudioProcessingOptionsResult stored() {
    return new AudioProcessingOptionsResult(Code.STORED, "");
  }

  public static AudioProcessingOptionsResult rejected(Code code, String message) {
    return new AudioProcessingOptionsResult(code, message);
  }

  public static AudioProcessingOptionsResult fromNativeCode(int nativeCode) {
    return fromNativeCodeAndMessage(nativeCode, "");
  }

  public static AudioProcessingOptionsResult fromNativeResult(String nativeResult) {
    if (nativeResult == null) {
      return rejected(Code.APPLY_FAILED, "Missing native audio processing result");
    }
    int separator = nativeResult.indexOf('\n');
    String nativeCode = separator < 0 ? nativeResult : nativeResult.substring(0, separator);
    String message = separator < 0 ? "" : nativeResult.substring(separator + 1);
    try {
      return fromNativeCodeAndMessage(Integer.parseInt(nativeCode), message);
    } catch (NumberFormatException e) {
      return rejected(Code.APPLY_FAILED, "Invalid native audio processing result");
    }
  }

  private static AudioProcessingOptionsResult fromNativeCodeAndMessage(
      int nativeCode, String message) {
    Code[] values = Code.values();
    if (nativeCode < 0 || nativeCode >= values.length) {
      return rejected(Code.APPLY_FAILED, "Unknown native audio processing result code");
    }
    Code code = values[nativeCode];
    if (message == null || message.isEmpty()) {
      message = defaultMessage(code);
    }
    return new AudioProcessingOptionsResult(code, message);
  }

  private static String defaultMessage(Code code) {
    switch (code) {
      case APPLIED:
      case STORED:
        return "";
      case REJECTED_REMOTE_TRACK:
        return "Audio processing options can only be set on local audio tracks";
      case REJECTED_INVALID_COMBINATION:
        return "Audio processing options contain an invalid platform/software combination";
      case REJECTED_PLATFORM_UNAVAILABLE:
        return "Requested platform audio processing is not available";
      case APPLY_FAILED:
      default:
        return "Failed to apply audio processing options";
    }
  }
}
