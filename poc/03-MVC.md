# POC 학습노트 — MVC (MVC 스켈레톤 코드)

> 원본 위치: `C:/review/MVC` (참고: `CLAUDE.md`, `Design.md`, `PLAN.md`, `PRD.md`, `MIGRATION.md`, `include/`, `src/`, `tests/`)

## 1. 목적과 요구사항

실제 제품이 아니라 "Model/View/Controller 계층을 명확히 분리한 디렉터리 구조와 의존 방향 규칙"이 실제로 컴파일·동작하는지 검증하는 참조 구현. 핵심 요구사항:

1. `include/<Layer>/`, `src/<Layer>/` 형태로 Model/View/Controller를 물리적으로 분리
2. 계층 간 의존 방향 규칙 확립 및 코드로 증명
3. CMake(C++17, 외부 의존성 없음)로 빌드 가능
4. 새 도메인 엔티티 추가 시 그대로 따라 할 수 있는 패턴 확보

비목표: 실제 DB/네트워크, 사용자 입력 파싱, 멀티스레딩, 외부 테스트 프레임워크, GUI (일부는 이후 Phase에서 재검토됨).

## 2. 계층별 책임과 클래스 구조

- **Model**: `Item`(id, name, price), `Order`(id, itemName, quantity)는 순수 데이터 객체(POCO). `ItemRepository`/`OrderRepository`는 인메모리 CRUD(add/remove/findById/all)만 담당하며 **View/Controller를 전혀 모른다**(include 자체가 없음).
- **View**: `ItemView`(콘솔 텍스트), `JsonItemView`(JSON), `OrderView`(콘솔 텍스트)는 `showXxx(const model::T&)`/`showXxxList(...)`/`showMessage(...)` 시그니처만 구현. Model 객체를 매개변수로 받아 읽기만 하고 절대 mutating 메서드를 호출하지 않는다.
- **Controller**: `ItemController<ViewT>`, `OrderController<ViewT>`가 유일하게 Model(`repository_`)과 View(`view_`)를 둘 다 참조하는 계층. `addItem(name, price)` 같은 메서드가 `repository_.add(...)` 호출 후 `view_.showMessage`/`showItem` 호출 순으로 "Model 변경 → View 렌더" 흐름을 캡슐화. 생성자 주입(`ItemController(repository, view)`) 방식.
- **`main.cpp`**는 조립(composition root)만 담당 — `runItemDemo()`/`runOrderDemo()`라는 자유 함수로 엔티티별 데모를 분리해 `main()`이 비즈니스 로직을 갖지 않도록 한다.

## 3. Repository.h의 공통 템플릿 패턴

`Repository<T>`(`include/Model/Repository.h`)는 `add`/`remove`/`findById`/`all`을 제네릭화한 템플릿 클래스다.

- `add(Args&&... args)`는 가변 인자 템플릿으로 받아 `T(nextId_++, args...)`에 그대로 전달(엔티티마다 생성자 시그니처가 달라도 대응).
- `remove`/`findById`는 `T::id()`에만 의존.
- `T`가 갖춰야 할 제약(`int id() const`, `(int id, Args...)` 생성 가능)은 concept 없이 주석으로만 문서화(POC 규모에 과하다고 판단).
- `ItemRepository`/`OrderRepository`는 `using Repository<Item>`/`using Repository<Order>` 별칭일 뿐이며, 원래 있던 `.cpp` 구현 파일은 템플릿이 header-only가 되면서 삭제됨(Phase 6 리팩토링).
- 반면 **Controller는 의도적으로 템플릿화하지 않음** — `addItem`/`addOrder`처럼 메서드명 자체가 도메인 의미를 갖고 본문이 한 줄짜리라 제네릭화의 이득보다 가독성 손실이 크다는 판단(Design.md, MIGRATION.md에 근거 명시).

## 4. View가 여러 구현체로 분리된 이유/방식

Controller가 `template <typename ViewT>`로 View 타입을 매개변수화했기 때문에, `ItemView`(사람이 읽는 텍스트)와 `JsonItemView`(한 줄 JSON)는 서로 무관한 클래스이면서도 동일한 메서드 시그니처(`showItem`/`showItemList`/`showMessage`)만 맞추면 된다. 이는 가상 인터페이스(`IView` 베이스 클래스)가 아니라 **컴파일 타임 구조적 계약**(duck typing 유사)이다 — PLAN.md Phase 2에서 "인터페이스 도입은 YAGNI"라 판단. `main.cpp`는 동일한 `ItemRepository`에 대해 `ItemController<ItemView>`와 `ItemController<JsonItemView>` 두 인스턴스를 만들어, View 교체가 Model/Controller 코드 변경 없이 가능함을 실증한다.

