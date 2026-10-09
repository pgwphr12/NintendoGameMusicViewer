# DS stability regression · V1.1 · 2026-10-10

The user reported abrupt exits when playing New Super Mario Bros. (EMU) mini2SF music. Windows logged access violations and heap corruption. The local diagnostic runner reproduced the crash on 01 Title Screen.

AddressSanitizer located a buffer write in SPU_Mix. Fetch8BitData/Fetch16BitData/FetchADPCMData can terminate a one-shot channel by setting SPU->bufpos to SPU->buflength. The original caller still invokes SPU_Mix afterward. The viewer's per-frame capture vector contains only buflength frames, so writing at that position overflowed the active vector and could corrupt the heap. SPU_Mix now returns before mixing/capturing that nonexistent sample. All other channel samples retain their timestamps.

Modified source: third_party/vio2sf/src/vio2sf/desmume/SPU.cpp. Original pinned archive is unchanged. The generated oneshot.2sf fixture exercises a finite PCM sample alongside ongoing PSG channels; tests verify its initial signal, termination and continued mix. NGMV_ASAN CMake option instruments the application and DS decoder; it is OFF for distribution. The matching MSVC AddressSanitizer runtime is required for diagnostic builds.

All 98 user mini2SF files passed actual decode for the first six seconds, replay, 500 ms seek and all-channel mute under AddressSanitizer. This does not cover all positions of every song. User music/library files are not redistributed. Windows UI also guards transition drawing and channel-vector bounds; C++ exceptions are handled inside the window callback.

Verification: instrumented CTest 5/5 passed; distribution Release CTest 5/5 passed, including actual DS folder auto-advance, keyboard/button previous/next wrapping, page 2 and recording-view repaint. The actual Windows audio-device test passed. A native recording-view screenshot of the supplied Title Screen track was inspected. Archive integrity and copied portable executable checks are performed during packaging.
