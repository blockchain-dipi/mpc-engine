# mpc-sdk/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(mpc-sdk VERSION 1.0.0 LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 빌드 타입 설정 (기본값: Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 컴파일 옵션 (수정됨)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# ========================================================================
# Include 경로 설정
# ========================================================================

# mpc-sdk 내부 include 경로
set(MPC_SDK_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

# Fireblocks MPC-lib include 경로
set(FIREBLOCKS_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/src
)

# Fireblocks MPC-lib static library 경로
set(FIREBLOCKS_LIB_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/build/src/common/libcosigner.a
)

# ========================================================================
# Core Library (mpc-sdk)
# ========================================================================

set(MPC_SDK_SOURCES
    core/src/MPCTypes.cpp
    core/src/KeyGenerator.cpp
    core/src/ECDSASigner.cpp
    core/src/EdDSASigner.cpp
)

set(MPC_SDK_HEADERS
    core/include/MPCTypes.h
    core/include/MPCException.h
    core/include/KeyGenerator.h
    core/include/ECDSASigner.h
    core/include/EdDSASigner.h
    interface/ICryptoProvider.h
    interface/IKeyGenerator.h
    interface/IECDSASigner.h
    interface/IEdDSASigner.h
)

add_library(mpc-sdk STATIC
    ${MPC_SDK_SOURCES}
)

target_include_directories(mpc-sdk
    PUBLIC
        ${MPC_SDK_INCLUDE_DIRS}
)

# ========================================================================
# Fireblocks Provider Library (mpc-sdk-fireblocks)
# ========================================================================

# Fireblocks Provider 소스 (Phase 2에서 추가 예정)
set(FIREBLOCKS_PROVIDER_SOURCES
    providers/fireblocks/FireblocksCryptoProvider.cpp
    providers/fireblocks/FireblocksKeyGenerator.cpp
    providers/fireblocks/FireblocksECDSASigner.cpp
    providers/fireblocks/FireblocksEdDSASigner.cpp
)

# Provider 구현체가 있을 때만 빌드
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/providers/fireblocks/FireblocksCryptoProvider.cpp)
    add_library(mpc-sdk-fireblocks STATIC
        ${FIREBLOCKS_PROVIDER_SOURCES}
    )

    target_include_directories(mpc-sdk-fireblocks
        PUBLIC
            ${MPC_SDK_INCLUDE_DIRS}
        PRIVATE
            ${FIREBLOCKS_INCLUDE_DIR}
    )

    target_link_libraries(mpc-sdk-fireblocks
        PUBLIC
            mpc-sdk
        PRIVATE
            ${FIREBLOCKS_LIB_PATH}
            pthread
            ssl
            crypto
    )
else()
    message(STATUS "Fireblocks provider not found, skipping mpc-sdk-fireblocks build")
endif()

# ========================================================================
# 설치 설정
# ========================================================================

install(TARGETS mpc-sdk
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${MPC_SDK_HEADERS}
    DESTINATION include/mpc-sdk
)

# ========================================================================
# 빌드 정보 출력
# ========================================================================

message(STATUS "===========================================")
message(STATUS "MPC-SDK Configuration")
message(STATUS "===========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Include directories:")
foreach(dir ${MPC_SDK_INCLUDE_DIRS})
    message(STATUS "  - ${dir}")
endforeach()
message(STATUS "Fireblocks lib: ${FIREBLOCKS_LIB_PATH}")
message(STATUS "===========================================")