# WebRTC Upgrade Manual Testing Cheatsheet

> Comprehensive QA checklist for validating a WebRTC milestone upgrade.
>
> This document is scoped to the `webrtc` repository. Test using any LiveKit example app that exposes the relevant controls.

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [TC-1: Connection & Handshake](#tc-1-connection--handshake)
- [TC-2: Audio - Capture & Playback](#tc-2-audio---capture--playback)
- [TC-3: Audio - Processing & Device Routing](#tc-3-audio---processing--device-routing)
- [TC-4: Video - Camera Capture](#tc-4-video---camera-capture)
- [TC-5: Video Codecs](#tc-5-video-codecs)
- [TC-6: Simulcast & SVC](#tc-6-simulcast--svc)
- [TC-7: Screen Sharing](#tc-7-screen-sharing)
- [TC-8: Data Channels](#tc-8-data-channels)
- [TC-9: Connectivity & Network Resilience](#tc-9-connectivity--network-resilience)
- [TC-10: End-to-End Encryption (E2EE)](#tc-10-end-to-end-encryption-e2ee)
- [TC-11: Statistics & Observability](#tc-11-statistics--observability)
- [TC-12: Lifecycle & Stress Testing](#tc-12-lifecycle--stress-testing)
- [TC-13: Platform-Specific Regressions](#tc-13-platform-specific-regressions)
- [Appendix A: Known Regression Hotspots](#appendix-a-known-regression-hotspots)

---

## Prerequisites

| Item | Details |
|------|---------|
| **LiveKit Server** | Running instance with a test room |
| **Test Token** | Valid JWT token with publish + subscribe permissions |
| **Devices** | At least one iOS device, one Android device, one macOS machine |
| **Network Tools** | Network Link Conditioner or equivalent for bandwidth throttling |
| **Peer** | A second client (browser or another device) to act as remote participant |
| **Stats Overlay** | Enable stats reporting and info overlay in the test app |

---

## TC-1: Connection & Handshake

> **Goal:** Verify ICE negotiation, DTLS handshake, and SDP exchange work after the WebRTC upgrade.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 1.1 | Basic connect | Connect to server with valid token | Connection established, room view appears, server region shown | |
| 1.2 | Connect with auto-subscribe ON | Enable auto-subscribe before connecting | Remote tracks appear automatically when peer publishes | |
| 1.3 | Connect with auto-subscribe OFF | Disable auto-subscribe before connecting | Remote tracks do NOT appear until manually subscribed | |
| 1.4 | Cancel connect | Initiate connection, then immediately cancel | Connection aborts cleanly, no crash | |
| 1.5 | Invalid server URL | Connect to a non-existent server URL | Graceful error, no crash | |
| 1.6 | Expired token | Use an expired JWT | Graceful error with descriptive message | |

---

## TC-2: Audio - Capture & Playback

> **Goal:** Verify microphone capture, remote audio playback, and basic audio pipeline integrity.
>
> **Risk level:** CRITICAL -- audio pipeline regressions are the #1 source of breakage in WebRTC upgrades.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 2.1 | Publish microphone | Publish local audio track | Local audio track published, remote peer hears audio | |
| 2.2 | Unpublish microphone | Unpublish local audio track | Audio track removed, remote peer no longer hears audio | |
| 2.3 | Mute/unmute audio | Publish mic, then mute | Remote peer hears silence when muted, audio resumes on unmute | |
| 2.4 | Mic volume control | Adjust mic volume from 0.0 to 1.0 | Volume changes reflected in outbound audio level | |
| 2.5 | App volume control | Adjust app volume while receiving remote audio | Remote audio volume changes accordingly | |
| 2.6 | No echo detected | Both peers publish audio in same room | No audible echo on either end (AEC working) | |
| 2.7 | Noise suppression | Publish audio with background noise | Background noise is reduced on remote end | |
| 2.8 | Voice processing toggle | Toggle voice processing ON/OFF | Audio quality changes appropriately; no crash | |
| 2.9 | Voice processing bypass | Bypass voice processing pipeline | Raw audio sent without processing | |
| 2.10 | AGC toggle | Toggle Auto Gain Control | Gain auto-adjusts when enabled; stays fixed when disabled | |
| 2.11 | Mute mode - voice processing | Mute via voice processing path | Fast mute, mic indicator turns off | |
| 2.12 | Mute mode - restart | Mute via audio session restart path | Slower mute, audio session reconfigures | |
| 2.13 | Mute mode - input mixer | Mute via input mixer path | Fast mute, mic indicator stays on | |
| 2.14 | Audio ducking levels | Change ducking level (Default/Min/Mid/Max) | Audio from other apps ducks accordingly during call | |

---

## TC-3: Audio - Processing & Device Routing

> **Goal:** Verify audio device enumeration, selection, and audio routing.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 3.1 | Input device enumeration | Open input device picker | Lists available microphones (built-in, headset, Bluetooth) | |
| 3.2 | Output device enumeration | Open output device picker | Lists available output devices (speaker, headphones, Bluetooth) | |
| 3.3 | Switch input device | Select different microphone | Audio captured from new device; remote peer hears change | |
| 3.4 | Switch output device | Select different output device | Remote audio plays through selected device | |
| 3.5 | Prefer speaker output | Route audio to speaker instead of earpiece | Audio plays through speaker | |
| 3.6 | Bluetooth headset connect | Connect Bluetooth headset during active call | Audio routes to Bluetooth device automatically | |
| 3.7 | Bluetooth headset disconnect | Disconnect Bluetooth headset during call | Audio falls back to built-in speaker/mic without interruption | |
| 3.8 | Wired headphones plug/unplug | Plug/unplug wired headphones during call | Audio route changes seamlessly, no audio dropout | |

---

## TC-4: Video - Camera Capture

> **Goal:** Verify camera enumeration, capture at various resolutions/framerates, and camera controls.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 4.1 | Publish camera (default) | Publish local video track | Local video visible, remote peer sees video | |
| 4.2 | Unpublish camera | Unpublish local video track | Video track removed, remote sees placeholder | |
| 4.3 | Switch camera (front/back) | Toggle between front and back cameras | Camera flips; no freeze or black frame | |
| 4.4 | Camera device selection | Select specific camera device | Video captured from selected device | |
| 4.5 | Publish at 720p | Set dimensions to 720p, publish | Stats show ~1280x720 outbound resolution | |
| 4.6 | Publish at 1080p | Set dimensions to 1080p, publish | Stats show ~1920x1080 outbound resolution | |
| 4.7 | Publish at 360p | Set dimensions to 360p, publish | Stats show ~640x360 outbound resolution | |
| 4.8 | Publish at custom resolution | Set custom width/height, publish | Stats show custom resolution | |
| 4.9 | Max FPS control | Set max FPS to 15, publish | Stats show FPS capped at ~15 | |
| 4.10 | Max FPS at 30 | Set max FPS to 30, publish | Stats show FPS up to ~30 | |
| 4.11 | Video mirror toggle | Toggle mirror mode | Local video view flips horizontally | |
| 4.12 | Render mode toggle | Switch rendering mode (e.g., sample buffer vs Metal) | Video rendering switches mode without artifacts | |
| 4.13 | Background blur | Enable background blur video processor | Background blurs in real-time; person remains sharp | |

---

## TC-5: Video Codecs

> **Goal:** Verify all video codecs negotiate, encode, and decode correctly after the WebRTC upgrade.
>
> **Risk level:** HIGH -- codec factory and encoder/decoder internals change frequently between milestones.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 5.1 | VP8 encode/decode | Set primary codec to VP8, publish | Stats show VP8 codec; video renders on remote | |
| 5.2 | H.264 encode/decode | Set primary codec to H264, publish | Stats show H264 codec; video renders on remote | |
| 5.3 | VP9 encode/decode | Set primary codec to VP9, publish | Stats show VP9 codec; video renders on remote | |
| 5.4 | AV1 encode/decode | Set primary codec to AV1, publish | Stats show AV1 codec; video renders on remote | |
| 5.5 | Auto codec selection | Set primary codec to Auto, publish | Appropriate codec selected; stats show codec | |
| 5.6 | Backup codec negotiation | Set primary + backup codec (e.g., VP8 + H264) | Backup codec negotiated; switching possible under constraint | |
| 5.7 | Hardware encoder active | Check stats for encoder implementation | Shows hardware encoder (e.g., VideoToolbox, MediaCodec) where available | |
| 5.8 | Hardware decoder active | Check remote participant stats for decoder | Shows hardware decoder where available | |
| 5.9 | Codec switch mid-session | Change codec preference mid-call (if supported) | Codec switches without call drop; brief quality blip acceptable | |
| 5.10 | H.264 constrained baseline | Publish H264, inspect SDP | Profile level ID shows constrained baseline (42e01f) | |

---

## TC-6: Simulcast & SVC

> **Goal:** Verify simulcast layer publishing, SVC scalability modes, and dynamic layer switching.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 6.1 | Simulcast enabled (VP8) | Enable simulcast, publish VP8 at 720p+ | Stats show 3 RIDs (q, h, f) with different resolutions | |
| 6.2 | Simulcast enabled (H264) | Enable simulcast, publish H264 at 720p+ | 3 simulcast layers visible in stats | |
| 6.3 | Simulcast at low resolution | Enable simulcast, publish at 360p | Only 2 layers (resolution too low for 3); no crash | |
| 6.4 | Simulcast layer switching | Subscribe from remote, vary rendered video size | Received layer switches (check RID in stats) based on subscriber viewport | |
| 6.5 | Dynacast toggle | Enable dynacast | Unsubscribed layers are paused server-side (verify via stats) | |
| 6.6 | Adaptive stream | Enable adaptive stream | Video quality adapts to rendered view size automatically | |
| 6.7 | VP9 SVC (L3T3_KEY) | Publish VP9 with SVC scalability mode | Stats show single encoding with scalabilityMode; layers delivered correctly | |
| 6.8 | AV1 SVC | Publish AV1 with SVC | Similar to VP9 SVC; verify layer decoding on remote | |
| 6.9 | Simulcast disabled | Disable simulcast, publish | Only single quality layer; no RID in stats | |
| 6.10 | Subscribe/unsubscribe layers | Subscribe/unsubscribe video per participant | Subscription state changes correctly; track appears/disappears | |

---

## TC-7: Screen Sharing

> **Goal:** Verify screen capture, window selection, and screen share audio.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 7.1 | Screen share - entire screen | Start screen share, select entire screen | Selected display is shared; remote sees screen content | |
| 7.2 | Screen share - window | Start screen share, select application window | Selected window is shared; remote sees window content | |
| 7.3 | Screen share stop | Stop screen sharing | Screen share track removed; remote no longer sees screen | |
| 7.4 | Screen share + camera simultaneous | Publish both camera and screen share | Both tracks visible on remote; no conflict | |
| 7.5 | Screen share resolution | Check stats during screen share | Resolution matches configured screen share presets | |
| 7.6 | Screen share FPS | Check FPS in stats during screen share | FPS matches configured screen share framerate | |
| 7.7 | Screen share with audio | Enable app audio capture during screen share | Remote hears app audio mixed with screen share | |
| 7.8 | Unpublish all | Unpublish all tracks with camera + screen share active | All tracks removed cleanly; no orphaned tracks | |

---

## TC-8: Data Channels

> **Goal:** Verify reliable and lossy data channels work for messaging.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 8.1 | Send text message | Send a text message via data channel | Message appears on remote peer | |
| 8.2 | Receive text message | Remote peer sends message | Message received locally | |
| 8.3 | Rapid messages | Send 20+ messages quickly | All messages delivered in order; no dropped messages | |
| 8.4 | Long message | Send a message with 1000+ characters | Message delivered intact; no truncation | |
| 8.5 | Data channel after reconnect | Disconnect and reconnect, then send message | Data channels re-established; messages work | |

---

## TC-9: Connectivity & Network Resilience

> **Goal:** Verify ICE, TURN, reconnection, and network transition handling.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 9.1 | Quick reconnect | Trigger quick reconnect (ICE restart) | Connection briefly interrupts, then resumes; tracks recover | |
| 9.2 | Full reconnect | Trigger full reconnect (new PeerConnection) | Full re-establishment of connection; all tracks restored | |
| 9.3 | Node failure simulation | Simulate server node failure | Handles failure gracefully; reconnects | |
| 9.4 | Server leave simulation | Simulate server-initiated disconnect | Client handles disconnect cleanly | |
| 9.5 | Migration simulation | Simulate server migration | Client migrates to new server node; tracks continue | |
| 9.6 | Force TCP | Force ICE transport over TCP | Connection established over TCP; audio/video work | |
| 9.7 | Force TLS | Force ICE transport over TLS | Connection established over TLS; audio/video work | |
| 9.8 | WiFi to cellular transition | Start on WiFi, switch to cellular | ICE restart occurs; call recovers within a few seconds | |
| 9.9 | Cellular to WiFi transition | Start on cellular, switch to WiFi | ICE restart occurs; call recovers | |
| 9.10 | Airplane mode toggle | Enable airplane mode for 5 seconds, then disable | Client reconnects after network returns | |
| 9.11 | Bandwidth throttle | Limit bandwidth to 500 Kbps | Video quality degrades gracefully; audio continues; no crash | |
| 9.12 | High latency (300ms) | Add 300ms latency | Audio/video still functional with increased delay | |
| 9.13 | High packet loss (10%) | Add 10% packet loss | Audio may have glitches; video may freeze briefly; no crash | |

---

## TC-10: End-to-End Encryption (E2EE)

> **Goal:** Verify E2EE key management, encrypted media, and key rotation.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 10.1 | Enable E2EE on connect | Set E2EE key, enable E2EE, connect | Encryption status shows encrypted on all tracks | |
| 10.2 | E2EE audio | Publish audio with E2EE enabled | Remote peer with same key hears audio clearly | |
| 10.3 | E2EE video (VP8) | Publish VP8 video with E2EE | Remote peer with same key sees video | |
| 10.4 | E2EE video (H264) | Publish H264 video with E2EE | Remote peer sees video (historically fragile -- verify no freezes) | |
| 10.5 | E2EE video (VP9) | Publish VP9 video with E2EE | Remote peer sees video | |
| 10.6 | E2EE key mismatch | Connect two peers with different E2EE keys | Audio/video are garbled; encryption status shows mismatch | |
| 10.7 | Ratchet key | Rotate encryption key | Key rotates; existing peers continue receiving; new key used going forward | |
| 10.8 | Toggle E2EE mid-session | Toggle E2EE enabled/disabled during call | Encryption state changes; no crash or deadlock | |
| 10.9 | E2EE with simulcast | Enable both E2EE and simulcast | All simulcast layers encrypted; remote decodes correctly | |

---

## TC-11: Statistics & Observability

> **Goal:** Verify WebRTC stats collection works correctly and reports expected metrics.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 11.1 | Stats overlay visible | Enable stats overlay | Each participant shows codec, bitrate, RID, dimensions | |
| 11.2 | Outbound codec info | Check local participant stats | Shows correct MIME type (e.g., video/VP8, video/H264) | |
| 11.3 | Outbound bitrate | Check local participant stats | Shows non-zero bitrate | |
| 11.4 | Outbound resolution | Check local participant stats | Shows correct width x height matching publish settings | |
| 11.5 | Inbound codec info | Check remote participant stats | Shows codec used by remote publisher | |
| 11.6 | Inbound bitrate | Check remote participant stats | Shows non-zero bitrate | |
| 11.7 | Quality limitation reason | Check stats during bandwidth throttle | Shows "bandwidth" limitation; during CPU load shows "cpu" | |
| 11.8 | Connection quality indicator | Observe connection quality per participant | Excellent/good/poor matches network conditions | |
| 11.9 | Simulcast RID display | Publish simulcast, check stats | RID (q/h/f) shown for each layer | |
| 11.10 | Speaker activity detection | Verify speaker update events | Speaking indicator triggers correctly based on audio activity | |

---

## TC-12: Lifecycle & Stress Testing

> **Goal:** Verify stability under rapid operations and edge cases.

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 12.1 | Rapid publish/unpublish | Toggle camera publish 10 times rapidly | No crash; track state consistent at end | |
| 12.2 | Rapid mute/unmute | Toggle mic mute 20 times rapidly | No crash; final mute state correct | |
| 12.3 | Rapid connect/disconnect | Connect and disconnect 5 times in succession | No crash; clean state after each cycle | |
| 12.4 | Publish all then unpublish all | Publish camera + mic + screen share, then unpublish all | All tracks removed cleanly; no orphaned resources | |
| 12.5 | Long duration call (30+ min) | Join a call and leave it running for 30+ minutes | No memory growth, no degradation, no disconnects | |
| 12.6 | Many participants (5+) | Join room with 5+ participants all publishing | All videos render; performance acceptable | |
| 12.7 | Background/foreground cycle | Send app to background, return to foreground | Audio continues in background; video resumes on foreground | |
| 12.8 | Phone call interruption | Receive phone call during active call | Audio pauses during phone call; resumes after ending phone call | |
| 12.9 | Track permissions allow | Allow all participant tracks | All remote tracks become subscribable | |
| 12.10 | Track permissions deny | Deny all participant tracks | All remote tracks unsubscribed | |

---

## TC-13: Platform-Specific Regressions

> **Goal:** Test areas known to regress on specific platforms after WebRTC upgrades.

### iOS / macOS

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 13.1 | AVAudioEngine lifecycle | Publish/unpublish audio multiple times | No audio engine crashes or orphaned sessions | |
| 13.2 | CallKit integration | Receive VoIP push, answer via CallKit | Audio session configures correctly; audio works | |
| 13.3 | Broadcast extension (ReplayKit) | Start screen share via broadcast extension | Screen frames transmitted via IPC; no crash | |
| 13.4 | Audio after phone call | Complete a phone call, return to WebRTC call | Audio pipeline recovers; AEC still working | |
| 13.5 | Background audio | Send app to background while in call | Audio continues playing and recording | |
| 13.6 | Camera + audio session conflict | Publish camera and audio simultaneously | Camera capturer does NOT reconfigure audio session | |
| 13.7 | visionOS AR camera | Publish AR camera on visionOS (if available) | AR camera frames captured and transmitted | |
| 13.8 | ScreenCaptureKit source picker (macOS) | Open screen share source picker | Shows all screens and windows with live previews | |
| 13.9 | Multiple displays (macOS) | Share specific display in multi-monitor setup | Correct display shared; others not captured | |

### Android

| ID | Test Case | Steps | Expected | P/F |
|----|-----------|-------|----------|-----|
| 13.10 | AEC enabled by default | Publish audio, check for echo | Echo cancellation active (`AudioOptions` defaults correct) | |
| 13.11 | AGC enabled by default | Publish audio at low volume, check remote | Auto gain control boosts quiet audio | |
| 13.12 | NS enabled by default | Publish audio with noise, check remote | Noise suppression is active | |
| 13.13 | SetMicrophoneMute no crash | Mute/unmute microphone | No assertion crash in `webrtc_voice_engine.cc` | |
| 13.14 | Camera2 capture | Publish video using Camera2 API | Video captures without crash | |
| 13.15 | Screen capture (MediaProjection) | Start screen share | Screen captured via MediaProjection; no crash | |
| 13.16 | Screen audio capture (Android Q+) | Enable screen audio during screen share | System audio captured and transmitted | |
| 13.17 | Audio focus handling | Another app plays audio during call | Audio focus managed correctly; call audio ducks or mixes | |
| 13.18 | USB audio device | Connect USB headset during call | Audio routes to USB device | |

---

## Appendix A: Known Regression Hotspots

Based on historical WebRTC milestone upgrades (m104, m114, m125, m137), these areas are most prone to regressions:

### Critical Priority (Test First)

1. **Android `AudioOptions` defaults** -- AEC/AGC/NS silently disabled when defaults changed from `true` to `false` in `webrtc_voice_engine.cc`
2. **iOS `AVAudioEngine` ADM** -- New audio device module lifecycle, start/stop timing, engine enable/disable sequencing
3. **iOS audio interruption recovery** -- Phone calls breaking audio pipeline; `AVAudioEngine` fails to restart after interruption
4. **`SetMicrophoneMute` assertion (Android)** -- Crash in `webrtc_voice_engine.cc` when `adm->SetMicrophoneMute(is_all_muted)` called on Android ADM

### High Priority

5. **H.264 + E2EE freezes** -- Frame cryptor interaction with H.264 NAL unit boundaries causing video freeze
6. **VP9 SVC layer mapping** -- `CodecSpecificInfoVP9` struct changes (deprecated `is_pod` etc.)
7. **Simulcast layer count at low resolutions** -- 2 vs 3 layer mapping errors causing unnecessary quality degradation
8. **Transceiver teardown crashes** -- Data channel not cleaned up before transceiver deinit
9. **Frame cryptor thread safety** -- Deadlocks during signaling/worker thread interactions in `SetDepacketizerToDecoderFrameTransformer`

### Medium Priority

10. **Field trial initialization** -- Multiple `RTCInitFieldTrialDictionary` calls unsafe; upstream Chrome promotes/removes field trials between milestones
11. **SDP attribute changes** -- New/modified `a=` lines for codec parameters, Dependency Descriptor extensions
12. **Audio ducking defaults** -- Default ducking level changes silently altering audio mixing behavior
13. **Stats API field changes** -- Deprecated or renamed statistics fields; new metric types for SVC/bandwidth estimation
14. **iOS camera capturer audio session** -- Camera capturer unintentionally reconfiguring audio session via `automaticallyConfiguresApplicationAudioSession`
15. **Audio mute default behavior** -- Default changed to stop recording when muted; requires explicit override to preserve previous behavior

### Reference PRs (webrtc-sdk/webrtc)

| PR | Area | Issue |
|----|------|-------|
| [#205](https://github.com/webrtc-sdk/webrtc/pull/205) | Android audio | `AudioOptions` defaults reverted for Android |
| [#178](https://github.com/webrtc-sdk/webrtc/pull/178) | iOS audio | `AVAudioEngine`-based ADM overhaul |
| [#207](https://github.com/webrtc-sdk/webrtc/pull/207) | iOS audio | Camera capturer audio session conflict |
| [#206](https://github.com/webrtc-sdk/webrtc/pull/206) | Audio mute | Default mute behavior change |
| [#212](https://github.com/webrtc-sdk/webrtc/pull/212) | iOS audio | Audio ducking default change |
| [#214](https://github.com/webrtc-sdk/webrtc/pull/214) | iOS audio | Phone call interruption recovery |
| [#196](https://github.com/webrtc-sdk/webrtc/pull/196) | iOS audio | CallKit + `AVAudioEngine` start timing |
| [#180](https://github.com/webrtc-sdk/webrtc/pull/180) | Android audio | `SetMicrophoneMute` assertion crash |
| [#194](https://github.com/webrtc-sdk/webrtc/pull/194) | Transceiver | Data channel cleanup crash at deinit |
| [#197](https://github.com/webrtc-sdk/webrtc/pull/197) | E2EE | Frame cryptor thread safety deadlock |
| [#134](https://github.com/webrtc-sdk/webrtc/pull/134) | Simulcast | Layer mapping at low resolutions |
| [#124](https://github.com/webrtc-sdk/webrtc/pull/124) | VP9 SVC | `CodecSpecificInfoVP9` struct changes |
| [#208](https://github.com/webrtc-sdk/webrtc/pull/208), [#211](https://github.com/webrtc-sdk/webrtc/pull/211) | Field trials | Multiple `RTCInitFieldTrialDictionary` calls |
