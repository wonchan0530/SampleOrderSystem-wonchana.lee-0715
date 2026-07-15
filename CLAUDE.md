# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 저장소 현재 상태

이 저장소는 아직 코드가 작성되지 않은 초기 상태다. `docs/[CRA_AI] Day3_개인과제_반도체시료관리.pdf` (4~26페이지)가 요구사항 명세(PRD) 역할을 하며, 이를 정리한 `PRD.md`가 루트에 있다. 소스 코드는 아직 없고, `SampleOrderSystem/` 아래 Visual Studio 솔루션/프로젝트 스캐폴드만 존재한다.

## 빌드 환경 (MSBuild / Visual Studio)

- **빌드 시스템은 MSBuild/Visual Studio만 사용한다. CMake 등 크로스플랫폼 빌드 시스템은 도입하지 않는다.**
- 솔루션: `SampleOrderSystem/SampleOrderSystem.slnx`, 프로젝트: `SampleOrderSystem/SampleOrderSystem/SampleOrderSystem.vcxproj`
- 언어/표준: C++20 (`LanguageStandard=stdcpp20`), 콘솔 애플리케이션(`SubSystem=Console`)
- `PlatformToolset=v145` — 이 툴셋은 **Visual Studio 2026 (버전 18, `C:\Program Files\Microsoft Visual Studio\18\Community`)** 에서만 제공된다. 이 머신에는 VS 2022(17.13, 툴셋 v143)도 설치되어 있지만 v145가 없어 이 프로젝트를 열 수 없으므로, 빌드/실행은 반드시 **VS 2026(18) 인스턴스**를 사용할 것.
- 구성/플랫폼 조합: `Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64`
- CLI 빌드 명령 (PowerShell):
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

### 생산라인
- 공장에서 시료 하나를 생산하는 설비 흐름. 하나의 생산라인은 시료를 하나씩 생산하며, 주문이 들어온 시료에 대해서만 생산한다.
- 실생산량 = `ceil(부족분 / 수율)`
- 총생산시간 = `평균생산시간 * 실생산량`
- 생산 대기열(생산 큐)은 **FIFO**로 스케줄링한다.

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
