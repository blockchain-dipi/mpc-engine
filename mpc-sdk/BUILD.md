<!-- mpc-sdk/BUILD.md -->
# MPC-SDK 빌드 가이드

이 문서는 mpc-sdk 프로젝트를 처음 빌드하는 개발자를 위한 가이드입니다.

---

## 📋 목차

1. [시스템 요구사항](#시스템-요구사항)
2. [빌드 순서](#빌드-순서)
3. [문제 해결](#문제-해결)

---

## 시스템 요구사항

### 필수 도구
- **CMake**: 3.15 이상
- **C++ 컴파일러**: GCC 9+ 또는 Clang 10+
- **OpenSSL**: 1.1.1 이상
- **Protobuf**: 3.0 이상
- **Git**: Submodule 관리용

### 설치 (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y \
    cmake \
    g++ \
    libssl-dev \
    libprotobuf-dev \
    protobuf-compiler \
    uuid-dev \
    git
```

### 설치 (macOS)
```bash
brew install cmake openssl protobuf
```

---

## 빌드 순서

### ✅ **Step 1: Fireblocks MPC-lib 빌드**

Fireblocks MPC-lib은 **Static Library (`.a`)** 로 빌드해야 합니다.

#### **자동 빌드 (권장)**
```bash
cd ~/Volche/mpc-engine/mpc-sdk
./scripts/build_fireblocks.sh
```

이 스크립트는 다음을 수행합니다:
1. 기존 빌드 디렉토리 삭제 (clean build)
2. CMake 설정 (`-DBUILD_SHARED_LIBS=OFF`)
3. Static library 빌드
4. 빌드 결과 확인

#### **수동 빌드**
```bash
cd ~/Volche/mpc-engine/external/fireblocks-mpc-lib
mkdir -p build && cd build
rm -rf *  # Clean build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)

# 확인
ls -la src/common/libcosigner.a
```

**예상 출력:**
```
-rw-r--r-- 1 user user 15234567 Oct 19 12:34 src/common/libcosigner.a
```

---

### ✅ **Step 2: mpc-sdk 빌드**

#### **현재 상태 (Phase 1 완료)**

**Phase 1**에서는 헤더 파일만 작성했으므로, 아직 실제 빌드는 불가능합니다.

**완료된 파일:**
```
mpc-sdk/
├── interface/              # 인터페이스 (헤더만)
│   ├── ICryptoProvider.h
│   ├── IKeyGenerator.h
│   ├── IECDSASigner.h
│   └── IEdDSASigner.h
└── core/include/           # Public API (헤더만)
    ├── MPCTypes.h
    ├── MPCException.h
    ├── KeyGenerator.h
    ├── ECDSASigner.h
    └── EdDSASigner.h
```

#### **Phase 2에서 추가될 파일 (예정)**
```
mpc-sdk/
├── core/src/                      # ← Phase 2에서 추가
│   ├── MPCTypes.cpp
│   ├── MPCException.cpp
│   ├── KeyGenerator.cpp
│   ├── ECDSASigner.cpp
│   └── EdDSASigner.cpp
├── providers/fireblocks/          # ← Phase 2에서 추가
│   ├── FireblocksCryptoProvider.cpp
│   ├── FireblocksKeyGenerator.cpp
│   ├── FireblocksECDSASigner.cpp
│   └── FireblocksEdDSASigner.cpp
└── CMakeLists.txt                 # ← Phase 2에서 추가
```

---

### ✅ **Step 3: CMakeLists.txt 작성 (Phase 2 예정)**

**Phase 2**에서 작성될 `CMakeLists.txt` 예시:
```cmake
cmake_minimum_required(VERSION 3.15)
project(mpc-sdk)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include 경로
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/src
)

# Core Library
add_library(mpc-sdk STATIC
    core/src/MPCTypes.cpp
    core/src/MPCException.cpp
    core/src/KeyGenerator.cpp
    core/src/ECDSASigner.cpp
    core/src/EdDSASigner.cpp
)

# Fireblocks Provider
add_library(mpc-sdk-fireblocks STATIC
    providers/fireblocks/FireblocksCryptoProvider.cpp
    providers/fireblocks/FireblocksKeyGenerator.cpp
    providers/fireblocks/FireblocksECDSASigner.cpp
    providers/fireblocks/FireblocksEdDSASigner.cpp
)

# Link Fireblocks MPC-lib
target_link_libraries(mpc-sdk-fireblocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/build/src/common/libcosigner.a
    pthread
    ssl
    crypto
)
```

#### **Phase 2에서 빌드하는 방법 (예정)**
```bash
cd ~/Volche/mpc-engine/mpc-sdk
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# 확인
ls -la libmpc-sdk.a
ls -la libmpc-sdk-fireblocks.a
```

---

## 문제 해결

### ❌ **문제: `libcosigner.a` 파일이 없습니다**

**원인:** Shared library (`.so`)로 빌드됨

**해결:**
```bash
cd ~/Volche/mpc-engine/external/fireblocks-mpc-lib/build
rm -rf *
cmake .. -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)
find . -name "*.a"
```

---

### ❌ **문제: `'../interface/ICryptoProvider.h' file not found`**

**원인:** Include 경로 문제

**해결 (Phase 2 예정):**
- CMakeLists.txt에서 `include_directories()` 설정
- 헤더 파일의 `#include "../../interface/..."` → `#include "interface/..."` 리팩토링

---

### ❌ **문제: Submodule이 비어있습니다**

**해결:**
```bash
cd ~/Volche/mpc-engine
git submodule update --init --recursive
```

---

## 다음 단계

**Phase 1 (완료)**: 헤더 파일 작성  
**Phase 2 (다음)**: 구현 파일 작성 + CMake 설정  
**Phase 3**: 테스트 코드 작성  
**Phase 4**: 통합 테스트  

자세한 내용은 [README.md](./README.md) 참조