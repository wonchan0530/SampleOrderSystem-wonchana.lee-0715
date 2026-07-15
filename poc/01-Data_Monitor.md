# POC 학습노트 — Data_Monitor (데이터 모니터링 Tool)

> 원본 위치: `C:/review/Data_Monitor` (참고: `CLAUDE.md`, `PRD.md`, `Design.md`, `Feedback.md`, `PLAN.md`, `src/`, `tests/scenario_test.sh`)

## 1. 목적과 요구사항

콘솔 기반 관리자 도구 POC. 시료(Sample)와 주문(Order) 두 엔티티를 등록/조회/수정/삭제하고, "실시간 조회" 메뉴로 현재 상태를 확인한다.

- 시료 CRUD (수율은 저장하지 않고 조회 시점에 계산)
- 주문 CRUD (시료 ID를 참조)
- **참조 무결성**: 존재하지 않는 시료 ID로 주문 등록 불가, 주문이 참조 중인 시료는 삭제 불가
- 삭제 전 확인 절차, 잘못된 입력에도 프로그램이 죽지 않고 안내 메시지 출력
- 한글 인코딩 정상 처리, 재실행 시 데이터 영속
- 비기능: C++17 + MSVC(cl.exe), **CMake 미사용**, 외부 JSON 라이브러리 없이 자체 구현, 실행파일 위치 기준 `data/` 폴더에 저장
- **명시적 Out of Scope**: 다중 사용자/네트워크/GUI, **파일 변경 감지(워칭)나 폴링 기반 자동 갱신**, 시료/주문 외 엔티티

## 2. 아키텍처

단방향 계층: `main.cpp` → `SampleRepository`/`OrderRepository` → `JsonValue`(파일 I/O).

- **`Json.h/.cpp`**: 외부 라이브러리 없는 최소 JSON 파서/직렬화기. `JsonValue`는 Null/Bool/Int/Double/String/Array/Object 지원. Object는 `map` 대신 `vector<pair<string,JsonValue>>`로 구현해 **삽입 순서를 보존**. `operator[]`/`find()`로 접근, `dump()`/`parse()` 제공.
- **`JsonFile.h/.cpp`**: `loadJsonArrayFile`/`saveJsonArrayFile` — 파일 없음/빈 파일이면 빈 배열 반환, 파싱 실패 시 경고 로그 후 빈 배열로 대체, 저장 시 상위 디렉터리 자동 생성. 두 Repository의 공통 보일러플레이트를 제거하려고 분리한 리팩토링 산출물.
- **`Sample.h`**: `id, name, avgProductionTimeMinutes, normalCount, totalCount` + `yieldPercent()` (totalCount가 0이면 0 반환).
- **`Order.h`**: `id, sampleId, customerName, quantity`. sampleId는 Sample.id를 참조하지만 **검증은 여기서 하지 않음**(주석: "참조 무결성은 상위 계층에서 검증").
- **Repository들**: `load()/save()`는 `JsonFile` 헬퍼에 위임하고 필드 매핑만 담당. `create/findById/exists/update/remove` 제공. ID는 기존 최대값 + 1로 자동 부여. `create`/`update`가 공유하는 검증은 private static `validateFields()`로 통합. `OrderRepository`에는 `findBySampleId()` 추가.
- **`main.cpp`**: 콘솔 메뉴 엔트리포인트(최상위: 시료관리/주문관리/실시간조회 → 서브메뉴: 등록/전체조회/ID검색/수정/삭제). **참조 무결성 검증은 Repository가 아니라 main.cpp에서** 두 Repository를 함께 호출하며 처리 — Repository들은 서로를 모르는 상태를 유지한다. `readInt`/`readDouble`은 `stoi`/`stod`의 `pos` 출력 인자로 trailing garbage(`"12abc"` 같은 값)를 검출. 표준입력 EOF는 `EofException`으로 처리해 정상 종료.

## 3. "데이터 모니터링" 기능의 실제 구현 — 가장 중요한 시사점

메뉴 "3. 실시간 조회"를 선택하면 `handleLiveView()`가 그 시점에 메모리에 있는 `sampleRepo.getAll()`/`orderRepo.getAll()`을 그대로 출력한다.

> **폴링도 파일 워칭도 아니다.** 별도 스레드/타이머 없이 "메뉴 진입 시점의 스냅샷 출력"일 뿐이다. 데이터가 항상 최신인 이유는 각 CRUD 연산(create/update/remove)이 매번 `save()`를 호출하고, 메모리 상 벡터도 즉시 갱신되기 때문(단일 프로세스 내에서 메모리와 파일이 항상 동기화됨). PRD 자체가 "파일 변경 감지나 주기적 폴링 기반 자동 갱신은 Out of Scope"라고 명시하고 있다.

