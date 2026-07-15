# PLAN.md — 반도체 시료 생산주문관리 시스템 개발 계획

> 참고 문서: [`PRD.md`](PRD.md), [`CLAUDE.md`](CLAUDE.md), `docs/01~06-*.md`(메뉴별 요구사항), `poc/01~04-*.md`(POC 학습노트)

## 개발 원칙

- **빌드는 MSBuild/Visual Studio(vcxproj)만 사용한다.** 참고 POC들은 CMake/Ninja 기반이므로, 각 Phase에서 "설계/클래스 구조는 재사용하되 빌드 설정은 vcxproj에 새로 반영"한다.
- 각 Phase는 이전 Phase의 산출물 위에서 컴파일 가능한 상태를 유지한다(Phase 종료 시 항상 빌드 성공 + 해당 Phase 범위의 수동/자동 검증 통과).
- 재고/주문 상태 전이 같은 핵심 도메인 규칙은 문서(PRD.md §4, §6)를 단일 진실 소스로 삼고, 구현이 문서와 달라지면 문서를 먼저 갱신한다.
- Phase마다 커밋을 남겨 "미션 2"의 주안점(CLAUDE.md/PRD.md 문서 관리, Harness, Test, Clean Code, Commit 이력)을 만족시킨다.

## 개발 프로세스 (Phase마다 반복)

각 Phase는 아래 6단계를 반복하며 진행한다. 4번(Harness 실행)이 "미션 2"의 Harness 요건을 실질적으로 충족시키는 단계다 — 코드 리뷰(사람이 읽고 판단)와 별개로, 빌드/테스트를 자동·반복 가능하게 검증하는 절차를 명시적으로 둔다.

1. **Design.md 작성** — 해당 Phase 구현 전 설계 문서를 `design/phaseN-*.md`에 작성 (설계 폴더는 Phase별 파일로 관리)
2. **사용자 리뷰 및 승인** — 설계 문서를 검토받고 승인된 뒤에만 구현을 시작한다
3. **Implement** — 승인된 설계대로 구현
4. **Harness 실행** — `scripts/verify.ps1`(가칭)로 MSBuild 4개 구성(Debug/Release × x86/x64) 빌드 + 테스트 프로젝트/시나리오 테스트를 자동 실행. 실패 시 3번으로 돌아가 수정 후 재실행
5. **코드 검토 및 수정** — Harness 통과를 전제로 Clean Code 관점에서 리뷰(중복/가독성/예외처리 일관성)
6. **Commit** — Phase 단위로 커밋을 남겨 이력 관리

진행 상황은 Phase/단계별로 Task 목록(TaskCreate/TaskUpdate)에 등록해 실시간으로 옆 패널에서 확인할 수 있도록 한다.

## 활용할 POC 요약

| POC | 재사용할 핵심 설계 | 학습노트 |
|---|---|---|
| MVC | Model/View/Controller 물리적 분리, `Repository<T>` 템플릿, View를 템플릿 파라미터로 교체 | [`poc/03-MVC.md`](poc/03-MVC.md) |
| Data_Persistence | model/storage/repository/console 4계층, `RepositoryResult`, `*Update` optional-patch, 손상 파일 방어, Repository 단방향 참조 무결성, EOF 안전 종료 | [`poc/02-Data_Persistence.md`](poc/02-Data_Persistence.md) |
| Data_Monitor | "모니터링 = 요청 시점 스냅샷"이라는 개념 정의, 실행파일 기준 데이터 경로 해석, 참조 무결성을 상위 계층(main)에서 처리 | [`poc/01-Data_Monitor.md`](poc/01-Data_Monitor.md) |
| Dummy_Data_Generator | Config 기반 스키마 검증, seed 재현성, 저장 후 재로드 검증 패턴 | [`poc/04-Dummy_Data_Generator.md`](poc/04-Dummy_Data_Generator.md) |

---

## Phase 0 — 프로젝트 기반 준비

**목표**: 빈 vcxproj를 실제 개발 가능한 상태로 만든다.

