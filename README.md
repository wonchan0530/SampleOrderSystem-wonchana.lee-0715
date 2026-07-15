# 반도체 시료 생산주문관리 시스템 (SampleOrderSystem)

## 개요

가상의 반도체 회사 **"S-Semi"**를 위한 콘솔 기반 반도체 시료(Sample) 생산주문관리 시스템이다. 시료 등록부터 주문 접수·승인·거절, 재고 부족 시 자동 생산, 출고, 현황 모니터링까지 하나의 콘솔 애플리케이션에서 처리한다.

## 목적

기존에 엑셀/메모장으로 주문을 관리하면서 발생하던 문제(주문 처리 여부 불명확, 완성 시점 파악 불가, 불필요한 추가 공정)를 해결하기 위해 개발되었다. 핵심 목표:

- 주문 상태(접수/거절/생산중/출고대기/출고완료)를 한눈에 파악
- 재고 부족 시 부족분만큼 자동으로 생산 계획을 세우고, 실제 시간 경과에 따라 생산 완료를 자동 반영
- 담당자가 재고·주문 현황을 실시간으로 모니터링

## 구조

```
SampleOrderSystem/
├── CLAUDE.md                # 코드베이스 아키텍처/빌드환경/개발 프로세스 안내 (AI 에이전트 및 개발자용)
├── PRD.md                    # 제품 요구사항 정의서(배경/도메인 모델/기능 요구사항)
├── PLAN.md                   # Phase 0~9 개발 계획 및 진행 현황
├── docs/                     # 메뉴별 상세 요구사항 (01-메인메뉴 ~ 06-생산라인)
├── poc/                      # 개발 착수 전 참고한 4개 POC 학습노트
├── design/                   # Phase별 설계 문서 (design/phaseN-design.md)
├── scripts/                  # Harness 스크립트 (verify.ps1, scenario_test.ps1)
└── SampleOrderSystem/         # 소스 루트 (Visual Studio 솔루션)
    ├── SampleOrderSystem.slnx
    ├── SampleOrderSystem/                      # 앱 본체
    │   ├── include/Model        Sample, Order, ProductionJob (+ JSON 직렬화)
    │   ├── include/Storage      JsonFileStore<T>(범용 파일 영속성), PathUtil(데이터 경로)
    │   ├── include/Repository   SampleRepository, OrderRepository, ProductionQueueRepository
    │   ├── include/Controller   OrderController(재고정책/승인/거절/출고), ProductionController(생산완료판정), MonitoringController(집계)
    │   ├── include/Console      ConsoleIO, MenuRunner, 5개 메뉴 클래스
    │   ├── third_party/nlohmann/json.hpp  (vendored, 네트워크 의존 없음)
    │   └── src/main.cpp          Composition Root
    ├── SampleOrderSystemTests/                 # 유닛/통합 테스트 (자체 경량 하네스, 외부 프레임워크 없음)
    └── SampleOrderSystemDummyDataGenerator/    # 더미 데이터 생성 CLI 도구
```

아키텍처에 대한 더 자세한 설명은 [`CLAUDE.md`](CLAUDE.md)를 참고한다.

## 빌드 방법

- **빌드 시스템은 MSBuild/Visual Studio만 사용한다.** CMake 등 크로스플랫폼 빌드 시스템은 사용하지 않는다.
- 요구 환경: **Visual Studio 2026 (버전 18)** — `PlatformToolset=v145`가 이 버전에서만 제공된다. C++20, `/utf-8` 컴파일 옵션 사용.

### Visual Studio에서 빌드

`SampleOrderSystem/SampleOrderSystem.slnx`를 Visual Studio 2026으로 열고 빌드(F7 또는 Ctrl+Shift+B)한다. `Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64` 4개 구성을 지원한다.

### 커맨드라인에서 빌드 (PowerShell)

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "SampleOrderSystem\SampleOrderSystem.slnx" /p:Configuration=Debug /p:Platform=x64
```

### Harness (빌드 + 테스트 + E2E 시나리오 자동 실행)

```powershell
powershell -ExecutionPolicy Bypass -File scripts\verify.ps1
```

4개 구성 빌드 → `SampleOrderSystemTests.exe` 유닛 테스트 → `scripts/scenario_test.ps1`(실제 exe를 콘솔 입력으로 조작하는 End-to-End 시나리오, 벽시계 기준 재시작 검증 포함)까지 한 번에 검증한다.

## 실행 방법

빌드 후 산출물은 솔루션 폴더 기준 `x64\Debug\`(또는 `Release\`) 아래에 생성된다.

### 메인 애플리케이션

```powershell
.\SampleOrderSystem\x64\Debug\SampleOrderSystem.exe
```

실행하면 메인 메뉴가 표시된다.

```
1. 시료관리
2. 주문(접수/승인/거절)
3. 모니터링
4. 출고처리
5. 생산라인
0. 종료
```

데이터는 실행 파일과 같은 폴더의 `data/`(`samples.json`, `orders.json`, `production_queue.json`)에 저장되며, 재실행해도 유지된다.

### 더미 데이터 생성 도구

테스트/시연용 시료·주문 데이터를 실제 Repository/Controller API를 통해 생성한다(참조 무결성이 항상 보장됨).

```powershell
.\SampleOrderSystem\x64\Debug\SampleOrderSystemDummyDataGenerator.exe [샘플개수=10] [주문개수=30] [seed] [dataDir]
```

인자를 생략하면 샘플 10개/주문 30개를 무작위(재현 불가) seed로 생성하고, 메인 애플리케이션과 동일한 `data/` 폴더에 저장한다.

## 문서

| 문서 | 내용 |
|---|---|
| [`CLAUDE.md`](CLAUDE.md) | 빌드 환경, 아키텍처, 도메인 모델, 개발 프로세스, 커밋 규칙 — AI 에이전트/개발자를 위한 코드베이스 안내 |
| [`PRD.md`](PRD.md) | 배경, 시스템 개요, 도메인 용어, 기능 요구사항, 비기능 요구사항 |
| [`PLAN.md`](PLAN.md) | Phase 0~9 개발 계획, 개발 프로세스(Design→리뷰→Implement→Harness→코드검토→Commit), 진행 현황 |
| `docs/01~06-*.md` | 메뉴별(메인메뉴/시료관리/주문관리/모니터링/출고처리/생산라인) 상세 요구사항·예외처리 |
| `poc/01~04-*.md` | 개발 착수 전 참고한 4개 POC(MVC, 데이터 영속성, 데이터 모니터링, 더미 데이터 생성) 학습노트 |
| `design/phase0~9-design.md` | Phase별 설계 문서(구현 전 승인받은 설계 + 구현 중 발견한 변경사항) |
