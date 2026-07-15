# Design — Phase 2: 시료관리 콘솔 메뉴

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 2, [`../docs/02-시료관리.md`](../docs/02-시료관리.md)
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 테스트 39/39 통과, 수동 시연 확인)

## 1. 목표

Phase 1에서 만든 `SampleRepository`를 콘솔에서 실제로 조작할 수 있게 만든다. 등록/조회/검색 3개 기능과, 모든 이후 Phase가 재사용할 `ConsoleIO` 입출력 계층을 이번에 확정한다.

## 2. 설계 변경 사항 — Controller 계층 도입 시점 조정

Phase 0 설계에서는 `Controller/` 폴더를 미리 예약해뒀지만, 시료관리는 `SampleRepository`가 이미 검증(수율/생산시간)과 CRUD를 전부 수행하므로 **이 Phase에서는 Controller를 만들지 않는다.** `Console/SampleMenu`가 `SampleRepository`를 직접 호출한다.

- 이유: 이 시점의 Controller는 아무 로직 없이 Repository 메서드를 그대로 전달(pass-through)하는 껍데기가 된다. MVC POC의 인사이트("가상의 반복이 아니라 실제 중복일 때 추출")를 따라, 실제 오케스트레이션이 필요해지는 시점(Phase 3의 주문 승인 — 재고 정책 + Repository 2개 조합)에 `OrderController`부터 도입한다.
- 이후 시료관리에도 Controller가 필요해지면(예: 여러 Repository를 조합해야 하는 로직이 생기면) 그때 추출한다.

## 3. ConsoleIO — 모든 메뉴가 공유하는 입출력 계층

`include/Console/ConsoleIO.h` + `src/Console/ConsoleIO.cpp`. 테스트에서 `std::istringstream`/`std::ostringstream`을 주입해 시나리오 테스트를 할 수 있도록 **스트림을 생성자에서 주입받는다**(기본값 `std::cin`/`std::cout`) — Data_Persistence POC는 `std::cin`/`std::cout`을 하드코딩했지만, 우리는 harness 안에서 콘솔 플로우까지 자동 테스트하기 위해 이 부분을 개선한다.

```cpp
class EofException : public std::runtime_error {
public:
    EofException() : std::runtime_error("input stream reached EOF") {}
};

class ConsoleIO {
public:
    explicit ConsoleIO(std::istream& in = std::cin, std::ostream& out = std::cout);

    void println(const std::string& text = "");

    std::string readLine(const std::string& prompt);                 // EOF -> throw EofException
    std::optional<std::string> readOptionalLine(const std::string& prompt); // "" 입력 -> nullopt(변경없음)

    int readInt(const std::string& prompt);                          // 유효할 때까지 재입력, EOF -> throw
    double readDouble(const std::string& prompt);
    std::optional<int> readOptionalInt(const std::string& prompt);    // "" -> nullopt, 잘못된 값 -> 재입력
    std::optional<double> readOptionalDouble(const std::string& prompt);

    void printResult(const RepositoryResult& result);

private:
    std::istream& in_;
    std::ostream& out_;
};
```

- **EOF 처리 방식(POC 대비 개선)**: Data_Persistence POC는 `readLine`이 EOF에서 빈 문자열을 반환하고, 호출부가 매번 `!std::cin`을 확인해야 했다(까먹어서 서브메뉴 무한루프 버그 발생 — [`../poc/02-Data_Persistence.md`](../poc/02-Data_Persistence.md) §5.1). 우리는 `readLine`이 EOF에서 **`EofException`을 던지도록** 하고, 각 메뉴의 `run()` 최상위 루프 단 한 곳에서만 `try { ... } catch (const EofException&) { }`로 잡아 상위 메뉴로 복귀한다. 개별 read 호출마다 확인할 필요가 없어 같은 버그 클래스가 구조적으로 재발하지 않는다.
- **숫자 파싱**: `std::stoi`/`std::stod` + 전체 문자열 소비 검증(`pos == line.size()`)을 공통 내부 템플릿(`detail::parseFull<T>`)으로 처리 — Data_Persistence POC가 나중에 리팩토링으로 도달한 형태를 처음부터 적용한다.
- **재입력 정책**: `readInt`/`readDouble`(필수값)은 파싱 실패 시 "잘못된 입력입니다. 다시 입력해주세요" 후 재프롬프트. `readOptionalInt`/`readOptionalDouble`은 빈 입력이면 즉시 `nullopt`, 비어있지 않은데 파싱 실패면 재프롬프트(빈 입력과 잘못된 입력을 구분).

