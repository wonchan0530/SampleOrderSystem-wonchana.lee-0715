# Design — Phase 8: 통합 테스트 / 회귀 시나리오 정리

> 관련 계획: [`../PLAN.md`](../PLAN.md) Phase 8
> 상태: **승인됨 — 구현 완료** (Harness 4개 구성 빌드 + 유닛테스트 87/87 + E2E 시나리오 통과)

> **구현 중 발견한 실제 버그**: `ConsoleIO::readLine`이 CRLF로 끝나는 입력(리다이렉션된 파일/파이프)에서 `\r`을 제거하지 않아 숫자 파싱이 항상 실패하는 문제를 E2E 스크립트 작성 중 발견해 수정함(§6). 이 외에 스크립트 자체의 PowerShell 관련 문제(파이프 개행 손상, `Set-Content -Encoding UTF8`의 BOM 삽입, 단일 객체 `.Count` 이슈)도 함께 해결함.

## 1. 목표

Phase 0~7에서 각 클래스 단위(Repository/Controller/Menu)로 쌓은 269개 테스트(Phase 7까지 85개... 실제로는 누적 85개 — 아래 §2에서 정확한 수치와 커버리지를 표로 정리)는 전부 **개별 컴포넌트를 in-process로 직접 호출**하는 테스트다. 지금까지 한 번도 검증하지 않은 것은 "실제로 빌드된 `SampleOrderSystem.exe`를 콘솔 입력만으로 처음부터 끝까지(시료 등록 → 주문 → 생산 → 출고 → 모니터링, 재시작 포함) 조작했을 때 전체가 맞물려 동작하는가"이다. Phase 8은 이 진짜 End-to-End 시나리오를 자동화하고, PRD 핵심 요구사항에 대한 테스트 커버리지를 표로 감사(audit)한다.

## 2. 커버리지 감사 결과 (신규 테스트 없이, 기존 테스트 존재 여부만 확인)

| PRD 요구사항 | 커버하는 기존 테스트 |
|---|---|
| 상태 전이 RESERVED→CONFIRMED(재고충분) | `OrderControllerTest::orderController_approveConfirmsImmediatelyWhenStockSufficient` |
| 상태 전이 RESERVED→PRODUCING→CONFIRMED | `ProductionControllerTest::productionController_completesJobAndAppliesSurplusToStock` |
| 상태 전이 RESERVED→REJECTED | `OrderControllerTest::orderController_rejectTransitionsToRejectedAndBlocksFurtherApproval` |
| 상태 전이 CONFIRMED→RELEASE | `OrderControllerTest::orderController_releaseTransitionsConfirmedOrderToRelease` |
| 재고 승인 시점 1회 차감 + 동일 시료 후속 주문 격리 (PRD §6.1) | `OrderControllerTest::orderController_subsequentOrderForSameSampleDoesNotOverlapEarlierShare` |
| 실생산량 `ceil(부족분/수율)` + 초과분 재고 반영 (PRD §6.2) | `OrderControllerTest::orderController_approveEnqueuesShortfallWhenStockInsufficient`, `ProductionControllerTest::productionController_completesJobAndAppliesSurplusToStock` |
| 벽시계 완료 판정 + 체인 완료 (PRD §6.3) | `ProductionControllerTest::productionController_chainCompletesMultipleJobsInOneCall` |
| 재시작 후 경과시간 반영 (PRD §6.3) | `ProductionControllerTest::productionController_restartScenarioCatchesUpElapsedTime` |
| 모니터링 REJECTED 제외 집계 (docs/04) | `MonitoringControllerTest::monitoringController_countOrdersByStatusExcludesRejected` |
| 재고 여유/부족/고갈 3단계 (docs/04) | `MonitoringControllerTest::monitoringController_stockLevel*` 3종 |
| 참조 무결성(미등록 시료 주문 거부) | `OrderRepositoryTest::orderRepository_createRejectsUnknownSampleId` |
| EOF 안전 종료 | 모든 `*MenuTest.cpp`의 `*_eofDuringMenuReturnsWithoutThrowing` |

**결론: 위 핵심 요구사항은 이미 전부 유닛/통합 테스트로 커버되어 있다.** Phase 8에서 추가로 필요한 것은 신규 커버리지가 아니라, 이 부품들이 **실제 조립된 실행 파일 안에서도** 맞물려 동작하는지 확인하는 진짜 E2E 시나리오다.

## 3. `scripts/scenario_test.ps1` — 신규 E2E 스크립트

Data_Monitor POC의 `tests/scenario_test.sh` 패턴(빌드된 실행 파일에 heredoc으로 입력 주입 → 최종 데이터 파일 내용 확인)을 PowerShell로 재현한다.

