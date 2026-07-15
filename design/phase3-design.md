# Design — Phase 3: 주문 접수/승인/거절

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 3, [`../docs/03-주문관리.md`](../docs/03-주문관리.md), [`../PRD.md`](../PRD.md) §6.1
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 테스트 53/53 통과, 수동 시연 확인)

## 1. 목표

주문 접수(RESERVED 생성은 Phase 1의 `OrderRepository::create`로 이미 구현됨)에 이어, **주문 승인/거절과 재고 차감 정책**을 구현한다. 이번 Phase에서 처음으로 진짜 오케스트레이션 로직(재고 확인 + Repository 2~3개 조합)이 필요해지므로 **`OrderController`를 여기서 처음 도입**한다(Phase 2 설계 §2 참고).

이번 Phase는 PLAN.md상 Phase 4(생산라인) 이전이지만, 승인 로직 자체가 "재고 부족 시 생산 큐에 등록"을 필요로 하므로, **생산 큐의 자료구조/영속성(enqueue까지)은 이번 Phase에서 함께 만든다.** 큐를 실제로 소비(생산 완료 판정, FIFO 진행)하는 로직은 Phase 4의 몫으로 남긴다 — "무엇을 큐에 넣을지 결정"(Phase 3, 정책)과 "큐를 어떻게 비울지"(Phase 4, 시간 기반 처리)를 분리한다.

## 2. Model — `ProductionJob`

`include/Model/ProductionJob.h`:

```cpp
struct ProductionJob {
    int orderId;
    int sampleId;
    int shortfall;                 // 주문 수량 - 승인 시점 재고
    int actualProductionQty;       // ceil(shortfall / yieldRate)
    double totalProductionTimeMin; // avgProductionTimeMin * actualProductionQty
    std::optional<std::chrono::system_clock::time_point> startedAt; // 큐 맨 앞(진행 중)일 때만 값 있음
};
void to_json(nlohmann::json& j, const ProductionJob& job);
void from_json(const nlohmann::json& j, ProductionJob& job);
```

- `startedAt`은 **큐의 맨 앞(현재 생산 중인 작업)일 때만 값을 가진다.** 대기 중인 작업은 `nullopt`. 이 값은 Phase 4에서 이전 작업이 끝날 때 "직전 작업의 완료 예정 시각"으로 채워진다(체인 완료, `docs/06-생산라인.md` §3).
- JSON 직렬화: `startedAt`은 `std::chrono::system_clock::to_time_t`로 epoch 초(정수)로 저장하고, 값이 없으면 `null`. `nlohmann::json`이 `std::optional`을 기본 지원하므로 `to_json`/`from_json`에서 `time_t`로 변환만 해주면 된다.

## 3. Repository — `ProductionQueueRepository`

`include/Repository/ProductionQueueRepository.h` / `src/Repository/ProductionQueueRepository.cpp`.

```cpp
class ProductionQueueRepository {
public:
    explicit ProductionQueueRepository(std::filesystem::path dataFile);

    std::vector<ProductionJob> findAll() const;   // FIFO 순서 그대로(맨 앞 = 인덱스 0)
    bool empty() const;

    // 큐가 비어있으면 즉시 시작(startedAt = now)하고, 아니면 대기(startedAt = nullopt)로 등록.
    void enqueue(int orderId, int sampleId, int shortfall, int actualProductionQty,
                 double totalProductionTimeMin, std::chrono::system_clock::time_point now);

    // Phase 4에서 사용할 저수준 조작. 이번 Phase에서는 enqueue만 호출된다.
    void popFront();
    void setFrontStartedAt(std::chrono::system_clock::time_point startedAt);

private:
    JsonFileStore<ProductionJob> store_;
    std::vector<ProductionJob> cache_;
    void persist() const;
};
```

- `popFront`/`setFrontStartedAt`은 Phase 4의 완료 처리 로직이 사용할 저수준 연산이며, 이번 Phase에는 호출자가 없다(설계만 미리 확정해 Phase 4에서 바로 쓸 수 있게 함).
- 실생산량/총생산시간 계산(`ceil`, 곱셈)은 **`ProductionQueueRepository`가 아니라 `OrderController`가 계산해서 값으로 넘긴다** — Repository는 여전히 "정책 판단 없이 반영만" 원칙(Phase 1 §4)을 유지한다.

## 4. `OrderRepository` 확장 — 상태 전이 메서드 추가

Phase 1에서 의도적으로 비워뒀던 상태 전이를 이번에 추가한다(`include/Repository/OrderRepository.h` 수정).

```cpp
RepositoryResult markConfirmed(int orderId);   // RESERVED 또는 PRODUCING에서만 허용
RepositoryResult markProducing(int orderId);   // RESERVED에서만 허용
RepositoryResult markRejected(int orderId);    // RESERVED에서만 허용
```

- 상태 전이 **합법성 검사(어떤 상태에서 어떤 상태로 갈 수 있는가)는 Repository가 담당**(CRUD 계층이 이미 데이터 무결성을 지키는 역할이므로 자연스러운 연장). **"지금 이 전이를 해야 하는가"(재고가 충분한가 등 정책 판단)는 `OrderController`가 담당.**

