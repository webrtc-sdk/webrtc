/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCConfiguration+Private.h"

#include <memory>

#import "RTCCertificate.h"
#import "RTCConfiguration+Native.h"
#import "RTCIceServer+Private.h"
#import "base/RTCLogging.h"

#include "rtc_base/checks.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_identity.h"

@implementation RTC_OBJC_TYPE (RTCConfiguration)

@synthesize enableDscp = _enableDscp;
@synthesize iceServers = _iceServers;
@synthesize certificate = _certificate;
@synthesize iceTransportPolicy = _iceTransportPolicy;
@synthesize bundlePolicy = _bundlePolicy;
@synthesize rtcpMuxPolicy = _rtcpMuxPolicy;
@synthesize tcpCandidatePolicy = _tcpCandidatePolicy;
@synthesize candidateNetworkPolicy = _candidateNetworkPolicy;
@synthesize continualGatheringPolicy = _continualGatheringPolicy;
@synthesize disableIPV6OnWiFi = _disableIPV6OnWiFi;
@synthesize maxIPv6Networks = _maxIPv6Networks;
@synthesize disableLinkLocalNetworks = _disableLinkLocalNetworks;
@synthesize audioJitterBufferMaxPackets = _audioJitterBufferMaxPackets;
@synthesize audioJitterBufferFastAccelerate = _audioJitterBufferFastAccelerate;
@synthesize iceConnectionReceivingTimeout = _iceConnectionReceivingTimeout;
@synthesize iceBackupCandidatePairPingInterval =
    _iceBackupCandidatePairPingInterval;
@synthesize keyType = _keyType;
@synthesize iceCandidatePoolSize = _iceCandidatePoolSize;
@synthesize shouldPruneTurnPorts = _shouldPruneTurnPorts;
@synthesize shouldPresumeWritableWhenFullyRelayed =
    _shouldPresumeWritableWhenFullyRelayed;
@synthesize shouldSurfaceIceCandidatesOnIceTransportTypeChanged =
    _shouldSurfaceIceCandidatesOnIceTransportTypeChanged;
@synthesize iceCheckMinInterval = _iceCheckMinInterval;
@synthesize sdpSemantics = _sdpSemantics;
@synthesize turnCustomizer = _turnCustomizer;
@synthesize activeResetSrtpParams = _activeResetSrtpParams;
@synthesize cryptoOptions = _cryptoOptions;
@synthesize turnLoggingId = _turnLoggingId;
@synthesize rtcpAudioReportIntervalMs = _rtcpAudioReportIntervalMs;
@synthesize rtcpVideoReportIntervalMs = _rtcpVideoReportIntervalMs;
@synthesize enableImplicitRollback = _enableImplicitRollback;
@synthesize offerExtmapAllowMixed = _offerExtmapAllowMixed;
@synthesize iceCheckIntervalStrongConnectivity =
    _iceCheckIntervalStrongConnectivity;
@synthesize iceCheckIntervalWeakConnectivity =
    _iceCheckIntervalWeakConnectivity;
@synthesize iceUnwritableTimeout = _iceUnwritableTimeout;
@synthesize iceUnwritableMinChecks = _iceUnwritableMinChecks;
@synthesize iceInactiveTimeout = _iceInactiveTimeout;
@synthesize enableIceGatheringOnAnyAddressPorts = _enableIceGatheringOnAnyAddressPorts;

- (instancetype)init {
  // Copy defaults.
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  return [self initWithNativeConfiguration:config];
}

