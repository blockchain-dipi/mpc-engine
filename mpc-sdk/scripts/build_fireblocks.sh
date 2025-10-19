# mpc-sdk/scripts/build_fireblocks.sh
#!/bin/bash
# scripts/build_fireblocks.sh
# Fireblocks MPC-lib Static Library 빌드 스크립트

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 프로젝트 루트 디렉토리
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FIREBLOCKS_DIR="$PROJECT_ROOT/../external/fireblocks-mpc-lib"
BUILD_DIR="$FIREBLOCKS_DIR/build"

log_info "=== Fireblocks MPC-lib Build Script ==="
log_info "Project Root: $PROJECT_ROOT"
log_info "Fireblocks Dir: $FIREBLOCKS_DIR"

# Fireblocks 디렉토리 존재 확인
if [ ! -d "$FIREBLOCKS_DIR" ]; then
    log_error "Fireblocks MPC-lib not found at: $FIREBLOCKS_DIR"
    log_info "Please run: git submodule update --init --recursive"
    exit 1
fi

cd "$FIREBLOCKS_DIR"

# 기존 빌드 디렉토리 삭제 (clean build)
if [ -d "$BUILD_DIR" ]; then
    log_warn "Removing existing build directory..."
    rm -rf "$BUILD_DIR"
fi

# 빌드 디렉토리 생성
log_info "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMake 설정 (Static Library 강제)
log_info "Configuring CMake (Static Library)..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# 빌드
log_info "Building Fireblocks MPC-lib..."
make -j$(nproc)

# 빌드 결과 확인
log_info "Checking build results..."

# .a 파일 검색
STATIC_LIBS=$(find "$BUILD_DIR" -name "*.a" 2>/dev/null)

if [ -z "$STATIC_LIBS" ]; then
    log_error "No static libraries (.a) found!"
    log_info "Found libraries:"
    find "$BUILD_DIR" -name "*.so" -o -name "*.a"
    
    log_warn "Attempting to manually create static library..."
    
    # .o 파일들을 모아서 static library 생성
    COSIGNER_OBJ_DIR="$BUILD_DIR/src/common/CMakeFiles/cosigner.dir"
    if [ -d "$COSIGNER_OBJ_DIR" ]; then
        log_info "Creating static library from object files..."
        cd "$BUILD_DIR/src/common"
        
        # 모든 .o 파일 수집
        OBJECT_FILES=$(find "$COSIGNER_OBJ_DIR" -name "*.o")
        
        if [ ! -z "$OBJECT_FILES" ]; then
            ar rcs libcosigner.a $OBJECT_FILES
            ranlib libcosigner.a
            
            if [ -f "libcosigner.a" ]; then
                log_success "Static library created manually!"
                log_info "Location: $BUILD_DIR/src/common/libcosigner.a"
                ls -lh libcosigner.a
            else
                log_error "Failed to create static library"
                exit 1
            fi
        else
            log_error "No object files found"
            exit 1
        fi
    else
        log_error "Object file directory not found"
        exit 1
    fi
else
    log_success "Static library built successfully!"
    echo "$STATIC_LIBS" | while read lib; do
        log_info "Found: $lib"
        ls -lh "$lib"
    done
fi

# 최종 확인
FINAL_LIB="$BUILD_DIR/src/common/libcosigner.a"
if [ -f "$FINAL_LIB" ]; then
    log_success "=== Fireblocks MPC-lib Build Complete ==="
    log_info "Library path: $FINAL_LIB"
    log_info "Library size: $(ls -lh $FINAL_LIB | awk '{print $5}')"
else
    log_error "Final library not found at expected location"
    log_info "Searching for all .a files..."
    find "$BUILD_DIR" -name "*.a"
    exit 1
fi

log_info ""
log_info "Next steps:"
log_info "  1. Build mpc-sdk: cd $PROJECT_ROOT && mkdir build && cd build"
log_info "  2. Run cmake: cmake .."
log_info "  3. Build: make -j\$(nproc)"

# 테스트 실행 (선택)
# echo ""
# read -p "Run tests? (y/N): " -n 1 -r
# echo
# if [[ $REPLY =~ ^[Yy]$ ]]; then
#     log_info "Running tests..."
#     cd "$BUILD_DIR"
#     ctest --output-on-failure
#     log_success "Tests completed!"
# fi