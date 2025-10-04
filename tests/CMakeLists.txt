# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 테스트 공통 설정
set(TEST_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# unit 테스트 ==========================================================================================
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

# === Local KMS 테스트 ===
add_executable(test_local_kms
    unit/local_kms_test.cpp
)

target_include_directories(test_local_kms PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_local_kms
    mpc_kms
    mpc_common
    Threads::Threads
)

add_test(NAME LocalKMS COMMAND test_local_kms)

# === HTTPS Library 기본 테스트 ===
add_executable(test_https_library
    unit/https/test_https_library.cpp
)

target_include_directories(test_https_library PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_https_library
    mpc_common
    Threads::Threads
)

add_test(NAME HttpsLibrary COMMAND test_https_library)


# integration 테스트 ==========================================================================================
# === Node TCP Server 테스트 ===
add_executable(test_node_tcp_server
    integration/node_tcp_server_test.cpp
)

target_include_directories(test_node_tcp_server PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_node_tcp_server
    node_network
    coordinator_node_protocol
    mpc_common
    Threads::Threads
)

add_test(NAME NodeTcpServer COMMAND test_node_tcp_server)

# === Message Validation 테스트 ===
add_executable(test_message_validation
    integration/message_validation_test.cpp
)

target_include_directories(test_message_validation PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_message_validation
    coordinator_node_protocol
    Threads::Threads
)

add_test(NAME MessageValidation COMMAND test_message_validation)

# === Coordinator-Node Basic Test ===
add_executable(test_coordinator_node_basic
    integration/coordinator_node_basic_test.cpp
)

target_include_directories(test_coordinator_node_basic PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_coordinator_node_basic
    coordinator_server         # CoordinatorServer 구현
    node_server               # NodeServer 구현
    coordinator_node_network  # NodeTcpClient
    node_network             # NodeTcpServer
    node_handlers            # NodeProtocolRouter
    coordinator_node_protocol # MessageTypes
    mpc_common
    Threads::Threads
)

add_test(NAME CoordinatorNodeBasic COMMAND test_coordinator_node_basic)

# === Manual Test (실제 Node 서버 필요) ===
add_executable(test_coordinator_with_real_nodes
    manual/coordinator_with_real_nodes_test.cpp
)

target_include_directories(test_coordinator_with_real_nodes PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_coordinator_with_real_nodes
    coordinator_server
    coordinator_node_protocol
    mpc_common
    Threads::Threads
)

# 일반 테스트에는 포함하지 않음 (수동 실행 전용)
# add_test(NAME CoordinatorWithRealNodes COMMAND test_coordinator_with_real_nodes)
message(STATUS "  - test_coordinator_with_real_nodes (manual)")

# === 테스트 빌드 정보 ===
message(STATUS "=== Test Configuration ===")
message(STATUS "Test executables:")
message(STATUS "  - test_threadsafe_queue")
message(STATUS "  - test_threadpool")
message(STATUS "  - test_socket_io")
message(STATUS "  - test_node_tcp_server")
message(STATUS "  - test_message_validation")
message(STATUS "  - test_coordinator_node_basic")
message(STATUS "  - test_local_kms")
message(STATUS "  - test_https_library")
message(STATUS "==========================")