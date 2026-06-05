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
#if defined(WEBRTC_MAC)
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

#if defined(WEBRTC_MAC)

struct VideoToolboxH264ProfileLevels {
  std::optional<webrtc::H264Level> constrainedBaseline;
  std::optional<webrtc::H264Level> constrainedHigh;
};

enum class VideoToolboxH264ProfileFamily {
  kBaseline,
  kHigh,
};

struct VideoToolboxH264ProfileLevel {
  CFStringRef profileLevel;
  VideoToolboxH264ProfileFamily family;
  webrtc::H264Level level;
};

bool H264LevelIsHigherThan(std::optional<webrtc::H264Level> current,
                           webrtc::H264Level candidate) {
  return !current ||
      static_cast<int>(candidate) > static_cast<int>(*current);
}

void UpdateMaxH264Level(std::optional<webrtc::H264Level> *current,
                        webrtc::H264Level candidate) {
  if (H264LevelIsHigherThan(*current, candidate)) {
    *current = candidate;
  }
}

VideoToolboxH264ProfileLevels ParseSupportedH264ProfileLevels(
    NSDictionary *supportedProperties) {
  VideoToolboxH264ProfileLevels levels;
  NSDictionary *profileLevelProperty = [supportedProperties
      objectForKey:(__bridge NSString *)kVTCompressionPropertyKey_ProfileLevel];
  if (![profileLevelProperty isKindOfClass:[NSDictionary class]]) {
    return levels;
  }

  NSArray *supportedValues =
      [profileLevelProperty objectForKey:(__bridge NSString *)kVTPropertySupportedValueListKey];
  if (![supportedValues isKindOfClass:[NSArray class]]) {
    return levels;
  }

  const VideoToolboxH264ProfileLevel kKnownProfileLevels[] = {
      {kVTProfileLevel_H264_Baseline_3_0, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel3},
      {kVTProfileLevel_H264_Baseline_3_1, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel3_1},
      {kVTProfileLevel_H264_Baseline_3_2, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel3_2},
      {kVTProfileLevel_H264_Baseline_4_0, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel4},
      {kVTProfileLevel_H264_Baseline_4_1, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel4_1},
      {kVTProfileLevel_H264_Baseline_4_2, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel4_2},
      {kVTProfileLevel_H264_Baseline_5_0, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel5},
      {kVTProfileLevel_H264_Baseline_5_1, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel5_1},
      {kVTProfileLevel_H264_Baseline_5_2, VideoToolboxH264ProfileFamily::kBaseline,
       webrtc::H264Level::kLevel5_2},
      {kVTProfileLevel_H264_High_3_0, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel3},
      {kVTProfileLevel_H264_High_3_1, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel3_1},
      {kVTProfileLevel_H264_High_3_2, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel3_2},
      {kVTProfileLevel_H264_High_4_0, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel4},
      {kVTProfileLevel_H264_High_4_1, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel4_1},
      {kVTProfileLevel_H264_High_4_2, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel4_2},
      {kVTProfileLevel_H264_High_5_0, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel5},
      {kVTProfileLevel_H264_High_5_1, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel5_1},
      {kVTProfileLevel_H264_High_5_2, VideoToolboxH264ProfileFamily::kHigh,
       webrtc::H264Level::kLevel5_2},
  };

  for (id value in supportedValues) {
    if (![value isKindOfClass:[NSString class]]) {
      continue;
    }

    CFStringRef profileLevel = (__bridge CFStringRef)value;
    for (const VideoToolboxH264ProfileLevel &knownProfileLevel : kKnownProfileLevels) {
      if (CFStringCompare(profileLevel, knownProfileLevel.profileLevel, 0) !=
          kCFCompareEqualTo) {
        continue;
      }

      switch (knownProfileLevel.family) {
        case VideoToolboxH264ProfileFamily::kBaseline:
          UpdateMaxH264Level(&levels.constrainedBaseline, knownProfileLevel.level);
          break;
        case VideoToolboxH264ProfileFamily::kHigh:
          UpdateMaxH264Level(&levels.constrainedHigh, knownProfileLevel.level);
          break;
      }
    }
  }

  return levels;
}

