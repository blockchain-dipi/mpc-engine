<!-- README.md -->
# MPC Wallet System Architecture

## 시스템 개요

2/3 Threshold MPC 기반 분산 지갑 서명 시스템

---

## 전체 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│                     User Layer                              │
└─────────────────────────────────────────────────────────────┘
                              │
                    HTTPS (TLS 1.3, mTLS)
                              │
┌─────────────────────────────▼─────────────────────────────┐
│                  Wallet Server Layer                       │
│                  (Go, Auto-Scaling)                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ Wallet 1 │  │ Wallet 2 │  │ Wallet 3 │  │ Wallet N │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
│       │             │             │             │          │
└───────┼─────────────┼─────────────┼─────────────┼──────────┘
        │             │             │             │
        └─────────────┴─────────────┴─────────────┘
                      │
            HTTPS (Proto, Keep-Alive)
                      │
┌─────────────────────▼─────────────────────────────────────┐
│              MPC Engine Server Set (N개)                   │
│         (각 Set = Coordinator + Node Group)                │
├───────────────────────────────────────────────────────────┤
│                                                            │
│  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓  │
│  ┃            MPC Engine Set 1                        ┃  │
│  ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫  │
│  ┃  ┌─────────────────────────────────────────┐       ┃  │
│  ┃  │      Coordinator Server (C++)           │       ┃  │
│  ┃  │  - HTTPS Listener (boost.beast)         │       ┃  │
│  ┃  │  - WalletMessageRouter                  │       ┃  │
│  ┃  │  - ThreadPool (16 workers)              │       ┃  │
│  ┃  │  - NodeTcpClient Manager                │       ┃  │
│  ┃  └──────┬────────────┬────────────┬─────────┘       ┃  │
│  ┃         │            │            │                 ┃  │
│  ┃    TCP/TLS      TCP/TLS      TCP/TLS               ┃  │
│  ┃   (mTLS)       (mTLS)       (mTLS)                 ┃  │
│  ┃         │            │            │                 ┃  │
│  ┃  ┌──────▼──────┐ ┌──▼──────┐ ┌──▼──────┐          ┃  │
│  ┃  │   Node 1    │ │ Node 2  │ │ Node 3  │          ┃  │
│  ┃  │   (C++)     │ │ (C++)   │ │ (C++)   │          ┃  │
│  ┃  │             │ │         │ │         │          ┃  │
│  ┃  │ AWS Nitro   │ │ Azure   │ │  IBM    │          ┃  │
│  ┃  │  Enclave    │ │  CC     │ │  SE     │          ┃  │
│  ┃  │             │ │         │ │         │          ┃  │
│  ┃  │  Shard 0    │ │ Shard 1 │ │ Shard 2 │          ┃  │
│  ┃  └─────────────┘ └─────────┘ └─────────┘          ┃  │
│  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛  │
│                                                            │
│  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓  │
│  ┃            MPC Engine Set 2                        ┃  │
│  ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫  │
│  ┃  [Coordinator + 3 Nodes]                           ┃  │
│  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛  │
│                                                            │
│  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓  │
│  ┃            MPC Engine Set N                        ┃  │
│  ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫  │
│  ┃  [Coordinator + 3 Nodes]                           ┃  │
│  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛  │
└────────────────────────────────────────────────────────────┘
```

---

## 계층 구조

### Layer 1: Wallet Server (Go)
- **역할**: 사용자 요청 수신 및 라우팅
- **언어**: Go
- **특징**:
  - Auto-Scaling (수평 확장)
  - Stateless
  - Load Balancer 뒤에 배치
- **통신**:
  - ↑ User: HTTPS (TLS 1.3, mTLS)
  - ↓ Coordinator: HTTPS (Proto, Keep-Alive)

### Layer 2: MPC Engine Server Set
각 Set은 **독립적인 MPC 서명 시스템**

#### 2-1. Coordinator Server (C++)
- **역할**: MPC 서명 오케스트레이션
- **언어**: C++ (boost.beast)
- **인스턴스**: Set당 1개 (고정)
- **주요 컴포넌트**:
  ```
  ┌─────────────────────────────────┐
  │    CoordinatorHttpsServer       │
  │  - HTTPS Listener (9080)        │
  │  - TLS 1.3, mTLS                │
  │  - boost.beast async I/O        │
  │  - Keep-Alive support           │
  └──────────────┬──────────────────┘
                 │
  ┌──────────────▼──────────────────┐
  │    WalletMessageRouter          │
  │  - SIGNING_REQUEST routing      │
  │  - Handler registration         │
  └──────────────┬──────────────────┘
                 │
  ┌──────────────▼──────────────────┐
  │    ThreadPool (16 workers)      │
  │  - Request processing           │
  │  - Proto serialization          │
  └──────────────┬──────────────────┘
                 │
  ┌──────────────▼──────────────────┐
  │    NodeTcpClient Manager        │
  │  - 3개 Node 연결 관리           │
  │  - TCP/TLS mTLS                 │
  │  - Broadcast & Collect          │
  └─────────────────────────────────┘
  ```

#### 2-2. Node Server (C++)
- **역할**: 개별 Shard 서명
- **언어**: C++ (MPC-lib)
- **인스턴스**: Set당 3개 (고정)
- **보안 환경**:
  - Node 1: AWS Nitro Enclaves
  - Node 2: Azure Confidential Computing
  - Node 3: IBM Secure Execution
- **격리 수준**: 
  - 커널 레벨 격리
  - 1 Coordinator만 통신 허용
  - 화이트리스트 IP 필터링

---

## 통신 프로토콜

### 1. Wallet ↔ Coordinator (HTTPS)

**프로토콜**: HTTPS (TLS 1.3)  
**포트**: 9080  
**인증**: mTLS (상호 인증)  
**메시지**: Protocol Buffers  
**Keep-Alive**: 지원 (최대 100 req/conn)

```protobuf
// WalletCoordinatorMessage
message WalletSigningRequest {
    WalletRequestHeader header = 1;
    string key_id = 2;
    string transaction_data = 3;
    uint32 threshold = 4;
    uint32 total_shards = 5;
}

