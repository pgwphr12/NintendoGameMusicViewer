# Supported target systems

See README.md / README.en.md for the tested per-system music formats, scope semantics and compatibility limits.

New decoder sources: src/formats/PsfBackend.cpp (bounded PSF chain, GBA/DS/N64) and src/formats/StreamBackend.cpp (3DS streams). Dispatch: src/formats/Registry.cpp. DS tap modifications: third_party/vio2sf/src/vio2sf/desmume/SPU.cpp, SPU.h and state.h.

Original fixture drivers: tests/make_psf_fixtures.py. Playback and malformed-container checks: tests/Tests.cpp. Existing first V1 ZIP is preserved; the current stability/folder-playback build is 1.2.0.

V1.2: src/formats/UsfVoiceTap.hpp traces Audio/NAudio dry slots through DMEM, interleave, saved RDRAM ranges and AI DMA. Other N64 paths fall back to stereo. BCSTM routing honors track channel groups. GBA Sound.cpp replaces coarse filter buckets with a source-rate bank. OutputResampler uses windowed-sinc kernels. See README V1.2 for source-bandwidth and stream limitations.