- (instancetype)initWithNativeConfiguration:
    (const webrtc::PeerConnectionInterface::RTCConfiguration &)config {
  self = [super init];
  if (self) {
    _enableDscp = config.dscp();
    NSMutableArray *iceServers = [NSMutableArray array];
    for (const webrtc::PeerConnectionInterface::IceServer &server :
         config.servers) {
      RTC_OBJC_TYPE(RTCIceServer) *iceServer =
          [[RTC_OBJC_TYPE(RTCIceServer) alloc] initWithNativeServer:server];
      [iceServers addObject:iceServer];
    }
    _iceServers = iceServers;
    if (!config.certificates.empty()) {
      webrtc::scoped_refptr<webrtc::RTCCertificate> native_cert;
      native_cert = config.certificates[0];
      webrtc::RTCCertificatePEM native_pem = native_cert->ToPEM();
      _certificate = [[RTC_OBJC_TYPE(RTCCertificate) alloc]
          initWithPrivateKey:@(native_pem.private_key().c_str())
                 certificate:@(native_pem.certificate().c_str())];
    }
    _iceTransportPolicy =
        [[self class] transportPolicyForTransportsType:config.type];
    _bundlePolicy =
        [[self class] bundlePolicyForNativePolicy:config.bundle_policy];
    _rtcpMuxPolicy =
        [[self class] rtcpMuxPolicyForNativePolicy:config.rtcp_mux_policy];
    _tcpCandidatePolicy = [[self class]
        tcpCandidatePolicyForNativePolicy:config.tcp_candidate_policy];
    _candidateNetworkPolicy = [[self class]
        candidateNetworkPolicyForNativePolicy:config.candidate_network_policy];
    webrtc::PeerConnectionInterface::ContinualGatheringPolicy nativePolicy =
        config.continual_gathering_policy;
    _continualGatheringPolicy =
        [[self class] continualGatheringPolicyForNativePolicy:nativePolicy];
    _disableIPV6OnWiFi = config.disable_ipv6_on_wifi;
    _maxIPv6Networks = config.max_ipv6_networks;
    _disableLinkLocalNetworks = config.disable_link_local_networks;
    _audioJitterBufferMaxPackets = config.audio_jitter_buffer_max_packets;
    _audioJitterBufferFastAccelerate =
        config.audio_jitter_buffer_fast_accelerate;
    _iceConnectionReceivingTimeout = config.ice_connection_receiving_timeout;
    _iceBackupCandidatePairPingInterval =
        config.ice_backup_candidate_pair_ping_interval;
    _keyType = RTC_OBJC_TYPE(RTCEncryptionKeyTypeECDSA);
    _iceCandidatePoolSize = config.ice_candidate_pool_size;
    _shouldPruneTurnPorts = config.prune_turn_ports;
    _shouldPresumeWritableWhenFullyRelayed =
        config.presume_writable_when_fully_relayed;
    _shouldSurfaceIceCandidatesOnIceTransportTypeChanged =
        config.surface_ice_candidates_on_ice_transport_type_changed;
    if (config.ice_check_min_interval) {
      _iceCheckMinInterval =
          [NSNumber numberWithInt:*config.ice_check_min_interval];
    }
    _sdpSemantics =
        [[self class] sdpSemanticsForNativeSdpSemantics:config.sdp_semantics];
    _turnCustomizer = config.turn_customizer;
    _activeResetSrtpParams = config.active_reset_srtp_params;
    if (config.crypto_options) {
      _cryptoOptions = [[RTC_OBJC_TYPE(RTCCryptoOptions) alloc]
               initWithSrtpEnableGcmCryptoSuites:config.crypto_options->srtp
                                                     .enable_gcm_crypto_suites
             srtpEnableAes128Sha1_32CryptoCipher:
                 config.crypto_options->srtp.enable_aes128_sha1_32_crypto_cipher
          srtpEnableEncryptedRtpHeaderExtensions:
              config.crypto_options->srtp.enable_encrypted_rtp_header_extensions
                    sframeRequireFrameEncryption:config.crypto_options->sframe
                                                     .require_frame_encryption];
    }
    _turnLoggingId =
        [NSString stringWithUTF8String:config.turn_logging_id.c_str()];
    _rtcpAudioReportIntervalMs = config.audio_rtcp_report_interval_ms();
    _rtcpVideoReportIntervalMs = config.video_rtcp_report_interval_ms();
    _enableImplicitRollback = config.enable_implicit_rollback;
    _offerExtmapAllowMixed = config.offer_extmap_allow_mixed;
    _iceCheckIntervalStrongConnectivity =
        config.ice_check_interval_strong_connectivity.has_value() ?
        [NSNumber
            numberWithInt:*config.ice_check_interval_strong_connectivity] :
        nil;
    _iceCheckIntervalWeakConnectivity =
        config.ice_check_interval_weak_connectivity.has_value() ?
        [NSNumber numberWithInt:*config.ice_check_interval_weak_connectivity] :
        nil;
    _iceUnwritableTimeout = config.ice_unwritable_timeout.has_value() ?
        [NSNumber numberWithInt:*config.ice_unwritable_timeout] :
        nil;
    _iceUnwritableMinChecks = config.ice_unwritable_min_checks.has_value() ?
        [NSNumber numberWithInt:*config.ice_unwritable_min_checks] :
        nil;
    _iceInactiveTimeout = config.ice_inactive_timeout.has_value() ?
        [NSNumber numberWithInt:*config.ice_inactive_timeout] :
        nil;
    _enableIceGatheringOnAnyAddressPorts = config.enable_any_address_ports;
  }
  return self;
}

