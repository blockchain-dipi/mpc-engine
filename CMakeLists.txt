# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(mpc-engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -pedantic -pthread)

find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.2.0)

if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# === Proto 라이브러리 (최우선 빌드) ===
set(PROTO_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/coordinator_node/generated)

# Proto 생성 파일들 자동 수집
file(GLOB PROTO_SRCS "${PROTO_GEN_DIR}/*.pb.cc")
file(GLOB PROTO_HDRS "${PROTO_GEN_DIR}/*.pb.h")

add_library(mpc_proto STATIC ${PROTO_SRCS})
target_include_directories(mpc_proto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/proto
    ${Protobuf_INCLUDE_DIRS}
)
target_link_libraries(mpc_proto
    ${Protobuf_LIBRARIES}
)

# === 공통 라이브러리 ===
add_library(mpc_common STATIC
    src/common/utils/socket/SocketUtils.cpp
    src/common/utils/firewall/KernelFirewall.cpp
    src/common/config/EnvConfig.cpp
    src/common/config/EnvManager.cpp
    src/common/network/https/src/HttpWriter.cpp
    src/common/network/https/src/HttpsClient.cpp
    src/common/network/https/src/HttpsConnectionPool.cpp
    src/common/network/tls/src/TlsContext.cpp
    src/common/network/tls/src/TlsConnection.cpp
)

target_include_directories(mpc_common PUBLIC src)
target_link_libraries(mpc_common 
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# === Resource Loader 라이브러리 ===
add_library(mpc_resource STATIC
    src/common/resource/src/ReadOnlyResLoaderManager.cpp
    src/common/resource/src/LocalReadOnlyResLoader.cpp
    src/common/resource/src/AWSReadOnlyResLoader.cpp
    src/common/resource/src/AzureReadOnlyResLoader.cpp
    src/common/resource/src/IBMReadOnlyResLoader.cpp
    src/common/resource/src/GoogleReadOnlyResLoader.cpp
)

target_include_directories(mpc_resource PUBLIC src)
target_link_libraries(mpc_resource mpc_common)

# === KMS 라이브러리 ===
add_library(mpc_kms STATIC
    src/common/kms/src/KMSManager.cpp
    src/common/kms/src/AwsKMS.cpp
    src/common/kms/src/AzureKMS.cpp
    src/common/kms/src/GoogleKMS.cpp
    src/common/kms/src/IbmKMS.cpp
    src/common/kms/src/LocalKMS.cpp
)

target_include_directories(mpc_kms PUBLIC src)
target_link_libraries(mpc_kms mpc_common)

# === Coordinator-Node 프로토콜 (올바른 경로) ===
add_library(coordinator_node_protocol STATIC
    src/coordinator/handlers/node/src/SigningHandler.cpp
)

target_include_directories(coordinator_node_protocol PUBLIC src)
target_link_libraries(coordinator_node_protocol mpc_common)

# === Wallet 프로토콜 ===
add_library(coordinator_wallet_protocol STATIC
    src/protocols/coordinator_wallet/src/WalletSigningProtocol.cpp
)

target_include_directories(coordinator_wallet_protocol PUBLIC src)
target_link_libraries(coordinator_wallet_protocol 
    mpc_common
    nlohmann_json::nlohmann_json
)

# === Coordinator Wallet 핸들러 ===
add_library(coordinator_wallet_handlers STATIC
    src/coordinator/handlers/wallet/src/WalletSigningHandler.cpp
)

target_include_directories(coordinator_wallet_handlers PUBLIC src)
target_link_libraries(coordinator_wallet_handlers
    coordinator_wallet_protocol
    mpc_common
    nlohmann_json::nlohmann_json
)

# === Node 핸들러 ===
add_library(node_handlers STATIC
    src/node/handlers/src/NodeMessageRouter.cpp
    src/node/handlers/src/NodeSigningHandler.cpp
)

target_include_directories(node_handlers PUBLIC src)
target_link_libraries(node_handlers 
    mpc_proto
    mpc_common 
    # coordinator_node_protocol
)

# === Node 네트워크 ===
add_library(node_network STATIC
    src/node/network/src/NodeConnectionInfo.cpp
    src/node/network/src/NodeTcpServer.cpp
)

target_include_directories(node_network PUBLIC src)
target_link_libraries(node_network 
    mpc_proto
    mpc_common 
    # coordinator_node_protocol
    Threads::Threads
)

# === Coordinator 네트워크 (Node 클라이언트) ===
add_library(coordinator_node_network STATIC
    src/coordinator/network/node_client/src/NodeConnectionInfo.cpp
    src/coordinator/network/node_client/src/NodeTcpClient.cpp
)

target_include_directories(coordinator_node_network PUBLIC src)
target_link_libraries(coordinator_node_network 
    mpc_proto
    mpc_common 
    # coordinator_node_protocol
    mpc_kms
    Threads::Threads
)

# === Coordinator Wallet 네트워크 ===
add_library(coordinator_wallet_network STATIC
src/coordinator/handlers/wallet/src/WalletMessageRouter.cpp
    src/coordinator/network/wallet_server/src/WalletConnectionInfo.cpp    
    src/coordinator/network/wallet_server/src/WalletServerManager.cpp
)

target_include_directories(coordinator_wallet_network PUBLIC src)
target_link_libraries(coordinator_wallet_network
    mpc_common
    coordinator_wallet_protocol
    coordinator_wallet_handlers
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
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
    mpc_proto
    mpc_common
    # coordinator_node_protocol
    mpc_kms
    Threads::Threads
)

# === Coordinator 서버 라이브러리 ===
add_library(coordinator_server STATIC
    src/coordinator/CoordinatorServer.cpp
)

target_include_directories(coordinator_server PUBLIC src)
target_link_libraries(coordinator_server
    coordinator_node_network
    coordinator_wallet_network
    # coordinator_node_protocol
    coordinator_wallet_protocol
    mpc_proto
    mpc_common
    mpc_kms
    Threads::Threads
)

# === Node 실행 파일 ===
add_executable(node 
    src/node/main.cpp
)

target_link_libraries(node
    node_server
    mpc_kms
    mpc_resource
)

# === Coordinator 실행 파일 ===
add_executable(coordinator 
    src/coordinator/main.cpp
)

target_link_libraries(coordinator
    coordinator_server
    coordinator_wallet_protocol
    mpc_kms
    mpc_resource
)

# === 테스트 ===
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS coordinator node DESTINATION bin)

message(STATUS "=== MPC Engine Build Configuration ===")
message(STATUS "Building: coordinator and node executables")
message(STATUS "KMS: Local (functional)")
message(STATUS "TLS: mTLS enabled")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "=======================================")
