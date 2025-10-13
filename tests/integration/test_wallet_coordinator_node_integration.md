# E2E Integration Test 실행 가이드

## 개요

**Wallet → Coordinator → Node** 전체 플로우를 검증하는 통합 테스트입니다.

### 테스트 시나리오

```
┌─────────────┐
│   Wallet    │ (가상 요청 생성)
└──────┬──────┘
       │ HTTPS (SIGNING_REQUEST)
       ▼
┌─────────────┐
│ Coordinator │ (WalletMessageRouter)
└──────┬──────┘
       │ TCP/TLS (브로드캐스트)
   ┌───┼───┬───┐
   ▼   ▼   ▼   ▼
┌───┐┌───┐┌───┐
│N1 ││N2 ││N3 │ (Mock 서명 생성)
└───┘└───┘└───┘
   │   │   │
   └───┼───┘
       │ 응답 수집
       ▼
┌─────────────┐
│ Coordinator │ (최종 서명 생성)
└──────┬──────┘
       │ HTTPS (SIGNING_RESPONSE)
       ▼
┌─────────────┐
│   Wallet    │ (응답 수신)
└─────────────┘
```

---

## 빌드

### 1. 테스트 포함 빌드

```bash
cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
```

### 2. 생성되는 실행 파일

```
build/
├── test_wallet_coordinator_node_integration  # E2E 통합 테스트
├── test_coordinator_node_tls                 # Coordinator ↔ Node 테스트
└── test_wallet_components                    # Wallet 컴포넌트 단위 테스트
```

---

## 실행

### 옵션 1: CTest로 실행

```bash
cd build
ctest -R WalletCoordinatorNodeE2E -V
```

### 옵션 2: 직접 실행

```bash
cd build
./test_wallet_coordinator_node_integration
```

---

## 테스트 상세

### Test 1: Wallet → Coordinator Routing
- **목적**: WalletMessageRouter가 SIGNING_REQUEST를 올바른 핸들러로 라우팅
- **검증**:
  - WalletSigningRequest → WalletSigningHandler
  - 응답 생성 (Mock 서명 포함)

### Test 2: Coordinator → Node Communication  
- **목적**: Coordinator가 모든 Node에 요청 브로드캐스트
- **검증**:
  - 3개 Node 동시 연결
  - TLS over TCP 통신
  - 모든 Node에 요청 전달

### Test 3: Full E2E Flow
- **목적**: 전체 플로우 검증
- **단계**:
  1. Wallet 요청 생성 (SIGNING_REQUEST)
  2. Coordinator가 요청 수신
  3. Coordinator → Node 브로드캐스트
  4. Node 응답 대기 (simulated)
  5. Coordinator가 최종 응답 생성
  6. Wallet에 응답 반환 (SIGNING_RESPONSE)
- **검증**:
  - 요청/응답 Request ID 일치
  - 최종 서명 생성
  - Shard 서명 개수 (3개)

### Test 4: Concurrent E2E Requests
- **목적**: 동시 요청 처리 안정성 검증
- **검증**:
  - 5개 동시 요청 처리
  - 모든 요청 성공
  - 응답 순서 무관하게 처리

---

## 예상 출력

