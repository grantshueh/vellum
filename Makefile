# ============================================================================
# Vellum — drum resynthesis instrument. Hand-rolled JUCE build (clang only,
# no CMake / Xcode.app required — Command Line Tools are enough).
#
#   make au          -> build/Vellum.component   (Audio Unit v2, for Logic Pro)
#   make standalone  -> build/Vellum.app         (standalone test app)
#   make tests       -> build/leveler_test, runs the auto-level behaviour tests
#   make install     -> copies the AU into ~/Library/Audio/Plug-Ins/Components
#   make validate    -> runs auval on the installed AU
# ============================================================================

JUCE      := external/JUCE/modules
BUILD     := build
SRC       := src
CXX       := clang++
CC        := clang
JOBS      ?= 8

MACOS_MIN := -mmacosx-version-min=11.0
OPT       := -O3 -fno-math-errno
CPPSTD    := -std=c++17

COMMON_FLAGS := $(OPT) $(MACOS_MIN) -fvisibility=hidden -fvisibility-inlines-hidden \
                -I$(JUCE) -I$(JUCE)/juce_audio_plugin_client/AU -I$(SRC)

MODULE_DEFS := \
  -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
  -DNDEBUG=1 \
  -DJUCE_MODULE_AVAILABLE_juce_core=1 \
  -DJUCE_MODULE_AVAILABLE_juce_events=1 \
  -DJUCE_MODULE_AVAILABLE_juce_data_structures=1 \
  -DJUCE_MODULE_AVAILABLE_juce_graphics=1 \
  -DJUCE_MODULE_AVAILABLE_juce_gui_basics=1 \
  -DJUCE_MODULE_AVAILABLE_juce_gui_extra=1 \
  -DJUCE_MODULE_AVAILABLE_juce_audio_basics=1 \
  -DJUCE_MODULE_AVAILABLE_juce_audio_devices=1 \
  -DJUCE_MODULE_AVAILABLE_juce_audio_formats=1 \
  -DJUCE_MODULE_AVAILABLE_juce_audio_processors=1 \
  -DJUCE_MODULE_AVAILABLE_juce_audio_utils=1 \
  -DJUCE_MODULE_AVAILABLE_juce_dsp=1 \
  -DJUCE_MODULE_AVAILABLE_juce_audio_plugin_client=1 \
  -DJUCE_USE_CURL=0 \
  -DJUCE_WEB_BROWSER=0 \
  -DJUCE_USE_MP3AUDIOFORMAT=0 \
  -DJUCE_STRICT_REFCOUNTEDPOINTER=1 \
  -DJUCE_VST3_CAN_REPLACE_VST2=0

# Manufacturer 'Vlum' = 0x566c756d, plugin 'Vel1' = 0x56656c31
# VersionCode 0x000100 == 256 — must match <version> in packaging/AU-Info.plist
PLUGIN_DEFS := \
  -DJucePlugin_Name='"Vellum"' \
  -DJucePlugin_Desc='"Drum resynthesis instrument"' \
  -DJucePlugin_Manufacturer='"Vellum"' \
  -DJucePlugin_ManufacturerWebsite='""' \
  -DJucePlugin_ManufacturerEmail='""' \
  -DJucePlugin_ManufacturerCode=0x566c756d \
  -DJucePlugin_PluginCode=0x56656c31 \
  -DJucePlugin_IsSynth=1 \
  -DJucePlugin_WantsMidiInput=1 \
  -DJucePlugin_ProducesMidiOutput=0 \
  -DJucePlugin_IsMidiEffect=0 \
  -DJucePlugin_EditorRequiresKeyboardFocus=0 \
  -DJucePlugin_Version=0.1.0 \
  -DJucePlugin_VersionString='"0.1.0"' \
  -DJucePlugin_VersionCode=0x000100 \
  -DJucePlugin_VSTUniqueID=JucePlugin_PluginCode \
  -DJucePlugin_Vst3Category='"Instrument|Drum"' \
  -DJucePlugin_AUMainType="'aumu'" \
  -DJucePlugin_AUSubType=JucePlugin_PluginCode \
  -DJucePlugin_AUExportPrefix=VellumAU \
  -DJucePlugin_AUExportPrefixQuoted='"VellumAU"' \
  -DJucePlugin_AUManufacturerCode=JucePlugin_ManufacturerCode \
  -DJucePlugin_CFBundleIdentifier=com.vellum.vellum

