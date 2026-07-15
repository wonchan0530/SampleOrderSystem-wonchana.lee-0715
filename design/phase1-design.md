# Design — Phase 1: MVC 스켈레톤 + 데이터 영속성 계층

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 1
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 테스트 26/26 통과 확인)

## 1. 목표

시료(Sample)/주문(Order) 두 엔티티에 대해 Model → Storage → Repository 계층을 구축하고, Repository 단위 테스트로 CRUD·검증·참조 무결성·영속성을 확인한다. 콘솔 메뉴 연결은 Phase 2부터이며, 이번 Phase는 도메인/저장 계층만 완성한다 (PLAN.md Phase 1 범위).

Phase 0에서 확정한 정책을 그대로 따른다: nlohmann/json vendored 사용([`phase0-design.md`](phase0-design.md) §3), 실행파일 경로 기준 데이터 폴더([`phase0-design.md`](phase0-design.md) §4), Controller는 엔티티별 구체 클래스로 시작(제네릭 템플릿 미도입).

## 2. Model

`include/Model/Sample.h`, `include/Model/Order.h` — Data_Persistence POC 필드셋을 그대로 채택([`../poc/02-Data_Persistence.md`](../poc/02-Data_Persistence.md) §2).

```cpp
// Sample.h
struct Sample {
    int id;
    std::string name;
    double avgProductionTimeMin;
    double yieldRate;   // (0, 1]
    int stock;          // 등록 시 0, 이후 승인/생산 완료 시점에만 변경
};
void to_json(nlohmann::json& j, const Sample& s);
void from_json(const nlohmann::json& j, Sample& s);
```

```cpp
// Order.h
enum class OrderStatus { RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE };

std::string toString(OrderStatus status);
OrderStatus orderStatusFromString(const std::string& s);

struct Order {
    int orderId;
    int sampleId;
    std::string customerName;
    int quantity;
    OrderStatus status = OrderStatus::RESERVED;
};
void to_json(nlohmann::json& j, const Order& o);
void from_json(const nlohmann::json& j, Order& o);
```

- `OrderStatus`는 PRD.md §4.2의 5개 상태를 그대로 enum class로 표현한다(문자열 대신 타입 안전성 확보). JSON 저장 시에는 문자열(`"RESERVED"` 등)로 직렬화해 사람이 읽을 수 있는 포맷을 유지한다(Data_Persistence POC와 동일).
- 이번 Phase에서는 `Sample`/`Order` 구조체와 (역)직렬화만 만든다. 상태 전이 로직(승인/거절/생산완료)은 Phase 3~4에서 Repository/Controller에 구현한다.

## 3. Storage — `JsonFileStore<T>`

`include/Storage/JsonFileStore.h` (헤더 전용 템플릿) — Data_Persistence POC 설계를 그대로 채택([`../poc/02-Data_Persistence.md`](../poc/02-Data_Persistence.md) §2, §4).

```cpp
template <typename T>
class JsonFileStore {
public:
    explicit JsonFileStore(std::filesystem::path filePath);

    std::vector<T> load() const;              // 파일 없음/빈 파일 → {}, 파싱 실패 → 경고 후 {}
    void save(const std::vector<T>& items) const;  // 상위 디렉터리 자동 생성, dump(2)로 pretty-print
private:
    std::filesystem::path filePath_;
};
```

- 도메인을 모르는 범용 계층 — `T`는 `to_json`/`from_json`만 갖추면 된다.
- 예외는 이 계층 안에서 흡수한다(파싱 실패가 상위로 전파되지 않음, NFR — PRD.md §7 데이터 영속성 요건과 연결).

`include/Storage/PathUtil.h`도 이번 Phase에 함께 만든다(Phase 0 §4 정책의 실제 구현):

```cpp
namespace PathUtil {
    std::filesystem::path executableDir();   // GetModuleFileNameW 기반
    std::filesystem::path dataDir();         // executableDir() / "data"
}
```

- `main.cpp`에서의 실제 배선(레포지토리에 이 경로를 넘겨 메뉴와 연결)은 Phase 2에서 진행한다. 이번 Phase에서는 `PathUtil` 자체의 동작만 테스트로 확인한다.

## 4. Repository

`include/Repository/RepositoryResult.h`:
```cpp
struct RepositoryResult {
    bool success;
    std::string message;
};
```

`include/Repository/SampleRepository.h`:
```cpp
struct SampleUpdate {
    std::optional<std::string> name;
    std::optional<double> avgProductionTimeMin;
    std::optional<double> yieldRate;
    // stock은 사용자가 직접 수정하지 않음 — 승인/생산 완료 로직(Phase 3~4)에서만 변경
};

class SampleRepository {
public:
    explicit SampleRepository(std::filesystem::path dataFile);

    std::vector<Sample> findAll() const;
    std::optional<Sample> findById(int id) const;
    std::vector<Sample> search(const std::string& keyword) const;   // id 완전일치 또는 name 부분일치

    RepositoryResult create(const std::string& name, double avgProductionTimeMin, double yieldRate);
    RepositoryResult update(int id, const SampleUpdate& patch);
    RepositoryResult remove(int id);

    // Phase 3~4에서 재고 차감/가산에 사용할 내부 훅. Repository 스스로 재고 정책을 판단하지 않고,
    // 호출자(Controller)가 계산한 델타만 반영한다 — Data_Monitor POC의 "참조 무결성은 상위 계층에서
    // 처리" 원칙과 동일하게, "재고 정책 판단은 상위 계층, Repository는 반영만" 원칙을 적용.
    RepositoryResult adjustStock(int id, int delta);

private:
    JsonFileStore<Sample> store_;
    std::vector<Sample> cache_;
    int nextId_ = 1;
    static RepositoryResult validateFields(double avgProductionTimeMin, double yieldRate);
};
```