message WalletSigningResponse {
    WalletResponseHeader header = 1;
    string key_id = 2;
    string final_signature = 3;
    repeated ShardSignature shard_signatures = 4;
}
```

### 2. Coordinator ↔ Node (TCP/TLS)

**프로토콜**: TCP over TLS 1.3  
**포트**: 8081, 8082, 8083  
**인증**: mTLS (상호 인증)  
**메시지**: Protocol Buffers  
**연결**: 지속 연결 (persistent)

```protobuf
// CoordinatorNodeMessage
message SigningRequest {
    RequestHeader header = 1;
    string key_id = 2;
    string transaction_data = 3;
    uint32 threshold = 4;
    uint32 total_shards = 5;
}

message SigningResponse {
    ResponseHeader header = 1;
    string signature_share = 2;
    uint32 shard_index = 3;
}
```

---

## 데이터 흐름

### 서명 요청 플로우

```
User
  │
  │ 1. POST /api/v1/sign
  │    (JWT Token)
  ▼
┌─────────────┐
│ Wallet      │ 2. Validate & Route
│ Server (Go) │    (Load Balance)
└──────┬──────┘
       │ 3. WalletSigningRequest (Proto)
       │    HTTPS Keep-Alive
       ▼
┌──────────────────┐
│  Coordinator     │ 4. Parse Proto
│  (C++)           │    → WalletMessageRouter
└──────┬───────────┘    → ThreadPool
       │
       │ 5. Broadcast SigningRequest
       ├───────────────────────┬───────────────────┐
       │                       │                   │
       ▼                       ▼                   ▼
┌─────────────┐      ┌─────────────┐     ┌─────────────┐
│  Node 1     │      │  Node 2     │     │  Node 3     │
│  (AWS)      │      │  (Azure)    │     │  (IBM)      │
│             │      │             │     │             │
│  Shard 0    │      │  Shard 1    │     │  Shard 2    │
└──────┬──────┘      └──────┬──────┘     └──────┬──────┘
       │                    │                    │
       │ 6. Partial Sign    │                    │
       │    (MPC-lib)       │                    │
       │                    │                    │
       └────────────────────┴────────────────────┘
                            │
       7. Collect Signatures (2/3 Threshold)
                            │
                            ▼
                   ┌──────────────────┐
                   │  Coordinator     │
                   │  Combine Sigs    │
                   └────────┬─────────┘
                            │
       8. WalletSigningResponse (Proto)
                            │
                            ▼
                   ┌─────────────┐
                   │ Wallet      │
                   │ Server (Go) │
                   └──────┬──────┘
                          │
       9. JSON Response   │
          (final_signature)
                          ▼
                        User
```

---

## 확장성 (Scalability)

### 수평 확장 (Horizontal Scaling)

#### Wallet Server Layer
```
Load Balancer
      │
      ├─── Wallet Server 1
      ├─── Wallet Server 2
      ├─── Wallet Server 3
      └─── Wallet Server N (Auto-Scaling)
```
- **방식**: Auto-Scaling Group
- **기준**: CPU, Request Rate
- **최소**: 2개
- **최대**: 무제한

#### MPC Engine Server Set
```
┌─────────────────┐
│  Set 1 (100 TPS)│ ← Default
└─────────────────┘

┌─────────────────┐
│  Set 2 (100 TPS)│ ← Scale out
└─────────────────┘

┌─────────────────┐
│  Set N (100 TPS)│ ← Scale out
└─────────────────┘
```
- **방식**: Manual Scaling (현재)
- **기준**: TPS 목표
- **Set당 처리량**: ~100 TPS
- **Set 추가**: 신규 Coordinator + 3 Nodes 배포

---

## 보안 구조

### 1. 네트워크 보안

#### Wallet ↔ Coordinator
```
┌─────────────┐           ┌──────────────┐
│   Wallet    │ ◄────────► │ Coordinator  │
│             │   TLS 1.3  │              │
│ Client Cert │   mTLS     │ Server Cert  │
└─────────────┘  Verified  └──────────────┘
     ↑                             ↑
     │                             │
  CA Cert                       CA Cert