## 4. SampleMenu

`include/Console/SampleMenu.h` + `src/Console/SampleMenu.cpp`.

```cpp
class SampleMenu {
public:
    SampleMenu(SampleRepository& repo, ConsoleIO& io);
    void run();   // 0(뒤로) 또는 EofException(상위로 처리 위임)까지 반복

private:
    void handleRegister();
    void handleList();
    void handleSearch();

    SampleRepository& repo_;
    ConsoleIO& io_;
};
```

- `handleRegister`: 이름(`readLine`, 빈 값 재입력), 평균생산시간(`readDouble`), 수율(`readDouble`) 입력 → `repo_.create(...)` → `io_.printResult(...)`. 검증(수율 범위, 생산시간 범위)은 Repository가 이미 수행하므로 메뉴는 결과 메시지만 출력한다(중복 검증 없음).
- `handleList`: `repo_.findAll()` → 표 형태(ID/이름/평균생산시간/수율/재고) 출력, 비어있으면 "등록된 시료가 없습니다."
- `handleSearch`: 검색어 입력(`readLine`) → `repo_.search(keyword)` → 목록 또는 "일치하는 시료가 없습니다."
- `run()`: 메뉴("1.등록 2.조회 3.검색 0.뒤로") 출력 → `io_.readInt`로 선택 → 분기. `EofException`은 `run()`이 잡아서 조용히 반환(상위 메인 메뉴 루프로 복귀).

## 5. `main.cpp` 배선 (임시 메인 메뉴)

Phase 6에서 메인 메뉴가 최종 완성되기 전까지, 각 Phase는 그 시점까지 구현된 메뉴만 노출하는 최소 메인 루프를 유지한다.

```cpp
int main() {
    ConsoleIO io;
    SampleRepository sampleRepo(PathUtil::dataDir() / "samples.json");
    SampleMenu sampleMenu(sampleRepo, io);

    while (true) {
        io.println("=== 반도체 시료 생산주문관리 시스템 ===");
        io.println("1. 시료관리");
        io.println("0. 종료");
        try {
            int choice = io.readInt("선택 > ");
            if (choice == 1) sampleMenu.run();
            else if (choice == 0) break;
            else io.println("잘못된 선택입니다.");
        } catch (const EofException&) {
            break;
        }
    }
    return 0;
}
```

## 6. 테스트 계획

`tests/ConsoleIOTest.cpp`: `std::istringstream`/`std::ostringstream` 주입으로 각 read 함수의 정상/재입력/EOF(`EofException` 발생) 경로 검증, trailing garbage(`"12abc"`) 거부 검증(Data_Monitor POC Feedback.md 교훈 재확인).

`tests/SampleMenuTest.cpp`: 스크립트 입력(줄바꿈으로 구분된 문자열)을 `istringstream`에 주입해 전체 시나리오 실행 후 `ostringstream` 출력과 `SampleRepository` 최종 상태를 함께 검증.
- 등록 → 조회에 나타나는지
- 잘못된 수율/생산시간 입력 시 등록 거부 메시지
- 검색(이름 부분일치/ID 완전일치)
- 메뉴 중간에 입력이 끊겨도(EOF) 예외 없이 종료되는지

## 7. 완료 기준

- `scripts/verify.ps1` PASS
- `docs/02-시료관리.md`의 하위 기능/예외처리표 전항목이 `SampleMenuTest`로 커버됨
- 실제로 빌드된 `SampleOrderSystem.exe`를 콘솔에서 수동 실행해 등록→조회→검색 플로우 1회 시연
