# Yolo (VST3)

Real-time monophonic pitch correction. C++/JUCE 8 port of the Yolo browser prototype.

- Source/YoloTune.h: DSP engine (YIN pitch tracking, scale snapping, two-tap delay-line shifter, doubler, soft limiter). No JUCE dependency.
- Tests/dsp_test.cpp: offline tests. `g++ -O2 -std=c++17 -ISource Tests/dsp_test.cpp -o t && ./t`
- Parameters: Speed (-3..200 ms), Amount, Sustain, Flex-Tune, Humanize, Doubler, Input/Output, Key, Scale (Chromatic/Major/Minor/Custom + piano), Stabilizer, Reference, Instrument, Low Latency, Bypass. Six factory presets (host program list).

## Build
Needs CMake 3.22+ and a C++17 compiler. JUCE 8.0.8 is fetched automatically (or put it in ./JUCE).
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Output: build/Yolo_artefacts/Release/VST3/Yolo.vst3
Windows/macOS: push to GitHub and download the artifacts from the Actions run (.github/workflows/build.yml).
Install: Windows `C:\Program Files\Common Files\VST3`, macOS `~/Library/Audio/Plugins/VST3`, Linux `~/.vst3`.

## Notes
Latency is reported to the host (about 288 samples at 48 kHz in Low Latency mode). Rain/lightning effects are web-only; the native editor is a simplified Yolo-styled UI.
