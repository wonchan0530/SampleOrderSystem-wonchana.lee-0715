# Design — Phase 4: 생산라인

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 4, [`../docs/06-생산라인.md`](../docs/06-생산라인.md), [`../PRD.md`](../PRD.md) §6.2~6.3
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 테스트 60/60 통과, 실제 7초 대기 후 재시작 시연으로 벽시계 완료 판정 확인)

## 1. 목표

Phase 3에서 만든 `ProductionQueueRepository`(enqueue까지)를 실제로 소비한다: 벽시계 기준 완료 판정, 체인 완료(오래 종료돼 있던 동안 밀린 여러 건을 한 번에 처리), 초과 생산분 재고 반영, 생산 현황/대기열 조회 메뉴. PLAN.md가 "가장 설계 난이도가 높은 Phase"로 지목한 부분이다.

## 2. `ProductionController` — 완료 판정 + 체인 완료

`include/Controller/ProductionController.h` / `src/Controller/ProductionController.cpp`.

```cpp
class ProductionController {
public:
    ProductionController(ProductionQueueRepository& queue, SampleRepository& sampleRepo,
                          OrderRepository& orderRepo);

    // 큐 맨 앞부터 완료 조건(now >= startedAt + totalProductionTimeMin)을 만족하는 작업을
    // 모두 순차적으로 완료 처리한다(체인). 완료된 작업 수를 반환(테스트/로그용).
    int processCompletions(std::chrono::system_clock::time_point now = std::chrono::system_clock::now());

    std::optional<ProductionJob> currentJob() const;      // 큐 맨 앞(진행 중), 없으면 nullopt
    std::vector<ProductionJob> waitingQueue() const;       // 맨 앞을 제외한 나머지(FIFO 순서)

private:
    ProductionQueueRepository& queue_;
    SampleRepository& sampleRepo_;
    OrderRepository& orderRepo_;
};
```

### `processCompletions(now)` 알고리즘

```
while (!queue_.empty()):
    front = queue_.findAll()[0]
    if front.startedAt is nullopt:            # 이론상 발생하지 않음(맨 앞은 항상 시작됨), 방어적으로 break
        break
    completionTime = *front.startedAt + front.totalProductionTimeMin(분 -> duration 변환)
    if now < completionTime:
        break                                  # 아직 완료 안 됨, 체인 종료

    # 완료 처리 (docs/06-생산라인.md "실생산량은 전량 정상품으로 취급")
    surplus = front.actualProductionQty - front.shortfall
    sampleRepo_.adjustStock(front.sampleId, surplus)      # 부족분은 재고 경유 없이 주문에 바로 배정됐던 것으로 간주
    orderRepo_.markConfirmed(front.orderId)
    queue_.popFront()
    completedCount += 1

    if !queue_.empty():
        queue_.setFrontStartedAt(completionTime)          # 다음 작업 시작 시각 = 직전 작업 완료 예정 시각(체인)
```

- **"부족분은 재고 경유 없이 배정"**: Phase 3의 `approve()`가 이미 부족분만큼 재고를 차감(0까지)해뒀으므로, 완료 시점에는 부족분을 다시 재고에 더했다가 빼는 왕복 연산을 하지 않는다 — 그냥 주문을 `CONFIRMED`로 바꾸고, **초과분(`surplus`)만** 재고에 가산한다. 이는 PRD.md §6.2("이미 정해진 부족분을 주문에 채우고 초과분만 재고에 가산")를 코드 레벨로 그대로 옮긴 것이다.
- **completionTime을 "이전 작업의 시작 시각 + 총생산시간"으로 계산**하고(now가 아니라) 다음 작업의 `startedAt`으로 재사용 — 실제 시간이 아무리 많이 지나 있어도 각 작업이 정확히 자기 몫의 총생산시간만큼만 걸린 것처럼 체인이 이어진다(docs/06-생산라인.md §3).
- `now`를 매개변수화한 이유: 테스트에서 "총생산시간이 지난 뒤"를 재현하려고 실제로 몇 분씩 기다릴 수 없으므로, 과거 시각(`startedAt`)이 저장된 큐에 대해 미래의 임의 `now`를 넘겨 즉시 검증한다.

## 3. `OrderRepository.markConfirmed` 허용 상태 재확인

Phase 3 설계(§4)에서 이미 "`RESERVED` 또는 `PRODUCING`에서 허용"으로 정의해뒀으므로 추가 변경 없음 — `ProductionController`는 `PRODUCING` → `CONFIRMED` 경로로 이 메서드를 사용한다.

## 4. 호출 시점 — "메뉴 진입/조회 시점"과 "재시작 시점"을 하나의 지점으로 통합

`docs/06-생산라인.md` §3~4는 "메뉴 진입/조회 시점"과 "프로그램 재시작 시점" 둘 다에서 완료 판정을 하라고 요구한다. 이를 메뉴마다 흩어놓지 않고 **`main.cpp`의 메인 메뉴 루프 최상단, 매 반복(iteration)마다 한 번** 호출하는 것으로 통합한다.

