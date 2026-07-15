# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 저장소 현재 상태

`docs/[CRA_AI] Day3_개인과제_반도체시료관리.pdf` (4~26페이지)가 요구사항 명세(PRD) 역할을 하며, 이를 정리한 `PRD.md`가 루트에 있다. 개발은 `PLAN.md`에 정의된 Phase 단위로 진행 중이며, 각 Phase의 설계 문서는 `design/phaseN-*.md`에 있다. Phase 0(프로젝트 기반 준비: 폴더 구조, JSON vendoring, 테스트 프로젝트, Harness 스크립트)까지 완료된 상태다.

## 빌드 환경 (MSBuild / Visual Studio)

- **빌드 시스템은 MSBuild/Visual Studio만 사용한다. CMake 등 크로스플랫폼 빌드 시스템은 도입하지 않는다.**
- 솔루션: `SampleOrderSystem/SampleOrderSystem.slnx`, 프로젝트 2개:
  - `SampleOrderSystem/SampleOrderSystem/SampleOrderSystem.vcxproj` — 앱 본체(`src/main.cpp`가 Composition Root)
  - `SampleOrderSystem/SampleOrderSystemTests/SampleOrderSystemTests.vcxproj` — 테스트 전용(자체 `tests/testing.h` 하네스, 외부 프레임워크 없음)