- 소스 트리 결정: `SampleOrderSystem/SampleOrderSystem/` 아래에 `Model/`, `Repository/`(또는 `Storage/`+`Repository/`), `Controller/`, `View/`, `Console/` 폴더 구성 (MVC POC의 `include/<Layer>/`, `src/<Layer>/` 구조를 vcxproj 필터로 재현)
- JSON 처리 방식 결정: Data_Monitor(자체 파서) vs Data_Persistence(nlohmann/json vendoring) 중 택1, 또는 자체 경량 파서를 신규 작성. **네트워크 의존(FetchContent 등) 없이 오프라인 빌드가 가능해야 하므로**, 라이브러리를 쓴다면 헤더 파일을 저장소에 직접 vendoring한다.
- 데이터 파일 저장 위치 정책 결정(cwd 기준 vs 실행파일 경로 기준 — Data_Monitor POC의 `resolveDataDir()` 패턴 참고 권장)
- `.vcxproj`/`.vcxproj.filters`에 위 폴더 구조 반영, 빈 `main.cpp`(Hello Console)로 x86/x64 Debug/Release 4개 구성 모두 빌드 확인
- 테스트 실행 방식 결정: 별도 테스트 프로젝트(vcxproj) 추가 여부, 또는 단일 실행파일에 `--test` 모드 등 — MVC/Data_Persistence POC처럼 앱과 테스트를 별도 타깃으로 분리하는 것을 권장하되, CMake의 `add_test`/CTest 대신 **vcxproj 두 개(App/Tests) + 배치 스크립트**로 대체

**완료 기준**: 폴더 구조/JSON 방식/데이터 경로 정책이 `CLAUDE.md`에 기록되고, 빈 프로젝트가 VS 2026(18)/MSBuild로 4개 구성 모두 정상 빌드된다.

---

## Phase 1 — MVC 스켈레톤 + 데이터 영속성 계층

**목표**: 시료/주문 두 엔티티에 대해 Model → Repository → Storage 계층과 최소 Controller/View 뼈대를 구축한다. (MVC POC + Data_Persistence POC 결합)

- **Model**: `Sample{id, name, avgProductionTimeMin, yieldRate, stock}`, `Order{orderId, sampleId, customerName, quantity, status}` (PRD.md §4 기준, status 기본값 RESERVED)
- **Storage**: 범용 `JsonFileStore<T>` 또는 동등한 파일 I/O 계층 — 파일 없음/빈 파일 → 빈 목록, 파싱 실패 → 경고 후 빈 목록(예외 비전파), 상위 디렉터리 자동 생성 (Data_Persistence POC 그대로 채택)
- **Repository**: `SampleRepository`(findAll/findById/search/create/update/remove), `OrderRepository`(동일 CRUD + `sampleId` 참조 무결성 검사를 위해 `SampleRepository`를 단방향 참조). 검증 실패/성공은 공통 `RepositoryResult{bool, message}`로 반환
- **Controller/View 뼈대**: MVC POC의 `Controller<ViewT>` 패턴 적용 여부 결정 — 엔티티가 2개뿐이고 메뉴 로직이 상태 전이 규칙과 얽혀 있어 무리하게 제네릭화하지 않고, 엔티티별 Controller 클래스(`SampleController`, `OrderController`)로 시작
- ID 자동 채번(기존 최대값+1) 정책 적용

**완료 기준**: Repository 단위 테스트(생성/조회/수정/삭제/참조무결성/손상파일/파일없음)가 모두 통과하고, 콘솔에는 아직 메뉴가 없어도 무방(Phase 2부터 메뉴 연결).

---

## Phase 2 — 시료관리 메뉴 (`docs/02-시료관리.md`)

**목표**: 메인 메뉴 없이도 단독 실행 가능한 시료관리 콘솔 기능 완성.

- 시료 등록(ID 중복/수율 범위(0 초과 1 이하)/평균생산시간(0 초과) 검증), 조회(재고 수량 포함), 검색(이름 등) 구현
- `ConsoleIO` 계층(readLine/readInt/readDouble/readOptional*, EOF 감지 후 상위 메뉴 복귀) 구축 — Data_Persistence POC의 EOF 무한루프 버그를 처음부터 방지하도록 **모든 메뉴 루프에 동일 패턴 적용**
- 등록 시 초기 재고 0

