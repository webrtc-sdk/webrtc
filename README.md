# WebRTC-SDK

This repository contains a fork of WebRTC from Google with various improvements.

## Main changes

### All

- Dynamically acquire decoder to mitigate decoder limitations [#25](https://github.com/webrtc-sdk/webrtc/pull/25)
- Support for video simulcast with hardware & software encoders [patch](https://github.com/webrtc-sdk/webrtc/commit/ee030264e2274a2c90548a99b448782049e48fb4)
- Frame cryptor support (for end-to-end encryption) [patch](https://github.com/webrtc-sdk/webrtc/commit/3a2c008529a15fecde5f979a6ebb75c05463d45e)

### Android

- WrappedVideoDecoderFactory [#74](https://github.com/webrtc-sdk/webrtc/pull/74)

### iOS / Mac

- Sane audio handling [patch](https://github.com/webrtc-sdk/webrtc/commit/272127d457ab48e36241e82549870405864851f6)
  - Do not acquire microphone/permissions unless actively publishing audio
  - Abililty to bypass voice processing on iOS
  - Remove hardcoded limitation of outputting only to right speaker on MacBook Pro
- Desktop capture for Mac [patch](https://github.com/webrtc-sdk/webrtc/commit/8e832d1163644ab504412c9b8f3ba8510d9890d6)

### Windows

- Fixed unable to acquire Mic when built-in AEC is enabled [#29](https://github.com/webrtc-sdk/webrtc/pull/29)

## LICENSE

- [Google WebRTC](https://chromium.googlesource.com/external/webrtc.git), is licensed under [BSD license](/LICENSE).

- Contains patches from [shiguredo-webrtc-build](https://github.com/shiguredo-webrtc-build), licensed under [Apache 2.0](/NOTICE).

- Contains changes from LiveKit, licensed under Apache 2.0.

## Who is using this project

- [flutter-webrtc](https://github.com/flutter-webrtc/flutter-webrtc)

- [LiveKit](https://github.com/livekit)

- [Membrane Framework](https://github.com/membraneframework/membrane_rtc_engine)

- [Louper](https://louper.io)

Are you using WebRTC SDK in your framework or app? Feel free to open a PR and add yourself!
