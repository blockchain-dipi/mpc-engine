# CMakeLists.txt (루트)
cmake_minimum_required(VERSION 3.16)
project(mpc-engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -pedantic -pthread)

find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# === 공통 라이브러리 ===
add_library(mpc_common STATIC
    src/common/utils/socket/SocketUtils.cpp
    src/common/utils/firewall/KernelFirewall.cpp
    src/common/config/EnvConfig.cpp
    src/common/types/NodePlatformType.cpp
)

target_include_directories(mpc_common PUBLIC src)
target_link_libraries(mpc_common Threads::Threads)

# === 🆕 KMS 라이브러리 ===
add_library(mpc_kms STATIC
    src/common/kms/src/LocalKMS.cpp
    src/common/kms/src/AwsKMS.cpp
    src/common/kms/src/AzureKMS.cpp
    src/common/kms/src/IbmKMS.cpp
    src/common/kms/src/GoogleKMS.cpp
    src/common/kms/src/KMSFactory.cpp
)

target_include_directories(mpc_kms PUBLIC src)
target_link_libraries(mpc_kms mpc_common)

# === Coordinator-Node 프로토콜 ===
add_library(coordinator_node_protocol STATIC
    src/coordinator/handlers/node/src/SigningProtocol.cpp
)

target_include_directories(coordinator_node_protocol PUBLIC src)
target_link_libraries(coordinator_node_protocol mpc_common)

# === Node 핸들러 ===
add_library(node_handlers STATIC
    src/node/handlers/src/NodeProtocolRouter.cpp
    src/node/handlers/src/NodeSigningProtocol.cpp
)

target_include_directories(node_handlers PUBLIC src)
target_link_libraries(node_handlers 
    mpc_common 
    coordinator_node_protocol
)

# === Node 네트워크 ===
add_library(node_network STATIC
    src/node/network/src/NodeConnectionInfo.cpp
    src/node/network/src/NodeTcpServer.cpp
)

target_include_directories(node_network PUBLIC src)
target_link_libraries(node_network 
    mpc_common 
    coordinator_node_protocol
    Threads::Threads
)

# === Coordinator 네트워크 (Node 클라이언트) ===
add_library(coordinator_node_network STATIC
    src/coordinator/network/node_client/src/NodeConnectionInfo.cpp
    src/coordinator/network/node_client/src/NodeTcpClient.cpp
)

target_include_directories(coordinator_node_network PUBLIC src)
target_link_libraries(coordinator_node_network 
    mpc_common 
    coordinator_node_protocol
    Threads::Threads
)

# === Node 서버 라이브러리 ===
add_library(node_server STATIC
    src/node/NodeServer.cpp
)

target_include_directories(node_server PUBLIC src)
target_link_libraries(node_server 
    node_network
    node_handlers
    mpc_common
    coordinator_node_protocol
    Threads::Threads
)

# === Coordinator 서버 라이브러리 ===
add_library(coordinator_server STATIC
    src/coordinator/CoordinatorServer.cpp
)

target_include_directories(coordinator_server PUBLIC src)
target_link_libraries(coordinator_server
    coordinator_node_network
    coordinator_node_protocol
    mpc_common
    Threads::Threads
)

# === Node 실행 파일 ===
add_executable(node 
    src/node/main.cpp
)

target_link_libraries(node
    node_server
    mpc_kms  # ✅ 추가
)

# === Coordinator 실행 파일 ===
add_executable(coordinator 
    src/coordinator/main.cpp
)

target_link_libraries(coordinator
    coordinator_server
    mpc_kms  # ✅ 추가
)

# === 테스트 (별도 파일) ===
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# === 설치 설정 ===
install(TARGETS coordinator node DESTINATION bin)

# === 빌드 정보 출력 ===
message(STATUS "=== MPC Engine Build Configuration ===")
message(STATUS "Building: coordinator and node executables")
message(STATUS "KMS: Local (functional), AWS/Azure/IBM/Google (stubs)")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "=======================================")