```cpp
int main() {
    ConsoleIO io;
    SampleRepository sampleRepo(PathUtil::dataDir() / "samples.json");
    ProductionQueueRepository productionQueue(PathUtil::dataDir() / "production_queue.json");
    OrderRepository orderRepo(PathUtil::dataDir() / "orders.json", sampleRepo);
    ProductionController productionController(productionQueue, sampleRepo, orderRepo);
    OrderController orderController(orderRepo, sampleRepo, productionQueue);

    SampleMenu sampleMenu(sampleRepo, io);
    OrderMenu orderMenu(orderController, io);
    ProductionMenu productionMenu(productionController, io);

    while (true) {
        productionController.processCompletions();   // <- 재시작 직후 첫 반복에서 밀린 완료 처리, 이후 매 메인메뉴 진입마다 재확인
        io.println("=== 반도체 시료 생산주문관리 시스템 ===");
        io.println("1. 시료관리");
        io.println("2. 주문(접수/승인/거절)");
        io.println("3. 생산라인");
        io.println("0. 종료");
        try {
            int choice = io.readInt("선택 > ");
            if (choice == 1) sampleMenu.run();
            else if (choice == 2) orderMenu.run();
            else if (choice == 3) productionMenu.run();
            else if (choice == 0) break;
            else io.println("잘못된 선택입니다.");
        } catch (const EofException&) {
            break;
        }
    }
    return 0;
}
```

- 이렇게 하면 "프로그램을 껐다가 총생산시간이 지난 뒤 재시작" 요구사항과 "메뉴 조회 시점마다 최신 상태 반영" 요구사항이 **한 곳의 호출**로 동시에 만족된다(서브메뉴 각각에 중복 삽입할 필요 없음).
- Phase 5(출고처리)/Phase 6(모니터링) 메뉴가 추가되어도 이 구조는 그대로 유지된다.

## 5. `ProductionMenu`

`include/Console/ProductionMenu.h` / `src/Console/ProductionMenu.cpp`.

```cpp
class ProductionMenu {
public:
    ProductionMenu(ProductionController& controller, ConsoleIO& io);
    void run();

private:
    void handleCurrentStatus();   // currentJob() 표시: 시료/주문 정보, 실생산량, 총생산시간, 경과/남은 시간
    void handleWaitingQueue();    // waitingQueue() FIFO 순서로 표시

    ProductionController& controller_;
    ConsoleIO& io_;
};
```

- `run()`은 진입 시 완료 판정을 다시 하지 않는다(이미 `main.cpp` 메인 루프에서 매번 처리됨 — 중복 호출 금지, 단일 호출 지점 원칙 유지).
- `handleCurrentStatus`에서 "경과 시간"은 `now - startedAt`, "남은 시간"은 `totalProductionTimeMin - 경과분`으로 표시(0 미만이면 이미 완료 대기 상태이므로 정상적으로는 발생하지 않음 — 방어적으로 0으로 clamp).

## 6. 테스트 계획

`tests/ProductionControllerTest.cpp`:
- **정상 완료**: 부족분60/수율0.6 → 실생산량100(ceil), 총생산시간 계산값으로 큐 등록 후, `now`를 완료 시각 이후로 주고 `processCompletions` 호출 → 주문 CONFIRMED, 재고에 40(초과분)만 추가됨(PRD.md §6.2 예시 그대로).
- **아직 미완료**: `now`가 완료 시각 이전이면 아무 변화 없음(주문 여전히 PRODUCING, 큐에 그대로 남음).
- **체인 완료**: 큐에 2건을 순서대로 등록(두 번째는 `startedAt=nullopt`로 대기) → 첫 번째 작업 시작 시각 기준으로 두 작업 총생산시간을 합친 것보다 뒤의 `now`를 주고 1회 `processCompletions` 호출 → 반환값 2, 두 주문 모두 CONFIRMED, 큐가 비어있음.
- **재시작(영속성) 시나리오**: `ProductionQueueRepository`/`SampleRepository`/`OrderRepository`를 블록 스코프로 한 번 생성해 큐에 작업을 등록한 뒤(과거 시각으로 `startedAt` 기록), 인스턴스를 모두 파괴하고 **새 인스턴스로 다시 열어서** `processCompletions(현재시각으로 가정한 미래 now)`를 호출 → 정상적으로 완료 처리됨(PLAN.md Phase 4 완료 기준 (b) 그대로).
- **완료 조건 경계값**: `now == completionTime`이면 완료로 처리되는지(`>=` 조건) 확인.

`tests/ProductionMenuTest.cpp`: istringstream 시나리오로 현재 생산 현황/대기열 조회 출력 형식 확인(빈 큐일 때 "생산 중인 시료 없음"/"대기 중인 생산 작업 없음" 메시지 포함).

## 7. 완료 기준

- `scripts/verify.ps1` PASS
- PLAN.md Phase 4의 (a)(b)(c) 시나리오(정상 흐름/재시작 후 즉시 완료/체인 완료) 전부 테스트로 커버
- 빌드된 exe로 "재고 부족 주문 승인 → 생산라인에서 대기열 확인 → (테스트가 아닌 실제 수동 시연에서는 시간을 앞당길 수 없으므로) 총생산시간이 매우 짧은 더미 시료로 실제 초 단위 대기 후 완료 확인" 1회 시연
