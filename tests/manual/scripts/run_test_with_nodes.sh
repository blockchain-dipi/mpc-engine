#!/bin/bash

set -e

# 경로 설정
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
NODE_BIN="$BUILD_DIR/node"
TEST_BIN="$BUILD_DIR/tests/test_coordinator_with_real_nodes"

# 로그 디렉토리 설정
LOG_DIR="/tmp/mpc-engine/manual_tests"
TEST_RUN_ID="$(date +%Y%m%d_%H%M%S)"
TEST_LOG_DIR="$LOG_DIR/$TEST_RUN_ID"

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 로그 함수
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# PID 저장
NODE_PIDS=()

# 정리 함수
cleanup() {
    log_info "Stopping all Node servers..."
    
    for pid in "${NODE_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -SIGTERM "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    
    log_success "Cleanup complete"
}

# 트랩 설정
trap cleanup EXIT INT TERM

# 로그 디렉토리 생성
setup_log_directory() {
    log_info "Setting up log directory: $TEST_LOG_DIR"
    mkdir -p "$TEST_LOG_DIR"
    
    # 심볼릭 링크 생성 (최신 로그 쉽게 접근)
    ln -sfn "$TEST_LOG_DIR" "$LOG_DIR/latest"
    
    log_success "Log directory ready"
}

# 빌드 확인
check_build() {
    log_info "Checking build files..."
    
    if [ ! -f "$NODE_BIN" ]; then
        log_error "Node binary not found at: $NODE_BIN"
        log_info "Building project..."
        cd "$BUILD_DIR"
        make -j$(nproc)
    fi
    
    if [ ! -f "$TEST_BIN" ]; then
        log_error "Test binary not found at: $TEST_BIN"
        log_info "Building test..."
        cd "$BUILD_DIR"
        make test_coordinator_with_real_nodes
    fi
    
    log_success "Build files ready"
}

# Node 서버 시작
start_node() {
    local node_id=$1
    local log_file="$TEST_LOG_DIR/${node_id}.log"
    
    log_info "Starting Node: $node_id"
    
    "$NODE_BIN" --id "$node_id" > "$log_file" 2>&1 &
    local pid=$!
    NODE_PIDS+=("$pid")
    
    # 프로세스 확인
    sleep 1
    if ! kill -0 "$pid" 2>/dev/null; then
        log_error "Node $node_id failed to start. Check $log_file"
        cat "$log_file"
        return 1
    fi
    
    log_success "Node $node_id started (PID: $pid, Log: $log_file)"
    return 0
}

# 메인 실행
main() {
    echo "========================================"
    echo "  Coordinator with Real Nodes Test"
    echo "========================================"
    echo ""
    
    # 로그 디렉토리 생성
    setup_log_directory
    
    echo ""
    
    # 빌드 확인
    check_build
    
    echo ""
    log_info "Starting Node servers..."
    echo ""
    
    # Node 서버들 시작
    start_node "node_1" || exit 1
    start_node "node_2" || exit 1
    start_node "node_3" || exit 1
    
    echo ""
    log_info "Waiting for nodes to initialize..."
    sleep 2
    
    echo ""
    log_info "Running test..."
    echo ""
    echo "========================================"
    echo ""
    
    # 테스트 실행
    if "$TEST_BIN"; then
        echo ""
        echo "========================================"
        log_success "All tests PASSED"
        echo "========================================"
        TEST_RESULT=0
    else
        echo ""
        echo "========================================"
        log_error "Tests FAILED"
        echo "========================================"
        TEST_RESULT=1
        
        echo ""
        log_info "Node logs for debugging:"
        echo ""
        for node_id in node_1 node_2 node_3; do
            echo "--- ${node_id}.log (last 20 lines) ---"
            tail -20 "$TEST_LOG_DIR/${node_id}.log"
            echo ""
        done
    fi
    
    echo ""
    log_info "Test logs saved at:"
    echo "  $TEST_LOG_DIR/"
    echo ""
    log_info "Access latest logs via symlink:"
    echo "  $LOG_DIR/latest/"
    echo ""
    
    # 오래된 로그 정리 (30일 이상)
    find "$LOG_DIR" -maxdepth 1 -type d -mtime +30 -exec rm -rf {} \; 2>/dev/null || true
    
    return $TEST_RESULT
}

main "$@"