- (NSString *)description {
  static NSString *formatString =
      @"RTC_OBJC_TYPE(RTCConfiguration): "
      @"{\n%@\n%@\n%@\n%@\n%@\n%@\n%@\n%@\n%d\n%d\n%d\n%d\n%d\n%d\n"
      @"%d\n%@\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n}\n";

  return [NSString
      stringWithFormat:
          formatString,
          _iceServers,
          [[self class] stringForTransportPolicy:_iceTransportPolicy],
          [[self class] stringForBundlePolicy:_bundlePolicy],
          [[self class] stringForRtcpMuxPolicy:_rtcpMuxPolicy],
          [[self class] stringForTcpCandidatePolicy:_tcpCandidatePolicy],
          [[self class]
              stringForCandidateNetworkPolicy:_candidateNetworkPolicy],
          [[self class]
              stringForContinualGatheringPolicy:_continualGatheringPolicy],
          [[self class] stringForSdpSemantics:_sdpSemantics],
          _audioJitterBufferMaxPackets,
          _audioJitterBufferFastAccelerate,
          _iceConnectionReceivingTimeout,
          _iceBackupCandidatePairPingInterval,
          _iceCandidatePoolSize,
          _shouldPruneTurnPorts,
          _shouldPresumeWritableWhenFullyRelayed,
          _shouldSurfaceIceCandidatesOnIceTransportTypeChanged,
          _iceCheckMinInterval,
          _disableLinkLocalNetworks,
          _disableIPV6OnWiFi,
          _maxIPv6Networks,
          _activeResetSrtpParams,
          _enableDscp,
          _enableImplicitRollback];
}

#pragma mark - Private