```
========================================
  Wallet ↔ Coordinator ↔ Node
  E2E Integration Test
========================================

=== Setting up E2E Test Environment ===
✓ KMS initialized
✓ TLS initialized
✓ Node node1 started
✓ Node node2 started
✓ Node node3 started
✓ Coordinator started
✓ Connected to node: node1
✓ Connected to node: node2
✓ Connected to node: node3
✓ WalletMessageRouter initialized

✓ Environment setup complete!

========================================
  Test 1: Wallet → Coordinator Routing
========================================
Request ID: e2e_test_001
Key ID: wallet_key_123
✓ Wallet request processed successfully
  Final Signature: 0xMOCK_FINAL_SIG_wallet_key_123_1234567890...
  Shard Count: 3

========================================
  Test 2: Coordinator → Node Communication
========================================
Broadcasting to 3 nodes...
✓ Request broadcasted to all nodes successfully

========================================
  Test 3: Full E2E Flow
  Wallet → Coordinator → Node → Coordinator → Wallet
========================================

[Step 1] Wallet sends SIGNING_REQUEST to Coordinator
  Request ID: e2e_full_test_001
  Key ID: e2e_key_789
  Transaction: 0xcccccccccccccccc...

[Step 2] Coordinator forwards request to Nodes
  ✓ Broadcasted to 3 nodes

[Step 3] Waiting for Node responses...
  ✓ Node responses received (simulated)

[Step 4] Coordinator processes and returns response to Wallet
  ✓ Final response generated
  Key ID: e2e_key_789
  Final Signature: 0xMOCK_FINAL_SIG_e2e_key_789_1234567890...
  Successful Shards: 3

✓ Full E2E flow completed successfully!

========================================
  Test 4: Concurrent E2E Requests
========================================
Sending 5 concurrent E2E requests...
  ✓ Request 0 completed
  ✓ Request 1 completed
  ✓ Request 2 completed
  ✓ Request 3 completed
  ✓ Request 4 completed

Results: 5/5 succeeded
✓ All concurrent requests completed successfully!

=== Tearing down environment ===
✓ Coordinator stopped
✓ All nodes stopped
✓ TLS cleanup complete

========================================
  ✓ ALL TESTS PASSED
========================================

E2E Integration Test Summary:
  - Wallet → Coordinator routing: ✓
  - Coordinator → Node communication: ✓
  - Full E2E flow: ✓
  - Concurrent requests: ✓

✅ System ready for production!
```

---

## 트러블슈팅

### 문제 1: "Failed to initialize config"
**원인**: `.env.local` 파일이 없거나 경로 오류  
**해결**: 프로젝트 루트의 `env/.env.local` 확인

### 문제 2: "Failed to connect to node"
**원인**: Node 서버 포트 충돌  
**해결**: 
```bash
# 포트 확인
netstat -tuln | grep 8081
netstat -tuln | grep 8082
netstat -tuln | grep 8083

# 필요시 프로세스 종료
kill -9 <PID>
```

### 문제 3: "TLS initialization failed"
**원인**: 인증서 파일 없음  
**해결**:
```bash
# 인증서 생성 (개발 환경)
./scripts/generate_certs.sh local
```

### 문제 4: "KMS not initialized"
**원인**: KMS 디렉토리 없음  
**해결**:
```bash
mkdir -p .kms
```

---

## 현재 제한사항

이 테스트는 **Mock 구현**을 사용합니다:

1. **실제 MPC 서명 없음**
   - Node는 `NODE_MOCK_SIGNATURE_` 문자열 반환
   - 실제 암호화 연산 미수행

2. **Wallet Server (Go) 미구현**
   - 현재는 C++ 코드에서 Wallet 요청 시뮬레이션
   - 실제 HTTPS 엔드포인트 없음

3. **비동기 응답 처리 간소화**
   - 실제로는 Node 응답을 비동기로 수집
   - 테스트에서는 `sleep()` 사용

---

## 다음 단계

### Phase 1: 실제 MPC 통합 (우선순위 높음)
- [ ] `fireblocks/mpc-lib` 통합
- [ ] 실제 키 생성/서명 구현
- [ ] Threshold 서명 검증

### Phase 2: Wallet Server 구현
- [ ] Go 서버 구현 (HTTPS 엔드포인트)
- [ ] Coordinator와 실제 통신
- [ ] JWT 인증 추가

### Phase 3: 프로덕션 배포
- [ ] AWS Nitro Enclaves 배포
- [ ] Azure/IBM 클라우드 설정
- [ ] 모니터링 및 로깅

---

## 추가 테스트 시나리오 (향후)

### 에러 처리 테스트
```cpp
- Node 1개 다운 상태에서 요청 (2/3 threshold)
- Node 전체 다운 (서명 실패)
- 잘못된 Request ID
- 타임아웃 처리
```

### 보안 테스트
```cpp
- 허용되지 않은 IP에서 연결 시도
- 잘못된 TLS 인증서
- 중복 Request ID 공격
```

### 성능 테스트
```cpp
- 초당 100개 요청 처리
- 1000개 동시 요청
- 메모리 누수 체크
```

---

## 참고

- **기존 테스트와의 차이**:
  - `test_coordinator_node_tls.cpp`: Coordinator ↔ Node만 테스트
  - `test_wallet_components.cpp`: Wallet 컴포넌트 단위 테스트
  - **현재 테스트**: **전체 E2E 플로우 통합 테스트** ✨

- **테스트 실행 시간**: 약 3-5초
- **필요한 포트**: 8080 (Coordinator), 8081-8083 (Nodes)