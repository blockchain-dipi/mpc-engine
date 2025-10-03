# MPC Engine

Multi-Party Computation 기반 분산 지갑 서명 시스템

---

## 개요

MPC Engine은 2/3 threshold 서명을 통해 안전한 암호화폐 지갑을 구현하는 시스템입니다.

**핵심 기능:**
- 분산 키 관리 (MPC-based)
- Coordinator-Node 아키텍처
- 멀티 클라우드 지원 (AWS, Azure, IBM, Google)
- HTTPS 기반 Wallet 서버 통신

---

## 아키텍처┌─────────────────┐
│  Wallet Server  │ (Go, 확장 가능)
└────────┬────────┘
│ HTTPS
▼
┌─────────────────┐
│  Coordinator    │ (C++)
└────────┬────────┘
│ TCP (보안)
┌────┼────┬────┐
▼    ▼    ▼    ▼
┌───┐┌───┐┌───┐┌───┐
│N1 ││N2 ││N3 ││...│ (각 클라우드)
└───┘└───┘└───┘└───┘

**구성 요소:**
1. **Wallet Server** - 사용자 요청 처리 (Go)
2. **Coordinator** - 서명 오케스트레이션 (C++)
3. **Node** - 개별 샤드 서명 (C++, 독립 보안 환경)

---

## 빌드

### 의존성 설치

**Ubuntu/Debian:**
```bashsudo apt-get update
sudo apt-get install -y 
build-essential 
cmake 
libssl-dev

**macOS:**
```bashbrew install cmake openssl

### 빌드 실행
```bashmkdir build
cd build
cmake ..
make -j$(nproc)

**결과:**build/
├── coordinator  # Coordinator 서버
└── node        # Node 서버

---

## 실행

### 1. Node 서버 실행 (3개 필요)
```bashTerminal 1
./build/node --env local --id node_1Terminal 2
./build/node --env local --id node_2Terminal 3
./build/node --env local --id node_3

### 2. Coordinator 서버 실행
```bashTerminal 4
./build/coordinator local

**확인:**========================================
Coordinator Server Running
Platform: LOCAL
Connected Nodes: 3/3
MPC Threshold: 2/3

---

## 환경 설정

### Local 개발 환경

`env/.env.local`:
```bashCoordinator
COORDINATOR_PLATFORM=LOCAL
COORDINATOR_LOCAL_KMS_PATH=.kmsNodes
NODE_IDS=node_1,node_2,node_3
NODE_HOSTS=127.0.0.1:8081,127.0.0.1:8082,127.0.0.1:8083
NODE_PLATFORMS=LOCAL,LOCAL,LOCALMPC
MPC_THRESHOLD=2
MPC_TOTAL_SHARDS=3Wallet Server
WALLET_SERVER_URL=https://localhost:3000
WALLET_SERVER_AUTH_TOKEN=Bearer wallet_dev_token_12345

### 멀티 클라우드 환경 (예시)
```bashNODE_PLATFORMS=AWS,AZURE,IBM
NODE_AWS_REGION=us-east-1
NODE_AZURE_REGION=eastus
NODE_IBM_REGION=us-south

---

## 보안

### Node 보안

1. **커널 방화벽**: Coordinator만 연결 허용
```bashENABLE_KERNEL_FIREWALL=true
TRUSTED_COORDINATOR_IP=10.0.0.100

2. **KMS 통합**: 클라우드별 키 관리 서비스
   - Local: 파일 기반 암호화
   - AWS: AWS KMS
   - Azure: Azure Key Vault
   - IBM: IBM Secrets Manager

3. **독립 실행 환경**:
   - AWS Nitro Enclaves
   - Azure Confidential Computing
   - IBM Secure Execution

### 통신 보안

- **Coordinator ↔ Node**: TLS over TCP
- **Coordinator ↔ Wallet**: HTTPS (TLS 1.2+)

---

## 테스트
```bash단위 테스트
cd build
cmake .. -DBUILD_TESTS=ON
make
ctest통합 테스트
./build/test_coordinator_node_basic

---

## 디렉토리 구조mpc-engine/
├── src/
│   ├── common/          # 공통 유틸리티
│   │   ├── config/      # 환경 설정
│   │   ├── kms/         # KMS 추상화
│   │   ├── utils/       # 소켓, 스레딩 등
│   │   └── https/       # HTTPS 라이브러리
│   ├── coordinator/     # Coordinator 서버
│   │   ├── network/     # Node/Wallet 통신
│   │   └── handlers/    # 요청 처리
│   ├── node/           # Node 서버
│   │   ├── network/    # TCP 서버
│   │   └── handlers/   # 서명 처리
│   └── protocols/      # 프로토콜 정의
│       ├── coordinator_node/   # 이진 프로토콜
│       └── coordinator_wallet/ # JSON 프로토콜
├── env/                # 환경 설정 파일
├── certs/             # TLS 인증서
├── tests/             # 테스트
└── docs/              # 문서

## 로드맵

- [x] Phase 1-7: TCP 통신, 멀티스레드, 보안
- [x] Phase 8: KMS 통합
- [ ] Phase 9: HTTPS 구현 (진행 중)
- [ ] Phase 10: TLS/mTLS 적용
- [ ] Phase 11: 프로덕션 배포

---

## 라이선스

Proprietary - Volche