- (webrtc::PeerConnectionInterface::RTCConfiguration *)
    createNativeConfiguration {
  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration>
      nativeConfig(new webrtc::PeerConnectionInterface::RTCConfiguration(
          webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive));

  nativeConfig->set_dscp(_enableDscp);
  for (RTC_OBJC_TYPE(RTCIceServer) * iceServer in _iceServers) {
    nativeConfig->servers.push_back(iceServer.nativeServer);
  }
  nativeConfig->type =
      [[self class] nativeTransportsTypeForTransportPolicy:_iceTransportPolicy];
  nativeConfig->bundle_policy =
      [[self class] nativeBundlePolicyForPolicy:_bundlePolicy];
  nativeConfig->rtcp_mux_policy =
      [[self class] nativeRtcpMuxPolicyForPolicy:_rtcpMuxPolicy];
  nativeConfig->tcp_candidate_policy =
      [[self class] nativeTcpCandidatePolicyForPolicy:_tcpCandidatePolicy];
  nativeConfig->candidate_network_policy = [[self class]
      nativeCandidateNetworkPolicyForPolicy:_candidateNetworkPolicy];
  nativeConfig->continual_gathering_policy = [[self class]
      nativeContinualGatheringPolicyForPolicy:_continualGatheringPolicy];
  nativeConfig->disable_ipv6_on_wifi = _disableIPV6OnWiFi;
  nativeConfig->max_ipv6_networks = _maxIPv6Networks;
  nativeConfig->disable_link_local_networks = _disableLinkLocalNetworks;
  nativeConfig->audio_jitter_buffer_max_packets = _audioJitterBufferMaxPackets;
  nativeConfig->audio_jitter_buffer_fast_accelerate =
      _audioJitterBufferFastAccelerate ? true : false;
  nativeConfig->ice_connection_receiving_timeout =
      _iceConnectionReceivingTimeout;
  nativeConfig->ice_backup_candidate_pair_ping_interval =
      _iceBackupCandidatePairPingInterval;
  webrtc::KeyType keyType =
      [[self class] nativeEncryptionKeyTypeForKeyType:_keyType];
  if (_certificate != nullptr) {
    // if offered a pemcert use it...
    RTC_LOG(LS_INFO) << "Have configured cert - using it.";
    std::string pem_private_key = [[_certificate private_key] UTF8String];
    std::string pem_certificate = [[_certificate certificate] UTF8String];
    webrtc::RTCCertificatePEM pem =
        webrtc::RTCCertificatePEM(pem_private_key, pem_certificate);
    webrtc::scoped_refptr<webrtc::RTCCertificate> certificate =
        webrtc::RTCCertificate::FromPEM(pem);
    RTC_LOG(LS_INFO) << "Created cert from PEM strings.";
    if (!certificate) {
      RTC_LOG(LS_ERROR) << "Failed to generate certificate from PEM.";
      return nullptr;
    }
    nativeConfig->certificates.push_back(certificate);
  } else {
    RTC_LOG(LS_INFO) << "Don't have configured cert.";
    // Generate non-default certificate.
    if (keyType != webrtc::KT_DEFAULT) {
      webrtc::scoped_refptr<webrtc::RTCCertificate> certificate =
          webrtc::RTCCertificateGenerator::GenerateCertificate(
              webrtc::KeyParams(keyType), std::optional<uint64_t>());
      if (!certificate) {
        RTCLogError(@"Failed to generate certificate.");
        return nullptr;
      }
      nativeConfig->certificates.push_back(certificate);
    }
  }
  nativeConfig->ice_candidate_pool_size = _iceCandidatePoolSize;
  nativeConfig->prune_turn_ports = _shouldPruneTurnPorts ? true : false;
  nativeConfig->presume_writable_when_fully_relayed =
      _shouldPresumeWritableWhenFullyRelayed ? true : false;
  nativeConfig->surface_ice_candidates_on_ice_transport_type_changed =
      _shouldSurfaceIceCandidatesOnIceTransportTypeChanged ? true : false;
  if (_iceCheckMinInterval != nil) {
    nativeConfig->ice_check_min_interval =
        std::optional<int>(_iceCheckMinInterval.intValue);
  }
  nativeConfig->sdp_semantics =
      [[self class] nativeSdpSemanticsForSdpSemantics:_sdpSemantics];
  if (_turnCustomizer) {
    nativeConfig->turn_customizer = _turnCustomizer;
  }
  nativeConfig->active_reset_srtp_params =
      _activeResetSrtpParams ? true : false;
  if (_cryptoOptions) {
    webrtc::CryptoOptions nativeCryptoOptions;
    nativeCryptoOptions.srtp.enable_gcm_crypto_suites =
        _cryptoOptions.srtpEnableGcmCryptoSuites ? true : false;
    nativeCryptoOptions.srtp.enable_aes128_sha1_32_crypto_cipher =
        _cryptoOptions.srtpEnableAes128Sha1_32CryptoCipher ? true : false;
    nativeCryptoOptions.srtp.enable_encrypted_rtp_header_extensions =
        _cryptoOptions.srtpEnableEncryptedRtpHeaderExtensions ? true : false;
    nativeCryptoOptions.sframe.require_frame_encryption =
        _cryptoOptions.sframeRequireFrameEncryption ? true : false;
    nativeConfig->crypto_options =
        std::optional<webrtc::CryptoOptions>(nativeCryptoOptions);
  }
  nativeConfig->turn_logging_id = [_turnLoggingId UTF8String];
  nativeConfig->set_audio_rtcp_report_interval_ms(_rtcpAudioReportIntervalMs);
  nativeConfig->set_video_rtcp_report_interval_ms(_rtcpVideoReportIntervalMs);
  nativeConfig->enable_implicit_rollback = _enableImplicitRollback;
  nativeConfig->offer_extmap_allow_mixed = _offerExtmapAllowMixed;
  if (_iceCheckIntervalStrongConnectivity != nil) {
    nativeConfig->ice_check_interval_strong_connectivity =
        std::optional<int>(_iceCheckIntervalStrongConnectivity.intValue);
  }
  if (_iceCheckIntervalWeakConnectivity != nil) {
    nativeConfig->ice_check_interval_weak_connectivity =
        std::optional<int>(_iceCheckIntervalWeakConnectivity.intValue);
  }
  if (_iceUnwritableTimeout != nil) {
    nativeConfig->ice_unwritable_timeout =
        std::optional<int>(_iceUnwritableTimeout.intValue);
  }
  if (_iceUnwritableMinChecks != nil) {
    nativeConfig->ice_unwritable_min_checks =
        std::optional<int>(_iceUnwritableMinChecks.intValue);
  }
  if (_iceInactiveTimeout != nil) {
    nativeConfig->ice_inactive_timeout =
        std::optional<int>(_iceInactiveTimeout.intValue);
  }
  nativeConfig->enable_any_address_ports = _enableIceGatheringOnAnyAddressPorts;
  return nativeConfig.release();
}

