# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 테스트 공통 설정
set(TEST_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# =======================================================================================================
# Unit 테스트
# =======================================================================================================

# === ThreadSafeQueue 테스트 ===
add_executable(test_threadsafe_queue
    unit/threadsafe_queue_test.cpp
)

target_include_directories(test_threadsafe_queue PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_threadsafe_queue
    Threads::Threads
)

add_test(NAME ThreadSafeQueue COMMAND test_threadsafe_queue)

# === ThreadPool 테스트 ===
add_executable(test_threadpool
    unit/threadpool_test.cpp
)

target_include_directories(test_threadpool PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_threadpool
    Threads::Threads
)

add_test(NAME ThreadPool COMMAND test_threadpool)

# === SocketIO 테스트 ===
add_executable(test_socket_io
    unit/socket_io_test.cpp
)

target_include_directories(test_socket_io PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_socket_io
    Threads::Threads
    mpc_common
)

add_test(NAME SocketIO COMMAND test_socket_io)

# =======================================================================================================
# Integration 테스트
# =======================================================================================================

# === Coordinator ↔ Node TLS Integration Test ===
add_executable(test_coordinator_node_tls
    integration/test_coordinator_node_tls.cpp
)

target_include_directories(test_coordinator_node_tls PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_coordinator_node_tls
    coordinator_server
    node_server
    coordinator_node_network
    node_network
    node_handlers
    coordinator_node_protocol
    proto_coordinator_node
    mpc_common
    mpc_kms
    mpc_resource
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

add_test(NAME CoordinatorNodeTLS COMMAND test_coordinator_node_tls)

# === E2E Integration Test: Wallet ↔ Coordinator ↔ Node ===
add_executable(test_wallet_coordinator_node_integration
    integration/test_wallet_coordinator_node_integration.cpp
)

target_include_directories(test_wallet_coordinator_node_integration PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_wallet_coordinator_node_integration
    coordinator_server
    node_server
    coordinator_node_network
    coordinator_wallet_server
    coordinator_wallet_handlers
    node_network
    node_handlers
    proto_coordinator_node
    proto_wallet_coordinator
    mpc_common
    mpc_kms
    mpc_resource
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

add_test(NAME WalletCoordinatorNodeE2E COMMAND test_wallet_coordinator_node_integration)

# =======================================================================================================
# 테스트 빌드 정보
# =======================================================================================================

message(STATUS "=== Test Configuration ===")
message(STATUS "Unit Tests:")
message(STATUS "  - test_threadsafe_queue")
message(STATUS "  - test_threadpool")
message(STATUS "  - test_socket_io")
message(STATUS "")
message(STATUS "Integration Tests:")
message(STATUS "  - test_coordinator_node_tls")
message(STATUS "  - test_wallet_coordinator_node_integration")
message(STATUS "")
message(STATUS "==========================")