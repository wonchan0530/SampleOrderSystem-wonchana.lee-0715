# POC 학습노트 — Dummy_Data_Generator (Dummy 데이터 생성 Tool)

> 원본 위치: `C:/review/Dummy_Data_Generator` (참고: 루트 문서, `CMakeLists.txt`, `src/`, `tests/`)

## 1. 목적과 요구사항

C++17/CMake로 만든 커맨드라인 도구. JSON 설정 파일에 정의된 필드 스키마 규칙에 따라 랜덤 더미 데이터를 생성해 **JSON 파일로 저장**한다. 실제 DB(PostgreSQL/MySQL 등) 연결은 명시적으로 범위 제외(PRD) — JSON이 유일한 영속화/교환 포맷이며 외부 인프라 의존 없이 로컬 단독 실행된다.

핵심 요구사항:
1. 필드별 타입/범위를 설정으로 정의
2. 건수(`count`) 지정 생성
3. `seed` 고정 시 재현 가능성 보장
4. JSON 저장/재로드
5. 설정 오류 시 명확한 에러 메시지
6. CLI 단일 실행

## 2. 전체 구조 (모듈 역할)

- **`src/Config.{hpp,cpp}`**: 설정 JSON을 `ddg::Config`(output 경로, `count`, optional `seed`, `FieldSpec` 목록)로 파싱·검증. `FieldType` enum과 이름 매핑(`kFieldTypesByName`)이 단일 진실 소스이며, 새 타입 추가 시 여기·`parseField`·Generator의 `switch` 세 곳만 고치면 된다. 로드 시점에 min<=max, length>=0, enum values 비어있지 않음, 날짜 유효성(파싱해서 캐시)까지 최대한 검증하고 실패 시 필드 인덱스/이름을 포함한 `std::runtime_error`를 던진다.
- **`src/DateUtil.{hpp,cpp}`**: Howard Hinnant 알고리즘 기반 civil-date ↔ day-count 변환. ISO 날짜 파싱/포맷 담당, Config(검증)와 Generator(샘플링) 둘 다 공용으로 사용.
- **`src/Generator.{hpp,cpp}`**: `ddg::DataGenerator`가 `std::mt19937` RNG를 들고 `FieldSpec`을 랜덤 JSON 값으로 변환(`generateField`의 exhaustive switch, default 없어 컴파일러가 누락 케이스를 감지), 레코드/데이터셋 조립.
- **`src/main.cpp`**: CLI 엔트리포인트: config 로드 → seed 결정(명시 seed 또는 `std::random_device`) → 데이터셋 생성 → 출력 디렉토리 자동 생성 → JSON 저장 → 재로드 검증(`verifyDataset`) → 결과 요약 출력. 종료코드: 0 성공, 1 CLI 사용법 오류, 2 config/생성/검증 실패.

## 3. 더미 데이터 생성 방식

지원 타입 7종(`int, float, bool, string, enum, date, object`), 필드별 세부 규칙을 config에서 지정:

| 타입 | 규칙 |
|---|---|
| int | `min`/`max` 정수 범위, `std::uniform_int_distribution` |
| float | `min`/`max` 실수 범위, `std::uniform_real_distribution` |
| bool | `bernoulli_distribution(0.5)` 고정 |
| string | `length` 만큼 영숫자(A-Z, a-z, 0-9) 랜덤 조합 |
| enum | `values` 배열에서 균등 랜덤 선택 |
| date | `minDate`/`maxDate`(YYYY-MM-DD, inclusive) 범위 내 랜덤 ISO 날짜, day-count로 변환 후 균등 샘플링(min/max는 load 시 1회만 파싱해 캐시) |
| object | 중첩 `fields` 배열을 재귀적으로 갖는 하위 스키마 → nested JSON object 생성 |

`count`로 생성 레코드 수 조절, `seed`(optional)로 재현성 제어(동일 seed → 동일 결과, 미지정 시 `random_device`로 매번 다름), `output`으로 저장 경로 지정. 예시(`config/sample_config.json`): 20건, seed 42, id/username/score/active/role/signupDate/address(중첩) 필드 조합.

## 4. 저장소에 데이터 추가하는 방식

- DB 연결은 없음 — 생성된 전체 데이터셋을 JSON 배열(`nlohmann::json::dump(2)`, pretty-print)로 통째로 파일에 write.
- 상위 디렉토리 없으면 `std::filesystem::create_directories`로 자동 생성.
- **기존 데이터와의 병합/append 개념은 없음** — 매 실행마다 새로 생성해 지정 경로에 덮어쓰기.
- 저장 직후 같은 파일을 다시 읽어(`verifyDataset`) 레코드 수와 필드 존재 여부를 재검증.
- "기존 데이터 포맷과의 호환"은 설계상 고려되지 않음(연동 대상 스키마가 없는 순수 생성 도구).