## 5. MIGRATION.md 핵심 내용

Phase 0~5 결정 사항을 실제 프로젝트 이관 관점에서 재정리한 문서.

- **그대로 가져갈 것**: 디렉터리 구조, 의존 방향 규칙, View-템플릿 Controller 패턴, `main.cpp`를 조립 전용으로 유지하는 것.
- **재검토할 것**:
  1. 영속성이 필요해지면 `IItemRepository` 추출 → `InMemoryItemRepository` 개명 → Controller를 `RepoT` 템플릿 또는 런타임 다형성으로 전환 → `FileItemRepository` 등 신규 구현 추가라는 4단계 절차
  2. 자체 테스트 하네스(`tests/testing.h`, 네트워크 의존성 회피 목적)를 CI 환경 확보 시 GoogleTest/Catch2로 교체 검토
  3. CI 도입 시 `verify_build.sh`를 CI 스텝으로 그대로 이관
- 비목표였던 "사용자 입력 처리"는 Controller 앞단에 별도 "입력 어댑터" 계층을 추가할 것을 권장(Controller 책임 비대화 방지).

## 6. 빌드 / 테스트

- CMake 3.15+, C++17, 외부 의존성 없음.
- `CMakeLists.txt`는 `mvc_poc`(실행 파일: main.cpp+Model+View 소스)와 `mvc_poc_tests`(test_main.cpp+각 RepositoryTest.cpp+Model 소스)를 분리 빌드하고 `add_test`로 CTest 등록.
- MSVC는 `/W4 /utf-8`(주석의 비ASCII 문자로 인한 C4819 경고 방지), 그 외는 `-Wall -Wextra -Wpedantic`.
- `CMakePresets.json`은 `default` 프리셋으로 configure/build/test를 `cmake --preset default` 한 줄로 묶음.
- `tests/testing.h`는 자체 초경량 하네스(`TEST`, `EXPECT_EQ`, `EXPECT_TRUE` 매크로, 정적 레지스트라 패턴)로 View/Controller는 테스트 대상에서 제외(콘솔 출력뿐이라 실익이 적다는 판단).
- `scripts/verify_build.sh`는 configure→build→ctest→실행을 한 번에 수행하는 셸 스크립트.

## 7. SampleOrderSystem에 적용할 인사이트

- `Sample`, `Order`(생산주문) 등 도메인 엔티티마다 `Item`/`Order`와 동일하게 **Model/View/Controller 3계층 세트**를 반복 구성하면 된다 — 엔티티 증가 시 파일 추가만으로 확장(엔티티가 5개 이상으로 늘면 별도 DI/조립 헬퍼 도입 재검토 권장, PLAN.md Phase 3 기준).
- CRUD가 동일하면 `Repository<T>` 템플릿을 그대로 재사용하되, 시료/주문 조회에 **상태 필터링·재고 계산 등 CRUD 이상의 로직**이 필요해지는 순간 해당 엔티티만 별도 Repository로 분리(Design.md의 "가상의 반복이 아니라 실제 중복일 때 추출" 원칙).
- 콘솔 메뉴가 여러 개(시료 등록/조회, 주문 접수/승인/거절, 출고, 생산라인 등)라면 각 메뉴 동작을 Controller의 개별 메서드(`registerSample`, `createOrder`, `approveOrder` 등)로 매핑하고, 메뉴 루프는 입력을 파싱해 Controller 메서드 호출로 변환하는 **"입력 어댑터" 역할만** 하도록 설계(MIGRATION.md 권장 사항).
- 향후 파일 영속성이 필요해지면 MIGRATION.md의 4단계 절차(`ISampleRepository` 추출 → `InMemorySampleRepository` 개명 → Controller 시그니처 전환 → `FileSampleRepository` 추가)를 그대로 적용할 수 있다. 실제로는 [`02-Data_Persistence.md`](02-Data_Persistence.md)의 Repository 설계와 결합해서 처음부터 파일 기반 Repository로 구현하는 편이 더 실용적이다.
- 콘솔 출력 외에 로그/리포트 출력이 필요하면 `JsonItemView` 패턴처럼 동일 시그니처의 새 View 클래스만 추가하면 Model/Controller 변경 없이 확장 가능.
- 이 POC도 CMake 기반이지만, `SampleOrderSystem`은 **MSBuild/Visual Studio(vcxproj)만 사용**하므로 빌드 설정은 그대로 가져오지 않고 계층 구조/클래스 설계만 참고한다.
