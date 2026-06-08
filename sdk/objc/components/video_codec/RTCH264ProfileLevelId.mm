/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCH264ProfileLevelId.h"

#import "helpers/NSString+StdString.h"
#if defined(WEBRTC_IOS)
#import "UIDevice+H264Profile.h"
#endif
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
#import <TargetConditionals.h>
#import <VideoToolbox/VideoToolbox.h>
#endif

#include "api/video_codecs/h264_profile_level_id.h"
#include "media/base/media_constants.h"

namespace {

NSString *MaxSupportedProfileLevelConstrainedHigh();
NSString *MaxSupportedProfileLevelConstrainedBaseline();

}  // namespace

NSString *const RTC_CONSTANT_TYPE(RTCVideoCodecH264Name) = @(webrtc::kH264CodecName);
NSString *const RTC_CONSTANT_TYPE(RTCLevel31ConstrainedHigh) = @"640c1f";
NSString *const RTC_CONSTANT_TYPE(RTCLevel31ConstrainedBaseline) = @"42e01f";
NSString *const RTC_CONSTANT_TYPE(RTCMaxSupportedH264ProfileLevelConstrainedHigh) =
    MaxSupportedProfileLevelConstrainedHigh();
NSString *const RTC_CONSTANT_TYPE(RTCMaxSupportedH264ProfileLevelConstrainedBaseline) =
    MaxSupportedProfileLevelConstrainedBaseline();

