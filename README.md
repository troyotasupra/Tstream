# Tstream

A VST3 plugin that streams audio out of a DAW over UDP — a ReaStream-style network audio sender.
Pairs with [TstreamOBS](https://github.com/troyotasupra/TstreamOBS), an OBS Studio source plugin
that receives the stream regardless of sample rate.

## Building

Requires the [JUCE](https://juce.com) SDK and Visual Studio 2022 (Windows only, for now).

```
cmake -B build -G "Visual Studio 17 2022" -A x64 -DTSTREAM_JUCE_DIR="C:/path/to/JUCE"
cmake --build build --config Release --target Tstream_VST3
```

Installs to `%LOCALAPPDATA%\Programs\Common\VST3` automatically after building.

## License

[GNU AGPLv3](LICENSE). This plugin links against the JUCE framework, which is free to use under
AGPLv3 or a separate commercial license from [juce.com](https://juce.com) — see JUCE's own
[LICENSE.md](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md) for details.