- 서드파티: `SampleOrderSystem/SampleOrderSystem/third_party/nlohmann/json.hpp` (단일 헤더 vendoring, 네트워크 의존 없음 — Data_Persistence POC와 동일 버전 3.11.3)
- 언어/표준: C++20 (`LanguageStandard=stdcpp20`), 콘솔 애플리케이션(`SubSystem=Console`)
- `PlatformToolset=v145` — 이 툴셋은 **Visual Studio 2026 (버전 18, `C:\Program Files\Microsoft Visual Studio\18\Community`)** 에서만 제공된다. 이 머신에는 VS 2022(17.13, 툴셋 v143)도 설치되어 있지만 v145가 없어 이 프로젝트를 열 수 없으므로, 빌드/실행은 반드시 **VS 2026(18) 인스턴스**를 사용할 것.
- 구성/플랫폼 조합: `Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64` (두 프로젝트 동일)
- **Harness**: `scripts/verify.ps1` — 4개 구성 빌드 + `SampleOrderSystemTests.exe`(Debug|x64) 실행을 한 번에 검증. `powershell -ExecutionPolicy Bypass -File scripts\verify.ps1`로 실행. 매 Phase의 "4. Harness 실행" 단계에서 사용.
- CLI 빌드 명령 (PowerShell, 단일 구성만 빌드할 때):
  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
    "SampleOrderSystem\SampleOrderSystem.slnx" /p:Configuration=Debug /p:Platform=x64
  ```
- VS 설치 정보 확인: `vswhere.exe` 는 `C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe` 에 있음
- `.vs/`, `x64/`, `x86/`, `Debug/`, `Release/`, `*.user` 등 IDE/빌드 산출물은 `.gitignore`로 제외되어 있으며 커밋 대상이 아니다.

## 프로젝트 개요

가상의 반도체 회사 "S-Semi"를 위한 **반도체 시료(Sample) 생산주문관리 시스템**을 개발한다. 현재 엑셀/메모장으로 주문을 관리하면서 발생하는 문제(주문 처리 여부 불명확, 완성 시점 파악 불가, 불필요한 추가 공정 등)를 해결하는 것이 목표다. 시스템은 **콘솔 기반**으로 동작하며, 담당자가 직접 명령을 입력해 시료를 등록하고 주문을 처리한다.

## 도메인 모델

### 시료 (Sample)
시스템의 가장 기본이 되는 단위. 시스템에 등록된 시료만 주문 가능하다.
- 속성: 시료 ID, 이름, 평균 생산시간, 수율
- 수율 = 정상적인 시료 수 / 총 생산 시료 수 (ex. 100개 생산 중 정상품 90개 → 수율 0.9)

### 주문 (Order)
모든 주문은 다음 5가지 상태 중 하나를 가지며, 아래 순서로만 전이된다. `REJECTED`는 정상 흐름 밖의 상태이며 모니터링 대상에서 제외된다.

| 상태 | 의미 |
|---|---|
| RESERVED | 주문 접수 (초기 상태) |
| REJECTED | 주문 거절 (정상 흐름 외, 모니터링 제외) |
| PRODUCING | 주문 승인 완료 + 재고 부족으로 생산 중 |
| CONFIRMED | 주문 승인 완료 + 출고 대기 중 |
| RELEASE | 출고 완료 |

상태 전이 규칙:
- `RESERVED` → 승인 시 재고 충분하면 즉시 `CONFIRMED`, 재고 부족하면 생산라인에 자동 등록되며 `PRODUCING`
- `RESERVED` → 거절 시 즉시 `REJECTED`
- `PRODUCING` → 생산 완료 시 `CONFIRMED`
- `CONFIRMED` → 출고 실행 시 `RELEASE`

**재고 확인/차감은 주문 승인 시점에만 1회 발생한다.** 재고가 충분하면 주문 수량 전체를 즉시 차감. 부족하면 남은 재고를 0까지 전부 이 주문에 할당(차감)하고 부족분만 생산 큐에 등록 — 이 즉시 차감 덕분에 동일 시료의 후속 주문은 이미 줄어든 재고를 보게 되어 앞 주문 몫과 겹치지 않는다. 생산이 완료된 후에는 재고 확인 로직이 재실행되지 않고 결과만 반영된다.

### 생산라인
- 공장에서 시료 하나를 생산하는 설비 흐름. 하나의 생산라인은 시료를 하나씩 생산하며, 주문이 들어온 시료에 대해서만 생산한다.
- 실생산량 = `ceil(부족분 / 수율)` — 수율은 필요한 생산량을 계산하는 데만 쓰이고, **실제 생산된 실생산량은 전량 정상품(사용 가능)으로 취급**한다. 생산 완료 시 부족분만큼은 해당 주문에 배정하고, 나머지(실생산량 - 부족분)는 재고에 가산한다.
- 총생산시간 = `평균생산시간 * 실생산량`
- 생산 대기열(생산 큐)은 **FIFO**로 스케줄링한다.
- **생산 완료 판정은 실제 시스템 시각(벽시계) 기준**: `현재 시각 ≥ 시작 시각 + 총생산시간`이면 완료. 메뉴 진입/조회 시점 또는 프로그램 재시작 시 지연 평가하며, 큐 맨 앞부터 순차적으로 여러 건이 한 번에 완료 처리될 수 있다(체인 완료). 이 때문에 **생산 큐 각 작업(주문ID/시료ID/부족분/실생산량/총생산시간/시작 시각)도 영속화 대상**이다 — 프로그램이 꺼져있던 동안 흐른 시간도 그대로 인정되어야 하기 때문. 상세: `docs/06-생산라인.md`, `PRD.md` §6.

## 주요 기능 (콘솔 메뉴 기준)

1. **시료관리**: 시료 등록(시료ID/이름/평균생산시간/수율 입력), 목록 조회(현재 재고 수량 포함), 이름 등 속성 검색
2. **주문(접수/승인/거절)**: 고객 주문 접수(시료ID/고객명/주문수량 → RESERVED 생성), 접수된 주문 목록 확인, 개별 주문 승인/거절 처리
3. **모니터링**: 상태별(RESERVED/CONFIRMED/PRODUCING/RELEASE) 주문 수 확인(REJECTED 제외), 시료별 재고 현황 확인 — 주문 대비 재고 수준을 여유/부족/고갈(0)로 표기
4. **출고처리**: CONFIRMED 상태 주문에 대해 출고 실행 → RELEASE로 전환
5. **생산라인**: 현재 생산 중인 시료 정보 표시, 대기 중인 생산 큐(FIFO) 목록 확인

## 개발 미션 요구사항

### 미션 1: PoC (Proof of Concept)
본 구현 전에 핵심 기술 요소를 검증하는 별도 Repository로 제출한다.
- **MVC 스켈레톤**: Model / Controller / View 패키지 구조와 역할 분리
- **데이터 영속성**: 팀(개인)별로 선택한 방식(파일, JSON, DB 등)으로 CRUD를 포함한 저장/조회 구조 구현 — 애플리케이션 재실행 후에도 데이터가 유지되어야 함
- **데이터 모니터링 Tool**: 현재 저장된 데이터 상태를 콘솔에서 실시간 조회 가능한 관리자 도구
- **Dummy 데이터 생성 Tool**: 테스트용 Dummy Data를 생성해 연결된 DB(저장소)에 추가하는 도구

### 미션 2: 본 프로젝트 개발
Agentic Engineering을 도입하여 기능 명세를 충족하는 고품질 코드를 개발한다. 주안점:
1. `CLAUDE.md`, `PRD.md` 등 문서 관리
2. Harness(자동화된 개발/검증 체계) 도입
3. Test
4. Clean Code
5. Commit 이력 관리

이 저장소에서 작업할 때는 위 5가지 주안점을 의식하고, 특히 상태 전이 규칙과 재고/생산량 계산식(수율, 실생산량, FIFO 큐)이 정확히 구현·테스트되는지 검증할 것.

## 개발 프로세스 (Phase별 반복)

`PLAN.md`에 정의된 각 Phase는 Design → 사용자 리뷰/승인 → Implement → Harness 실행 → 코드 검토 → Commit 순으로 진행한다. 설계 문서는 `design/phaseN-*.md`에 작성하고, 승인 전에는 구현에 착수하지 않는다. Harness(`scripts/verify.ps1`)는 매 Phase마다 빌드+테스트를 자동 실행하는 필수 단계다. 상세 내용은 `PLAN.md`의 "개발 프로세스" 절 참고.

## 커밋 메시지 규칙

커밋 제목은 아래 키워드로 시작한다.

| 키워드 | 용도 |
|---|---|
| `[test]` | 테스트 코드 반영 |
| `[fix]` | 버그 수정 |
| `[feat]` | 기능 구현 |
| `[refac]` | 코드 리팩토링 |
| `[etc]` | 위에 해당하지 않는 기타 변경 — `etc` 자리를 실제 내용을 나타내는 단어로 대체 (예: `[docs]`, `[chore]`, `[build]` 등) |

예: `[feat] 시료 등록 기능 구현`, `[fix] 주문 승인 시 재고 차감 오류 수정`, `[docs] PRD.md 재고 정책 보완`.
