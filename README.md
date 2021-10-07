# WebRTC-SDK

This repository contains a fork of WebRTC from Google with various improvements.

## Changes

### All

- Dynamically acquire decoder to mitigate decoder limitations #25 #26

### Android

- Support for video simulcast #3

### iOS

- Do not request microphone permissions for playback-only #2 #5
- Improvements to AVAudioSession interactions #7 #8
- Support for video simulcast #4
- Support for voice processing bypass #15

### Mac

- Support for video simulcast #10
- Remove hardcoded limitation of outputting to only right speaker on MBP #22
- Screen capture support #24 #36 #37
- Support for audio output device selection #35
- Cross-platform RTCMTLVideoView #40

### Windows

- Fixed unable to acquire Mic when built-in AEC is enabled #29

## LICENSE

- [Google WebRTC](https://chromium.googlesource.com/external/webrtc.git), is licensed under [BSD license](/LICENSE).

- Contains patches from [shiguredo-webrtc-build](https://github.com/shiguredo-webrtc-build), licensed under [Apache 2.0](/NOTICE).

- Contains changes from LiveKit, licensed under Apache 2.0.

## Who is using this project

- [flutter-webrtc](https://github.com/flutter-webrtc/flutter-webrtc)

- [LiveKit](https://github.com/livekit)

- [Membrane Framework](https://github.com/membraneframework/membrane_rtc_engine)

