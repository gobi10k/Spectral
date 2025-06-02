cmake_minimum_required(VERSION 3.15)
project(SpectralVocoder)

# Try to find JUCE. This requires JUCE to be installed in a way CMake can find it,
# or JUCE_ROOT environment variable to be set.
# For this environment, I'm hoping it's discoverable or that the JUCE modules
# being in the same parent directory structure (if fetched) would help.
# This is the most likely point of failure without a proper JUCE SDK setup.
find_package(JUCE REQUIRED)

# Add the audio plugin target
juce_add_plugin(SpectralVocoder
    VERSION "1.0.0"
    PLUGIN_MANUFACTURER_CODE "Manu"
    PLUGIN_CODE "SpVo"
    FORMATS VST3 AU # Add other formats if needed e.g. Standalone
    PRODUCT_NAME "SpectralVocoder"
    COMPANY_NAME "MyCompany" # Change as needed
    IS_SYNTH FALSE # True if it's a synth
    NEEDS_MIDI_INPUT TRUE # Change if your plugin doesn't need MIDI
    NEEDS_MIDI_OUTPUT FALSE # Change if your plugin doesn't produce MIDI
    IS_MIDI_EFFECT FALSE # True if it's a MIDI effect
)

# Specify C++ standard
target_compile_features(SpectralVocoder PUBLIC cxx_std_17)

# Add source files from the Source directory
target_sources(SpectralVocoder PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

# Link to the JUCE modules that were defined in the .jucer file
# juce_add_plugin should handle module dependencies based on the .jucer file,
# or they might need to be listed explicitly if find_package(JUCE) doesn't fully
# configure it from the .jucer file directly in this manual setup.
# The .jucer file lists: juce_audio_basics, juce_audio_devices, etc.
# juce_target_juce_modules(SpectralVocoder
#     PUBLIC
#         juce_audio_basics
#         juce_audio_devices
#         juce_audio_formats
#         juce_audio_plugin_client
#         juce_audio_processors
#         juce_audio_utils
#         juce_core
#         juce_data_structures
#         juce_dsp
#         juce_events
#         juce_graphics
#         juce_gui_basics
#         juce_gui_extra
# )

# The above juce_target_juce_modules is often not needed if juce_add_plugin can
# parse the .jucer file, or if JuceLibraryCode is generated and used.
# For a manual CMake setup without Projucer's generated JuceLibraryCode directory,
# ensuring modules are linked can be tricky.
# We will rely on juce_add_plugin and find_package(JUCE) to correctly set up includes and links.

# If JuceLibraryCode is not generated (usually by Projucer), CMake needs to be
# told where to find the JUCE module headers. This is typically handled by
# find_package(JUCE) setting up include directories.

# Set the output directory for the plugin
juce_set_plugin_output_directory(SpectralVocoder DESTINATION "${CMAKE_BINARY_DIR}/Plugins")