+ (webrtc::PeerConnectionInterface::IceTransportsType)
    nativeTransportsTypeForTransportPolicy:(RTC_OBJC_TYPE(RTCIceTransportPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCIceTransportPolicyNone):
      return webrtc::PeerConnectionInterface::kNone;
    case RTC_OBJC_TYPE(RTCIceTransportPolicyRelay):
      return webrtc::PeerConnectionInterface::kRelay;
    case RTC_OBJC_TYPE(RTCIceTransportPolicyNoHost):
      return webrtc::PeerConnectionInterface::kNoHost;
    case RTC_OBJC_TYPE(RTCIceTransportPolicyAll):
      return webrtc::PeerConnectionInterface::kAll;
  }
}

+ (RTC_OBJC_TYPE(RTCIceTransportPolicy))transportPolicyForTransportsType:
    (webrtc::PeerConnectionInterface::IceTransportsType)nativeType {
  switch (nativeType) {
    case webrtc::PeerConnectionInterface::kNone:
      return RTC_OBJC_TYPE(RTCIceTransportPolicyNone);
    case webrtc::PeerConnectionInterface::kRelay:
      return RTC_OBJC_TYPE(RTCIceTransportPolicyRelay);
    case webrtc::PeerConnectionInterface::kNoHost:
      return RTC_OBJC_TYPE(RTCIceTransportPolicyNoHost);
    case webrtc::PeerConnectionInterface::kAll:
      return RTC_OBJC_TYPE(RTCIceTransportPolicyAll);
  }
}

+ (NSString *)stringForTransportPolicy:(RTC_OBJC_TYPE(RTCIceTransportPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCIceTransportPolicyNone):
      return @"NONE";
    case RTC_OBJC_TYPE(RTCIceTransportPolicyRelay):
      return @"RELAY";
    case RTC_OBJC_TYPE(RTCIceTransportPolicyNoHost):
      return @"NO_HOST";
    case RTC_OBJC_TYPE(RTCIceTransportPolicyAll):
      return @"ALL";
  }
}