namespace {

#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)

// H264 sender capabilities are advertised through SDP profile-level-id strings,
// but the Apple encoder is configured with VideoToolbox profile-level constants.
// Keep this helper as the single place where Apple runtime capabilities are
// converted back into WebRTC's H264Profile/H264Level model.
struct VideoToolboxH264ProfileLevels {
  std::optional<webrtc::H264Level> constrainedBaseline;
  std::optional<webrtc::H264Level> main;
  std::optional<webrtc::H264Level> constrainedHigh;
};

bool HasAnyVideoToolboxH264ProfileLevel(const VideoToolboxH264ProfileLevels &levels) {
  return levels.constrainedBaseline || levels.main || levels.constrainedHigh;
}

struct VideoToolboxH264LevelSupport {
  std::optional<webrtc::H264Level> level;
  bool queried;
};

enum class VideoToolboxH264ProfileFamily {
  kBaseline,
  kMain,
  kHigh,
};

struct VideoToolboxH264ProfileLevel {
  CFStringRef profileLevel;
  VideoToolboxH264ProfileFamily family;
  webrtc::H264Level level;
};

bool H264LevelIsHigherThan(std::optional<webrtc::H264Level> current, webrtc::H264Level candidate) {
  return !current || static_cast<int>(candidate) > static_cast<int>(*current);
}

void UpdateMaxH264Level(std::optional<webrtc::H264Level> *current, webrtc::H264Level candidate) {
  if (H264LevelIsHigherThan(*current, candidate)) {
    *current = candidate;
  }
}

VideoToolboxH264ProfileLevels ParseSupportedH264ProfileLevels(NSDictionary *supportedProperties) {
  VideoToolboxH264ProfileLevels levels;
  NSDictionary *profileLevelProperty =
      [supportedProperties objectForKey:(__bridge NSString *)kVTCompressionPropertyKey_ProfileLevel];
  if (![profileLevelProperty isKindOfClass:[NSDictionary class]]) {
    return levels;
  }

  NSArray *supportedValues = [profileLevelProperty objectForKey:(__bridge NSString *)kVTPropertySupportedValueListKey];
  if (![supportedValues isKindOfClass:[NSArray class]]) {
    return levels;
  }

  // This is not a device capability table. VideoToolbox returns its supported
  // profile levels as CFString constants, while WebRTC stores them as enums.
  // The fixed list below is only the ABI translation layer between those two
  // representations; the actual capability decision comes from VideoToolbox's
  // supported value list.
  const VideoToolboxH264ProfileLevel kKnownProfileLevels[] = {
      {kVTProfileLevel_H264_Baseline_3_0, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel3},
      {kVTProfileLevel_H264_Baseline_3_1, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel3_1},
      {kVTProfileLevel_H264_Baseline_3_2, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel3_2},
      {kVTProfileLevel_H264_Baseline_4_0, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel4},
      {kVTProfileLevel_H264_Baseline_4_1, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel4_1},
      {kVTProfileLevel_H264_Baseline_4_2, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel4_2},
      {kVTProfileLevel_H264_Baseline_5_0, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel5},
      {kVTProfileLevel_H264_Baseline_5_1, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel5_1},
      {kVTProfileLevel_H264_Baseline_5_2, VideoToolboxH264ProfileFamily::kBaseline, webrtc::H264Level::kLevel5_2},
      {kVTProfileLevel_H264_Main_3_0, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel3},
      {kVTProfileLevel_H264_Main_3_1, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel3_1},
      {kVTProfileLevel_H264_Main_3_2, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel3_2},
      {kVTProfileLevel_H264_Main_4_0, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel4},
      {kVTProfileLevel_H264_Main_4_1, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel4_1},
      {kVTProfileLevel_H264_Main_4_2, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel4_2},
      {kVTProfileLevel_H264_Main_5_0, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel5},
      {kVTProfileLevel_H264_Main_5_1, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel5_1},
      {kVTProfileLevel_H264_Main_5_2, VideoToolboxH264ProfileFamily::kMain, webrtc::H264Level::kLevel5_2},
      {kVTProfileLevel_H264_High_3_0, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel3},
      {kVTProfileLevel_H264_High_3_1, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel3_1},
      {kVTProfileLevel_H264_High_3_2, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel3_2},
      {kVTProfileLevel_H264_High_4_0, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel4},
      {kVTProfileLevel_H264_High_4_1, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel4_1},
      {kVTProfileLevel_H264_High_4_2, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel4_2},
      {kVTProfileLevel_H264_High_5_0, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel5},
      {kVTProfileLevel_H264_High_5_1, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel5_1},
      {kVTProfileLevel_H264_High_5_2, VideoToolboxH264ProfileFamily::kHigh, webrtc::H264Level::kLevel5_2},
  };

  for (id value in supportedValues) {
    if (![value isKindOfClass:[NSString class]]) {
      continue;
    }

    CFStringRef profileLevel = (__bridge CFStringRef)value;
    for (const VideoToolboxH264ProfileLevel &knownProfileLevel : kKnownProfileLevels) {
      if (CFStringCompare(profileLevel, knownProfileLevel.profileLevel, 0) != kCFCompareEqualTo) {
        continue;
      }

      switch (knownProfileLevel.family) {
        case VideoToolboxH264ProfileFamily::kBaseline:
          UpdateMaxH264Level(&levels.constrainedBaseline, knownProfileLevel.level);
          break;
        case VideoToolboxH264ProfileFamily::kMain:
          UpdateMaxH264Level(&levels.main, knownProfileLevel.level);
          break;
        case VideoToolboxH264ProfileFamily::kHigh:
          UpdateMaxH264Level(&levels.constrainedHigh, knownProfileLevel.level);
          break;
      }
    }
  }

  return levels;
}

bool CanQueryVideoToolboxEncoderProperties() {
  if (@available(iOS 11.0, macCatalyst 13.0, macOS 10.13, tvOS 11.0, visionOS 1.0, *)) {
    return true;
  }
  return false;
}

bool CanRequireHardwareAcceleratedVideoToolboxEncoder() {
  if (@available(iOS 17.4, macCatalyst 17.4, macOS 10.9, tvOS 17.4, visionOS 1.1, *)) {
    return true;
  }
  return false;
}

NSDictionary *HardwareRequiredVideoToolboxEncoderSpecification() {
  // On platforms where this key is available, ask VideoToolbox specifically for
  // the hardware encoder's property dictionary. The encoder path itself also
  // prefers hardware acceleration, so advertising software-only H264 levels here
  // would create SDP that may not be encodable by the session we later create.
  if (@available(iOS 17.4, macCatalyst 17.4, macOS 10.9, tvOS 17.4, visionOS 1.1, *)) {
    return @{
      (__bridge NSString *)kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder : @(YES),
    };
  }
  return nil;
}

bool ShouldQueryVideoToolboxWithoutHardwareRequirement() {
#if defined(WEBRTC_IOS) && TARGET_OS_IOS && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
  // Before iOS 17.4, VideoToolbox exposes the property query API but not the
  // "require hardware encoder" selector. For physical iPhone/iPad builds, keep
  // using the known device table in that case instead of accepting a possibly
  // software-backed capability list.
  return false;
#else
  // Other Apple targets either do not have an iOS device table fallback
  // (macOS/Catalyst/tvOS/visionOS) or are simulator builds where software
  // encoding is already expected. Querying VideoToolbox is still the best
  // available signal for those targets when the hardware selector is unavailable.
  return true;
#endif
}

VideoToolboxH264ProfileLevels QueryVideoToolboxH264ProfileLevelsForSize(int32_t width, int32_t height) {
  VideoToolboxH264ProfileLevels levels;

  if (!CanQueryVideoToolboxEncoderProperties()) {
    return levels;
  }

  const bool requireHardware = CanRequireHardwareAcceleratedVideoToolboxEncoder();
  if (!requireHardware && !ShouldQueryVideoToolboxWithoutHardwareRequirement()) {
    return levels;
  }

  NSDictionary *encoderSpecification = requireHardware ? HardwareRequiredVideoToolboxEncoderSpecification() : nil;

  CFDictionaryRef supportedPropertiesRef = nullptr;
  OSStatus status = VTCopySupportedPropertyDictionaryForEncoder(width, height, kCMVideoCodecType_H264,
                                                                (__bridge CFDictionaryRef)encoderSpecification, nullptr,
                                                                &supportedPropertiesRef);
  if (status != noErr || supportedPropertiesRef == nullptr) {
    if (supportedPropertiesRef != nullptr) {
      CFRelease(supportedPropertiesRef);
    }
    return levels;
  }

  NSDictionary *supportedProperties = CFBridgingRelease(supportedPropertiesRef);
  return ParseSupportedH264ProfileLevels(supportedProperties);
}

VideoToolboxH264ProfileLevels QueryVideoToolboxH264ProfileLevels() {
  VideoToolboxH264ProfileLevels levels;
  // VideoToolbox capability dictionaries can vary by requested dimensions.
  // Probe representative high-to-common sender sizes and keep the highest level
  // returned for each profile family so we do not under-advertise devices that
  // expose their larger H264 levels only for larger encode configurations.
  const struct {
    int32_t width;
    int32_t height;
  } kQuerySizes[] = {
      {3840, 2160},
      {1920, 1080},
      {1280, 720},
  };

  for (const auto &querySize : kQuerySizes) {
    VideoToolboxH264ProfileLevels queryLevels =
        QueryVideoToolboxH264ProfileLevelsForSize(querySize.width, querySize.height);
    if (queryLevels.constrainedBaseline) {
      UpdateMaxH264Level(&levels.constrainedBaseline, *queryLevels.constrainedBaseline);
    }
    if (queryLevels.main) {
      UpdateMaxH264Level(&levels.main, *queryLevels.main);
    }
    if (queryLevels.constrainedHigh) {
      UpdateMaxH264Level(&levels.constrainedHigh, *queryLevels.constrainedHigh);
    }
  }

  return levels;
}

const VideoToolboxH264ProfileLevels &MaxSupportedVideoToolboxH264ProfileLevels() {
  // The advertised codec list is built repeatedly, but Apple encoder
  // capabilities are effectively static for the process. Cache the query result
  // to avoid creating VideoToolbox property dictionaries on every factory call.
  static const VideoToolboxH264ProfileLevels levels = QueryVideoToolboxH264ProfileLevels();
  return levels;
}

VideoToolboxH264LevelSupport SupportedVideoToolboxLevelForProfile(webrtc::H264Profile profile) {
  const VideoToolboxH264ProfileLevels &profileLevels = MaxSupportedVideoToolboxH264ProfileLevels();
  const bool queried = HasAnyVideoToolboxH264ProfileLevel(profileLevels);
  switch (profile) {
    case webrtc::H264Profile::kProfileConstrainedBaseline:
    case webrtc::H264Profile::kProfileBaseline:
      return {profileLevels.constrainedBaseline, queried};
    case webrtc::H264Profile::kProfileMain:
      return {profileLevels.main, queried};
    case webrtc::H264Profile::kProfileConstrainedHigh:
    case webrtc::H264Profile::kProfileHigh:
    case webrtc::H264Profile::kProfilePredictiveHigh444:
      return {profileLevels.constrainedHigh, queried};
  }
  return {std::nullopt, queried};
}

#if defined(WEBRTC_IOS)
std::optional<webrtc::H264Level> SupportedUIDeviceLevelForProfile(webrtc::H264Profile profile) {
  const std::optional<webrtc::H264ProfileLevelId> profileLevelId = [UIDevice maxSupportedH264Profile];
  if (profileLevelId && profileLevelId->profile >= profile) {
    return profileLevelId->level;
  }
  return std::nullopt;
}
#endif

std::optional<webrtc::H264Level> FallbackLevelForCurrentPlatform() {
#if defined(WEBRTC_IOS) && TARGET_OS_IOS && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
  // Physical iOS has the UIDevice table fallback above. If both VideoToolbox and
  // the table fail, fall through to the historical WebRTC Level 3.1 constants
  // rather than inventing a level for an unknown iOS device.
  return std::nullopt;
#else
  // Native macOS used to fall back to Level 3.1 because it did not have the iOS
  // UIDevice table. That is too low for common captures: Level 3.1 caps
  // 1920x1080 to roughly 13 fps, which causes VideoToolbox to reject 1080p30
  // and the sender to time out. Level 5 matches the previous LiveKit workaround
  // and keeps non-iOS-device Apple targets above that failure mode when the
  // runtime query cannot return a useful profile list.
  return webrtc::H264Level::kLevel5;
#endif
}

NSString *ProfileStringForProfileAndLevel(webrtc::H264Profile profile, webrtc::H264Level level) {
  const std::optional<std::string> profileString =
      H264ProfileLevelIdToString(webrtc::H264ProfileLevelId(profile, level));
  if (profileString) {
    return [NSString stringForStdString:*profileString];
  }
  return nil;
}

NSString *MaxSupportedLevelForProfile(webrtc::H264Profile profile) {
  // Selection order:
  // 1. Prefer VideoToolbox, because it reflects the current machine/OS encoder.
  // 2. On iOS, keep the UIDevice table as a conservative fallback for older OSes
  //    where VideoToolbox cannot be constrained to the hardware encoder.
  // 3. For non-iOS-device Apple platforms, fall back to Level 5 only when
  //    VideoToolbox returned no useful H264 profile list. If VideoToolbox
  //    reports a list but omits this profile family, avoid inventing support for
  //    it and let the caller use the historical WebRTC constants instead.
  const VideoToolboxH264LevelSupport videoToolboxSupport = SupportedVideoToolboxLevelForProfile(profile);
  std::optional<webrtc::H264Level> supportedLevel = videoToolboxSupport.level;
#if defined(WEBRTC_IOS)
  if (!supportedLevel) {
    supportedLevel = SupportedUIDeviceLevelForProfile(profile);
  }
#endif
  if (!supportedLevel && !videoToolboxSupport.queried) {
    supportedLevel = FallbackLevelForCurrentPlatform();
  }

  if (supportedLevel) {
    return ProfileStringForProfileAndLevel(profile, *supportedLevel);
  }
  return nil;
}
#endif

NSString *MaxSupportedProfileLevelConstrainedBaseline() {
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
  NSString *profile = MaxSupportedLevelForProfile(webrtc::H264Profile::kProfileConstrainedBaseline);
  if (profile != nil) {
    return profile;
  }
#endif
  return RTC_CONSTANT_TYPE(RTCLevel31ConstrainedBaseline);
}

NSString *MaxSupportedProfileLevelConstrainedHigh() {
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
  NSString *profile =
      MaxSupportedLevelForProfile(webrtc::H264Profile::kProfileConstrainedHigh);
  if (profile != nil) {
    return profile;
  }
#endif
  return RTC_CONSTANT_TYPE(RTCLevel31ConstrainedHigh);
}

}  // namespace

