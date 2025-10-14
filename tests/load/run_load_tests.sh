#!/bin/bash

# tests/load/run_load_tests.sh
# Coordinator + Node 서버를 실행하고 Go 부하 테스트를 수행

set -e

# ========================================
# 설정
# ========================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# 색상
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# ========================================
# 환경 확인
# ========================================

check_environment() {
    log_info "Checking environment..."
    
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        exit 1
    fi
    
    if [ ! -f "$BUILD_DIR/coordinator" ]; then
        log_error "Coordinator binary not found"
        exit 1
    fi
    
    if [ ! -f "$BUILD_DIR/node" ]; then
        log_error "Node binary not found"
        exit 1
    fi
    
    if [ ! -f "$PROJECT_ROOT/env/.env.local" ]; then
        log_error "Environment config not found: env/.env.local"
        exit 1
    fi
    
    log_success "Environment check passed"
}

# ========================================
# 서버 관리
# ========================================

NODE1_PID=""
NODE2_PID=""
NODE3_PID=""
COORDINATOR_PID=""

start_servers() {
    log_info "Starting Node servers..."
    
    # 프로젝트 루트에서 실행 (중요!)
    cd "$PROJECT_ROOT"
    
    "$BUILD_DIR/node" --env local --id node1 > /tmp/node1.log 2>&1 &
    NODE1_PID=$!
    log_success "Node 1 started (PID: $NODE1_PID)"
    
    "$BUILD_DIR/node" --env local --id node2 > /tmp/node2.log 2>&1 &
    NODE2_PID=$!
    log_success "Node 2 started (PID: $NODE2_PID)"
    
    "$BUILD_DIR/node" --env local --id node3 > /tmp/node3.log 2>&1 &
    NODE3_PID=$!
    log_success "Node 3 started (PID: $NODE3_PID)"
    
    log_info "Waiting for Nodes to initialize (3 seconds)..."
    sleep 3
    
    log_info "Starting Coordinator server..."
    "$BUILD_DIR/coordinator" local > /tmp/coordinator.log 2>&1 &
    COORDINATOR_PID=$!
    log_success "Coordinator started (PID: $COORDINATOR_PID)"
    
    log_info "Waiting for Coordinator to initialize (5 seconds)..."
    sleep 5
    
    if ! ps -p $NODE1_PID > /dev/null; then
        log_error "Node 1 failed to start. Check /tmp/node1.log"
        cat /tmp/node1.log
        cleanup
        exit 1
    fi
    
    if ! ps -p $COORDINATOR_PID > /dev/null; then
        log_error "Coordinator failed to start. Check /tmp/coordinator.log"
        cat /tmp/coordinator.log
        cleanup
        exit 1
    fi
    
    log_success "All servers started successfully"
}

cleanup() {
    log_info "Stopping servers..."
    
    [ ! -z "$COORDINATOR_PID" ] && kill $COORDINATOR_PID 2>/dev/null || true
    [ ! -z "$NODE1_PID" ] && kill $NODE1_PID 2>/dev/null || true
    [ ! -z "$NODE2_PID" ] && kill $NODE2_PID 2>/dev/null || true
    [ ! -z "$NODE3_PID" ] && kill $NODE3_PID 2>/dev/null || true
    
    sleep 2
    log_success "Cleanup completed"
}

trap cleanup EXIT INT TERM

# ========================================
# Go 테스트 실행
# ========================================

run_go_test() {
    log_info "========================================="
    log_info "Running Go HTTPS Load Test"
    log_info "========================================="
    
    cd "$SCRIPT_DIR"
    
    # Proto 생성
    if [ ! -d "proto" ] || [ ! -f "proto/wallet_message.pb.go" ]; then
        log_info "Generating Go proto files..."
        if [ -f "generate_proto_go.sh" ]; then
            chmod +x generate_proto_go.sh
            ./generate_proto_go.sh
        else
            log_error "generate_proto_go.sh not found"
            return 1
        fi
    fi
    
    # Go 모듈 초기화
    if [ ! -f "go.mod" ]; then
        log_info "Initializing Go module..."
        go mod init load-test
        go get google.golang.org/protobuf/proto
    fi
    
    # 테스트 실행
    if go run load_client.go; then
        log_success "Go load test PASSED"
        return 0
    else
        log_error "Go load test FAILED"
        return 1
    fi
}

show_logs() {
    log_info "========================================="
    log_info "Server Logs"
    log_info "========================================="
    
    echo ""
    log_info "--- Node 1 Log (last 10 lines) ---"
    tail -n 10 /tmp/node1.log 2>/dev/null || echo "No log file"
    
    echo ""
    log_info "--- Coordinator Log (last 10 lines) ---"
    tail -n 10 /tmp/coordinator.log 2>/dev/null || echo "No log file"
}

# ========================================
# Main
# ========================================

main() {
    echo ""
    log_info "========================================="
    log_info "MPC Engine Go Load Test Runner"
    log_info "========================================="
    echo ""
    
    check_environment
    start_servers
    
    if run_go_test; then
        log_success "✓ Go load test PASSED! 🎉"
        exit 0
    else
        log_error "✗ Go load test FAILED!"
        show_logs
        exit 1
    fi
}

main