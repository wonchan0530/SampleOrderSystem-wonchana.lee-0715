# Design — Phase 0: 프로젝트 기반 준비

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 0
> 상태: **승인됨 — 구현 완료** (Harness(`scripts/verify.ps1`) 4개 구성 빌드 + 테스트 통과 확인)

## 1. 목표

빈 vcxproj를 이후 모든 Phase가 그 위에서 개발 가능한 상태로 만든다. 이 문서에서 확정하는 4가지: (1) 소스 트리 구조, (2) JSON 처리 방식, (3) 데이터 파일 경로 정책, (4) 테스트/Harness 구성.

## 2. 소스 트리 구조

MVC POC([`../poc/03-MVC.md`](../poc/03-MVC.md))의 `include/<Layer>/`, `src/<Layer>/` 분리와 Data_Persistence POC([`../poc/02-Data_Persistence.md`](../poc/02-Data_Persistence.md))의 `model/storage/repository/console` 4계층을 결합한다.

```
SampleOrderSystem/
├── SampleOrderSystem.slnx
├── SampleOrderSystem/                     (앱 프로젝트, 기존 vcxproj)
│   ├── SampleOrderSystem.vcxproj
│   ├── include/
│   │   ├── Model/            Sample.h, Order.h  (순수 데이터 구조체)
│   │   ├── Storage/           JsonValue.h, JsonFileStore.h  (파일 I/O, 도메인 모름)
│   │   ├── Repository/       RepositoryResult.h, SampleRepository.h, OrderRepository.h,
│   │   │                     ProductionQueueRepository.h (Phase 4에서 채움)
│   │   ├── Controller/       SampleController.h, OrderController.h, ProductionController.h, ...
│   │   └── Console/          ConsoleIO.h, MainMenu.h, SampleMenu.h, OrderMenu.h, ...
│   └── src/                  (위 각 폴더와 동일한 하위 구조의 .cpp)
│       └── main.cpp          (Composition Root — 조립만 담당)
└── SampleOrderSystemTests/                (신규: 테스트 전용 vcxproj)
    ├── SampleOrderSystemTests.vcxproj
    └── tests/
        ├── testing.h          (자체 경량 TEST/EXPECT 매크로, 외부 프레임워크 없음)
        ├── test_main.cpp
        ├── SampleRepositoryTest.cpp
        ├── OrderRepositoryTest.cpp
        └── ProductionQueueTest.cpp  (Phase 4에서 채움)
```

- Controller는 MVC POC와 달리 **엔티티별 구체 클래스**로 시작한다(`Repository<T>` 같은 제네릭 템플릿은 도입하지 않음) — 우리 도메인은 CRUD 이상의 상태 전이 규칙(승인 시 재고 차감 등)이 많아 엔티티마다 로직이 크게 달라지기 때문. MVC POC의 인사이트(§7)대로 "실제 중복이 확인되면 그때 추출"한다.
- `SampleOrderSystemTests`는 `SampleOrderSystem`의 `Model/Storage/Repository` 소스를 함께 컴파일해 링크하고 `main.cpp`는 제외한다(Data_Persistence POC의 `DataPersistenceApp`/`DataPersistenceTests` 2-타깃 구조와 동일).

## 3. JSON 처리 방식

**결정: `nlohmann/json` 단일 헤더를 저장소에 vendoring한다** (`SampleOrderSystem/third_party/nlohmann/json.hpp`).

- Data_Persistence POC가 이미 이 라이브러리로 Sample/Order 필드 구조를 검증했고, 우리 도메인 모델이 그 구조와 거의 동일하므로 그대로 재사용 가치가 높다.
- Data_Monitor POC의 자체 파서는 외부 의존성이 없다는 장점은 있지만, 유니코드/이스케이프 처리와 동시쓰기 등에 대한 고려가 없다고 이미 학습노트에 기록되어 있어([`../poc/01-Data_Monitor.md`](../poc/01-Data_Monitor.md) §7) 이번 실제 구현에서는 검증된 라이브러리 쪽이 더 안전하다고 판단.
- **네트워크 의존 금지**: Dummy_Data_Generator POC처럼 CMake `FetchContent`로 받는 방식은 쓰지 않는다. `json.hpp` 파일 자체를 저장소에 커밋해 오프라인 빌드를 보장한다.
- 필드 네이밍은 Data_Persistence POC와 동일하게 맞춘다(재사용성 + 이미 검증된 필드셋):
  - `Sample { id, name, avgProductionTimeMin, yieldRate, stock }`
  - `Order { orderId, sampleId, customerName, quantity, status }` (status 기본값 `"RESERVED"`)

