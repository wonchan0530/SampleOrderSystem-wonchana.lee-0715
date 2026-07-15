# Design — Phase 7: Dummy 데이터 생성 도구

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 7, [`../poc/04-Dummy_Data_Generator.md`](../poc/04-Dummy_Data_Generator.md)
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 테스트 85/85 통과, 실제 도구 실행 후 모니터링 메뉴에서 데이터 확인)

> **구현 시 설계 대비 변경 사항**: (1) CLI에 4번째 선택 인자 `dataDir`를 추가해 테스트가 실제 앱 데이터(`data/`)를 건드리지 않고 격리된 경로에 생성/검증할 수 있게 함. (2) E2E 테스트에서 `std::system()` 호출 시 명령 전체를 따옴표로 감싸면 Windows cmd.exe의 중첩 따옴표 파싱 버그로 실행 파일을 찾지 못하는 문제를 코드 리뷰 중 발견 — 저장소 경로에 공백이 없으므로 인자를 따옴표 없이 전달하도록 수정.

## 1. 목표

테스트/시연용 시료·주문 더미 데이터를 생성해, `SampleOrderSystem.exe`가 읽는 것과 **동일한 데이터 파일**(`data/samples.json`, `data/orders.json`)에 반영하는 개발자 도구를 만든다. Dummy_Data_Generator POC와의 핵심 차이는 참조 무결성(주문은 반드시 실재하는 시료를 참조)을 지켜야 한다는 점이다([`../poc/04-Dummy_Data_Generator.md`](../poc/04-Dummy_Data_Generator.md) §7).

## 2. 프로젝트 구조 — 별도 실행파일 프로젝트로 분리

**결정: 메인 앱에 `--seed-data` 같은 모드를 추가하지 않고, 별도 콘솔 프로젝트를 만든다.**

- 이유: 이 도구는 대화형 메뉴가 아니라 커맨드라인 인자 한 번으로 끝나는 비대화형 스크립트이고(사용자 시나리오가 다름), 원본 PRD의 "Dummy 데이터 생성 Tool" 자체가 독립된 도구로 요구되었다(PRD.md 미션 1). `SampleOrderSystemTests`가 이미 앱의 소스를 재사용하는 전례가 있으므로 동일한 패턴을 따른다.

```
SampleOrderSystem/
├── SampleOrderSystem.slnx                          (신규 프로젝트 추가)
├── SampleOrderSystem/                               (기존 앱)
├── SampleOrderSystemTests/                          (기존 테스트)
└── SampleOrderSystemDummyDataGenerator/              (신규)
    ├── SampleOrderSystemDummyDataGenerator.vcxproj
    └── src/
        └── main.cpp
```

- `Model/Storage/Repository`는 새로 만들지 않고 `..\SampleOrderSystem\src\...`의 기존 `.cpp`를 그대로 컴파일에 포함한다(`SampleOrderSystemTests`가 이미 쓰는 방식과 동일). `Controller`(`OrderController`)도 재사용해 참조 무결성뿐 아니라 실제 승인 로직까지 실제 코드 경로로 실행한다(아래 §4).
- 데이터 경로도 `PathUtil::dataDir()`를 그대로 사용 — 도구를 앱과 같은 폴더에 두고 실행하면 자동으로 같은 `data/`를 공유한다.

## 3. CLI 인터페이스

```
SampleOrderSystemDummyDataGenerator.exe [샘플개수] [주문개수] [seed]
```

- 인자 생략 시 기본값: 샘플개수 10, 주문개수 30, seed는 `std::random_device`(재현 불가, 매번 다름).
- seed를 명시하면 `std::mt19937`를 그 값으로 고정해 **동일 seed → 동일 결과**를 보장한다(Dummy_Data_Generator POC의 재현성 패턴 재사용, [`../poc/04-Dummy_Data_Generator.md`](../poc/04-Dummy_Data_Generator.md) §3).
- 종료 코드: 0 성공, 1 인자 파싱 오류.

## 4. 생성 로직 — 참조 무결성을 실제 Repository/Controller로 보장

