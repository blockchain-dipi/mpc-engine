# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 테스트 공통 설정
set(TEST_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src)

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

# === Node TCP Server 테스트 (통합 테스트) ===
add_executable(test_node_tcp_server
    integration/node_tcp_server_test.cpp
)

target_include_directories(test_node_tcp_server PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_node_tcp_server
    node_network              # NodeTcpServer 구현
    coordinator_node_protocol # NetworkMessage
    mpc_common                # SocketUtils 등
    Threads::Threads
)

add_test(NAME NodeTcpServer COMMAND test_node_tcp_server)

# === Message Validation 테스트 (통합 테스트) ===
add_executable(test_message_validation
    integration/message_validation_test.cpp
)

target_include_directories(test_message_validation PRIVATE
    ${TEST_INCLUDE_DIRS}
)

target_link_libraries(test_message_validation
    coordinator_node_protocol # NetworkMessage, ValidationResult
    Threads::Threads
)

add_test(NAME MessageValidation COMMAND test_message_validation)

# === 테스트 빌드 정보 ===
message(STATUS "=== Test Configuration ===")
message(STATUS "Test executables:")
message(STATUS "  - test_threadsafe_queue")
message(STATUS "  - test_threadpool")
message(STATUS "  - test_node_tcp_server")
message(STATUS "  - test_message_validation")
message(STATUS "==========================")