AU_DEFS   := -DJUCE_STANDALONE_APPLICATION=0 -DJucePlugin_Build_AU=1 \
             -DJucePlugin_Build_Standalone=0 -DJucePlugin_Build_VST3=0 \
             -DJucePlugin_Build_VST=0 -DJucePlugin_Build_AAX=0 \
             -DJucePlugin_Build_AUv3=0 -DJucePlugin_Build_LV2=0 \
             -DJucePlugin_Build_Unity=0

SA_DEFS   := -DJUCE_STANDALONE_APPLICATION=1 -DJucePlugin_Build_Standalone=1 \
             -DJucePlugin_Build_AU=0 -DJucePlugin_Build_VST3=0 \
             -DJucePlugin_Build_VST=0 -DJucePlugin_Build_AAX=0 \
             -DJucePlugin_Build_AUv3=0 -DJucePlugin_Build_LV2=0 \
             -DJucePlugin_Build_Unity=0 \
             -DJUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0

DEFS      := $(MODULE_DEFS) $(PLUGIN_DEFS)

JUCE_CXXFLAGS := $(COMMON_FLAGS) $(CPPSTD) -w
OUR_CXXFLAGS  := $(COMMON_FLAGS) $(CPPSTD) -Wall -Wextra -Wno-unused-parameter

JUCE_MM := \
  juce_core/juce_core.mm \
  juce_events/juce_events.mm \
  juce_data_structures/juce_data_structures.mm \
  juce_graphics/juce_graphics.mm \
  juce_gui_basics/juce_gui_basics.mm \
  juce_gui_extra/juce_gui_extra.mm \
  juce_audio_basics/juce_audio_basics.mm \
  juce_audio_devices/juce_audio_devices.mm \
  juce_audio_formats/juce_audio_formats.mm \
  juce_audio_processors/juce_audio_processors.mm \
  juce_audio_utils/juce_audio_utils.mm \
  juce_dsp/juce_dsp.mm

JUCE_CPP := \
  juce_core/juce_core_CompilationTime.cpp \
  juce_graphics/juce_graphics_Harfbuzz.cpp

JUCE_C := \
  juce_graphics/juce_graphics_Sheenbidi.c

OUR_CPP := \
  PluginProcessor.cpp \
  PluginEditor.cpp \
  dsp/Analyzer.cpp \
  dsp/Humanizer.cpp \
  dsp/Voice.cpp \
  dsp/Slicer.cpp \
  seq/Groove.cpp \
  ui/Views.cpp

