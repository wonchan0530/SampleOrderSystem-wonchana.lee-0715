# Design — Phase 6: 모니터링 + 메인 메뉴 최종 통합

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 6, [`../docs/04-모니터링.md`](../docs/04-모니터링.md), [`../docs/01-메인메뉴.md`](../docs/01-메인메뉴.md)
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 테스트 76/76 통과, 메인 메뉴 최종 순서/요약정보 수동 시연 확인)

## 1. 목표

상태별 주문 집계와 시료별 재고 여유/부족/고갈 판정을 제공하는 모니터링 기능을 추가하고, Phase 2~5에서 "임시로" 유지해온 메인 메뉴를 `docs/01-메인메뉴.md`가 정의한 최종 형태(번호 순서 고정 + 요약 정보 표시)로 정리한다.

## 2. `MonitoringController` — 새 Controller 도입 근거

시료관리/주문관리와 달리 모니터링은 **`SampleRepository`와 `OrderRepository` 양쪽을 동시에 조합**해야 하는 진짜 오케스트레이션이 처음 필요한 지점이다(Phase 2 설계 §2의 "실제 조합 로직이 필요할 때 Controller 도입" 원칙). `OrderController`에 끼워 넣지 않고 별도 클래스로 분리하는 이유는, 이 로직이 Order의 생명주기 관리(접수/승인/거절/출고)와는 성격이 다른 **읽기 전용 집계**이기 때문이다.

`include/Controller/MonitoringController.h` / `src/Controller/MonitoringController.cpp`:

```cpp
struct OrderStatusCounts {
    int reserved = 0;
    int producing = 0;
    int confirmed = 0;
    int release = 0;
    // REJECTED는 집계 대상에서 제외 (docs/04-모니터링.md)
};

enum class StockLevel { Depleted, Insufficient, Sufficient };  // 고갈 / 부족 / 여유

struct SampleStockStatus {
    Sample sample;
    int pendingQuantity;  // 이 시료를 참조하는 RESERVED+PRODUCING+CONFIRMED 주문 수량 합
    StockLevel level;
};

class MonitoringController {
public:
    MonitoringController(const SampleRepository& sampleRepo, const OrderRepository& orderRepo);

    OrderStatusCounts countOrdersByStatus() const;
    std::vector<SampleStockStatus> stockStatusBySample() const;

private:
    const SampleRepository& sampleRepo_;
    const OrderRepository& orderRepo_;
};
```

- `countOrdersByStatus()`: `orderRepo_.findAll()`을 순회해 상태별로 카운트. `REJECTED`는 제외.
- `stockStatusBySample()`: 각 시료에 대해 `orderRepo_.findBySampleId(sample.id)` 결과 중 `RESERVED`/`PRODUCING`/`CONFIRMED` 상태(출고 전 = 아직 대응 재고가 필요한 상태)의 `quantity` 합을 `pendingQuantity`로 계산. `RELEASE`는 이미 출고 완료라 재고 부담이 없고, `REJECTED`는 유효하지 않으므로 둘 다 제외.
- **재고 상태 판정 순서**(`docs/04-모니터링.md`의 제안 규칙을 그대로 코드화):
  1. `sample.stock == 0` → `Depleted`(고갈) — `pendingQuantity`와 무관하게 우선 적용.
  2. `sample.stock < pendingQuantity` → `Insufficient`(부족)
  3. 그 외(`stock >= pendingQuantity`) → `Sufficient`(여유)

## 3. `MonitoringMenu`

`include/Console/MonitoringMenu.h` / `src/Console/MonitoringMenu.cpp`.

```cpp
class MonitoringMenu {
public:
    MonitoringMenu(MonitoringController& controller, ConsoleIO& io);
    void run();

private:
    void handleOrderCounts();
    void handleStockLevels();

    MonitoringController& controller_;
    ConsoleIO& io_;
};
```

- 메뉴: "1. 주문량 확인 / 2. 재고량 확인 / 0. 뒤로".
- `handleOrderCounts`: `RESERVED N건 / PRODUCING N건 / CONFIRMED N건 / RELEASE N건` 형식 출력.
- `handleStockLevels`: 시료별로 `이름/재고/대기수량/상태(여유·부족·고갈)` 출력. 등록된 시료가 없으면 "등록된 시료가 없습니다."

## 4. 메인 메뉴 최종 통합

