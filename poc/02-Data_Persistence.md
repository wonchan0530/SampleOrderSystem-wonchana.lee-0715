# POC 학습노트 — Data_Persistence (데이터 영속성 처리)

> 원본 위치: `C:/review/Data_Persistence` (참고: `CLAUDE.md`, `DESIGN.md`, `CMakeLists.txt`, `include/`, `src/`, `tests/`)

## 1. 목적과 요구사항

`SampleOrderSystem` 착수 전, "데이터 영속성"(재시작해도 데이터 유지) 개념 하나만 검증하는 독립 POC. PRD는 FR-1~FR-14(시료/주문 CRUD, 공통 요구사항)와 NFR-1~3(I/O 오류 시 비정상종료 금지, 모듈 재사용성, 파일 없을 때 자동 초기화)을 정의한다. 시료 승인/생산/출고 워크플로우, 모니터링, 더미데이터 생성, 동시성/인증은 명시적으로 범위 밖.

## 2. 저장 방식

- JSON 파일 기반, `nlohmann/json`(header-only, vendored `third_party/nlohmann/json.hpp`) 사용.
- 시료(`data/samples.json`)와 주문(`data/orders.json`)을 **별도 파일로 분리**(FR-2).
- 각 파일은 해당 구조체 배열을 그대로 담은 JSON 배열(`arr.dump(2)`로 pretty-print).
  - Sample: `{"id","name","avgProductionTimeMin","yieldRate","stock"}`
  - Order: `{"orderId","sampleId","customerName","quantity","status"}` (status 기본값 `"RESERVED"`)

## 3. CRUD / Repository 계층 구조

계층: `model`(순수 구조체 + `to_json`/`from_json` ADL) → `storage/JsonFileStore<T>`(범용, 도메인을 모름) → `repository`(도메인 규칙) → `console`(뷰).

- `SampleRepository`: `findAll/findById/search(keyword: id∨name 부분일치)/create/update(SampleUpdate patch)/remove`.
- `OrderRepository`는 생성자로 `const SampleRepository&`를 **단방향 주입**받아 `create` 시 `sampleId` 참조 무결성을 검사(FR-10).
- 반환 타입은 공통 `RepositoryResult{bool success; std::string message}` — Console은 메시지만 출력.
- `SampleUpdate`/`OrderUpdate`는 필드를 `std::optional<T>`로 둬 "미입력 필드는 유지"를 자연스럽게 표현.
- Repository는 매 호출마다 전체 파일을 load/save(단순성 우선, 성능은 희생).

## 4. 예외처리 / 손상 데이터 대응

- `JsonFileStore::load()`: 파일이 없으면(`ifstream::is_open()==false`) 빈 벡터 반환(NFR-3).
- 파싱 실패 시 `nlohmann::json::exception`을 catch해 stderr 경고 출력 후 빈 벡터로 대체(NFR-1) — 예외가 상위로 전파되지 않음.
- `save()`는 상위 디렉터리가 없으면 `std::filesystem::create_directories`로 자동 생성.
- 테스트(`test_corrupted_file_returns_empty`)는 `"{ this is not valid json "` 같은 깨진 텍스트를 파일에 직접 써서 `findAll()`이 빈 목록을 반환하는지 검증.

## 5. 콘솔 메뉴 / ConsoleIO

- `main.cpp`가 Composition Root: 1/2/0으로 SampleMenu/OrderMenu/종료 분기.
- `SampleMenu`/`OrderMenu`는 각자 `run()` 루프(제목→선택→분기, 등록/전체조회/검색/수정/삭제/뒤로) — **의도적으로 공통 추상화하지 않음**(Rule of Three 미달, 문구 차이 보존: 시료="등록/수정/삭제" vs 주문="접수/수정/삭제").
- `ConsoleIO.hpp`는 `readLine/readInt/readDouble/readOptionalLine/readOptionalDouble/readOptionalInt`(빈 입력 = nullopt = 변경없음)와 반복되던 `printResult(RepositoryResult)` 헬퍼 제공.
- 숫자 파싱은 `std::stoi/stod`를 문자열 전체 소비 검증(`pos==line.size()`)과 함께 사용, 실패 시 nullopt.

### 5.1 업데이트 (Phase 8 이후 버그 수정 — `DESIGN.md` "추가 리뷰" 섹션)

