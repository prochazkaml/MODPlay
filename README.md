# MODPlay
Yet another simple MOD player library written in C. But I don't care, because this one's _mine_. Supports up to 32-channel files by default (though this can be changed by editing the header file).

To hear for yourself how it sounds, you may visit [this website](https://mod.prochazkaml.eu/), which uses this exact library (compiled to WebAssembly with Emscripten), wrapped around a nice Windows 3.11-themed UI. There are several examples available to pick from, but you may also upload your own files (OUTDATED - only supports 4-channel files).

A simple example program is supplied with this library, which uses SDL for audio output. To compile it, run the following:

```
cc example.c modplay.c -lSDL -o example
```

There are two valid ways to use this library: via an audio callback (whose buffer will be filled by `RenderMOD()`) or via repeatedly calling`ProcessMOD()`.
Use the former whenever it is possible, unless you want to write your own Paula emulator.
Use the latter only when hardware restrictions limit you from setting up an audio callback,
and said hardware contains a dedicated hardware sampler/mixer, for which you will have to write a translation layer
to translate Paula parameters to the hardware's native parameters (e.g. the original Sony PlayStation).

DO NOT USE BOTH AT ONCE! Only use the `RenderMOD()` method, or the `ProcessMOD()` method.
`RenderMOD()` calls `ProcessMOD()` internally by itself, by calling both functions, the player will misbehave badly.

## Library compiled size

<!-- LIBRARY_SIZE_START -->
Compiled with the default number of channels:

|Compiler|Arch|.text|.data|.bss
|-|-|-|-|-|
|clang 17.0.6 (-O0)|amd64|9599|0|3152|
|clang 17.0.6 (-O3)|amd64|6117|0|3152|
|clang 17.0.6 (-Os)|amd64|4928|0|3152|
|clang 17.0.6 (-O0)|i386|7607|0|2632|
|clang 17.0.6 (-O3)|i386|5525|0|2632|
|clang 17.0.6 (-Os)|i386|4373|0|2632|
|clang 17.0.6 (-O0)|aarch64|10556|0|3152|
|clang 17.0.6 (-O3)|aarch64|5516|0|3152|
|clang 17.0.6 (-Os)|aarch64|4244|0|3152|
|clang 17.0.6 (-O0)|armv7m|6046|0|2632|
|clang 17.0.6 (-O3)|armv7m|4564|0|2632|
|clang 17.0.6 (-Os)|armv7m|3028|0|2632|

Compiled for 4 channels maximum:

|Compiler|Arch|.text|.data|.bss
|-|-|-|-|-|
|clang 17.0.6 (-O0)|amd64|9599|0|912|
|clang 17.0.6 (-O3)|amd64|6117|0|912|
|clang 17.0.6 (-Os)|amd64|4928|0|912|
|clang 17.0.6 (-O0)|i386|7607|0|616|
|clang 17.0.6 (-O3)|i386|5525|0|616|
|clang 17.0.6 (-Os)|i386|4378|0|616|
|clang 17.0.6 (-O0)|aarch64|10556|0|912|
|clang 17.0.6 (-O3)|aarch64|5580|0|912|
|clang 17.0.6 (-Os)|aarch64|4244|0|912|
|clang 17.0.6 (-O0)|armv7m|6008|0|616|
|clang 17.0.6 (-O3)|armv7m|4592|0|616|
|clang 17.0.6 (-Os)|armv7m|3032|0|616|

<!-- LIBRARY_SIZE_END -->

## Supported effects

The following list was taken from the [OpenMPT wiki](https://wiki.openmpt.org/Manual:_Effect_Reference#MOD_Effect_Commands).

|Eff|Name|Category|Supported|
|-|-|-|-|
|`0xy`|Arpeggio|Pitch|✓|
|`1xx`|Portamento Up|Pitch|✓|
|`2xx`|Portamento Down|Pitch|✓|
|`3xx`|Tone Portamento|Pitch|✓|
|`4xy`|Vibrato|Pitch|✓|
|`5xy`|Volume Slide + Tone Portamento|Volume + Pitch|✓|
|`6xy`|Volume Slide + Vibrato|Volume + Pitch|✓|
|`7xy`|Tremolo|Volume|✓|
|`8xx`|Set Panning|Panning|- (non-standard)|
|`9xx`|Sample Offset|Other|✓|
|`Axy`|Volume Slide|Volume|✓|
|`Bxx`|Position Jump|Global|✓|
|`Cxx`|Set Volume|Volume|✓|
|`Dxx`|Pattern Break|Global|✓|
|`E0x`|Set Filter|Other|- (and never will be, there is no filter to begin with)|
|`E1x`|Fine Portamento Up|Pitch|✓|
|`E2x`|Fine Portamento Down|Pitch|✓|
|`E3x`|Glissando|Pitch|- (not widely supported anyway)|
|`E4x`|Set Vibrato Waveform|Pitch|✓|
|`E5x`|Set Finetune|Pitch|✓|
|`E60`|Pattern Loop Start|Global|✓|
|`E6x`|Pattern Loop|Global|✓|
|`E7x`|Set Tremolo Waveform|Volume|✓|
|`E8x`|Set Panning|Panning|- (non-standard)|
|`E9x`|Retrigger|Other|✓|
|`EAx`|Fine Volume Slide Up|Volume|✓|
|`EBx`|Fine Volume Slide Down|Volume|✓|
|`ECx`|Note Cut|Other|✓|
|`EDx`|Note Delay|Other|✓|
|`EEx`|Pattern Delay|Global|✓|
|`EFx`|Invert Loop|Other|- (noone is sure what it even does)|
|`Fxx`|Set Speed / Tempo|Global|✓|