+ (webrtc::PeerConnectionInterface::BundlePolicy)nativeBundlePolicyForPolicy:
    (RTC_OBJC_TYPE(RTCBundlePolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCBundlePolicyBalanced):
      return webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
    case RTC_OBJC_TYPE(RTCBundlePolicyMaxCompat):
      return webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
    case RTC_OBJC_TYPE(RTCBundlePolicyMaxBundle):
      return webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
  }
}

+ (RTC_OBJC_TYPE(RTCBundlePolicy))bundlePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::BundlePolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
      return RTC_OBJC_TYPE(RTCBundlePolicyBalanced);
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
      return RTC_OBJC_TYPE(RTCBundlePolicyMaxCompat);
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
      return RTC_OBJC_TYPE(RTCBundlePolicyMaxBundle);
  }
}

+ (NSString *)stringForBundlePolicy:(RTC_OBJC_TYPE(RTCBundlePolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCBundlePolicyBalanced):
      return @"BALANCED";
    case RTC_OBJC_TYPE(RTCBundlePolicyMaxCompat):
      return @"MAX_COMPAT";
    case RTC_OBJC_TYPE(RTCBundlePolicyMaxBundle):
      return @"MAX_BUNDLE";
  }
}

+ (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativeRtcpMuxPolicyForPolicy:
    (RTC_OBJC_TYPE(RTCRtcpMuxPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCRtcpMuxPolicyNegotiate):
      return webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
    case RTC_OBJC_TYPE(RTCRtcpMuxPolicyRequire):
      return webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
  }
}

+ (RTC_OBJC_TYPE(RTCRtcpMuxPolicy))rtcpMuxPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
      return RTC_OBJC_TYPE(RTCRtcpMuxPolicyNegotiate);
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
      return RTC_OBJC_TYPE(RTCRtcpMuxPolicyRequire);
  }
}

+ (NSString *)stringForRtcpMuxPolicy:(RTC_OBJC_TYPE(RTCRtcpMuxPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCRtcpMuxPolicyNegotiate):
      return @"NEGOTIATE";
    case RTC_OBJC_TYPE(RTCRtcpMuxPolicyRequire):
      return @"REQUIRE";
  }
}

+ (webrtc::PeerConnectionInterface::TcpCandidatePolicy)nativeTcpCandidatePolicyForPolicy:
    (RTC_OBJC_TYPE(RTCTcpCandidatePolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCTcpCandidatePolicyEnabled):
      return webrtc::PeerConnectionInterface::kTcpCandidatePolicyEnabled;
    case RTC_OBJC_TYPE(RTCTcpCandidatePolicyDisabled):
      return webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled;
  }
}

+ (webrtc::PeerConnectionInterface::CandidateNetworkPolicy)nativeCandidateNetworkPolicyForPolicy:
    (RTC_OBJC_TYPE(RTCCandidateNetworkPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCCandidateNetworkPolicyAll):
      return webrtc::PeerConnectionInterface::kCandidateNetworkPolicyAll;
    case RTC_OBJC_TYPE(RTCCandidateNetworkPolicyLowCost):
      return webrtc::PeerConnectionInterface::kCandidateNetworkPolicyLowCost;
  }
}

+ (RTC_OBJC_TYPE(RTCTcpCandidatePolicy))tcpCandidatePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::TcpCandidatePolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kTcpCandidatePolicyEnabled:
      return RTC_OBJC_TYPE(RTCTcpCandidatePolicyEnabled);
    case webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled:
      return RTC_OBJC_TYPE(RTCTcpCandidatePolicyDisabled);
  }
}

+ (NSString *)stringForTcpCandidatePolicy:(RTC_OBJC_TYPE(RTCTcpCandidatePolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCTcpCandidatePolicyEnabled):
      return @"TCP_ENABLED";
    case RTC_OBJC_TYPE(RTCTcpCandidatePolicyDisabled):
      return @"TCP_DISABLED";
  }
}

+ (RTC_OBJC_TYPE(RTCCandidateNetworkPolicy))candidateNetworkPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::CandidateNetworkPolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kCandidateNetworkPolicyAll:
      return RTC_OBJC_TYPE(RTCCandidateNetworkPolicyAll);
    case webrtc::PeerConnectionInterface::kCandidateNetworkPolicyLowCost:
      return RTC_OBJC_TYPE(RTCCandidateNetworkPolicyLowCost);
  }
}

