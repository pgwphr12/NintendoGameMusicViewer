# V1 지원 확장판 (1.0.1) 검증 기록 · 2026-10-10

- Windows x64, VS2022/MSVC 19.44, CMake에서 Release 빌드 성공.
- CTest 4/4 통과: 자체 음악 fixtures 생성, 실제 core/audio/channel 테스트, native UI layout 및 fullscreen/mute click 테스트.
- NSF/NSFE: 5개의 실제 voice, 두 트랙 전환, 채널 PCM 독립성, 무음 NOISE, 실제 mute 확인.
- FDS NSF: 6 voice, FDS WAVE의 실제 PCM 에너지와 독립 파형 확인.
- GBS: 4 voice, 실제 PCM 생성과 서로 다른 채널 확인.
- SPC: 8 voice, 실제 PCM 생성과 서로 다른 두 voice 확인.
- GBA/DS: 자체 ARM 드라이버의 GSF/miniGSF 및 2SF/mini2SF 실제 분리 채널 PCM, 메타데이터, 음소거, 탐색, 끝 검사.
- N64/3DS: 자체 MIPS 드라이버 USF/miniUSF 및 PCM/DSP-ADPCM BCSTM/BCWAV 실제 독립 좌우 출력, 음소거, 탐색, 끝 검사.
- 새 포맷의 상용 음악 덤프는 아직 검증하지 못했습니다. 전체 소스를 포함한 GPL-2.0-or-later 배포입니다.
- 3DS 한글 파일명·경로의 PCM/DSP-ADPCM 열기 및 실제 오디오 출력 검사 통과.
- 비정상/빈/잘린 파일 거부. NSFE 곡 이름·길이 메타데이터 확인.
- 고정 48kHz 생성, 44.1/48/96kHz 출력 및 같은 소비 시각을 유지하는 레이트 전환 검사 통과.
- SPSC 100,000 frames 순서/동시성 검사, high-frequency min/max envelope 및 silent baseline 검사 통과.
- pause/resume/seek/stop/metadata 끝/replay, 개별 mute/volume 상태 검사 통과.
- 실제 WASAPI 장치 경로에서도 동일 player 테스트 통과. 테스트 구간 rate change 후 underrun=0. 청취 품질의 주관적 평가나 모든 장치의 호환성을 확인했다는 뜻은 아님.
- 실제 native GDI snapshot 1280×720, 1600×900, 1920×1080, 2560×1440, 3840×2160 및 recording view 생성·크기 검사 통과. FDS 및 SPC 화면을 직접 확인.
- exe와 gme.dll 의존성 검사: Windows 시스템 DLL 외 gme.dll만 필요. 외부 VC 런타임/.NET 없음.

GitHub Actions workflow는 제공하지만 실제 저장소에서 실행하지 않았습니다. 이 PC의 로컬 CMake/CTest만 실행했습니다. 제공된 FDS 두 파일의 81 tracks에서 첫 6초의 실제 mix/voice PCM과 seek/mute를 확인했습니다. 모든 구간을 확인한 것은 아닙니다. 제공된 Zelda MP4는 화면 참고 자료이며 소스 음악 파일이 아닙니다. 자체 제작 테스트 음악을 samples에 포함합니다.

길이가 없는 파일의 자동 끝 검출, 검색/라이브러리, 생 레지스터/주파수 표시, hot-device 재연결, 3SF/BCSAR/BCSEQ는 미구현입니다. 개별 volume 변경은 isolated PCM의 합산이므로 비선형 믹서 상호작용이 원래 믹스와 달라질 수 있습니다. 자세한 제한은 README에 기록했습니다.

정식 V1 (1.0.0): 기본 전체 음량 100%, 0.1~3.0초 잔향 시간 조절, 원음이 끝난 뒤 0.5~1초 구간의 잔향 유지 및 짧은/긴 잔향 비교 검사 통과.
V1.5: 반향 강도 0~100% 수치 조절, 반사음 진폭/에너지/범위 검사 통과. 녹화뷰 개별 음량 조작부 숨김과 전체 음량 유지를 native 화면으로 확인.
V1.4: 0.01/0.10 피치 단위, 반향 wet 0.45, F9 포함 전체/채널 ±5% 및 슬라이더, FDS/CHANNEL 이름 표시를 native UI로 확인.
V1.3: FDS banked NSF/NSFE 회귀 및 사용자 81 tracks, default 96kHz 및 16~96kHz 선택, voice gain 200%, unused/active DMC detection 및 숨김 후 FDS 클릭 mapping, varispeed pitch/clock, reverb bypass/tail/decay 검사 통과. docs/FDS-COMPATIBILITY.md 참조.
V1.2: 일반 UI 한국어 / 녹화 UI 영어 전환과 일반 화면에서만 단축키 표시를 native snapshot으로 확인.
V1.1: native fullscreen 복원 뒤 클릭 mute/unmute와 원본 파형 보존 검사 통과. 4K 120-frame GDI 자원 수 유지 검사 통과. FDS +4dB / SPC +10dB 출력 보정, silence/stereo 유지, limiter peak ceiling/release와 실제 PCM 에너지 증가 검사 통과. 렌더링 측정은 docs/PERFORMANCE.md 참조.