## 5. `OrderController` — 승인/거절 정책 (PRD.md §6.1)

`include/Controller/OrderController.h` / `src/Controller/OrderController.cpp`.

```cpp
class OrderController {
public:
    OrderController(OrderRepository& orderRepo, SampleRepository& sampleRepo,
                     ProductionQueueRepository& productionQueue);

    RepositoryResult placeOrder(int sampleId, const std::string& customerName, int quantity);
    std::vector<Order> listReserved() const;
    RepositoryResult approve(int orderId, std::chrono::system_clock::time_point now = std::chrono::system_clock::now());
    RepositoryResult reject(int orderId);

private:
    OrderRepository& orderRepo_;
    SampleRepository& sampleRepo_;
    ProductionQueueRepository& productionQueue_;
};
```

- `placeOrder`: `orderRepo_.create(...)`로 위임(참조 무결성 검사는 Phase 1에서 이미 `OrderRepository::create`에 구현됨).
- `approve(orderId, now)`:
  1. 주문 조회, `RESERVED`가 아니면 실패 반환.
  2. 시료 조회(참조 무결성상 항상 존재).
  3. **재고 ≥ 주문 수량** → `sampleRepo_.adjustStock(sampleId, -quantity)` → `orderRepo_.markConfirmed(orderId)`.
  4. **재고 < 주문 수량** → `shortfall = quantity - stock`; `sampleRepo_.adjustStock(sampleId, -stock)`(0까지 전부 차감); `actualQty = ceil(shortfall / yieldRate)`; `totalTime = avgProductionTimeMin * actualQty`; `productionQueue_.enqueue(orderId, sampleId, shortfall, actualQty, totalTime, now)`; `orderRepo_.markProducing(orderId)`.
  - **재고 확인/차감은 이 메서드 안에서 딱 1번만 일어난다**(PRD.md §6.1) — 이후 Phase 4의 생산 완료 처리는 이 메서드를 다시 호출하지 않는다.
  - `now`를 매개변수로 받게 해서(기본값은 실제 시각) 테스트에서 임의의 시각을 주입할 수 있게 한다.
- `reject(orderId)`: `orderRepo_.markRejected(orderId)` 위임.

## 6. `OrderMenu`

`include/Console/OrderMenu.h` / `src/Console/OrderMenu.cpp`. Phase 2의 `SampleMenu`와 동일한 패턴.

```cpp
class OrderMenu {
public:
    OrderMenu(OrderController& controller, ConsoleIO& io);
    void run();

private:
    void handlePlaceOrder();     // 시료ID/고객명/수량 입력 -> placeOrder
    void handleListReserved();   // RESERVED 목록 표시
    void handleApprove();        // 주문ID 입력 -> approve
    void handleReject();         // 주문ID 입력 -> reject

    OrderController& controller_;
    ConsoleIO& io_;
};
```

`main.cpp`에 "2. 주문(접수/승인/거절)" 메뉴로 연결한다(Phase 2 임시 메인 루프에 옵션 추가).

## 7. 테스트 계획

`tests/ProductionQueueRepositoryTest.cpp`: 빈 큐에 enqueue하면 `startedAt`이 즉시 채워짐, 이미 값이 있는 큐에 enqueue하면 새 작업은 `startedAt=nullopt`, 저장 후 재로드 시 `startedAt` 유무와 값이 그대로 유지(영속성), `popFront`/`setFrontStartedAt` 동작.

`tests/OrderControllerTest.cpp` (PRD.md §6.1 시나리오 직접 재현):
- 재고 충분: 승인 시 즉시 CONFIRMED, 재고가 정확히 차감됨, 생산 큐에 아무것도 안 쌓임.
- 재고 부족: 승인 시 PRODUCING, 재고 0, 큐에 `shortfall`/`actualProductionQty`(예: 부족분 5, 수율 0.9 → 6)/`totalProductionTimeMin` 정확한 값으로 1건 등록.
- **동일 시료 후속 주문 격리**: 재고10에서 주문A(수량15) 승인 → 부족분5 큐 등록, 재고0. 이어서 주문B(같은 시료, 수량8) 승인 → 부족분8 전체 큐 등록(재고가 이미 0이므로) — 두 큐 항목이 각자 독립적인지 확인(PRD.md §6.1 예시).
- 이미 RESERVED가 아닌 주문 재승인/재거절 시도 거부.
- `reject`: REJECTED 전환, 이후 승인 시도 거부.

`tests/OrderMenuTest.cpp`: Phase 2와 동일한 방식(istringstream 스크립트)으로 접수→목록→승인/거절 전체 플로우 검증.

## 8. 완료 기준

- `scripts/verify.ps1` PASS
- `docs/03-주문관리.md` 상태 전이표/예외처리표, `PRD.md` §6.1의 재고 차감/후속 주문 격리 예시가 테스트로 커버됨
- 빌드된 exe로 시료 등록 → 주문 접수 → 승인(재고 충분/부족 각각) → 거절까지 수동 시연