`docs/01-메인메뉴.md`가 정의한 번호와 순서로 고정한다: **1.시료관리 / 2.주문(접수·승인·거절) / 3.모니터링 / 4.출고처리 / 5.생산라인 / 0.종료**. Phase 5까지 임시로 붙여온 순서(3.생산라인, 4.출고처리)를 여기서 재배치한다.

요약 정보는 `MonitoringController`를 재사용해 계산한다(새 로직 추가 없음):

```cpp
int main() {
    ConsoleIO io;
    SampleRepository sampleRepo(...);
    OrderRepository orderRepo(...);
    ProductionQueueRepository productionQueue(...);
    OrderController orderController(orderRepo, sampleRepo, productionQueue);
    ProductionController productionController(productionQueue, sampleRepo, orderRepo);
    MonitoringController monitoringController(sampleRepo, orderRepo);

    SampleMenu sampleMenu(sampleRepo, io);
    OrderMenu orderMenu(orderController, io);
    MonitoringMenu monitoringMenu(monitoringController, io);
    ShippingMenu shippingMenu(orderController, io);
    ProductionMenu productionMenu(productionController, io);

    while (true) {
        productionController.processCompletions();

        const auto samples = sampleRepo.findAll();
        const auto stockStatus = monitoringController.stockStatusBySample();
        const int depletedOrShort = static_cast<int>(std::count_if(
            stockStatus.begin(), stockStatus.end(),
            [](const SampleStockStatus& s) { return s.level != StockLevel::Sufficient; }));

        io.println("=== 반도체 시료 생산주문관리 시스템 ===");
        io.println("등록된 시료: " + std::to_string(samples.size()) + "종 (재고 주의 필요: "
                    + std::to_string(depletedOrShort) + "종)");
        io.println("1. 시료관리");
        io.println("2. 주문(접수/승인/거절)");
        io.println("3. 모니터링");
        io.println("4. 출고처리");
        io.println("5. 생산라인");
        io.println("0. 종료");
        try {
            const int choice = io.readInt("선택 > ");
            if (choice == 1) sampleMenu.run();
            else if (choice == 2) orderMenu.run();
            else if (choice == 3) monitoringMenu.run();
            else if (choice == 4) shippingMenu.run();
            else if (choice == 5) productionMenu.run();
            else if (choice == 0) break;
            else io.println("잘못된 선택입니다.");
        } catch (const EofException&) {
            break;
        }
    }
    return 0;
}
```

- "전체 시료에 대한 요약 정보"(docs/01 §요약 정보 항목)를 **등록 시료 종류 수 + 재고 주의(부족/고갈) 시료 종류 수** 한 줄로 표시한다. 상세 목록은 여전히 모니터링 메뉴에서 확인한다(메인 메뉴는 한눈에 보는 요약만 담당).

## 5. `main.cpp`는 여전히 별도 테스트 대상이 아님

Phase 2 설계 이후 유지해온 방침대로, `main.cpp`는 조립(composition root)만 담당하며 각 메뉴 클래스가 이미 개별적으로 테스트되므로 `main.cpp` 자체를 위한 단위 테스트는 추가하지 않는다(MVC POC의 원칙과 동일). 대신 완료 기준의 수동 시연으로 조립이 올바른지 확인한다.

## 6. 테스트 계획

`tests/MonitoringControllerTest.cpp`:
- 상태별 집계: RESERVED/PRODUCING/CONFIRMED/RELEASE 각각 1건 이상 + REJECTED 1건을 만들고 `countOrdersByStatus()`가 REJECTED를 제외한 4개 카운트를 정확히 반환하는지.
- 재고 판정 3단계: 재고 0(고갈), `0 < 재고 < 대기수량`(부족), `재고 >= 대기수량`(여유) 각각의 시료를 만들어 검증.
- `pendingQuantity`가 RESERVED+PRODUCING+CONFIRMED만 합산하고 RELEASE/REJECTED는 제외하는지.
- 시료/주문이 전혀 없을 때 빈 결과를 반환하는지.

`tests/MonitoringMenuTest.cpp`: istringstream 시나리오로 주문량/재고량 조회 출력에 기대한 상태 라벨(여유/부족/고갈)이 포함되는지, 빈 상태 메시지, EOF 안전 종료.

## 7. 완료 기준

- `scripts/verify.ps1` PASS
- `docs/04-모니터링.md`의 집계 규칙과 재고 상태 3단계가 테스트로 커버됨
- 빌드된 exe로 메인 메뉴가 `docs/01-메인메뉴.md`의 번호/순서와 일치하고 요약 정보가 표시되는지, 전체 메뉴(1~5)가 모두 정상 진입되는지 수동 시연
