# mpc-sdk/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# ❌ 삭제: project(mpc-sdk VERSION 1.0.0 LANGUAGES CXX)
# ❌ 삭제: set(CMAKE_CXX_STANDARD 17)  # 루트에서 이미 설정됨
# ❌ 삭제: set(CMAKE_CXX_STANDARD_REQUIRED ON)
# ❌ 삭제: set(CMAKE_CXX_EXTENSIONS OFF)
# ❌ 삭제: if(NOT CMAKE_BUILD_TYPE) ... # 루트에서 제어

# ========================================================================
# Include Directories
# ========================================================================

set(MPC_SDK_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

# ========================================================================
# Fireblocks MPC-lib Configuration
# ========================================================================

set(FIREBLOCKS_INCLUDE_DIR 
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/include
)

set(FIREBLOCKS_BUILD_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/build/src/common
)

set(FIREBLOCKS_LIB_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/build/src/common/libcosigner.so
)

if(NOT EXISTS ${FIREBLOCKS_LIB_PATH})
    message(FATAL_ERROR "Fireblocks library not found: ${FIREBLOCKS_LIB_PATH}")
endif()

message(STATUS "===========================================")
message(STATUS "MPC-SDK Configuration")
message(STATUS "===========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Include directories:")
foreach(dir ${MPC_SDK_INCLUDE_DIRS})
    message(STATUS "  - ${dir}")
endforeach()
message(STATUS "Fireblocks include: ${FIREBLOCKS_INCLUDE_DIR}")
message(STATUS "Fireblocks build include: ${FIREBLOCKS_BUILD_INCLUDE_DIR}")
message(STATUS "Fireblocks lib: ${FIREBLOCKS_LIB_PATH}")
message(STATUS "===========================================")

# ========================================================================
# Core Library (mpc-sdk)
# ========================================================================

set(CORE_SOURCES
    core/src/MPCTypes.cpp
    core/src/KeyGenerator.cpp
    core/src/ECDSASigner.cpp
    core/src/EdDSASigner.cpp
)

add_library(mpc-sdk STATIC ${CORE_SOURCES})

target_include_directories(mpc-sdk
    PUBLIC
        ${MPC_SDK_INCLUDE_DIRS}
)

# ========================================================================
# Fireblocks Provider Library (mpc-sdk-fireblocks)
# ========================================================================

set(FIREBLOCKS_PROVIDER_SOURCES
    providers/fireblocks/FireblocksCryptoProvider.cpp
    providers/fireblocks/FireblocksKeyGenerator.cpp
    providers/fireblocks/FireblocksECDSASigner.cpp
    providers/fireblocks/FireblocksEdDSASigner.cpp
    providers/fireblocks/FireblocksPlatformService.cpp
    providers/fireblocks/FireblocksPersistency.cpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/providers/fireblocks/FireblocksCryptoProvider.cpp)
    add_library(mpc-sdk-fireblocks STATIC
        ${FIREBLOCKS_PROVIDER_SOURCES}
    )

    target_include_directories(mpc-sdk-fireblocks
        PUBLIC
            ${MPC_SDK_INCLUDE_DIRS}
            ${FIREBLOCKS_INCLUDE_DIR}
            ${FIREBLOCKS_BUILD_INCLUDE_DIR}
    )

    target_link_libraries(mpc-sdk-fireblocks
        PUBLIC
            mpc-sdk
            ${FIREBLOCKS_LIB_PATH}
            pthread
            ssl
            crypto
            dl
    )

    message(STATUS "Fireblocks provider library: libmpc-sdk-fireblocks.a")
endif()

# ========================================================================
# Tests (Optional) - 루트에서 BUILD_TESTS 제어
# ========================================================================

# ❌ 삭제: option(BUILD_TESTS "Build tests" ON)
# 루트에서 이미 정의됨

if(BUILD_TESTS)
    find_package(GTest QUIET)
    
    if(GTest_FOUND)
        # enable_testing() 불필요 (루트에서 이미 호출됨)
        
        add_executable(mpc-sdk-tests
            tests/fireblocks_keygen_test.cpp
        )
        
        target_link_libraries(mpc-sdk-tests
            PRIVATE
                mpc-sdk-fireblocks
                GTest::GTest
                GTest::Main
        )
        
        target_include_directories(mpc-sdk-tests
            PRIVATE
                ${MPC_SDK_INCLUDE_DIRS}
                ${FIREBLOCKS_INCLUDE_DIR}
                ${FIREBLOCKS_BUILD_INCLUDE_DIR}
                providers/fireblocks
        )
        
        set_target_properties(mpc-sdk-tests PROPERTIES
            BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/build/src/common"
            INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../external/fireblocks-mpc-lib/build/src/common"
        )
        
        include(GoogleTest)
        gtest_discover_tests(mpc-sdk-tests)
        
        message(STATUS "MPC-SDK Tests enabled")
    else()
        message(WARNING "Google Test not found. MPC-SDK tests will not be built.")
        message(STATUS "Install: sudo apt install libgtest-dev")
    endif()
endif()