+ (NSString *)stringForCandidateNetworkPolicy:(RTC_OBJC_TYPE(RTCCandidateNetworkPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCCandidateNetworkPolicyAll):
      return @"CANDIDATE_ALL_NETWORKS";
    case RTC_OBJC_TYPE(RTCCandidateNetworkPolicyLowCost):
      return @"CANDIDATE_LOW_COST_NETWORKS";
  }
}

+ (webrtc::PeerConnectionInterface::ContinualGatheringPolicy)
    nativeContinualGatheringPolicyForPolicy:
        (RTC_OBJC_TYPE(RTCContinualGatheringPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCContinualGatheringPolicyGatherOnce):
      return webrtc::PeerConnectionInterface::GATHER_ONCE;
    case RTC_OBJC_TYPE(RTCContinualGatheringPolicyGatherContinually):
      return webrtc::PeerConnectionInterface::GATHER_CONTINUALLY;
  }
}

+ (RTC_OBJC_TYPE(RTCContinualGatheringPolicy))continualGatheringPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::ContinualGatheringPolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::GATHER_ONCE:
      return RTC_OBJC_TYPE(RTCContinualGatheringPolicyGatherOnce);
    case webrtc::PeerConnectionInterface::GATHER_CONTINUALLY:
      return RTC_OBJC_TYPE(RTCContinualGatheringPolicyGatherContinually);
  }
}

+ (NSString *)stringForContinualGatheringPolicy:
    (RTC_OBJC_TYPE(RTCContinualGatheringPolicy))policy {
  switch (policy) {
    case RTC_OBJC_TYPE(RTCContinualGatheringPolicyGatherOnce):
      return @"GATHER_ONCE";
    case RTC_OBJC_TYPE(RTCContinualGatheringPolicyGatherContinually):
      return @"GATHER_CONTINUALLY";
  }
}

+ (webrtc::KeyType)nativeEncryptionKeyTypeForKeyType:(RTC_OBJC_TYPE(RTCEncryptionKeyType))keyType {
  switch (keyType) {
    case RTC_OBJC_TYPE(RTCEncryptionKeyTypeRSA):
      return rtc::KT_RSA;
    case RTC_OBJC_TYPE(RTCEncryptionKeyTypeECDSA):
      return rtc::KT_ECDSA;
  }
}

+ (webrtc::SdpSemantics)nativeSdpSemanticsForSdpSemantics:(RTC_OBJC_TYPE(RTCSdpSemantics))sdpSemantics {
  switch (sdpSemantics) {
    case RTC_OBJC_TYPE(RTCSdpSemanticsPlanB):
      return webrtc::SdpSemantics::kPlanB_DEPRECATED;
    case RTC_OBJC_TYPE(RTCSdpSemanticsUnifiedPlan):
      return webrtc::SdpSemantics::kUnifiedPlan;
  }
}

+ (RTC_OBJC_TYPE(RTCSdpSemantics))sdpSemanticsForNativeSdpSemantics:(webrtc::SdpSemantics)sdpSemantics {
  switch (sdpSemantics) {
    case webrtc::SdpSemantics::kPlanB_DEPRECATED:
      return RTC_OBJC_TYPE(RTCSdpSemanticsPlanB);
    case webrtc::SdpSemantics::kUnifiedPlan:
      return RTC_OBJC_TYPE(RTCSdpSemanticsUnifiedPlan);
  }
}

+ (NSString *)stringForSdpSemantics:(RTC_OBJC_TYPE(RTCSdpSemantics))sdpSemantics {
  switch (sdpSemantics) {
    case RTC_OBJC_TYPE(RTCSdpSemanticsPlanB):
      return @"PLAN_B";
    case RTC_OBJC_TYPE(RTCSdpSemanticsUnifiedPlan):
      return @"UNIFIED_PLAN";
  }
}

@end
