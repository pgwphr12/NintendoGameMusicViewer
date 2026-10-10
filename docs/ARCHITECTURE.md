# Nintendo Game Music Viewer V1 설계

새 C++17 Windows 음악 플레이어. Mesen/NTSC 프로그램 코드를 사용하지 않는다.

## 경계

`formats`는 NSF/NSFE/SPC/GBS/GSF/2SF/USF/BCSTM/BCWAV를 파일 헤더로 식별한다. PSF 라이브러리는 파일 경로를 전달해 상대 참조와 압축·메모리 범위를 검사한다. GBA/DS/N64는 PsfBackend, 3DS 스트림은 StreamBackend가 담당한다. N64와 3DS의 파형은 최종 출력/저장 스트림 채널이며 악기별 하드웨어 채널이 아니다. `IMusicBackend`는 트랙 메타데이터, 실제 스테레오 믹스, 실제 채널별 분리 신호를 생성한다. libgme 0.6.5를 독립적인 음악 코어로 사용한다. 게임 ROM, 게임 화면, PPU 및 범용 게임 실행 기능은 제공하지 않는다. 음악 포맷 코어 자체의 음악 명령 실행은 필요하다.

## 실제 채널 분리

동일 파일·트랙·48kHz 설정으로 전체 믹스 코어 하나와 채널별 코어를 생성한다. 채널별 코어에서는 libgme의 voice mute API로 다른 모든 하드웨어 채널을 음소거한다. 코어들은 같은 수의 샘플을 생성하며 자동 초기 무음 건너뛰기를 비활성화한다. 다른 채널이 만든 믹스를 복제하거나 FFT로 추정하지 않는다. 분리 신호는 코어의 필터를 지난 채널 신호이며 생 DAC/레지스터 값은 아니다. 기본 전체 볼륨에서는 원래 믹스를 사용한다. 개별 볼륨 변경 시 분리 신호를 합산하므로 NES 비선형 믹서/공유 효과의 상호작용까지 동일하지 않을 수 있다. 이 제한을 README에 공개한다.

## 시간축과 스레드

재생 워커만 음악 코어를 접근한다. 내부 생성 레이트는 항상 48,000 stereo frames/s이다. 워커는 bounded SPSC audio ring에 믹스를 쓰고 각 채널의 별도 stamped history ring에 시각화 데이터를 기록한다. 출력 콜백은 코어/UI/파일 접근을 하지 않으며 미리 확보한 메모리에서 장치 레이트로 보간한다. 화면은 60Hz 타이머로 실제 소비한 내부 샘플 시각의 history를 읽는다. UI가 느려져도 음악 생성과 콜백은 독립적으로 진행한다. 버퍼 부족 시 시간을 진행시키지 않고 underrun 횟수를 기록하며, 파형 history를 지우지 않는다.

pause는 장치를 일시정지한다. stop/track/seek는 워커를 정지하고 장치를 잠근 뒤 큐를 초기화한다. 장치 출력 레이트 변경은 기존 장치가 열린 상태에서 대체 장치를 준비한 뒤 교체하며 코어와 history를 다시 만들지 않는다. 장치 재개 전에는 오디오를 prefill한다. 성공한 변경도 오디오 장치 전환에 짧은 하드웨어 전환 지연이 있을 수 있다.

## 화면

Win32 네이티브 화면. 배경/텍스트는 GDI, 파형은 실제 화면 크기의 persistent 32-bit DIB에 직접 rasterize한다. GDI 작업을 flush한 뒤 같은 메모리에 파형을 쓰고 BitBlt로 표시한다. 버퍼는 크기 변경 때만 교체하고 글꼴/펜/브러시는 UI thread에서 재사용한다. GPU renderer는 아니다. 기준 1920×1080 좌표를 현재 client rectangle에 16:9 letterbox로 맞춘다. 기본 client 1280×720. 각 채널은 항상 동일한 가로 lane에 남는다. 안정된 시간 구간의 min/max envelope와 rising trigger로 고주파 aliasing을 줄인다. F9 recording view는 편집 controls를 숨긴다. 외부 캡처용이며 내장 영상 인코더는 없다.

## 빌드와 배포

CMake + MSVC, SDL2 2.30.11(audio), libgme 0.6.5(format/voice). SHA256로 고정한 upstream source ZIP을 third_party에 포함하므로 GitHub Actions는 의존성 버전 변화나 외부 다운로드에 의존하지 않는다. LGPL libgme는 교체 가능한 별도 DLL로 배포한다. 앱 라이선스는 GPL-2.0-or-later이며 실행 ZIP과 전체 소스 ZIP을 별도로 제공한다. CTest는 자체 제작 음악 fixtures로 실제 포맷/채널/리샘플링을 검사한다. Windows GitHub Actions가 빌드·테스트·ZIP artifact를 생성한다.

출력 보정: FDS +4dB / SPC +10dB, 나머지 unity. history 기록 뒤 mix만 고정 gain과 stereo-linked limiter에 통과시킨다. 내부 48kHz, instant attack, 150ms release, ceiling 0.98. seek/track/stop/file 변경 때 limiter를 reset한다. master는 device callback에서 적용한다.

V1.3: 내부 48k mix에 optional damped-comb reverb와 final limiter를 적용한다. callback은 atomic speed(0.5–2.0)와 output rate(16–96k)로 소비 clock을 진행한다. history는 원음 48k, voice gain은 0–2. FDS RAM과 bank-copy는 local LGPL patch로 구현한다.