```cpp
int main(int argc, char** argv) {
    // 인자 파싱: sampleCount, orderCount, seed(optional)
    std::mt19937 rng(seed);

    SampleRepository sampleRepo(PathUtil::dataDir() / "samples.json");
    OrderRepository orderRepo(PathUtil::dataDir() / "orders.json", sampleRepo);
    ProductionQueueRepository productionQueue(PathUtil::dataDir() / "production_queue.json");
    OrderController orderController(orderRepo, sampleRepo, productionQueue);

    // 1) 시료 sampleCount개 생성 (이름/평균생산시간/수율 랜덤, 생성 직후 임의 재고를 adjustStock으로 반영)
    // 2) 방금 만든 시료 ID 목록 중에서 무작위로 골라 orderCount개 주문 생성 (orderRepo.create)
    // 3) 생성된 주문 중 일부(예: 약 50%)를 orderController.approve(orderId)로 승인 처리
    //    -> 실제 승인 로직(재고 차감/생산 큐 등록)을 그대로 타므로 상태 분포(RESERVED/CONFIRMED/PRODUCING)가
    //       자연스럽게 섞인 현실적인 더미 데이터가 만들어진다. 상태를 직접 조작하지 않는다.

    // 결과 요약 출력: 생성된 시료 수 / 주문 수 / 상태별 분포
    return 0;
}
```

- **핵심 설계 원칙: 더미 데이터도 반드시 실제 Repository/Controller의 공개 API만 사용해 생성한다.** JSON을 직접 조작하지 않는다 — 이렇게 하면 생성 결과가 항상 애플리케이션이 강제하는 모든 불변식(참조 무결성, 검증 규칙, 상태 전이 규칙)을 만족한다는 것이 보장된다(POC의 "생성 순서를 새로 설계해야 함" 과제를 "기존 API 재사용"으로 해결).
- 랜덤 값 범위(제안): 이름 `"Sample-" + std::to_string(index)`, 평균생산시간 1.0~30.0분, 수율 0.5~1.0, 초기 재고(adjustStock) 0~50, 주문 수량 1~20, 고객명은 짧은 목록(`"Acme"`, `"Globex"`, ...)에서 무작위 선택.

## 5. 빌드 설정

- CMake 없이 vcxproj로 구성(프로젝트 전체 원칙 유지). `SampleOrderSystem.vcxproj`와 동일한 `PlatformToolset=v145`, C++20, `/utf-8`.
- `SampleOrderSystem.slnx`에 세 번째 프로젝트로 등록.
- `scripts/verify.ps1`에 이 프로젝트의 빌드 단계를 추가(4개 구성 × 이제 3개 프로젝트).

## 6. 테스트 계획

새 테스트 파일은 `SampleOrderSystemTests`에 추가하지 않고, **이 도구 자신의 최소 로직(랜덤 범위 검증, 참조 무결성)**을 검증할 별도 방법이 필요하다. 두 가지를 채택한다.

- `tests/DummyDataGeneratorTest.cpp`(`SampleOrderSystemTests`에 포함): 생성 로직을 함수 단위(`generateSamples(rng, count)`, `generateOrders(rng, sampleIds, count)`처럼 순수 함수로 분리)로 만들어 유닛 테스트 가능하게 한다.
  - 지정 개수만큼 생성되는지
  - 모든 주문의 `sampleId`가 생성된 시료 ID 집합에 포함되는지(참조 무결성)
  - 값 범위(평균생산시간/수율/수량) 검증
  - 동일 seed로 두 번 생성 시 동일 결과(재현성) — Dummy_Data_Generator POC의 `test_generator.cpp` 패턴 재사용
- `tests/DummyDataGeneratorE2ETest.cpp`: 빌드된 `SampleOrderSystemDummyDataGenerator.exe`를 `std::system`으로 직접 실행해(Dummy_Data_Generator POC의 `test_e2e.cpp` 패턴) 종료 코드 0, 결과 파일이 실제 `SampleRepository`/`OrderRepository`로 정상 로드되는지 확인.

## 7. 완료 기준

- `scripts/verify.ps1` PASS(3개 프로젝트 전부)
- 생성 로직 유닛 테스트 + E2E 테스트 통과
- 실제로 `SampleOrderSystemDummyDataGenerator.exe 10 30 42` 실행 후 `SampleOrderSystem.exe`를 열어 모니터링 메뉴에서 생성된 데이터가 정상적으로 조회되는지 수동 시연