@interface RTC_OBJC_TYPE (RTCH264ProfileLevelId)
()

    @property(nonatomic, assign) RTC_OBJC_TYPE(RTCH264Profile) profile;
@property(nonatomic, assign) RTC_OBJC_TYPE(RTCH264Level) level;
@property(nonatomic, strong) NSString *hexString;

@end

@implementation RTC_OBJC_TYPE (RTCH264ProfileLevelId)

@synthesize profile = _profile;
@synthesize level = _level;
@synthesize hexString = _hexString;

- (instancetype)initWithHexString:(NSString *)hexString {
  self = [super init];
  if (self) {
    self.hexString = hexString;

    std::optional<webrtc::H264ProfileLevelId> profile_level_id =
        webrtc::ParseH264ProfileLevelId(
            [hexString cStringUsingEncoding:NSUTF8StringEncoding]);
    if (profile_level_id.has_value()) {
      self.profile = static_cast<RTC_OBJC_TYPE(RTCH264Profile)>(profile_level_id->profile);
      self.level = static_cast<RTC_OBJC_TYPE(RTCH264Level)>(profile_level_id->level);
    }
  }
  return self;
}

- (instancetype)initWithProfile:(RTC_OBJC_TYPE(RTCH264Profile))profile level:(RTC_OBJC_TYPE(RTCH264Level))level {
  self = [super init];
  if (self) {
    self.profile = profile;
    self.level = level;

    std::optional<std::string> hex_string = webrtc::H264ProfileLevelIdToString(
        webrtc::H264ProfileLevelId(static_cast<webrtc::H264Profile>(profile),
                                   static_cast<webrtc::H264Level>(level)));
    self.hexString = [NSString stringWithCString:hex_string.value_or("").c_str()
                                        encoding:NSUTF8StringEncoding];
  }
  return self;
}

@end
