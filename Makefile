BUILD_DIR := out
ARCH      := x64

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

.PHONY: build
build:
	gn gen $(BUILD_DIR) --args="target_os=\"linux\" target_cpu=\"$(ARCH)\" is_debug=true rtc_include_tests=false rtc_use_h264=true ffmpeg_branding=\"Chrome\" is_component_build=false use_rtti=true use_custom_libcxx=false rtc_enable_protobuf=false rtc_enable_symbol_export=true"
	ninja -C $(BUILD_DIR) audio_processing