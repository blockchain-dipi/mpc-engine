# scripts/generate_proto.sh
#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROTO_SRC_DIR="${PROJECT_ROOT}/src/proto"

# protoc 확인
if ! command -v protoc &> /dev/null; then
    echo "Error: protoc not found. Please install Protocol Buffers compiler."
    exit 1
fi

echo "=== Generating Protocol Buffer files ==="

# Coordinator-Node proto 파일들 생성
PROTO_FILES=(
    "coordinator_node/common.proto"
    "coordinator_node/signing.proto"
    "coordinator_node/message.proto"
)

for proto_file in "${PROTO_FILES[@]}"; do
    # 출력 디렉토리 추출 (예: coordinator_node)
    proto_dir=$(dirname "$proto_file")
    output_dir="${PROTO_SRC_DIR}/${proto_dir}/generated"
    
    # 출력 디렉토리 생성
    mkdir -p "$output_dir"
    
    echo "Generating ${proto_file}..."
    protoc \
        --cpp_out="${output_dir}" \
        --proto_path="${PROTO_SRC_DIR}/${proto_dir}" \
        "${PROTO_SRC_DIR}/${proto_file}"
done

echo ""
echo "✅ Proto generation completed!"
echo ""
echo "Generated files:"

# 모든 generated 폴더에서 생성된 파일 찾기
find "${PROTO_SRC_DIR}" -path "*/generated/*.pb.h" -o -path "*/generated/*.pb.cc" 2>/dev/null | sort || echo "No files generated"