시료는 수율(계산값)까지 함께 출력, 주문은 시료ID/고객명/수량을 출력한다.

## 4. 데이터 저장/조회 방식

- Repository 패턴: 엔티티(Sample/Order)마다 독립된 Repository가 자신의 JSON 파일(`samples.json`, `orders.json`)을 전담. 생성자에서 파일 경로만 받고, `load()`는 main에서 명시적으로 호출.
- 관심사 3단 분리: `Json.cpp`(순수 파싱/직렬화) / `JsonFile.cpp`(파일 존재·빈 파일·파싱 실패 등 "JSON 배열 ↔ 파일" 공통 로직) / Repository(엔티티별 필드 매핑).
- 자동 증가 ID: `recalculateNextId()`가 로드 시 기존 최대 id + 1로 설정.
- 데이터 파일 위치는 cwd가 아니라 **실행파일 자신의 경로 기준**(`GetModuleFileNameW` 사용, `resolveDataDir()`) — 어디서 실행하든 동일 데이터를 본다.

## 5. 빌드 / 테스트

- **`build.bat`**: `vcvars64.bat`로 x64 개발자 환경 구성 → `build` 폴더 생성 → `cl /nologo /EHsc /std:c++17 /utf-8 /Fo:build\ /Fe:build\data_monitor.exe src\main.cpp src\SampleRepository.cpp src\OrderRepository.cpp src\Json.cpp src\JsonFile.cpp`. **`/utf-8` 플래그 필수** (빠지면 CP949로 오인식되어 컴파일 에러).
- **`tests/scenario_test.sh`**(Git Bash): `build/data` 정리 → exe에 heredoc으로 메뉴 입력 시퀀스 주입(시료 등록→조회→검색→수정 → 주문 등록(정상 + 존재하지 않는 시료ID 거부) → 조회→검색→수정 → 실시간 조회 → 참조 중인 시료 삭제 시도(거부) → 주문 삭제 → 시료 삭제(성공)) → `samples.json`/`orders.json` 내용을 `cat`으로 출력해 육안 확인 → 다시 정리. 별도 assertion 프레임워크 없이 로그+파일 내용을 사람이 확인하는 방식.

## 6. Feedback.md에 기록된 교훈

- **Phase 4**: `stoi`/`stod`는 `"5.5abc"` 같은 입력에서 뒤 문자를 무시하고 조용히 통과시키는 함정 발견 → `pos` 출력 인자로 trailing garbage 검사(`hasTrailingGarbage`) 추가.
- **Phase 5**: 회귀 테스트 1회 실행으로 전체 시나리오(참조 무결성 거부 포함) 통과.
- **Phase 6**: 문서(CLAUDE.md)에 적은 함수/파일명이 실제 코드와 일치하는지 grep으로 재확인하는 습관.
- **리팩토링**: 두 Repository의 `load()/save()` 중복을 `JsonFile`로 추출, `create/update` 검증 중복을 `validateFields()`로 통합 — 동작 변경 없이 중복만 제거하고 빌드+회귀테스트로 무변경 확인.
- 공통 패턴: "설계(Design.md) → 구현 → 검증(Feedback.md)"을 Phase마다 문서화, 각 Phase는 작고 검증 가능한 단위로 분할.

## 7. SampleOrderSystem에 적용할 인사이트

- **Repository 패턴 그대로 재사용 가능**: 엔티티별(시료/주문) 독립 Repository + JSON 영속 계층 + `validateFields()` 통합 검증 패턴은 이번 시스템의 뼈대로 바로 적용 가능.
- **참조 무결성은 Repository가 아닌 상위 계층(main/서비스)에서 처리**하는 설계 원칙 — 주문↔시료 등 참조 관계가 늘어날 때 Repository 간 결합을 막는 데 유용.
- **"실시간 모니터링"의 정의를 명확히 할 것**: 이 POC는 진짜 실시간(푸시/폴링)이 아니라 "요청 시점 스냅샷"이다. 우리 시스템의 [`04-모니터링.md`](../docs/04-모니터링.md) 기능도 동일하게 "메뉴 진입 시 스냅샷 출력" 수준으로 충분한지 구현 전에 명확히 할 것 — **이 POC의 가장 큰 시사점**.
- 자체 JSON 파서는 소규모 POC엔 적합하나, 동시 쓰기 충돌·이스케이프/유니코드 처리 등이 미흡하므로 실제 프로젝트에서는 채택 여부를 별도 판단.
- UTF-8/한글 처리, 실행파일 기준 경로 해석(cwd 독립성), 입력 검증(trailing garbage), EOF 안전 종료 등은 Windows 콘솔 C++ 앱에서 흔히 놓치는 디테일이므로 그대로 참고할 가치가 높다.
