# Third-party notices

## Game_Music_Emu 0.6.5

Upstream: https://github.com/libgme/game-music-emu/tree/0.6.5

Used for NSF/NSFE/SPC/GBS/VGM music playback, metadata and actual voice mute/isolation APIs. Built as a separate, replaceable gme.dll with local FDS memory/bank modifications dated 2026-10-10. Complete modified files (Nsf_Emu.h, Nsf_Emu.cpp, nes_cpu_io.h) are in third_party/gme-fds-patch. third_party/apply_gme_fds.cmake checks source hashes and applies them. Modifications retain LGPL-2.1-or-later and original notices. The original archive is unchanged. Corresponding full source archive: third_party/game-music-emu-0.6.5.zip; SHA256 95444046148720dfa74ad47641dcb0232ec3e3bbd8ab5c6a013c0857da8b74bf. LGPL-2.1-or-later for the selected music-core modules; copyright Shay Green and upstream contributors. emu2413 (VRC7) uses MIT terms, copyright Mitsutaka Okazaki. Full archive retains individual notices and licenses.

The upstream archive also contains GPL-only components (such as the alternative MAME YM2612 core); those and non-target systems are not built. The archive's GPL text is included to preserve upstream notices, the new application now separately uses GPL-2.0-or-later. Consult per-file notices when changing the supported core selection. Do not replace gme.dll with a different architecture or incompatible API. Users may rebuild and replace the LGPL library for their own modified library use.

## SDL2 2.30.11

Upstream: https://github.com/libsdl-org/SDL/tree/release-2.30.11

Used for device audio callbacks and output device management; native Win32/GDI handles the UI. SDL2 is statically linked; zlib license, copyright Sam Lantinga and contributors. Full unmodified source: third_party/SDL-2.30.11.zip; SHA256 175eff804dface261f0ea239244da6c2f851b2b296d1ba70c42b6b710a90d6f5. LICENSE.txt is also copied to licenses/SDL2.txt.

## Application and fixtures

No Mesen/MesenCE source, UI or ROM is included. Reference video is used only for visual study and is not distributed. Own test music drivers are original generated diagnostic music, explicitly labeled as such; they are not Zelda music. The newly authored application and fixtures are licensed GPL-2.0-or-later; see LICENSE.

## GBA and DS music cores

GBA: viogsf/VBA-M from https://github.com/xbmc/audiodecoder.gsf at b0c965cfa5c1e6242925f23acdba391aaf4517c7. Original archive audiodecoder.gsf-b0c965cfa5c1e6242925f23acdba391aaf4517c7.zip, SHA256 6fc66b01855bde6286ad991efefea28e391abb4b7711c101ab52434659c8037b. Full compiled core source in third_party/viogsf; original notices and per-file GPL/LGPL terms retained. Application builds the core only, without Kodi APIs.

DS: vio2sf/DeSmuME from https://github.com/xbmc/audiodecoder.2sf at 039eeb7de76bf28d20c64ab4ef778982e70c639d. Original archive audiodecoder.2sf-039eeb7de76bf28d20c64ab4ef778982e70c639d.zip, SHA256 c455496f2d71b830f4e3369a5885a0e7c31ab6de33cc277214d237455f424a44. GPL-2.0-or-later; full compiled source in third_party/vio2sf. Local modifications dated 2026-10-10 add pre-mute channel PCM capture to SPU.cpp, SPU.h and state.h, and remove GCC-specific SSE flags from its CMakeLists.txt. The original archive remains intact.

zlib 1.3.1: https://github.com/madler/zlib/tree/v1.3.1, zlib license. Full source third_party/zlib; original archive zlib-1.3.1.zip SHA256 50b24b47bf19e1f35d2a21ff36d2a366638cdf958219a66f30ce0861201760e6. Used by the new bounded PSF container loader.

The user selected GPL-2.0-or-later for the combined application on 2026-10-10. See LICENSE. Complete corresponding application and core sources, patches, build files and notices are included in the separate corresponding source ZIP. The original first V1 package predates this licensing choice.

## Nintendo 64 and 3DS

LazyUSF from https://github.com/xbmc/audiodecoder.usf at 0df2a0a09fd8f68e3275c20e6fec3d5cab14f621. Archive audiodecoder.usf-0df2a0a09fd8f68e3275c20e6fec3d5cab14f621.zip SHA256 6b7a041f17d8c26b1512864b9ed8c51c584be538c0d536651ca632409b67bd62. Full compiled source third_party/lazyusf; CC0 core and GPL-2.0-or-later RSP HLE portions retain per-file notices. Local CMake removes GCC-only optional compiler probes. The application's own PSF parser validates reserved upload bounds before passing them to LazyUSF.

vgmstream from https://github.com/vgmstream/vgmstream at 7dc938fa2f210943b37c7b6511852b516ef432ab. Archive vgmstream-7dc938fa2f210943b37c7b6511852b516ef432ab.zip SHA256 7618962679274a1d3b7da886d82365628ad15af55be8937db2f6df322643606f. Full compiled source third_party/vgmstream and COPYING are included. ISC-style license plus per-file notices. External codec libraries and player plugins are disabled; only native BCSTM/BCWAV header dispatch is exposed by the application. Build-generated version_auto.h is included with the source tree.

The application CMake configuration enables UTF-8 compilation for libvgmstream and its upstream VGM_STDIO_UNICODE path support for streamfile_stdio.c on MSVC. Upstream source files are unchanged.

V1.1 modifications dated 2026-10-10: SPU_Mix in vio2sf SPU.cpp now skips the terminal one-shot fetch position before waveform capture or mixing; see docs/DS-STABILITY.md. Original upstream notices and pinned archives are retained.

V1.2 modifications (2026-10-10): viogsf GBA.h/Sound.cpp use a 16-kernel source-rate PCM reconstruction bank. LazyUSF usf.h/usf.c, audio.c and rsp_hle/{hle_internal.h,alist.c,alist_audio.c,alist_naudio.c} add read-only dry envelope-slot taps and buffer/DMA observers. Original archives and licenses remain unchanged; full modified sources are included.

## V1.3 Sega changes · 2026-10-10

VGM is enabled with the LGPL-2.1-or-later GENS YM2612 implementation, copyright Stéphane Dallongeville and Shay Green. The alternative GPL MAME and Nuked implementations are not compiled. YM2413 uses the bundled MIT emu2413 by Mitsutaka Okazaki; licenses/emu2413-MIT.txt preserves its notice. Local LGPL interface modifications replace the empty YM2413 stub and separate SMS FM/rhythm/PSG and Genesis FM/DAC/PSG voices. Complete modified Vgm_Emu.cpp, Ym2413_Emu.cpp and Ym2413_Emu.h are in third_party/gme-sega-patch; third_party/apply_gme_sega.cmake verifies original/patched hashes. Vgm_Emu.cpp retains original notices. The original libgme archive is unchanged. The application validates VGM command/PCM bounds and decompresses VGZ using zlib. Distribute the matching source ZIP alongside the runtime ZIP; see SOURCE.md.