OUR_HEADERS := $(wildcard $(SRC)/*.h) $(wildcard $(SRC)/dsp/*.h) \
               $(wildcard $(SRC)/seq/*.h) $(wildcard $(SRC)/ui/*.h)

AU_CLIENT := \
  juce_audio_plugin_client/juce_audio_plugin_client_AU_1.mm \
  juce_audio_plugin_client/juce_audio_plugin_client_AU_2.mm

SA_CLIENT := \
  juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp

FRAMEWORKS := \
  -framework Accelerate -framework AudioToolbox -framework AudioUnit \
  -framework Cocoa -framework CoreAudio -framework CoreAudioKit \
  -framework CoreMIDI -framework CoreImage -framework CoreText \
  -framework CoreServices -framework Foundation -framework IOKit \
  -framework Metal -framework MetalKit -framework QuartzCore \
  -framework Security -weak_framework UniformTypeIdentifiers

define objs_for
$(patsubst %.mm,$(BUILD)/obj/$(1)/%.o,$(JUCE_MM)) \
$(patsubst %.cpp,$(BUILD)/obj/$(1)/%.o,$(JUCE_CPP)) \
$(patsubst %.c,$(BUILD)/obj/$(1)/%.o,$(JUCE_C)) \
$(patsubst %.cpp,$(BUILD)/obj/$(1)/src/%.o,$(OUR_CPP))
endef

AU_OBJS := $(call objs_for,au) $(patsubst %.mm,$(BUILD)/obj/au/%.o,$(AU_CLIENT))
SA_OBJS := $(call objs_for,sa) $(patsubst %.cpp,$(BUILD)/obj/sa/%.o,$(SA_CLIENT))

AU_BUNDLE := $(BUILD)/Vellum.component
SA_APP    := $(BUILD)/Vellum.app

.PHONY: all au standalone install validate clean tests
all: au standalone tests
au: $(AU_BUNDLE)/Contents/MacOS/Vellum
standalone: $(SA_APP)/Contents/MacOS/Vellum

# --- AU flavor ---------------------------------------------------------------
$(BUILD)/obj/au/%.o: $(JUCE)/%.mm
	@mkdir -p $(dir $@)
	$(CXX) $(JUCE_CXXFLAGS) $(DEFS) $(AU_DEFS) -ObjC++ -c $< -o $@

$(BUILD)/obj/au/%.o: $(JUCE)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(JUCE_CXXFLAGS) $(DEFS) $(AU_DEFS) -c $< -o $@

$(BUILD)/obj/au/%.o: $(JUCE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(OPT) $(MACOS_MIN) -I$(JUCE) $(MODULE_DEFS) -w -c $< -o $@

$(BUILD)/obj/au/src/%.o: $(SRC)/%.cpp $(OUR_HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(OUR_CXXFLAGS) $(DEFS) $(AU_DEFS) -c $< -o $@

# --- standalone flavor -------------------------------------------------------
$(BUILD)/obj/sa/%.o: $(JUCE)/%.mm
	@mkdir -p $(dir $@)
	$(CXX) $(JUCE_CXXFLAGS) $(DEFS) $(SA_DEFS) -ObjC++ -c $< -o $@

$(BUILD)/obj/sa/%.o: $(JUCE)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(JUCE_CXXFLAGS) $(DEFS) $(SA_DEFS) -c $< -o $@

$(BUILD)/obj/sa/%.o: $(JUCE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(OPT) $(MACOS_MIN) -I$(JUCE) $(MODULE_DEFS) -w -c $< -o $@

$(BUILD)/obj/sa/src/%.o: $(SRC)/%.cpp $(OUR_HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(OUR_CXXFLAGS) $(DEFS) $(SA_DEFS) -c $< -o $@

# --- link: Audio Unit bundle -------------------------------------------------
$(AU_BUNDLE)/Contents/MacOS/Vellum: $(AU_OBJS) packaging/AU-Info.plist
	@mkdir -p $(AU_BUNDLE)/Contents/MacOS $(AU_BUNDLE)/Contents/Resources
	$(CXX) -bundle $(MACOS_MIN) -dead_strip -o $@ $(AU_OBJS) $(FRAMEWORKS)
	cp packaging/AU-Info.plist $(AU_BUNDLE)/Contents/Info.plist
	printf 'BNDL????' > $(AU_BUNDLE)/Contents/PkgInfo
	codesign --force -s - $(AU_BUNDLE)

# --- link: standalone app ----------------------------------------------------
$(SA_APP)/Contents/MacOS/Vellum: $(SA_OBJS) packaging/App-Info.plist
	@mkdir -p $(SA_APP)/Contents/MacOS $(SA_APP)/Contents/Resources
	$(CXX) $(MACOS_MIN) -dead_strip -o $@ $(SA_OBJS) $(FRAMEWORKS)
	cp packaging/App-Info.plist $(SA_APP)/Contents/Info.plist
	printf 'APPL????' > $(SA_APP)/Contents/PkgInfo
	codesign --force -s - $(SA_APP)

# --- tests (pure C++, no JUCE) ------------------------------------------------
tests: $(BUILD)/leveler_test
	$(BUILD)/leveler_test

$(BUILD)/leveler_test: tests/leveler_test.cpp $(SRC)/dsp/Leveler.h $(SRC)/dsp/GainStage.h
	@mkdir -p $(BUILD)
	$(CXX) $(CPPSTD) -O2 -Wall -Wextra -I$(SRC) $< -o $@

install: au
	rm -rf ~/Library/Audio/Plug-Ins/Components/Vellum.component
	cp -R $(AU_BUNDLE) ~/Library/Audio/Plug-Ins/Components/

validate: install
	killall -9 AudioComponentRegistrar 2>/dev/null || true
	auval -v aumu Vel1 Vlum

clean:
	rm -rf $(BUILD)

# precompile only the vendored JUCE objects (lets the framework build overlap with app work)
.PHONY: juce-au juce-sa
juce-au: $(filter-out $(BUILD)/obj/au/src/%,$(AU_OBJS))
juce-sa: $(filter-out $(BUILD)/obj/sa/src/%,$(SA_OBJS))

# offline engine test: analyse a one-shot, check identity, render a humanised family
TEST_JUCE_OBJS := $(BUILD)/obj/sa/juce_core/juce_core.o $(BUILD)/obj/sa/juce_core/juce_core_CompilationTime.o \
  $(BUILD)/obj/sa/juce_events/juce_events.o $(BUILD)/obj/sa/juce_data_structures/juce_data_structures.o \
  $(BUILD)/obj/sa/juce_audio_basics/juce_audio_basics.o $(BUILD)/obj/sa/juce_audio_formats/juce_audio_formats.o \
  $(BUILD)/obj/sa/juce_dsp/juce_dsp.o
$(BUILD)/render_test: tests/render_test.cpp $(BUILD)/obj/sa/src/dsp/Analyzer.o $(BUILD)/obj/sa/src/dsp/Humanizer.o $(BUILD)/obj/sa/src/dsp/Voice.o $(TEST_JUCE_OBJS)
	$(CXX) $(OUR_CXXFLAGS) $(DEFS) $(SA_DEFS) $< $(BUILD)/obj/sa/src/dsp/Analyzer.o $(BUILD)/obj/sa/src/dsp/Humanizer.o $(BUILD)/obj/sa/src/dsp/Voice.o $(TEST_JUCE_OBJS) $(FRAMEWORKS) -o $@
render-test: $(BUILD)/render_test
.PHONY: render-test
$(BUILD)/slice_test: tests/slice_test.cpp $(BUILD)/obj/sa/src/dsp/Slicer.o $(BUILD)/obj/sa/src/dsp/Analyzer.o $(TEST_JUCE_OBJS)
	$(CXX) $(OUR_CXXFLAGS) $(DEFS) $(SA_DEFS) $< $(BUILD)/obj/sa/src/dsp/Slicer.o $(BUILD)/obj/sa/src/dsp/Analyzer.o $(TEST_JUCE_OBJS) $(FRAMEWORKS) -o $@
slice-test: $(BUILD)/slice_test
.PHONY: slice-test
UI_SNAP_OBJS := $(filter-out %juce_audio_plugin_client_Standalone.o,$(SA_OBJS))
$(BUILD)/ui_snapshot: tests/ui_snapshot.cpp $(UI_SNAP_OBJS)
	$(CXX) $(OUR_CXXFLAGS) $(DEFS) $(SA_DEFS) $< $(UI_SNAP_OBJS) $(FRAMEWORKS) -o $@
ui-snapshot: $(BUILD)/ui_snapshot
.PHONY: ui-snapshot
$(BUILD)/drop_test: tests/drop_test.cpp $(UI_SNAP_OBJS)
	$(CXX) $(OUR_CXXFLAGS) $(DEFS) $(SA_DEFS) $< $(UI_SNAP_OBJS) $(FRAMEWORKS) -o $@
$(BUILD)/drag_harness: tests/drag_harness.mm $(UI_SNAP_OBJS)
	$(CXX) $(OUR_CXXFLAGS) $(DEFS) $(SA_DEFS) -ObjC++ -fobjc-arc $< $(UI_SNAP_OBJS) $(FRAMEWORKS) -o $@