**완료 기준**: `docs/02-시료관리.md`의 하위 기능/예외 처리 표 전항목 시나리오 테스트로 검증.

---

## Phase 3 — 주문 접수/승인/거절 메뉴 (`docs/03-주문관리.md`, `PRD.md` §6.1)

**목표**: 주문 상태 전이와 재고 차감 정책을 정확히 구현.

- 주문 접수(RESERVED 생성, 미등록 시료ID/수량<=0 거부)
- 접수된 주문(RESERVED) 목록 조회
- **주문 승인**: 재고 충분 시 전량 차감 후 CONFIRMED / 부족 시 재고를 0까지 전부 차감하고 부족분만 생산 큐 등록 후 PRODUCING — **재고 확인·차감은 승인 시점 1회만** 수행하는 것이 핵심 규칙(이후 Phase 4의 생산 완료 처리는 재고를 "재확인"하지 않음)
- 주문 거절(REJECTED 전이)
- 동일 시료 후속 주문이 앞 주문 몫과 겹치지 않는지 검증하는 시나리오 테스트 작성(예: 재고10, 주문A 15 → 부족5 생산큐, 주문B 8 → 부족8 생산큐)

**완료 기준**: `docs/03-주문관리.md` 상태 전이표/예외처리표 전항목 커버, PRD §6.1 예시 시나리오 통과.

---

## Phase 4 — 생산라인 (`docs/06-생산라인.md`, `PRD.md` §6.2~6.3)

**목표**: FIFO 생산 큐와 벽시계 기준 생산 완료 판정, 재시작 시 진행 상황 반영을 구현. 이번 프로젝트에서 **가장 설계 난이도가 높은 Phase**.

- 생산 큐 자료구조: 각 작업에 주문ID/시료ID/부족분/실생산량(`ceil(부족분/수율)`)/총생산시간(`평균생산시간*실생산량`)/시작시각(`startedAt`) 보관
- **생산 큐도 영속화 대상**(파일 저장/로드) — 프로그램 재시작 후에도 진행 중이던 생산 정보를 잃지 않아야 함(PRD §6.3, §7)
- 완료 판정 로직: `현재 시각 ≥ startedAt + 총생산시간`이면 완료. 메뉴 진입/조회 시점 및 프로그램 시작 시점에 지연 평가하며, 큐 맨 앞부터 체인으로 여러 건을 연속 완료 처리할 수 있어야 함(오래 종료된 동안 여러 작업이 밀렸을 경우 대비)
- 완료 처리: 부족분만큼 주문에 배정 후 `PRODUCING`→`CONFIRMED`, 나머지(실생산량-부족분)는 재고에 가산 — **실생산량은 수율과 무관하게 전량 사용 가능으로 취급**(불량 없음 가정)
- 생산 현황 표시(현재 생산 중 시료/진행률), 대기 큐 목록(FIFO 순서) 조회 기능

**완료 기준**: (a) 정상 흐름 시나리오, (b) 프로그램을 종료했다가 총생산시간 경과 후 재시작 시 즉시 완료 처리되는 시나리오, (c) 여러 큐 작업이 밀려 있다가 한 번에 체인 완료되는 시나리오까지 테스트로 검증.

---

## Phase 5 — 출고처리 (`docs/05-출고처리.md`)

**목표**: CONFIRMED 상태 주문의 출고 처리.

- CONFIRMED 목록 조회, 특정 주문 ID 지정 출고 실행 → RELEASE 전이
- CONFIRMED가 아닌 주문/존재하지 않는 주문ID/이미 RELEASE된 주문에 대한 재출고 시도 거부

**완료 기준**: `docs/05-출고처리.md` 예외처리표 전항목 커버.

---

## Phase 6 — 모니터링 + 메인 메뉴 통합 (`docs/04-모니터링.md`, `docs/01-메인메뉴.md`)

**목표**: 상태별 주문 집계, 재고 여유/부족/고갈 표기, 메인 메뉴에서 전체 요약 정보 제공.