```powershell
# scripts/scenario_test.ps1 개요
# 1. data/ 폴더가 이미 있으면 data.scenario_backup/ 으로 잠시 옮겨 보호한다.
# 2. SampleOrderSystem.exe에 아래 시나리오를 표준입력으로 주입:
#    - 시료 2개 등록 (하나는 평균생산시간을 아주 짧게(예: 0.02분=1.2초) 설정 -> 실제 생산 대기 가능)
#    - 시료 A 주문 접수 -> 승인(재고0 이므로 PRODUCING) -> 프로그램 종료
#    - (스크립트에서 실제로 몇 초 Start-Sleep)
#    - 프로그램 재실행 -> 생산 큐가 재시작 시점에 자동 완료되는지 모니터링으로 확인
#    - 시료 B 주문 접수 -> 거절
#    - 위에서 CONFIRMED된 주문을 출고 처리
#    - 모니터링에서 최종 상태 확인 후 종료
# 3. 표준출력에서 기대 문자열(등록/승인/CONFIRMED/RELEASE/여유·부족·고갈 등)이 나타나는지 확인
# 4. data/samples.json, data/orders.json을 직접 파싱해 최종 상태(재고 수치, 주문 상태)를 검증
# 5. data/ 를 정리하고, 1번에서 백업해둔 폴더가 있었다면 원래대로 복원
# 6. 실패 시 어떤 단계에서 기대 문자열을 찾지 못했는지 출력하고 0이 아닌 코드로 종료
```

- 이 스크립트는 **빌드를 직접 수행하지 않는다** — `scripts/verify.ps1`가 이미 빌드한 `x64\Debug\SampleOrderSystem.exe`를 사용한다고 가정한다(존재하지 않으면 에러 메시지와 함께 종료).
- 시료 A의 평균생산시간을 짧게 잡아 **실제로 몇 초 기다렸다가 재시작하는 진짜 벽시계 검증**을 이 스크립트에서 1회 수행한다 — 유닛 테스트(`ProductionControllerTest`)는 `now`를 주입해 즉시 검증하지만, E2E에서는 실제 프로세스 재시작 + 실제 시간 경과를 검증해 "가짜 시계가 아니라 진짜로 동작한다"는 것까지 증명한다.

## 4. `scripts/verify.ps1`에 통합

Phase 8부터는 Harness(`scripts/verify.ps1`)가 "빌드 4개 구성 + 유닛 테스트"에 더해 **"E2E 시나리오 1회"**까지 수행하도록 확장한다.

```powershell
# verify.ps1 마지막에 추가
Write-Host "`n=== Running end-to-end scenario ===" -ForegroundColor Cyan
& "$PSScriptRoot\scenario_test.ps1"
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n=== HARNESS RESULT: FAIL (scenario test failed) ===" -ForegroundColor Red
    exit 1
}
```

- 이후 모든 Phase(이미 지나간 Phase 0~7 포함, 앞으로도)의 "Harness 실행" 단계는 이 확장된 `verify.ps1` 한 번으로 빌드+유닛테스트+E2E 시나리오를 전부 검증하게 된다.

## 6. 구현 중 발견/수정한 이슈

E2E 스크립트를 실제로 동작시키는 과정에서 설계에는 없던 문제 4가지를 발견해 수정했다.

1. **`ConsoleIO::readLine`의 CRLF 미처리 (실제 제품 버그)**: `std::getline`은 `\n`만 구분자로 보므로, 입력이 CRLF로 끝나면(리다이렉션된 파일/파이프 입력에서 흔함) 줄 끝에 `\r`이 남는다. 우리 숫자 파싱은 "전체 문자열이 정확히 숫자여야 함"을 요구하므로 `"1\r"`을 유효하지 않은 입력으로 거부해 항상 재입력을 요구하는 버그가 있었다. `readLine`에서 trailing `\r`을 제거하도록 수정하고 회귀 테스트 2개를 추가했다.
2. **PowerShell → 네이티브 프로세스 파이프의 신뢰성 문제**: `Get-Content | & $exe` 방식으로 여러 줄 입력을 흘려보내면 줄 내용이 깨지는 현상을 발견(순수 ASCII, BOM 없음에도 발생). 원인을 더 파고들기보다, Windows에서 콘솔 앱에 표준입력을 주입하는 표준적인 방법인 **cmd.exe의 `<` 파일 리다이렉션**으로 전환해 해결했다.
3. **`Set-Content -Encoding UTF8`의 BOM 삽입**: Windows PowerShell 5.1의 `UTF8` 인코딩은 파일 맨 앞에 BOM(`EF BB BF`)을 붙인다. 이 BOM이 프로그램이 읽는 첫 줄 앞에 섞여 들어가 첫 정수 파싱을 깨뜨렸다. 시나리오 입력은 순수 ASCII이므로 `Encoding ASCII`로 전환해 BOM 자체를 없앴다.
4. **PowerShell 단일 객체의 `.Count` 부재**: `Where-Object` 결과가 정확히 1건이면 PowerShell이 배열이 아닌 단일 객체를 반환해 `.Count`가 기대와 다르게 동작한다. `@(...)`로 배열 컨텍스트를 강제해 해결했다.

## 5. 완료 기준

- `scripts/verify.ps1` 전체(빌드 4/4 + 유닛테스트 85/85 + 시나리오 1/1) PASS
- §2 커버리지 표가 실제 테스트 코드와 일치함(임의로 작성한 표가 아니라 실행해서 확인)
- 시나리오 스크립트가 실패할 경우 어느 단계인지 즉시 알 수 있는 메시지 출력
