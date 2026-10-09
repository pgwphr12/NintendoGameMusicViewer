# V1.1 렌더링 성능 · 2026-10-09

같은 Windows PC에서 Release x64로 측정. 8채널 SPC 원본 테스트 파일과 모든 8 voice가 켜진 자체 제작 scope-stress.spc 사용. 매 크기에서 10회 warmup 후 120 frames. 프레임별 실제 화면과 동일한 renderFrame을 출력 크기의 DIB에 그려 BitBlt까지 수행하고 GdiFlush 후 wall time을 기록. 음악은 별도의 worker/SDL callback으로 실제 재생한다. UI 창은 offscreen이고 입력은 자체 제작 테스트 음악이다.

| 버전/입력 | 크기 | 평균 ms | p95 ms | GDI objects 시작 → 끝 |
|---|---|---:|---:|---|
| V1 | 1920x1080 | 107.28 | 129.49 | 50 → 50 |
| V1 | 3840x2160 | 115.96 | 129.74 | 50 → 50 |
| V1.1 | 1920x1080 | 4.31 | 4.93 | 78 → 78 |
| V1.1 | 3840x2160 | 13.43 | 15.64 | 78 → 78 |
| V1.1 all 8 active | 1920x1080 | 3.98 | 4.44 | 78 → 78 |
| V1.1 all 8 active | 3840x2160 | 13.25 | 14.96 | 78 → 78 |

V1과 V1.1 channels.spc 비교는 8개의 lane 중 2개가 활성화된 동일 음악 파일이다. 추가 stress 입력은 8개 모두 활성화되어 있다. 최종 버전에는 UI 문구 제거와 출력 음량 보정도 포함한다. V1 수치는 렌더러에 동일 측정 코드만 추가한 수정 전 구현이다.

파형은 GDI 선 호출 대신 native-resolution 32-bit DIB에 min/max와 연결선을 직접 rasterize한다. 버퍼 재사용, 글꼴/펜/브러시 캐시도 적용. 화면을 낮은 해상도로 그렸다가 확대하거나 시각화/audio rate를 낮추지 않는다. CPU 렌더링이며 GPU renderer로 변경한 것은 아니다.

이는 renderer 처리 시간이며 모니터의 실제 표시 FPS, compositor/VSync 지연, 모든 음악 파일·장치에서의 성능을 보장하는 측정은 아니다. 60Hz 프레임 시간은 약 16.67ms이다. 복잡한 확장음은 독립 music core 수가 많아 별도의 CPU 부하를 만들 수 있다. resource 수가 warmup 뒤 120회 동안 유지됨을 검사했다. 전체 화면 진입/복원 뒤 채널 이름 클릭 mute/unmute와 파형 색상/보존을 별도로 검사했다.

재현: NGMV_RENDER_BENCHMARK=1 환경 변수 아래 --snapshot INPUT OUTPUT.bmp --size=3840x2160 실행. OUTPUT.bmp.render.csv에 평균/p95/자원 수를 쓴다. 일반 실행에서는 측정 루프를 실행하지 않는다. 원본 CSV를 이 폴더에 포함한다.