VideoToolboxH264ProfileLevels QueryVideoToolboxH264ProfileLevelsForSize(
    int32_t width,
    int32_t height) {
  VideoToolboxH264ProfileLevels levels;

  if (@available(macOS 10.13, *)) {
    NSDictionary *encoderSpecification = @{
      (__bridge NSString *)kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder :
          @(YES),
    };

    CFDictionaryRef supportedPropertiesRef = nullptr;
    OSStatus status = VTCopySupportedPropertyDictionaryForEncoder(
        width,
        height,
        kCMVideoCodecType_H264,
        (__bridge CFDictionaryRef)encoderSpecification,
        nullptr,
        &supportedPropertiesRef);
    if (status != noErr || supportedPropertiesRef == nullptr) {
      return levels;
    }

    NSDictionary *supportedProperties = CFBridgingRelease(supportedPropertiesRef);
    return ParseSupportedH264ProfileLevels(supportedProperties);
  }

  return levels;
}

VideoToolboxH264ProfileLevels QueryVideoToolboxH264ProfileLevels() {
  VideoToolboxH264ProfileLevels levels;
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
    if (queryLevels.constrainedHigh) {
      UpdateMaxH264Level(&levels.constrainedHigh, *queryLevels.constrainedHigh);
    }
  }

  return levels;
}

const VideoToolboxH264ProfileLevels &MaxSupportedH264ProfileLevelsFromVideoToolbox() {
  static const VideoToolboxH264ProfileLevels levels =
      QueryVideoToolboxH264ProfileLevels();
  return levels;
}

#endif

NSString *MaxSupportedLevelForProfile(webrtc::H264Profile profile) {
#if defined(WEBRTC_IOS)
  const std::optional<webrtc::H264ProfileLevelId> profileLevelId =
      [UIDevice maxSupportedH264Profile];
  if (profileLevelId && profileLevelId->profile >= profile) {
    const std::optional<std::string> profileString = H264ProfileLevelIdToString(
        webrtc::H264ProfileLevelId(profile, profileLevelId->level));
    if (profileString) {
      return [NSString stringForStdString:*profileString];
    }
  }
#elif defined(WEBRTC_MAC)
  const VideoToolboxH264ProfileLevels &profileLevels =
      MaxSupportedH264ProfileLevelsFromVideoToolbox();
  std::optional<webrtc::H264Level> supportedLevel;
  switch (profile) {
    case webrtc::H264Profile::kProfileConstrainedBaseline:
    case webrtc::H264Profile::kProfileBaseline:
      supportedLevel = profileLevels.constrainedBaseline;
      break;
    case webrtc::H264Profile::kProfileConstrainedHigh:
    case webrtc::H264Profile::kProfileHigh:
    case webrtc::H264Profile::kProfilePredictiveHigh444:
      supportedLevel = profileLevels.constrainedHigh;
      break;
    case webrtc::H264Profile::kProfileMain:
      break;
  }

  if (supportedLevel) {
    const std::optional<std::string> profileString = H264ProfileLevelIdToString(
        webrtc::H264ProfileLevelId(profile, *supportedLevel));
    if (profileString) {
      return [NSString stringForStdString:*profileString];
    }
  }

  // Keep macOS above Level 3.1 if VideoToolbox cannot return a useful profile
  // list. Level 3.1 rejects 1080p30 before encoding starts.
  const std::optional<std::string> profileString = H264ProfileLevelIdToString(
      webrtc::H264ProfileLevelId(profile, webrtc::H264Level::kLevel5));
  if (profileString) {
    return [NSString stringForStdString:*profileString];
  }
#endif
  return nil;
}
#endif

NSString *MaxSupportedProfileLevelConstrainedBaseline() {
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
  NSString *profile = MaxSupportedLevelForProfile(
      webrtc::H264Profile::kProfileConstrainedBaseline);
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
