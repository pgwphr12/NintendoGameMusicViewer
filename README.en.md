# Nintendo Game Music Viewer V1.3.1 (1.3.1)

A new C++17 native Windows game-music player with real independent channel scopes. No Mesen or earlier NTSC application code is included. No game ROM execution or game-screen functionality is exposed. The separate music core necessarily simulates the hardware needed to execute music drivers.

Extract the entire portable package and run NintendoGameMusicViewer.exe on Windows 10/11 x64. Keep gme.dll beside the executable. No .NET, Python, CMake, Visual Studio or FFmpeg installation is needed to run the player.

V1.3.1 embeds an original waveform/music-note icon. On startup, an optional prompt offers to register all 14 supported file types for the current Windows user and open Windows Default apps settings. **Complete the default-app choice in Windows Settings.** No administrator permission is required. Choose Not now or Don't ask again; F10 or the title-bar icon menu opens the prompt again. Recording view uses an English prompt; normal view uses Korean. Snapshot mode and automated UI tests suppress this prompt.

Registered types: `.nsf`, `.nsfe`, `.spc`, `.gbs`, `.gsf`, `.minigsf`, `.2sf`, `.mini2sf`, `.usf`, `.miniusf`, `.bcstm`, `.bcwav`, `.vgm`, `.vgz`. Companion libraries, game ROMs and ZIP/Zophar archives are excluded. Registration adds this player as a candidate without overwriting existing extension defaults or Windows UserChoice.

Double-clicked files are sent to the existing player and played there. A minimized window is restored; launching without a file activates the existing window. Simultaneous launches use one player per Windows user/session. An unresponsive existing player produces an error. After moving a registered portable folder, run the executable from its new location once to refresh its own registration under your previous consent. Windows default choices are preserved. Close V1.3 or earlier builds before running this version because they do not implement file handoff.

Open or drop NSF/NSFE/SPC/GBS/GSF/2SF/USF/BCSTM/BCWAV/VGM/VGZ. Select an internal track with the track menu. Space pauses/resumes; arrow keys switch tracks; Ctrl+O opens a file. Click a channel label to mute and its slider to change volume. F9 toggles recording view; F11 fullscreen; Esc exits fullscreen. +/- changes scope time scale, More than eight channels are shown together in two columns divided down the center. The output-rate menu selects 16000/22050/32000/44100/48000/96000 Hz. The master slider is separate. A metadata-defined duration enables the progress/seek bar; unknown lengths remain unknown and require manual stopping. Repeat restarts the current track at a metadata-defined end.

The client defaults to 1280x720 with a 1920x1080 logical canvas, 16:9 letterboxing and native double-buffered GDI drawing. Recording is external (OBS etc.). There is no built-in video exporter, search library, raw register/frequency display, automatic unknown-length detection or 3SF support.

Each hardware voice is rendered by an independent synchronized libgme core with all other voices muted. Initial silence skipping is disabled. These are actual isolated voice PCM signals, not FFT guesses or copies of the final mix. The isolated outputs include the core's filtering/effects and are not raw DAC values. Scopes are pre-fader; mute does not erase their source data. Redundant ACTIVE/SILENT labels are omitted. NES labels are PULSE 1/2, TRIANGLE, NOISE, DMC; FDS is separate. SPC has CHANNEL 1..8 in recording view; GBS has SQUARE 1/2, WAVE, NOISE.

At unity individual volume the original mix and real core mute API are used. Adjusting a channel volume mixes isolated signals, so nonlinear/shared mixer interactions may differ from the original mix. Multiple cores increase CPU/memory requirements. All commercial dumps and exotic expansion behavior are not guaranteed.

The producer renders at fixed 48kHz into a bounded SPSC audio ring; device callbacks consume and windowed-sinc resample without file I/O, core calls, UI calls or dynamic allocation. Separate timestamped channel histories feed an approximately 60Hz visualization clock tied to consumed audio frames. Min/max envelopes and rising triggers reduce aliasing. Histories survive temporary audio underruns; underrun counters remain available internally. Physical device latency can cause a small scope/audio offset.