## 5. 테스트 구조

- **`test_config.cpp`**: `Config::loadFromFile`의 정상 파싱(7개 타입 포함) 및 각 검증 실패 케이스(음수 count, 미지원 타입, min>max, 음수 length, 빈 enum values, 잘못된 날짜, minDate>maxDate, object의 fields 누락/빈 배열, 중첩 필드 오류, 필수 키 누락)를 개별 체크.
- **`test_generator.cpp`**: 동일 seed로 두 번 생성한 데이터셋이 동일한지(재현성), 각 타입별 값이 지정 범위/규칙 내에 있는지(int/float 범위, string 길이, enum 소속, date 범위, 중첩 object) 검증.
- **`test_e2e.cpp`**: 실제 빌드된 바이너리를 `std::system`으로 직접 실행해 정상 config(exit 0, 파일 생성, 레코드 수/필드 확인) 및 비정상 케이스(인자 없음→1, 존재하지 않는 config→2) 종료 코드까지 검증. Windows에서 cmd.exe 인용부호 처리 이슈 등 실제 실행 환경 특유의 버그를 잡아낸 이력이 있다.
- **`TestUtil.hpp`**: 프레임워크 없이 `CHECK`/`CHECK_THROWS` 매크로 + 임시파일 헬퍼로 자체 구현.

## 6. 빌드 시스템 (CMake)

- CMake 3.15+, C++17. `FetchContent`로 `nlohmann/json` v3.11.3을 네트워크에서 받아옴(오프라인 빌드 미지원, PRD의 "미결정 사항"으로 명시).
- 실행파일 `dummy_data_generator`(main+Config+Generator+DateUtil)와 테스트 실행파일 3개(`test_config`, `test_generator`, `test_e2e`)를 별도 타겟으로 빌드. `enable_testing()` + `add_test`로 CTest 등록, `test_e2e`는 `$<TARGET_FILE:dummy_data_generator>`를 인자로 받아 실제 바이너리 경로를 전달.

## 7. SampleOrderSystem에 적용할 인사이트

- **Config 기반 스키마 정의 + validation-at-load 패턴**을 그대로 응용 가능: 시료(Sample)/주문(Order) 엔티티별 필드 스펙(평균생산시간 범위, 수율 범위, 주문 상태 enum, 접수일자 range 등)을 JSON으로 선언하고 로드 시점에 엄격 검증하는 구조는 테스트 데이터 생성기 설계의 좋은 출발점.
- **enum 타입**은 주문 상태(`RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE`) 생성에 바로 매핑 가능.
- **date 타입과 DateUtil**은 주문 접수일시 등 날짜 필드가 필요해지면 재사용 가능(civil-date 변환 로직은 검증된 채로 그대로 가져다 쓸 수 있음).
- **object(중첩) 타입**은 필요 시 응용 가능하나, PRD가 명시하듯 **스키마 간 참조 관계(FK, cross-schema relationship)는 이 POC의 범위 밖**이다. 우리 시스템은 주문이 반드시 등록된 시료 ID를 참조해야 하므로(참조 무결성), 더미 데이터 생성기를 만들 때는 **시료를 먼저 생성한 뒤, 그 시료 ID 목록 중에서 주문의 sampleId를 선택**하도록 생성 순서/로직을 직접 설계해야 한다 — 이 부분은 이 POC에 없는 내용이므로 새로 만들어야 함.
- **seed 기반 재현성**은 반도체 공정처럼 추적성이 중요한 도메인의 테스트에서 "동일 조건 재현" 요구에 유용.
- **저장 후 재로드 검증(`verifyDataset`) 패턴**은 우리 시스템의 파일 영속성 계층에 더미 데이터를 넣을 때도 "생성 → 저장 → 재조회 검증" 흐름으로 그대로 재사용 가능.
- 이 POC는 CMake + `nlohmann/json`(FetchContent) 기반이지만, `SampleOrderSystem`은 **MSBuild/Visual Studio(vcxproj)만 사용**하고 네트워크 의존 없는 빌드를 지향해야 하므로, Config/Generator/DateUtil의 "필드 단위 값 생성 로직"만 참고하고 빌드 설정과 JSON 라이브러리 도입 여부는 별도로 결정해야 한다.