## 4. 데이터 파일 경로 정책

**결정: 실행파일 경로 기준**(cwd 아님) — Data_Monitor POC의 `resolveDataDir()` 패턴(`GetModuleFileNameW`)을 채택한다. 어떤 디렉터리에서 실행하든(더블클릭, VS 디버거, 다른 cwd) 항상 같은 데이터를 본다.

- 데이터 폴더: `<실행파일 경로>/data/`
- 파일: `data/samples.json`, `data/orders.json`, `data/production_queue.json`(Phase 4에서 사용 시작, 지금은 빈 배열로 폴더 구조만 예약)
- 파일 없음/빈 파일 → 빈 목록으로 시작(자동 초기화), 파싱 실패 → stderr 경고 후 빈 목록으로 대체(예외 비전파) — Data_Persistence POC의 `JsonFileStore` 정책 그대로 채택.
- 테스트(`SampleOrderSystemTests`)는 별도의 `test_data/` 폴더를 사용해 실제 `data/`와 절대 섞이지 않게 한다.

## 5. 테스트/Harness 구성

- **테스트 프레임워크**: 외부 의존성 없는 자체 `testing.h`(TEST/EXPECT_EQ/EXPECT_TRUE 매크로 + 정적 레지스트라 패턴) — MVC/Data_Persistence 두 POC 모두 동일한 방식을 검증했으므로 그대로 채택. GoogleTest 등 외부 프레임워크는 도입하지 않는다(오프라인 빌드 원칙과 충돌).
- **Harness 스크립트**: `scripts/verify.ps1` 신규 작성.
  1. `MSBuild.exe`로 `SampleOrderSystem.slnx`를 `(Debug,Release) × (x86,x64)` 4개 조합으로 빌드. 하나라도 실패하면 즉시 실패 종료.
  2. `SampleOrderSystemTests.exe`(Debug|x64 산출물)를 실행하고 종료 코드 확인.
  3. 모든 단계 성공 시 "PASS", 요약(빌드 4/4, 테스트 결과) 출력. 실패 시 어느 단계/구성에서 실패했는지 명확히 출력하고 0이 아닌 코드로 종료.
- 이 스크립트가 이후 모든 Phase의 "4. Harness 실행" 단계에서 그대로 재사용된다(각 Phase는 여기에 자기 Phase의 테스트 파일만 추가).

## 6. Phase 0에서 실제로 만드는 것 (Implement 범위)

- 위 폴더 구조 생성, `.vcxproj`/`.vcxproj.filters`에 반영
- `third_party/nlohmann/json.hpp` vendoring
- 최소 컴파일 가능한 `main.cpp`(콘솔에 "SampleOrderSystem" 출력 후 종료) — 아직 메뉴/도메인 로직 없음
- `SampleOrderSystemTests` 프로젝트 생성 + `testing.h` + 더미 테스트 1개(항상 통과)로 파이프라인만 검증
- `scripts/verify.ps1` 작성 및 1회 실행해 통과 확인
- `.gitignore`에 `third_party/nlohmann/json.hpp`는 **포함하지 않음**(예외적으로 커밋 대상 — vendoring 목적)

## 7. 완료 기준 (Definition of Done)

- `scripts/verify.ps1` 실행 시 4개 구성 빌드 + 더미 테스트 통과로 "PASS" 출력
- 폴더 구조/JSON 방식/데이터 경로 정책이 `CLAUDE.md`에 반영됨 (Implement 단계 마지막에 갱신)