- 상태별(RESERVED/CONFIRMED/PRODUCING/RELEASE) 주문 수 집계(REJECTED 제외)
- 시료별 재고 여유/부족/고갈 판정 로직 확정(현재 재고 vs 미출고 대기 주문 수량 합계 기준, `docs/04-모니터링.md` 제안값 채택)
- **모니터링은 "메뉴 진입 시점 스냅샷" 조회로 구현**(Data_Monitor POC의 정의를 그대로 채택 — 별도 폴링/파일워칭 불필요), 단 Phase 4의 벽시계 완료 판정은 메뉴 진입 시 먼저 수행한 뒤 스냅샷을 뜬다
- 메인 메뉴에서 하위 메뉴(시료관리/주문/모니터링/출고처리/생산라인) 라우팅 및 전체 요약 정보 표시로 전체 앱 조립

**완료 기준**: 앱을 처음부터 끝까지 콘솔에서 조작해 전체 워크플로우(시료 등록 → 주문 접수 → 승인(생산 필요) → 생산 완료 대기 → 출고 → 모니터링 확인)가 한 번에 시연 가능.

---

## Phase 7 — Dummy 데이터 생성 도구 (`poc/04-Dummy_Data_Generator.md`)

**목표**: 테스트/시연용 시료·주문 더미 데이터를 생성해 저장소(JSON 파일)에 추가하는 보조 도구.

- Dummy_Data_Generator POC의 Config 기반 스키마 검증 + seed 재현성 패턴 재사용
- **POC와의 핵심 차이**: 이 시스템은 주문이 반드시 등록된 시료 ID를 참조해야 하므로(참조 무결성), 생성 순서를 "시료 먼저 생성 → 그 시료 ID 목록 중에서 주문의 sampleId를 랜덤 선택"으로 새로 설계해야 함(POC 범위 밖 내용)
- 생성된 데이터는 Phase 1에서 만든 Repository/Storage 계층을 그대로 통해 저장(별도 포맷 재구현 금지)
- 별도 실행 모드(예: 메인 실행파일의 `--seed-data` 인자) 또는 별도 프로젝트로 구성할지는 Phase 0에서 결정한 프로젝트 구조에 맞춰 진행

**완료 기준**: 지정한 개수만큼 참조 무결성이 지켜진 시료/주문 더미 데이터가 생성되고, 앱이 이를 정상적으로 읽어들여 모든 메뉴에서 조회 가능함을 확인.

---

## Phase 8 — 통합 테스트 / 회귀 시나리오 정리

**목표**: 개별 Phase에서 작성한 테스트를 모아 전체 회귀 시나리오로 정리.

- 콘솔 입력 시퀀스를 heredoc/스크립트로 주입해 전체 플로우를 검증하는 시나리오 테스트 작성(Data_Monitor POC의 `scenario_test.sh` 패턴 참고)
- 상태 전이표(PRD §4.3), 재고/생산 계산식(PRD §6) 각각에 대응하는 케이스가 최소 1개 이상 존재하는지 커버리지 점검
- EOF/잘못된 입력/존재하지 않는 ID 등 예외 경로 회귀 테스트 포함

**완료 기준**: 전체 테스트 스위트 1회 실행으로 통과, 실패 시 어떤 요구사항(PRD 섹션)이 깨졌는지 바로 알 수 있도록 테스트 이름/메시지 정리.

---

## Phase 9 — Clean Code 리뷰 및 문서/커밋 마무리

**목표**: "미션 2"의 나머지 주안점(Clean Code, 문서 관리, Commit 이력)을 마무리.

- 중복 로직 추출(예: Data_Persistence POC의 `validateFields()`, `parseFull<T>` 통합 패턴처럼 실제 중복이 발견된 곳만), 불필요한 추상화 제거
- `CLAUDE.md`/`PRD.md`/`docs/*`가 최종 구현과 어긋나지 않는지 재검토 후 갱신
- 커밋 이력이 Phase 단위로 의미 있게 분리되어 있는지 확인(필요 시 이후 작업부터라도 작은 단위 커밋 유지)

**완료 기준**: 코드 리뷰 체크리스트(중복/가독성/예외처리 일관성) 통과, 문서와 코드 간 불일치 없음.