- ID는 로드 시 기존 최대값+1로 자동 채번(Data_Monitor/Data_Persistence 공통 패턴).
- 검증: `avgProductionTimeMin > 0`, `0 < yieldRate <= 1`(PRD.md §5.2 예외처리표) — `create`/`update` 공통 `validateFields()`로 통합.
- `adjustStock`은 이번 Phase에 만들어두지만 실제 호출자는 아직 없다(Phase 3의 주문 승인, Phase 4의 생산 완료에서 사용 예정). 음수 델타로 재고가 음수가 되는 경우는 거부.

`include/Repository/OrderRepository.h`:
```cpp
struct OrderUpdate {
    std::optional<std::string> customerName;
    std::optional<int> quantity;
    // status는 이 patch로 바꾸지 않음 — 상태 전이는 Phase 3~5에서 전용 메서드(approve/reject/...)로 처리
};

class OrderRepository {
public:
    OrderRepository(std::filesystem::path dataFile, const SampleRepository& sampleRepository);

    std::vector<Order> findAll() const;
    std::optional<Order> findById(int orderId) const;
    std::vector<Order> findBySampleId(int sampleId) const;

    RepositoryResult create(int sampleId, const std::string& customerName, int quantity);
    RepositoryResult update(int orderId, const OrderUpdate& patch);   // RESERVED 상태에서만 허용
    RepositoryResult remove(int orderId);   // 관리용, PRD에 명시된 사용자 기능은 아님 — 테스트 편의 목적

private:
    JsonFileStore<Order> store_;
    std::vector<Order> cache_;
    int nextId_ = 1;
    const SampleRepository& sampleRepository_;   // 단방향 참조 — 참조 무결성 검사 전용
};
```

- `create()`는 `sampleRepository_.findById(sampleId)`로 참조 무결성을 검사한다(PRD.md §5.3, §7). 존재하지 않으면 실패 반환.
- **상태 전이 메서드(approve/reject/생산완료 반영 등)는 이번 Phase에 만들지 않는다** — PRD.md §6의 정책(승인 시점 1회 재고 차감, 벽시계 기준 생산완료)은 Phase 3~4에서 Controller 계층에 구현하고, Repository는 상태값 자체를 그대로 두거나(Order) 재고를 반영(Sample.adjustStock)하는 저수준 연산만 제공한다. Repository에 상태 전이 규칙을 넣으면 Phase 3~4 설계와 충돌할 수 있어 지금은 최소 범위로 제한한다.

## 5. 테스트 계획 (`SampleOrderSystemTests`)

새 테스트 파일: `tests/SampleRepositoryTest.cpp`, `tests/OrderRepositoryTest.cpp`, `tests/JsonFileStoreTest.cpp`, `tests/PathUtilTest.cpp`. 각 테스트는 `test_data/<파일명>` 등 전용 임시 파일을 사용하고 테스트 종료 후 정리한다(Data_Persistence POC 패턴).

- **JsonFileStore**: 파일 없음→빈 목록, 빈 파일→빈 목록, 손상된 JSON→경고 후 빈 목록(예외 비전파), 상위 디렉터리 자동 생성, save 후 다시 load하면 동일 데이터.
- **PathUtil**: `executableDir()`가 존재하는 디렉터리를 반환하는지, `dataDir()`가 `executableDir()/data`인지.
- **SampleRepository**: 생성/조회/검색(이름 부분일치, id 완전일치)/수정(부분 패치)/삭제, 검증 실패 케이스(수율 0/음수/1초과, 평균생산시간 0/음수), `adjustStock`(정상 증가/감소, 음수 재고가 되는 감소는 거부), 재시작 시나리오(인스턴스를 새로 만들어도 이전에 저장한 데이터가 그대로 보이는지 — 영속성 확인).
- **OrderRepository**: 생성(정상/미등록 시료ID 거부/수량<=0 거부), 조회, `findBySampleId`, 수정(RESERVED 상태에서만 허용), 참조 무결성(존재하지 않는 시료 참조 거부) — 실제 `SampleRepository` 인스턴스를 함께 구성해 통합적으로 검증.

## 6. 이번 Phase에서 하지 않는 것 (범위 제외)

- 콘솔 메뉴/Controller/View 실제 구현 (Phase 2부터)
- 주문 상태 전이(승인/거절/생산완료), 생산 큐 (Phase 3~4)
- `main.cpp`를 레포지토리와 배선하는 작업 (Phase 2)

## 7. 완료 기준 (Definition of Done)

- `scripts/verify.ps1` 실행 시 4개 구성 빌드 + 위 신규 테스트 전부 통과로 "PASS"
- PRD.md §5.2(시료관리 예외처리), §5.3/§7(주문 참조 무결성) 요구사항이 테스트로 커버됨
