# V1.3 (1.3.0) 검증 기록 · 2026-10-10

- Release CTest 5/5: 기존 음악 포맷·오디오, SMS PSG/YM2413 melody/rhythm, Genesis FM/DAC/PSG의 독립 파형·음소거·탐색, VGM/VGZ 폴더 순서 및 실제 자동재생.
- 화면: 9개 이상은 두 열, 31개까지 geometry 범위 검사. native 18채널 오른쪽 마지막 채널의 음소거·+버튼·슬라이더 드래그 및 주변 채널 유지, F9 음소거 검사. 사용자 N64 31항목을 처음부터 표시하는 1080p 화면 확인.
- 사용자 Sonic 파일 35개(메가 드라이브 19, SMS 16): 첫 6초와 탐색·재시작·전체 음소거 검사 통과. 음악 파일은 배포하지 않습니다. 모든 구간·모든 VGM 버전·칩의 호환을 보증하지 않습니다.
- ASAN 메모리 검사: CTest 5/5 및 확장 native UI/폴더 검사 3/3 통과. 앱·libgme/Sega·GBA·DS·N64 계측. 동일 Sonic 35개와 N64/Sega native 화면 검사 통과. ASAN 런타임은 정식 빌드에 포함하지 않습니다.
- 새 빈 빌드 폴더에서 원본 의존성 추출·FDS/Sega 수정 파일 적용 및 CMake 구성 확인. GitHub Actions는 실제 GitHub 서버에서 실행하지 않았습니다.
- 실제 Windows WASAPI 오디오 장치에서 기존 기능 및 Genesis 워커/96kHz 출력 재생 검사 통과. FileVersion/ProductVersion은 1.3.0입니다.
- 복사한 portable 실행 파일의 한글 파일 경로에서 기존/신규 포맷 화면 확인. 실행 ZIP에는 소스가 없으며 전체 소스는 별도 source ZIP에 포함됩니다. 두 ZIP의 CRC·SHA256 확인.

VGM은 실제 대기 명령에서 첫 재생 구간 길이를 구하며, 파일 자체의 반복점으로 돌아가지 않습니다. 앱의 Repeat는 파일 처음부터 반복하고 자동재생은 다음 파일로 이동합니다. streaming DAC 0x90~0x95·다른 칩·이중 칩은 명확한 오류로 거부합니다. GENS FM은 실제 칩과 음색 차이가 있을 수 있습니다. 지원 범위는 README 및 docs/V1.3-SEGA.md에 기록했습니다. 기존 V1.2 오디오 개선과 V1.1 DS one-shot 안정성 수정을 유지합니다.

---

# V1.2 (1.2.0) 검증 기록 · 2026-10-10

- Release CTest 5/5: 실제 코어 PCM, 샘플레이트·피치·음량·잔향·탐색·끝, native UI·4K·음소거·폴더·자동재생 검사.
- 새 회귀 검사: 10512/16384 Hz GBA PCM FIFO, 실제 N64 HLE envelope → interleave → save → DMA의 독립 신호·시간 정렬·잔여 데이터 제거, 3DS 5채널 track table의 stereo/mono 배치, sinc 다운샘플링 alias 억제.
- 사용자 음악 140개: Super Mario 64 38개 USF, New Super Mario Bros. 2 99개 BCSTM, Super Mario Advance 4 3개 GSF의 첫 6초 및 탐색·전체 음소거 통과. 게임 음악은 배포에 포함하지 않습니다.
- 메모리 검증: ASAN CTest 5/5, GBA/DS/N64 코어 및 앱 계측. 사용자 N64/GBA 표본의 동일 디코딩 검사 통과. 모든 재생 구간을 검증한 것은 아닙니다.
- 복사한 portable의 한글 파일 경로 및 GBA/DS/N64/3DS 입력 실행 검사. ZIP CRC·소스 포함·SHA256 검사.

GBA PCM 원본은 약 10.5kHz/8비트인 경우가 있으며, 96kHz 출력으로 대역폭을 복원하지 않습니다. N64는 Audio/NAudio 슬롯을 지원하고 다른 합성 방식은 stereo fallback입니다. 3DS에는 저장된 채널만 존재하며 이미 합쳐진 악기를 분리하지 않습니다. 48kHz 내부 시간축, 외부 영상 녹화, GPL 전체 소스 배포를 유지합니다. V1.1 DS one-shot 수정과 자동재생 기능을 포함합니다. 자세한 변경 및 제한은 README 및 docs/V1.2-AUDIO.md를 참조하세요.

정식 V1 (1.0.0): 기본 전체 음량 100%, 0.1~3.0초 잔향 시간 조절, 원음이 끝난 뒤 0.5~1초 구간의 잔향 유지 및 짧은/긴 잔향 비교 검사 통과.
V1.5: 반향 강도 0~100% 수치 조절, 반사음 진폭/에너지/범위 검사 통과. 녹화뷰 개별 음량 조작부 숨김과 전체 음량 유지를 native 화면으로 확인.
V1.4: 0.01/0.10 피치 단위, 반향 wet 0.45, F9 포함 전체/채널 ±5% 및 슬라이더, FDS/CHANNEL 이름 표시를 native UI로 확인.
V1.3: FDS banked NSF/NSFE 회귀 및 사용자 81 tracks, default 96kHz 및 16~96kHz 선택, voice gain 200%, unused/active DMC detection 및 숨김 후 FDS 클릭 mapping, varispeed pitch/clock, reverb bypass/tail/decay 검사 통과. docs/FDS-COMPATIBILITY.md 참조.
V1.2: 일반 UI 한국어 / 녹화 UI 영어 전환과 일반 화면에서만 단축키 표시를 native snapshot으로 확인.
V1.1: native fullscreen 복원 뒤 클릭 mute/unmute와 원본 파형 보존 검사 통과. 4K 120-frame GDI 자원 수 유지 검사 통과. FDS +4dB / SPC +10dB 출력 보정, silence/stereo 유지, limiter peak ceiling/release와 실제 PCM 에너지 증가 검사 통과. 렌더링 측정은 docs/PERFORMANCE.md 참조.
