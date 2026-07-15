# Design — Phase 5: 출고처리

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 5, [`../docs/05-출고처리.md`](../docs/05-출고처리.md)
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 테스트 67/67 통과, 수동 시연 확인)

## 1. 목표

`CONFIRMED` 상태 주문에 대해 출고를 실행해 `RELEASE`로 전환하는 기능을 추가한다. 이번 Phase는 범위가 작으므로 새 Controller를 만들지 않고 **`OrderController`를 확장**한다 — 이미 주문 생명주기(접수/승인/거절)를 담당하고 있고, 출고도 같은 Order 애그리게이트의 상태 전이이기 때문(Phase 2 설계 §2의 "실제 오케스트레이션이 필요할 때만 Controller/메서드 추가" 원칙을 그대로 적용).

## 2. `OrderRepository` 확장

Phase 3~4에서 만든 상태 전이 메서드 패턴을 그대로 따른다.

```cpp
RepositoryResult markReleased(int orderId);  // allowed only from CONFIRMED
```

- `include/Repository/OrderRepository.h`에 선언 추가, `.cpp`에 구현 추가. 로직은 `markConfirmed`/`markProducing`/`markRejected`와 동일한 패턴(존재 확인 → 상태 검사 → 전이 → persist).

## 3. `OrderController` 확장

```cpp
std::vector<Order> listConfirmed() const;   // status == CONFIRMED 필터링 (listReserved()와 동일 패턴)
RepositoryResult release(int orderId);      // orderRepo_.markReleased(orderId) 위임
```

- 재고 관련 판단은 전혀 없다 — `docs/05-출고처리.md`가 명시하듯 재고 차감은 이미 승인 시점에 끝났으므로, 출고는 상태만 바꾸는 순수 전이다.

## 4. `ShippingMenu`

`include/Console/ShippingMenu.h` / `src/Console/ShippingMenu.cpp`. Phase 2~4의 메뉴들과 동일한 구조.

```cpp
class ShippingMenu {
public:
    ShippingMenu(OrderController& controller, ConsoleIO& io);
    void run();

private:
    void handleListConfirmed();
    void handleRelease();

    OrderController& controller_;
    ConsoleIO& io_;
};
```

- 메뉴: "1. 출고 대상 주문 목록(CONFIRMED) / 2. 출고 실행 / 0. 뒤로".
- `handleRelease`: 주문 ID 입력 → `controller_.release(orderId)` → `io_.printResult(...)`.

## 5. `main.cpp` 배선

이번 Phase는 아직 메인 메뉴 최종 정리(Phase 6) 이전이므로, 지금까지의 관례대로 **번호를 재배치하지 않고 끝에 추가**한다.

```cpp
io.println("1. 시료관리");
io.println("2. 주문(접수/승인/거절)");
io.println("3. 생산라인");
io.println("4. 출고처리");
io.println("0. 종료");
...
else if (choice == 4) shippingMenu.run();
```

`ShippingMenu`는 Phase 3에서 만든 `OrderController` 인스턴스를 그대로 공유한다(새 Repository/Controller 생성 없음).

## 6. 테스트 계획

`tests/OrderRepositoryTest.cpp`에 케이스 추가: `markReleased`가 `CONFIRMED`에서만 허용되는지(`RESERVED`/`PRODUCING`/`REJECTED`/이미 `RELEASE`된 주문에서 거부), 존재하지 않는 주문 ID 거부.

`tests/OrderControllerTest.cpp`에 케이스 추가:
- 재고 충분 승인(CONFIRMED) → `release()` 성공 → 상태 `RELEASE`.
- `RESERVED`/`PRODUCING` 상태에서 `release()` 시도 시 거부.
- 이미 `RELEASE`된 주문 재출고 시도 거부.
- `listConfirmed()`가 `CONFIRMED` 상태만 정확히 필터링하는지.

`tests/ShippingMenuTest.cpp`: istringstream 시나리오로 목록 조회 → 출고 실행 → 상태 확인, 잘못된 주문 ID 처리, EOF 안전 종료.

## 7. 완료 기준

- `scripts/verify.ps1` PASS
- `docs/05-출고처리.md` 예외처리표 전항목 커버
- 빌드된 exe로 시료 등록 → 주문 접수 → 승인(재고 충분하게 미리 등록) → 출고까지 수동 시연
