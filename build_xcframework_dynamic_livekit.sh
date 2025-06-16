#!/bin/sh
if [ ! -n "$1" ]; then
  echo "Usage: $0 'debug' | 'release'"
  exit 0
fi

MODE=$1
OUT_DIR=./out-$MODE
DEBUG="false"
if [ "$MODE" == "debug" ]; then
  DEBUG="true"
fi

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "build_xcframework_dynamic_livekit.sh: MODE=$MODE, DEBUG=$DEBUG"

COMMON_ARGS="
      rtc_objc_prefix = \"LK\"
      treat_warnings_as_errors = false
      ios_enable_code_signing = false
      is_component_build = false
      rtc_enable_symbol_export = true
      rtc_libvpx_build_vp9 = true
      rtc_include_tests = false
      rtc_build_examples = false
      rtc_use_h264 = false
      rtc_enable_protobuf = false
      enable_libaom = true
      rtc_include_dav1d_in_internal_decoder_factory = true
      use_rtti = true
      is_debug = $DEBUG
      enable_dsyms = $DEBUG
      enable_stripping = true"

PLATFORMS=(
  "tvOS-arm64-device:target_os=\"ios\" target_environment=\"appletv\" target_cpu=\"arm64\" ios_deployment_target=\"17.0\""
  "tvOS-arm64-simulator:target_os=\"ios\" target_environment=\"appletvsimulator\" target_cpu=\"arm64\" ios_deployment_target=\"17.0\""
  "xrOS-arm64-device:target_os=\"ios\" target_environment=\"xrdevice\" target_cpu=\"arm64\" ios_deployment_target=\"1.1.0\""
  "xrOS-arm64-simulator:target_os=\"ios\" target_environment=\"xrsimulator\" target_cpu=\"arm64\" ios_deployment_target=\"1.1.0\""
  "catalyst-arm64:target_os=\"ios\" target_environment=\"catalyst\" target_cpu=\"arm64\" ios_deployment_target=\"14.0\""
  "catalyst-x64:target_os=\"ios\" target_environment=\"catalyst\" target_cpu=\"x64\" ios_deployment_target=\"14.0\""
  "iOS-arm64-device:target_os=\"ios\" target_environment=\"device\" target_cpu=\"arm64\" ios_deployment_target=\"13.0\""
  "iOS-x64-simulator:target_os=\"ios\" target_environment=\"simulator\" target_cpu=\"x64\" ios_deployment_target=\"13.0\""
  "iOS-arm64-simulator:target_os=\"ios\" target_environment=\"simulator\" target_cpu=\"x64\" ios_deployment_target=\"13.0\""
  "macOS-x64:target_os=\"mac\" target_cpu=\"x64\" mac_deployment_target=\"10.15\""
  "macOS-arm64:target_os=\"mac\" target_cpu=\"arm64\" mac_deployment_target=\"10.15\""
)

for platform_config in "${PLATFORMS[@]}"; do
  platform="${platform_config%%:*}"
  config="${platform_config#*:}"
  
  echo "Generating configuration for $platform..."
  gn gen $OUT_DIR/$platform --args="$COMMON_ARGS $config" --ide=xcode
  
  if [[ $platform == *"macOS"* ]]; then
    build_target="mac_framework_bundle"
  else
    build_target="ios_framework_bundle"
  fi
  
  echo "${YELLOW}Building $platform...${NC}"
  ninja -C $OUT_DIR/$platform $build_target -j 10 --quiet
  echo "${GREEN}Build $platform completed${NC}"
done

rm -rf $OUT_DIR/*-lib $OUT_DIR/LiveKitWebRTC.*

mkdir -p $OUT_DIR/macOS-lib
cp -R $OUT_DIR/macOS-x64/LiveKitWebRTC.framework $OUT_DIR/macOS-lib/LiveKitWebRTC.framework
lipo -create -output $OUT_DIR/macOS-lib/LiveKitWebRTC.framework/LiveKitWebRTC $OUT_DIR/macOS-arm64/LiveKitWebRTC.framework/LiveKitWebRTC $OUT_DIR/macOS-x64/LiveKitWebRTC.framework/LiveKitWebRTC

mkdir -p $OUT_DIR/catalyst-lib
cp -R $OUT_DIR/catalyst-arm64/LiveKitWebRTC.framework $OUT_DIR/catalyst-lib/LiveKitWebRTC.framework
lipo -create -output $OUT_DIR/catalyst-lib/LiveKitWebRTC.framework/LiveKitWebRTC $OUT_DIR/catalyst-arm64/LiveKitWebRTC.framework/LiveKitWebRTC $OUT_DIR/catalyst-x64/LiveKitWebRTC.framework/LiveKitWebRTC

mkdir -p $OUT_DIR/iOS-device-lib
cp -R $OUT_DIR/iOS-arm64-device/LiveKitWebRTC.framework $OUT_DIR/iOS-device-lib/LiveKitWebRTC.framework
lipo -create -output $OUT_DIR/iOS-device-lib/LiveKitWebRTC.framework/LiveKitWebRTC $OUT_DIR/iOS-arm64-device/LiveKitWebRTC.framework/LiveKitWebRTC

mkdir -p $OUT_DIR/iOS-simulator-lib
cp -R $OUT_DIR/iOS-arm64-simulator/LiveKitWebRTC.framework $OUT_DIR/iOS-simulator-lib/LiveKitWebRTC.framework
lipo -create -output $OUT_DIR/iOS-simulator-lib/LiveKitWebRTC.framework/LiveKitWebRTC $OUT_DIR/iOS-arm64-simulator/LiveKitWebRTC.framework/LiveKitWebRTC $OUT_DIR/iOS-x64-simulator/LiveKitWebRTC.framework/LiveKitWebRTC

xcodebuild -create-xcframework \
  -framework $OUT_DIR/iOS-device-lib/LiveKitWebRTC.framework \
  -framework $OUT_DIR/iOS-simulator-lib/LiveKitWebRTC.framework \
  -framework $OUT_DIR/xrOS-arm64-device/LiveKitWebRTC.framework \
  -framework $OUT_DIR/xrOS-arm64-simulator/LiveKitWebRTC.framework \
  -framework $OUT_DIR/tvOS-arm64-device/LiveKitWebRTC.framework \
  -framework $OUT_DIR/tvOS-arm64-simulator/LiveKitWebRTC.framework \
  -framework $OUT_DIR/catalyst-lib/LiveKitWebRTC.framework \
  -framework $OUT_DIR/macOS-lib/LiveKitWebRTC.framework \
  -output $OUT_DIR/LiveKitWebRTC.xcframework

cp ./src/LICENSE $OUT_DIR/LiveKitWebRTC.xcframework/

cd $OUT_DIR/LiveKitWebRTC.xcframework/macos-arm64_x86_64/LiveKitWebRTC.framework/
mv LiveKitWebRTC Versions/A/LiveKitWebRTC
ln -s Versions/Current/LiveKitWebRTC LiveKitWebRTC
cd ../../../../

cd $OUT_DIR/LiveKitWebRTC.xcframework/ios-arm64_x86_64-maccatalyst/LiveKitWebRTC.framework/
mv LiveKitWebRTC Versions/A/LiveKitWebRTC
ln -s Versions/Current/LiveKitWebRTC LiveKitWebRTC
cd ../../../
zip --symlinks -9 -r LiveKitWebRTC.xcframework.zip LiveKitWebRTC.xcframework

# hash
shasum -a 256 LiveKitWebRTC.xcframework.zip >LiveKitWebRTC.xcframework.zip.shasum
cat LiveKitWebRTC.xcframework.zip.shasum
