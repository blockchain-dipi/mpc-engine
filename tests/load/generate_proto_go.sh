#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PROTO_SRC_DIR="${PROJECT_ROOT}/src/proto/wallet_coordinator"
GO_OUT_DIR="${SCRIPT_DIR}/proto"

mkdir -p "${GO_OUT_DIR}"

protoc \
    --go_out="${GO_OUT_DIR}" \
    --go_opt=paths=source_relative \
    --go_opt=Mwallet_common.proto=load-test/proto \
    --go_opt=Mwallet_message.proto=load-test/proto \
    --go_opt=Mwallet_signing.proto=load-test/proto \
    --proto_path="${PROTO_SRC_DIR}" \
    "${PROTO_SRC_DIR}"/*.proto

echo "✅ Generated proto files"