```

#### Coordinator ↔ Node
```
┌──────────────┐          ┌─────────────┐
│ Coordinator  │ ◄──────► │    Node     │
│              │  TLS 1.3 │             │
│ Client Cert  │  mTLS    │ Server Cert │
└──────────────┘ Verified └─────────────┘
                               │
                         IP Whitelist
                      (Coordinator IP만)
```

### 2. 데이터 보안

#### At Rest (저장 시)
- **Shard**: 클라우드 KMS 암호화
- **Certificate**: KMS에 개인키 저장
- **Transaction**: 암호화 없음 (일회성)

#### In Transit (전송 시)
- **모든 통신**: TLS 1.3 암호화
- **Protocol**: Protocol Buffers (Binary)

### 3. Node 격리 보안

```
┌─────────────────────────────────────┐
│         Host OS (Untrusted)         │
├─────────────────────────────────────┤
│                                     │
│  ┌───────────────────────────────┐ │
│  │   AWS Nitro Enclave (Node 1)  │ │
│  │   - Isolated CPU/Memory       │ │
│  │   - No network (vsock only)   │ │
│  │   - Attestation required      │ │
│  │   - Coordinator IP whitelist  │ │
│  └───────────────────────────────┘ │
│                                     │
└─────────────────────────────────────┘
```

---

## 성능 지표

### Coordinator HTTPS Server
- **처리량**: 14,496 req/sec (High Load)
- **지연시간**: 
  - 평균: 2-5ms
  - 최대: 186ms (Stress Test)
- **동시 연결**: 100+ clients
- **에러율**: 0%

### MPC Engine Set
- **예상 TPS**: ~100 TPS/Set
- **서명 지연**: ~100ms (MPC 연산)
- **Threshold**: 2/3 (최소 2개 Node 필요)

---

## 배포 구조

### 개발 환경 (Local)
```
1개 Machine
  ├─ Coordinator (127.0.0.1:9080)
  ├─ Node 1 (127.0.0.1:8081)
  ├─ Node 2 (127.0.0.1:8082)
  └─ Node 3 (127.0.0.1:8083)
```

### 프로덕션 환경
```
Region: us-east-1

┌─────────────────────────────────────┐
│         VPC (10.0.0.0/16)           │
├─────────────────────────────────────┤
│                                     │
│  Public Subnet (10.0.1.0/24)       │
│  ┌──────────────────────────────┐  │
│  │   Load Balancer (ALB)        │  │
│  └────────────┬─────────────────┘  │
│               │                     │
│  Private Subnet (10.0.2.0/24)      │
│  ┌────────────▼─────────────────┐  │
│  │  Wallet Servers (ASG)        │  │
│  │   - instance 1               │  │
│  │   - instance 2               │  │
│  │   - instance N               │  │
│  └────────────┬─────────────────┘  │
│               │                     │
│  Private Subnet (10.0.3.0/24)      │
│  ┌────────────▼─────────────────┐  │
│  │  Coordinator 1 (EC2)         │  │
│  └────────────┬─────────────────┘  │
│               │                     │
│  Isolated Subnets                  │
│  ┌────────────▼─────────────────┐  │
│  │  Node 1 (AWS Nitro Enclave)  │  │
│  └──────────────────────────────┘  │
│  ┌──────────────────────────────┐  │
│  │  Node 2 (Azure CC)           │  │ (VPN/Direct Connect)
│  └──────────────────────────────┘  │
│  ┌──────────────────────────────┐  │
│  │  Node 3 (IBM SE)             │  │ (VPN/Direct Connect)
│  └──────────────────────────────┘  │
│                                     │
└─────────────────────────────────────┘
```

---

## 향후 확장 계획

### Phase 1: 기본 MPC (현재)
- ✅ 1 Set (Coordinator + 3 Nodes)
- ✅ 2/3 Threshold
- ✅ Mock 서명

### Phase 2: 실제 MPC 통합
- [ ] fireblocks/mpc-lib 통합
- [ ] 실제 키 생성/서명
- [ ] Threshold 검증

### Phase 3: Multi-Set 확장
- [ ] N개 Set 동시 운영
- [ ] Wallet Server 로드밸런싱
- [ ] Set 간 Failover

### Phase 4: Dynamic Sharding
- [ ] 사용자별 Shard 추가/제거
- [ ] 3개 → N개 Shard 확장
- [ ] Threshold 조정 (2/3 → M/N)

### Phase 5: Google Cloud 추가
- [ ] Node 4: Google Confidential VM
- [ ] 4개 클라우드 분산
- [ ] 3/4 Threshold

---

## 참고 문서

- [AWS Nitro Enclaves - MPC Wallet](https://aws.amazon.com/ko/blogs/web3/build-secure-multi-party-computation-mpc-wallets-using-aws-nitro-enclaves/)
- [fireblocks/mpc-lib](https://github.com/fireblocks/mpc-lib)
- [boost.beast Documentation](https://www.boost.org/doc/libs/1_82_0/libs/beast/doc/html/index.html)