- **EOF 무한 루프 버그 수정**: `SampleMenu::run()`/`OrderMenu::run()`에서 `readLine("선택 > ")` 직후 `std::cin` 상태를 확인해 EOF면 `break`로 상위 메뉴로 복귀하도록 수정.
  ```cpp
  if (!std::cin) {
      // 입력 스트림이 종료됨(EOF) - "잘못된 입력"으로 무한 반복하지 않고 상위 메뉴로 복귀한다.
      break;
  }
  ```
  기존에는 표준입력이 EOF로 끊기면 `readLine`이 빈 문자열을 반환하고 메뉴 루프가 이를 "잘못된 선택"으로 처리해 재프롬프트 → 즉시 재-EOF → 재프롬프트 …가 반복되며 무한 루프/CPU 폭주가 발생했다(재현 시 2초 만에 314만 줄 출력). `main.cpp`의 최상위 루프에는 이미 있던 EOF 감지를 서브메뉴에도 동일하게 적용한 것.
- **숫자 파싱 중복 제거**: `readInt`/`readDouble`/`readOptionalInt`/`readOptionalDouble` 4곳에 중복되던 "stoi/stod 시도 → 전체 문자열 소비 검증 → 실패 시 nullopt" 로직을 `detail::parseFull<T>(line, parseFn)` 템플릿 + `detail::parseInt`/`detail::parseDouble` 헬퍼로 통합. 동작은 기존과 동일하고 코드 중복만 제거됨.
- 데이터 모델(Sample/Order), Repository 인터페이스, `JsonFileStore`의 load/save 정책, 빌드(CMakeLists.txt)·테스트(8개 테스트/26개 CHECK) 구성은 **변경 없음** — 이번 수정은 콘솔 계층 한정의 버그 수정 + 소규모 리팩토링이다.

## 6. 빌드 / 테스트

- CMake(3.16+) + Ninja + MSVC, C++17.
- MSVC에 `/utf-8 /W4` 필수(한글 소스 코드페이지 이슈).
- 두 타깃: `DataPersistenceApp`(실제 앱), `DataPersistenceTests`(Repository만 링크, GoogleTest 없이 **자체 CHECK 매크로 기반 경량 하네스**, `ctest`로 등록).
- 테스트는 `test_data/`에서 독립 실행(실제 `data/`와 분리), 8개 테스트/26개 CHECK로 중복생성 거부·미존재ID 실패·부분수정·참조무결성·인스턴스 재생성을 통한 영속성 검증·파일없음/손상파일 시나리오를 커버.

## 7. SampleOrderSystem에 적용할 인사이트

- **model / storage / repository / console 4계층 분리**를 그대로 MVC의 Model 계층에 이식 가능. Storage(순수 파일 I/O)와 Repository(도메인 규칙)를 분리해두면 이후 DB 전환 시 Repository 인터페이스는 유지한 채 Storage만 교체 가능.
- `RepositoryResult` 패턴으로 View/도메인 로직 분리, `*Update` optional-patch 구조체로 부분수정을 일관되게 처리.
- Repository 간 **단방향 의존**(Order→Sample) 패턴은 우리 시스템의 주문↔시료 참조 무결성에 그대로 적용 가능. 다만 실제 시스템은 상태 전이(RESERVED→CONFIRMED/PRODUCING/RELEASE/REJECTED)가 추가되므로 **상태 검증/전이 규칙 계층**을 별도로 추가해야 한다.
- 손상 JSON/파일 없음에 대한 방어적 load 패턴, `/utf-8` 컴파일 옵션, 경량 자체 테스트 하네스 방식은 그대로 채택 가능한 인프라 요소.
- CMake 기반 POC이지만, `SampleOrderSystem`은 **MSBuild/Visual Studio(vcxproj)만 사용**하기로 결정했으므로(`CMakeLists.txt`/Ninja 구성은 그대로 가져올 수 없음), 위 계층 구조와 클래스 설계만 참고하고 빌드 설정은 vcxproj에 맞게 새로 구성해야 한다.
- **모든 콘솔 메뉴 루프는 처음부터 EOF(`!std::cin`) 감지 후 상위 메뉴로 복귀하는 처리를 넣을 것.** Data_Persistence POC는 이를 최상위 루프에만 넣었다가 서브메뉴 무한 루프 버그를 뒤늦게 발견해 수정했다 — 우리 시스템은 메인 메뉴뿐 아니라 시료관리/주문관리/모니터링/출고처리/생산라인의 모든 하위 메뉴 루프에 동일한 EOF 처리를 처음부터 일관되게 적용해 같은 버그를 재발시키지 않는다.
