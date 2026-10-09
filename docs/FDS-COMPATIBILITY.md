# FDS 호환 수정 · V1.3 · 2026-10-10

두 파일 모두 FDS flag 0x04, load $6000, init $E000. 기존 libgme 0.6.5는 $8000보다 낮은 load를 Corrupt file (invalid load/init/play address)로 거부했다. 헤더를 바꾸는 대신 core를 수정했다.

- RAM $6000–$DFFF 읽기/쓰기/코드 실행.
- $5FF6–$5FFD bank register가 원본 4KiB를 RAM으로 복사. $5FFE/$5FFF는 기존 ROM mapping.
- 초기 $6000/$7000 bank는 헤더의 $E000/$F000 값. non-banked FDS는 실제 load 주소로 RAM에 복사.
- 오디오/다른 칩 register 우선순위 유지.
- 원본 ZIP과 완전한 LGPL 수정 파일을 모두 포함. 원본 SHA 검사 후 CMake PATCH_COMMAND로 적용.

자체 fds-banked.nsf/nsfe fixture가 RAM 쓰기/실행, ROM과 RAM의 독립성, low-bank/9000 bank 복사를 실제 driver 명령으로 확인한다. 실패하면 sound init 전 반환해 PCM 테스트가 실패한다. 기존 NSF/NSFE/FDS/GBS/SPC 검사도 유지한다.

36 + 45 = 81 tracks 각각 첫 6초를 full mix 및 실제 6개 isolated channel core로 렌더링했다. 모두 유한한 nonzero mix PCM을 생성했고 예외가 없었다. seek 500ms/replay/all-channel mute도 통과했다. 전체 곡의 모든 구간이나 주관적 청취 품질을 보장하는 검사는 아니다. 사용자 음악과 생성 PCM은 배포하지 않는다.

- fds-user-1.nsf = Yume Koujou - Doki Doki Panic FDS; SHA256 be3f2c8fa870185a195b62e9051d530cd41067a87b795b7ddd92c23012bcb4c6
- fds-user-2.nsf = Zelda II - The Adventure of Link FDS; SHA256 2408d71412ac9781bf467513183bfb9da833e0c79bb44f5aa77caa4694dee6a1

수치는 효과 전 PCM 에너지다. 설계 참고: https://github.com/libgme/game-music-emu/pull/167 및 https://www.nesdev.org/wiki/NSF .
