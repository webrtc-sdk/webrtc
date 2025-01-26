/*
 * Copyright 2024 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio_device_utils_mac.h"

#include <IOKit/audio/IOAudioTypes.h>

#include <unordered_set>
#include <utility>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace mac_audio_utils {
namespace {

static const CFStringEncoding kNarrowStringEncoding = kCFStringEncodingUTF8;

template <typename StringType>
static StringType CFStringToSTLStringWithEncodingT(CFStringRef cfstring,
                                                   CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0) return StringType();
  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring, whole_string, encoding,
                                       0,      // lossByte
                                       false,  // isExternalRepresentation
                                       NULL,   // buffer
                                       0,      // maxBufLen
                                       &out_size);
  if (converted == 0 || out_size == 0) return StringType();

  typename StringType::size_type elements =
      out_size * sizeof(UInt8) / sizeof(typename StringType::value_type) + 1;
  std::vector<typename StringType::value_type> out_buffer(elements);
  converted =
      CFStringGetBytes(cfstring, whole_string, encoding,
                       0,      // lossByte
                       false,  // isExternalRepresentation
                       reinterpret_cast<UInt8*>(&out_buffer[0]), out_size,
                       NULL);  // usedBufLen
  if (converted == 0) return StringType();
  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0], elements - 1);
}

std::string CFStringRefToUTF8(CFStringRef ref) {
  return CFStringToSTLStringWithEncodingT<std::string>(ref,
                                                       kNarrowStringEncoding);
}

std::string ToLowerASCII(const std::string& input) {
  std::string result = input;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) {
                   return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
                 });
  return result;
}

AudioObjectPropertyScope InputOutputScope(bool is_input) {
  return is_input ? kAudioObjectPropertyScopeInput
                  : kAudioObjectPropertyScopeOutput;
}

std::optional<std::string> GetDeviceStringProperty(
    AudioObjectID device_id, AudioObjectPropertySelector property_selector) {
  CFStringRef property_value = nullptr;
  UInt32 size = sizeof(property_value);
  AudioObjectPropertyAddress property_address = {
      property_selector, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};

  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, &property_value);
  if (result != noErr) {
    RTC_LOG(LS_WARNING) << "Failed to read string property "
                        << property_selector << " for device " << device_id;
    return std::nullopt;
  }

  if (!property_value) return std::nullopt;

  std::string device_property = CFStringToSTLStringWithEncodingT<std::string>(
      property_value, kNarrowStringEncoding);
  CFRelease(property_value);

  return device_property;
}

std::optional<uint32_t> GetDeviceUint32Property(
    AudioObjectID device_id, AudioObjectPropertySelector property_selector,
    AudioObjectPropertyScope property_scope) {
  AudioObjectPropertyAddress property_address = {
      property_selector, property_scope, kAudioObjectPropertyElementMain};
  UInt32 property_value;
  UInt32 size = sizeof(property_value);
  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, &property_value);
  if (result != noErr) return std::nullopt;

  return property_value;
}

uint32_t GetDevicePropertySize(AudioObjectID device_id,
                               AudioObjectPropertySelector property_selector,
                               AudioObjectPropertyScope property_scope) {
  AudioObjectPropertyAddress property_address = {
      property_selector, property_scope, kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  OSStatus result = AudioObjectGetPropertyDataSize(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size);
  if (result != noErr) {
    RTC_LOG(LS_WARNING) << "Failed to read size of property "
                        << property_selector << " for device " << device_id;
    return 0;
  }
  return size;
}

std::vector<AudioObjectID> GetAudioObjectIDs(
    AudioObjectID audio_object_id,
    AudioObjectPropertySelector property_selector) {
  AudioObjectPropertyAddress property_address = {
      property_selector, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  OSStatus result = AudioObjectGetPropertyDataSize(
      audio_object_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size);
  if (result != noErr) {
    RTC_LOG(LS_WARNING) << "Failed to read size of property "
                        << property_selector << " for device/object "
                        << audio_object_id;
    return {};
  }

  if (size == 0) return {};

  size_t device_count = size / sizeof(AudioObjectID);
  // Get the array of device ids for all the devices, which includes both
  // input devices and output devices.
  std::vector<AudioObjectID> device_ids(device_count);
  result = AudioObjectGetPropertyData(
      audio_object_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, device_ids.data());
  if (result != noErr) {
    RTC_LOG(LS_WARNING) << "Failed to read object IDs from property "
                        << property_selector << " for device/object "
                        << audio_object_id;
    return {};
  }

  return device_ids;
}

std::optional<std::string> GetDeviceName(AudioObjectID device_id) {
  return GetDeviceStringProperty(device_id, kAudioObjectPropertyName);
}

std::optional<std::string> GetDeviceModel(AudioObjectID device_id) {
  return GetDeviceStringProperty(device_id, kAudioDevicePropertyModelUID);
}

bool ModelContainsVidPid(const std::string& model) {
  return model.size() > 10 && model[model.size() - 5] == ':' &&
         model[model.size() - 10] == ':';
}

std::string UsbVidPidFromModel(const std::string& model) {
  return ModelContainsVidPid(model)
             ? ToLowerASCII(model.substr(model.size() - 9))
             : std::string();
}

std::string TransportTypeToString(uint32_t transport_type) {
  switch (transport_type) {
    case kAudioDeviceTransportTypeBuiltIn:
      return "Built-in";
    case kAudioDeviceTransportTypeAggregate:
      return "Aggregate";
    case kAudioDeviceTransportTypeAutoAggregate:
      return "AutoAggregate";
    case kAudioDeviceTransportTypeVirtual:
      return "Virtual";
    case kAudioDeviceTransportTypePCI:
      return "PCI";
    case kAudioDeviceTransportTypeUSB:
      return "USB";
    case kAudioDeviceTransportTypeFireWire:
      return "FireWire";
    case kAudioDeviceTransportTypeBluetooth:
      return "Bluetooth";
    case kAudioDeviceTransportTypeBluetoothLE:
      return "Bluetooth LE";
    case kAudioDeviceTransportTypeHDMI:
      return "HDMI";
    case kAudioDeviceTransportTypeDisplayPort:
      return "DisplayPort";
    case kAudioDeviceTransportTypeAirPlay:
      return "AirPlay";
    case kAudioDeviceTransportTypeAVB:
      return "AVB";
    case kAudioDeviceTransportTypeThunderbolt:
      return "Thunderbolt";
    case kAudioDeviceTransportTypeUnknown:
    default:
      return std::string();
  }
}

std::optional<std::string> TranslateDeviceSource(AudioObjectID device_id,
                                                 UInt32 source_id,
                                                 bool is_input) {
  CFStringRef source_name = nullptr;
  AudioValueTranslation translation;
  translation.mInputData = &source_id;
  translation.mInputDataSize = sizeof(source_id);
  translation.mOutputData = &source_name;
  translation.mOutputDataSize = sizeof(source_name);

  UInt32 translation_size = sizeof(AudioValueTranslation);
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyDataSourceNameForIDCFString,
      InputOutputScope(is_input), kAudioObjectPropertyElementMain};

  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &translation_size, &translation);
  if (result) return std::nullopt;

  std::string ret = CFStringRefToUTF8(source_name);
  CFRelease(source_name);

  return ret;
}

}  // namespace

std::vector<AudioObjectID> GetAllAudioDeviceIDs() {
  // Get all device IDs
  std::vector<AudioObjectID> all_devices = GetAudioObjectIDs(
      kAudioObjectSystemObject, kAudioHardwarePropertyDevices);

  // Get default devices
  std::optional<AudioObjectID> default_input = GetDefaultInputDeviceID();
  std::optional<AudioObjectID> default_output = GetDefaultOutputDeviceID();

  // Create new ordered list with defaults first
  std::vector<AudioObjectID> ordered_devices;
  std::unordered_set<AudioObjectID> added_devices;

  // Add default input if exists
  if (default_input) {
    ordered_devices.push_back(*default_input);
    added_devices.insert(*default_input);
  }

  // Add default output if exists and different from input
  if (default_output && !added_devices.count(*default_output)) {
    ordered_devices.push_back(*default_output);
    added_devices.insert(*default_output);
  }

  // Add remaining devices in original order
  for (AudioObjectID device : all_devices) {
    if (!added_devices.count(device)) {
      ordered_devices.push_back(device);
    }
  }

  return ordered_devices;
}

std::optional<AudioObjectID> GetDefaultInputDeviceID() {
  AudioObjectID device_id = kAudioObjectUnknown;
  UInt32 size = sizeof(device_id);
  AudioObjectPropertyAddress property_address = {
      kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};

  OSStatus result =
      AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0,
                                 nullptr, &size, &device_id);

  if (result != noErr || device_id == kAudioObjectUnknown) {
    RTC_LOG(LS_WARNING) << "Failed to get default input device.";
    return std::nullopt;
  }
  return device_id;
}

std::optional<AudioObjectID> GetDefaultOutputDeviceID() {
  AudioObjectID device_id = kAudioObjectUnknown;
  UInt32 size = sizeof(device_id);
  AudioObjectPropertyAddress property_address = {
      kAudioHardwarePropertyDefaultOutputDevice,
      kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

  OSStatus result =
      AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0,
                                 nullptr, &size, &device_id);

  if (result != noErr || device_id == kAudioObjectUnknown) {
    RTC_LOG(LS_WARNING) << "Failed to get default output device.";
    return std::nullopt;
  }
  return device_id;
}

std::vector<AudioObjectID> GetRelatedDeviceIDs(AudioObjectID device_id) {
  return GetAudioObjectIDs(device_id, kAudioDevicePropertyRelatedDevices);
}

std::optional<std::string> GetDeviceUniqueID(AudioObjectID device_id) {
  return GetDeviceStringProperty(device_id, kAudioDevicePropertyDeviceUID);
}

std::optional<std::string> GetDeviceLabel(AudioObjectID device_id,
                                          bool is_input) {
  std::optional<std::string> device_label;
  std::optional<uint32_t> source = GetDeviceSource(device_id, is_input);
  if (source) {
    device_label = TranslateDeviceSource(device_id, *source, is_input);
  }

  if (!device_label) {
    device_label = GetDeviceName(device_id);
    if (!device_label) return std::nullopt;
  }

  std::string suffix;
  std::optional<uint32_t> transport_type = GetDeviceTransportType(device_id);
  if (transport_type) {
    if (*transport_type == kAudioDeviceTransportTypeUSB) {
      std::optional<std::string> model = GetDeviceModel(device_id);
      if (model) {
        suffix = UsbVidPidFromModel(*model);
      }
    } else {
      suffix = TransportTypeToString(*transport_type);
    }
  }

  RTC_DCHECK(device_label);
  if (!suffix.empty()) *device_label += " (" + suffix + ")";

  return device_label;
}

uint32_t GetNumStreams(AudioObjectID device_id, bool is_input) {
  return GetDevicePropertySize(device_id, kAudioDevicePropertyStreams,
                               InputOutputScope(is_input));
}

std::optional<uint32_t> GetDeviceSource(AudioObjectID device_id,
                                        bool is_input) {
  return GetDeviceUint32Property(device_id, kAudioDevicePropertyDataSource,
                                 InputOutputScope(is_input));
}

std::optional<uint32_t> GetDeviceTransportType(AudioObjectID device_id) {
  return GetDeviceUint32Property(device_id, kAudioDevicePropertyTransportType,
                                 kAudioObjectPropertyScopeGlobal);
}

bool IsPrivateAggregateDevice(AudioObjectID device_id) {
  if (GetDeviceTransportType(device_id) != kAudioDeviceTransportTypeAggregate)
    return false;

  const AudioObjectPropertyAddress property_address = {
      kAudioAggregateDevicePropertyComposition, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  CFDictionaryRef dictionary = nullptr;
  UInt32 size = sizeof(dictionary);
  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, &dictionary);

  if (result != noErr) {
    RTC_LOG(LS_WARNING) << "Failed to read property "
                        << kAudioAggregateDevicePropertyComposition
                        << " for device " << device_id;
    return false;
  }

  if (!dictionary) {
    RTC_LOG(LS_WARNING) << "Property "
                        << kAudioAggregateDevicePropertyComposition
                        << " is null for device " << device_id;
    return false;
  }

  RTC_DCHECK(CFGetTypeID(dictionary) == CFDictionaryGetTypeID());
  bool is_private = false;
  CFTypeRef value = CFDictionaryGetValue(
      dictionary, CFSTR(kAudioAggregateDeviceIsPrivateKey));

  if (value && CFGetTypeID(value) == CFNumberGetTypeID()) {
    int number = 0;
    if (CFNumberGetValue(reinterpret_cast<CFNumberRef>(value), kCFNumberIntType,
                         &number)) {
      is_private = number != 0;
    }
  }
  CFRelease(dictionary);

  return is_private;
}

bool IsInputDevice(AudioObjectID device_id) {
  std::vector<AudioObjectID> streams =
      GetAudioObjectIDs(device_id, kAudioDevicePropertyStreams);

  int num_undefined_input_streams = 0;
  int num_defined_input_streams = 0;
  int num_output_streams = 0;

  for (auto stream_id : streams) {
    auto direction =
        GetDeviceUint32Property(stream_id, kAudioStreamPropertyDirection,
                                kAudioObjectPropertyScopeGlobal);
    if (!direction.has_value()) continue;
    const UInt32 kDirectionOutput = 0;
    const UInt32 kDirectionInput = 1;
    if (direction == kDirectionOutput) {
      ++num_output_streams;
    } else if (direction == kDirectionInput) {
      auto terminal =
          GetDeviceUint32Property(stream_id, kAudioStreamPropertyTerminalType,
                                  kAudioObjectPropertyScopeGlobal);
      if (terminal.has_value() && terminal == INPUT_UNDEFINED) {
        ++num_undefined_input_streams;
      } else {
        ++num_defined_input_streams;
      }
    }
  }

  return num_defined_input_streams > 0 ||
         (num_undefined_input_streams > 0 && num_output_streams == 0);
}

bool IsOutputDevice(AudioObjectID device_id) {
  return GetNumStreams(device_id, false) > 0;
}

}  // namespace mac_audio_utils
}  // namespace webrtc
