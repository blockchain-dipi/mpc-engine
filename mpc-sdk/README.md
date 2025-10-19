<!-- mpc-sdk/README.md -->
# MPC-SDK

MPC(Multi-Party Computation) 기반 분산 지갑 서명 시스템을 위한 암호학 SDK

---

## 🎯 프로젝트 목표

- **암호학 라이브러리 추상화**: Fireblocks, ZenGo, 자체 구현 등 쉽게 교체 가능
- **2/3 Threshold 서명**: 분산 키 관리 및 서명
- **ECDSA & EdDSA 지원**: Bitcoin/Ethereum (ECDSA), Solana (EdDSA)
- **확장 가능한 설계**: Provider 패턴으로 다양한 MPC 라이브러리 지원

---

## 📂 프로젝트 구조
```
mpc-sdk/
├── interface/              # 추상화 인터페이스
│   ├── ICryptoProvider.h
│   ├── IKeyGenerator.h
│   ├── IECDSASigner.h
│   └── IEdDSASigner.h
│
├── core/                   # Public API
│   ├── include/
│   │   ├── MPCTypes.h
│   │   ├── MPCException.h
│   │   ├── KeyGenerator.h
│   │   ├── ECDSASigner.h
│   │   └── EdDSASigner.h
│   └── src/                # (Phase 2 예정)
│
├── providers/              # Provider 구현체
│   └── fireblocks/         # (Phase 2 예정)
│
├── scripts/
│   └── build_fireblocks.sh # Fireblocks 빌드 스크립트
│
├── BUILD.md                # 빌드 가이드
└── README.md               # 이 파일
```

---

## 🚀 빌드 방법

자세한 빌드 가이드는 **[BUILD.md](./BUILD.md)** 참조

### 빠른 시작
```bash
# 1. Fireblocks MPC-lib 빌드
./scripts/build_fireblocks.sh

# 2. mpc-sdk 빌드 (Phase 2 이후)
mkdir build && cd build
cmake ..
make -j$(nproc)
```

---

## 📋 개발 로드맵

### ✅ Phase 1: SDK 기본 구조 (완료)
- [x] 인터페이스 정의 (ICryptoProvider, IKeyGenerator, IECDSASigner, IEdDSASigner)
- [x] 공통 타입 정의 (MPCTypes, MPCException)
- [x] Public API 정의 (KeyGenerator, ECDSASigner, EdDSASigner)

### 🔄 Phase 2: Fireblocks Provider 구현 (진행 중)
- [ ] FireblocksCryptoProvider
- [ ] FireblocksKeyGenerator (5 Phase)
- [ ] FireblocksECDSASigner (5 Phase)
- [ ] FireblocksEdDSASigner (5 Phase)
- [ ] CMakeLists.txt 작성

### 📅 Phase 3: 테스트
- [ ] 단위 테스트
- [ ] 통합 테스트
- [ ] Threshold 서명 검증

### 📅 Phase 4: 확장
- [ ] Mock Provider (테스트용)
- [ ] ZenGo MPC Provider
- [ ] 자체 MPC 구현

---

## 🔐 지원 알고리즘

| 알고리즘 | 곡선 | 사용처 | 상태 |
|---------|-----|--------|------|
| **ECDSA** | secp256k1 | Bitcoin, Ethereum | Phase 2 |
| **ECDSA** | secp256r1 | NIST P-256 | Phase 2 |
| **ECDSA** | STARK | StarkNet | Phase 2 |
| **EdDSA** | Ed25519 | Solana, Polkadot | Phase 2 |

---

## 📚 참고 문서

- [Fireblocks MPC-lib](https://github.com/fireblocks/mpc-lib)
- [AWS Nitro Enclaves - MPC Wallet](https://aws.amazon.com/ko/blogs/web3/build-secure-multi-party-computation-mpc-wallets-using-aws-nitro-enclaves/)

---

## 📄 라이선스

(추후 추가)