Changing output rate prepares a replacement device before closing the old one and retains core position/history. Single-device drivers briefly close/reopen output while retaining the sample clock and queued audio. A failed new-format open restores the previous settings and reports the error. Short hardware transition latency is possible; arbitrary device hot removal/default-device changes are not automatically reconnected. Output conversion uses a 32-tap windowed-sinc low-pass filter; it cannot restore information missing from the source PCM. Track changes and seek reset queues; long seeks can block while music cores compute skipped playback.

Upload the full source folder from the source ZIP to a GitHub repository root. The Windows-2022 Actions workflow uses runner-provided VS2022/CMake/Python to configure, build, test real cores and callbacks, install and upload separate V1.3.1 runtime and corresponding-source ZIPs. Actual GitHub execution must be verified after upload. Third-party sources are bundled and pinned by SHA256, so dependency fetching is not required in CI. Local rebuilding is optional:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --component Runtime --prefix dist/NintendoGameMusicViewer
```

Tests generate original music-driver fixtures containing no commercial music or ROM. They check actual decoding, voice separation, track changes, metadata, FDS, mute, resampling, ring concurrency, envelopes and playback states using SDL's dummy device. See VALIDATION.md for hardware/manual verification limits, docs/ARCHITECTURE.md for architecture and THIRD_PARTY_NOTICES.md for licenses. The application is licensed GPL-2.0-or-later; see LICENSE. The LGPL music core is a replaceable DLL with complete corresponding upstream source included.

## V1.1 changes

Normal-view controls and channel labels are Korean. F9 recording view switches channel labels and mute indicators to English. Fallback titles use Track in both views. Music-file metadata keeps its original text. The in-canvas app title/version is removed. Keyboard shortcuts appear only at the bottom of normal view and are hidden in recording view. Channel-label clicks still toggle mute, and sliders still control volume. Muted scopes remain visible in grey with a localized mute label (음소거 normally, MUTED in recording view). Recording view continues to hide editing controls.

The renderer reuses a native-size 32-bit DIB, reallocating only on resize, and caches fonts/pens/brushes. Actual min/max envelopes and connecting lines are rasterized directly into the buffer instead of issuing thousands of GDI calls per frame. 4K retains a native 4K buffer; output resolution and audio sample counts are not reduced. See docs/PERFORMANCE.md for measured results and limits.

FDS-bearing NSF/NSFE files receive a fixed +4 dB output trim and SPC (SNES) receives +10 dB. Plain NES/GBS trim is unchanged. A stereo-linked peak limiter (0.98 ceiling, instantaneous attack, 150 ms release) runs before master volume. This is fixed trim, not automatic silence/quiet-passage normalization. Channel histories retain the original pre-trim PCM; channel mute/volume and MASTER remain functional. Perceived loudness still depends on the music dump and playback device.

## Expansion compatibility

The bundled libgme 0.6.5 NSF/NSFE core implements VRC6, VRC7, FDS, MMC5, Namco 163/106 and Sunsoft 5B/FME-7. This does not guarantee every chip, file revision or music driver works. Unusual bank switching and newer file features need file-specific verification. Only FDS currently has an explicit expansion-audio fixture test here; the other expansion implementations have not been verified with actual music dumps. SPC playback does not execute every SNES game-ROM coprocessor.


## Features carried forward from earlier versions

FDS RAM and copy-on-bank-switch support fixes the supplied Doki Doki Panic and Zelda II dumps loading at $6000. Original archives stay intact; CMake applies the local LGPL source replacements in third_party/gme-fds-patch. See docs/FDS-COMPATIBILITY.md.

Channel volume ranges from 0 to 200%, default 100% (slider midpoint). Raise only the quiet voice; default voice balance is unchanged. Pitch/speed uses 0.1x steps, range 0.5–2.0x, reset 1.0x. Pitch and tempo change together through varispeed. Playback position uses source time. Recording view hides controls.

Output defaults to 96kHz; 16/22.05/32/44.1/48kHz are selectable. Generation stays at 48kHz. V1.2 uses windowed-sinc output filtering to suppress aliasing at lower rates. Reverb adds subtle damped room reflections (off by default), and does not remove echo encoded in a source SPC. Channel histories remain pre-effect. The final limiter bounds effect peaks; stop/seek/track changes reset the tail.

NES DMC stays hidden until actual isolated PCM exceeds peak 0.0001, then remains visible for that track. New files/different tracks reset the latch; same-track seek/stop retain it. Extremely quiet DMC below the threshold may remain hidden. Audio voices remain intact and UI controls map to original core indices.

## V1.4 changes

Pitch/speed offers selectable 0.10x or 0.01x increments with two-decimal display and preserves fine offsets when changing the step. Added reverb reflections increase from 0.18 to 0.45, with a smooth toggle and default off. Master and individual channel volumes have ±5 percentage-point buttons plus sliders, available in F9 recording view too. These controls remain visible in recordings. Channel-label mute clicks work in both views. FDS is named FDS in either view; SNES uses 채널 1..8 normally and CHANNEL 1..8 in recording view.

## V1.5 changes

Reverb strength is adjustable from 0 to 100% with a slider and ±5 percentage-point buttons. Default is 45%, matching V1.4; the on/off toggle is independent. Strength controls added reflection amplitude, not decay duration: 0% adds no reflections and 100% adds unity-gain reflections to the dry signal. Changes ramp smoothly and the output limiter bounds peaks.

F9 recording view retains only master volume controls. Per-channel sliders, ± buttons and percentages are hidden. Channel-label mute clicks and MUTED indicators remain available. Normal view retains all channel controls.

## Official V1 · 1.0.0

This package is the official V1. Earlier V1.1–V1.5 labels refer to development builds. Master volume now defaults to 100%.

Reverb leaves a decaying tail after source notes end. Decay time is adjustable from 0.1 to 3.0 seconds in 0.1-second steps, default 1.2 seconds. This is an approximate low-frequency RT60; damping shortens high-frequency tails, and perceived duration depends on the sound and wet amount. Reflection strength remains independently adjustable from 0 to 100%, default 45%, with the effect off by default. Stop, seek and track changes clear the tail. Recording view retains master volume only, with per-channel controls hidden and channel-label mute still available.

## Expanded V1 · 1.0.1

Supports all seven target systems through specific music formats: NES NSF/NSFE, SNES SPC, GB GBS, GBA GSF/miniGSF, DS 2SF/mini2SF, N64 USF/miniUSF and 3DS BCSTM/BCWAV. Mini files require their referenced gsflib/2sflib/usflib companions and original relative folder layout. PSF formats contain one track per file and read title/game/artist/length/fade metadata; open a different file for another track. CRC, section bounds, decompression caps and dependency cycle checks apply. Keep native 3DS file extensions for decoder filename checks.

GBA runs synchronized full-mix plus six isolated cores at 48kHz (pulse 1/2, wave, noise, PCM A/B). Software instruments already mixed inside PCM A/B are not separated. DS extracts actual 16-channel PCM before UI mute/final clipping from a single instrumented vio2sf core; all channels appear together in two columns. DS 44.1kHz and native USF/3DS rates share a linear 48kHz resampling clock.

N64 exposes supported Audio/NAudio dry synthesis slots plus residual effects, with stereo fallback for other mixing paths. 3DS displays stored stream channels, not individual instruments. Native PCM/DSP-ADPCM and other built-in vgmstream codecs are available; external FFmpeg/Vorbis/MP3 codecs are disabled. 3DS game ROMs, BCSAR archives, BCSEQ sequences and 3SF are not supported. These are specific music-format implementations, not a guarantee of every dump/driver/codec for each console.

Originally authored ARM/MIPS music drivers and PCM/DSP-ADPCM streams test actual decoding, independent PCM, source-preserving mute, metadata, seek/end, audio device and native UI paths. Supplied GBA/N64/3DS files were checked with the limited coverage described in V1.2; see V1.1 for the supplied DS set. Complete corresponding application/core sources, modified files and pinned original archives are in the separate source ZIP. Supply both ZIPs together. Application license: GPL-2.0-or-later.

## V1.1 stability and folder playback

Fixed an out-of-range DS waveform-capture write when a one-shot PCM/ADPCM channel finishes. AddressSanitizer checks passed for the first six seconds plus seek/mute in 98 user-provided New Super Mario Bros. (EMU) mini2SF tracks. Entire songs and every music dump remain unverified. See docs/DS-STABILITY.md.

SPC, GSF/miniGSF, 2SF/mini2SF, USF/miniUSF and BCSTM/BCWAV use same-system music files from the current folder, sorted by filename with natural numeric ordering. Tracks, Previous/Next and Left/Right select folder files. Subfolders and music libraries are excluded. Manual selection keeps playback/paused state. Known-duration tracks automatically advance to the next file; the final file stops. Manual navigation wraps. Repeat repeats the current track. Unknown-duration PSF tracks still need manual navigation. NSF/NSFE/GBS retain internal track selection.

## Autoplay toggle (1.2.0)

Use the normal-view Autoplay button or A key to toggle automatic advance to the next folder file. Default: on. Turning it off stops at the current track end; manual Previous/Next and Tracks still work. The setting survives file changes and recording-view switches for the current session, and returns to its default on app restart. Recording view hides the button; A remains available. Repeat independently repeats the current track. Enabling autoplay alone does not resume stopped playback.

## V1.2 · 1.2.0

GBA PCM now uses a 16-kernel reconstruction bank selected from the actual timer sample rate. Output conversion uses 32-tap windowed-sinc interpolation with anti-alias filtering for lower rates and varispeed. The supplied Super Mario Advance 4 PCM is approximately 10,512 Hz, 8-bit; 96kHz output cannot recover missing source bandwidth. PCM A/B may already contain software-mixed instruments.

N64 Audio/NAudio dry envelope slots are captured before summation and traced through interleave/save/Audio Interface DMA to preserve sample alignment. Up to 30 synthesis slots plus residual game effects are supported; V1.3.1 displays all declared slots immediately, including silent slots. These are synthesizer state slots, not MIDI instrument names or fixed hardware voices. Unsupported NEAD/MusyX/software paths retain final LEFT/RIGHT output. No duplicate full-mix waveforms or 30 emulation cores are used.

3DS BCSTM track tables provide stereo/mono grouping, volume and pan. Mono stems are centered instead of being assigned to a single side. Stored channels cannot expose instruments already mixed together: ATHLETIC has 5 channels, MENU 8, BIG_MARIO 2.

The first six seconds plus seek and mute were checked on 38 supplied N64 files, 99 3DS files and 3 GBA files. This is limited coverage, not a guarantee for every game or every position. Autoplay/folder navigation and the DS one-shot crash fix remain included. All corresponding source and GPL notices are included.

## V1.3 series · SMS/Mega Drive support

- Added SMS and Mega Drive/Genesis VGM/VGZ playback: SMS PSG 4 voices; YM2413 expansion 9 FM + 5 drums + 4 PSG; Genesis 6 FM + DAC + 4 PSG.
- Folder navigation and autoplay also apply to VGM/VGZ. Actual command waits determine the first-pass duration; Repeat restarts the entire file.
- More than eight channels appear together in two columns, ordered down the left then the right. N64 slots appear before they produce sound. Unused NES DMC hiding is retained.
- Runtime and complete corresponding source are separate: `NintendoGameMusicViewer-V1.3.1-Windows-x64.zip` and `NintendoGameMusicViewer-V1.3.1-source.zip`. Provide both when distributing under GPL. Runtime contains the executable, replaceable DLL, documentation and licenses.

Supported VGM/VGZ uses single SN76489/YM2413/YM2612 chips and legacy DAC commands. Other consoles/chips, dual chips, streaming DAC commands 0x90–0x95, GYM/SGC and ROMs are not supported. Encoded/decoded files are capped at 64MiB and the first pass at one hour. The GENS FM core may sound different from real hardware. See docs/V1.3-SEGA.md in the source ZIP.
