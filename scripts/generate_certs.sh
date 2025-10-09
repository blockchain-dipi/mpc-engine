#!/bin/bash
# scripts/generate_certs.sh
# MPC Engine TLS 인증서 발급 스크립트 (환경 변수 기반)

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 로깅 함수
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

# 설정
CERT_DIR="./certs/generated"  # 발급 전용 임시 폴더
CA_KEY_PASS="mpc-engine-ca-key"

# 환경 변수 기본값 설정
DEFAULT_DOMAIN_SUFFIX=".local"
DEFAULT_DEPLOY_ENV="local"

# 환경 변수 로드 함수
load_environment_config() {
    local env_name=${1:-$DEFAULT_DEPLOY_ENV}
    local env_file="./env/.env.${env_name}"
    
    log_info "환경 설정 로드 중: $env_name"
    
    # 환경 파일 존재 확인
    if [[ ! -f "$env_file" ]]; then
        log_warn "환경 파일이 없습니다: $env_file"
        log_info "기본값 사용: DEPLOY_ENV=$DEFAULT_DEPLOY_ENV, TLS_DOMAIN_SUFFIX=$DEFAULT_DOMAIN_SUFFIX"
        export DEPLOY_ENV="$DEFAULT_DEPLOY_ENV"
        export TLS_DOMAIN_SUFFIX="$DEFAULT_DOMAIN_SUFFIX"
        return 0
    fi
    
    # 환경 변수 로드
    while IFS= read -r line; do
        # 주석과 빈 줄 제외
        if [[ $line =~ ^[[:space:]]*# ]] || [[ -z "${line// }" ]]; then
            continue
        fi
        
        # KEY=VALUE 형태만 처리
        if [[ $line =~ ^[[:space:]]*([A-Z_][A-Z0-9_]*)=(.*)$ ]]; then
            local key="${BASH_REMATCH[1]}"
            local value="${BASH_REMATCH[2]}"
            
            # 줄바꿈 문자 제거 (Windows/Unix 호환성)
            value=$(echo "$value" | tr -d '\r\n' | xargs)
            
            # 필요한 환경 변수만 export
            case "$key" in
                "DEPLOY_ENV"|"TLS_DOMAIN_SUFFIX"|"TLS_COORDINATOR_FQDN"|"TLS_NODE_FQDNS"|"NODE_IDS")
                    export "$key=$value"
                    ;;
            esac
        fi
    done < "$env_file"
    
    # 기본값 설정
    export DEPLOY_ENV="${DEPLOY_ENV:-$DEFAULT_DEPLOY_ENV}"
    export TLS_DOMAIN_SUFFIX="${TLS_DOMAIN_SUFFIX:-$DEFAULT_DOMAIN_SUFFIX}"
    
    log_success "환경 설정 로드 완료"
    log_info "  DEPLOY_ENV: $DEPLOY_ENV"
    log_info "  TLS_DOMAIN_SUFFIX: $TLS_DOMAIN_SUFFIX"
    
    # NODE_IDS 파싱 (선택적, 공백 제거)
    if [[ -n "$NODE_IDS" ]]; then
        IFS=',' read -ra NODE_ID_ARRAY <<< "$NODE_IDS"
        # 각 노드 ID에서 공백과 줄바꿈 문자 제거
        for i in "${!NODE_ID_ARRAY[@]}"; do
            NODE_ID_ARRAY[i]=$(echo "${NODE_ID_ARRAY[i]}" | tr -d '\r\n' | xargs)
        done
        log_info "  NODE_IDS: ${NODE_ID_ARRAY[*]}"
    fi
}

# 사용법 출력 (환경 지원 추가)
show_usage() {
    echo "사용법: $0 <target> [environment]"
    echo ""
    echo "발급 명령어:"
    echo "  all [env]        - 모든 인증서 발급 (CA + coordinator + nodes)"
    echo "  ca [env]         - CA 인증서만 발급"
    echo "  coordinator [env] - Coordinator 서버 인증서 발급"
    echo "  node1 [env]      - Node 1 서버 인증서 발급"
    echo "  node2 [env]      - Node 2 서버 인증서 발급"
    echo "  node3 [env]      - Node 3 서버 인증서 발급"
    echo ""
    echo "관리 명령어:"
    echo "  info             - 발급된 인증서 정보 확인"
    echo "  verify           - 인증서 유효성 검증"
    echo "  copy <dir>       - 인증서를 지정된 디렉토리로 복사"
    echo "  clean            - 발급 폴더 정리"
    echo "  help             - 이 도움말 출력"
    echo ""
    echo "환경별 발급 예시:"
    echo "  $0 all local                    # 로컬 개발 환경 (.local)"
    echo "  $0 all dev                      # 개발 서버 환경 (.dev.mpc-engine.com)"
    echo "  $0 all prod                     # 프로덕션 환경 (.mpc-engine.com)"
    echo "  $0 coordinator staging          # 스테이징용 coordinator만"
    echo ""
    echo "배포 예시:"
    echo "  $0 copy certs/local      # 로컬 개발용으로 복사"
    echo "  $0 copy certs/dev        # 개발 서버용으로 복사"
    echo "  $0 copy /media/usb       # USB로 복사"
    echo ""
    echo "지원하는 환경:"
    echo "  local   - 로컬 개발 환경 (기본값)"
    echo "  dev     - 개발 서버 환경"
    echo "  staging - 스테이징 환경"
    echo "  prod    - 프로덕션 환경"
    echo ""
    echo "환경 설정 파일:"
    echo "  ./env/.env.local    - 로컬 개발용"
    echo "  ./env/.env.dev      - 개발 서버용"
    echo "  ./env/.env.staging  - 스테이징용"
    echo "  ./env/.env.prod     - 프로덕션용"
    echo ""
    echo "⚠️  발급된 인증서는 certs/generated/에 저장됩니다."
    echo "   필요한 환경으로 복사 후 발급 폴더를 정리하세요."
}

# 디렉토리 생성
setup_directories() {
    log_info "인증서 발급 디렉토리 설정: $CERT_DIR"
    mkdir -p "$CERT_DIR"
    
    # 기존 발급 파일 정리 (새로 시작)
    if [[ -d "$CERT_DIR" && "$(ls -A "$CERT_DIR" 2>/dev/null)" ]]; then
        log_warn "기존 발급 파일이 있습니다. 정리 중..."
        rm -rf "$CERT_DIR"/*
    fi
    
    # .gitignore 생성 (generated 폴더 제외)
    local gitignore_file="./certs/.gitignore"
    if [[ ! -f "$gitignore_file" ]]; then
        mkdir -p "./certs"
        echo "# 발급 임시 폴더 제외" > "$gitignore_file"
        echo "generated/" >> "$gitignore_file"
        echo "" >> "$gitignore_file"
        echo "# 환경별 인증서 관리" >> "$gitignore_file"
        echo "# local/ - Git 포함 (개발용)" >> "$gitignore_file"  
        echo "# dev/ - Git 제외 (개발 서버용)" >> "$gitignore_file"
        echo "dev/" >> "$gitignore_file"
        echo "# staging/ - Git 제외 (스테이징용)" >> "$gitignore_file"
        echo "staging/" >> "$gitignore_file"
        echo "# prod/ - Git 제외 (프로덕션용)" >> "$gitignore_file"
        echo "prod/" >> "$gitignore_file"
        log_info ".gitignore 생성 완료"
    fi
}

# CA 인증서 존재 확인
check_ca_exists() {
    if [[ -f "$CERT_DIR/ca-cert.pem" && -f "$CERT_DIR/ca-key.pem" ]]; then
        return 0  # CA 존재
    else
        return 1  # CA 없음
    fi
}

# CA 인증서 발급
generate_ca() {
    log_info "Root CA 인증서 발급 중..."
    log_info "  환경: $DEPLOY_ENV"
    log_info "  도메인 접미사: $TLS_DOMAIN_SUFFIX"
    
    # CA 개인키 생성
    openssl genpkey -algorithm RSA -out "$CERT_DIR/ca-key.pem" -pkcs8 -aes256 \
        -pass pass:$CA_KEY_PASS
    
    # CA 인증서 생성 (환경별 정보 포함)
    local ca_common_name="MPC Engine Root CA ($DEPLOY_ENV)"
    
    openssl req -new -x509 -key "$CERT_DIR/ca-key.pem" -sha256 -days 3650 \
        -out "$CERT_DIR/ca-cert.pem" \
        -passin pass:$CA_KEY_PASS \
        -config <(cat << EOF
[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_ca
prompt = no

[req_distinguished_name]
C = KR
ST = Seoul
L = Seoul
O = MPC Engine Development
OU = Security Team ($DEPLOY_ENV)
CN = $ca_common_name

[v3_ca]
basicConstraints = critical,CA:TRUE
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer
EOF
)
    
    log_success "Root CA 인증서 발급 완료"
    log_info "  CA 인증서: $CERT_DIR/ca-cert.pem"
    log_info "  CA 개인키: $CERT_DIR/ca-key.pem (암호: $CA_KEY_PASS)"
    log_info "  CA CN: $ca_common_name"
}

# 서버 인증서 발급 (환경 변수 기반)
generate_server_cert() {
    local server_name=$1
    # 줄바꿈 문자 제거
    server_name=$(echo "$server_name" | tr -d '\r\n' | xargs)
    local common_name="$server_name$TLS_DOMAIN_SUFFIX"
    
    log_info "$server_name 서버 인증서 발급 중..."
    log_info "  환경: $DEPLOY_ENV"
    log_info "  FQDN: $common_name"
    
    # 서버 개인키 생성
    openssl genpkey -algorithm RSA -out "$CERT_DIR/$server_name-key.pem"
    
    # SAN 설정 (환경별)
    local alt_names="DNS.1 = $common_name"
    alt_names="$alt_names"$'\n'"DNS.2 = localhost"
    alt_names="$alt_names"$'\n'"IP.1 = 127.0.0.1"
    alt_names="$alt_names"$'\n'"IP.2 = ::1"
    
    # 추가 SAN (환경별로 다를 수 있음)
    case "$DEPLOY_ENV" in
        "local")
            alt_names="$alt_names"$'\n'"DNS.3 = $server_name"
            ;;
        "dev"|"staging"|"prod")
            alt_names="$alt_names"$'\n'"DNS.3 = $server_name.internal"
            alt_names="$alt_names"$'\n'"DNS.4 = $server_name-service"
            ;;
    esac
    
    # CSR 생성
    openssl req -new -key "$CERT_DIR/$server_name-key.pem" \
        -out "$CERT_DIR/$server_name.csr" \
        -config <(cat << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
C = KR
ST = Seoul
L = Seoul
O = MPC Engine
OU = $server_name Server ($DEPLOY_ENV)
CN = $common_name

[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation,digitalSignature,keyEncipherment
subjectAltName = @alt_names

[alt_names]
$alt_names
EOF
)
    
    # 서버 인증서 발급
    openssl x509 -req -in "$CERT_DIR/$server_name.csr" \
        -CA "$CERT_DIR/ca-cert.pem" -CAkey "$CERT_DIR/ca-key.pem" \
        -CAcreateserial -out "$CERT_DIR/$server_name-cert.pem" \
        -days 1825 -sha256 \
        -passin pass:$CA_KEY_PASS \
        -extensions v3_req -extfile <(cat << EOF
[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation,digitalSignature,keyEncipherment
subjectAltName = @alt_names

[alt_names]
$alt_names
EOF
)
    
    log_success "$server_name 서버 인증서 발급 완료"
    log_info "  인증서: $CERT_DIR/$server_name-cert.pem"
    log_info "  개인키: $CERT_DIR/$server_name-key.pem"
    log_info "  CN: $common_name"
}

# Coordinator 인증서 발급
generate_coordinator() {
    if ! check_ca_exists; then
        log_warn "CA 인증서가 없습니다. CA부터 발급합니다."
        generate_ca
    fi
    
    generate_server_cert "coordinator"
}

# Node 인증서 발급
generate_node() {
    local node_id=$1
    
    if ! check_ca_exists; then
        log_warn "CA 인증서가 없습니다. CA부터 발급합니다."
        generate_ca
    fi
    
    generate_server_cert "$node_id"
}

# 모든 인증서 발급 (환경 설정 기반)
generate_all() {
    log_info "=== 전체 인증서 발급 시작 ==="
    log_info "환경: $DEPLOY_ENV"
    log_info "도메인 접미사: $TLS_DOMAIN_SUFFIX"
    
    # 1. CA 발급
    if check_ca_exists; then
        log_warn "CA 인증서가 이미 존재합니다."
        echo -n "기존 CA를 삭제하고 새로 발급하시겠습니까? [y/N]: "
        read -r answer
        if [[ $answer =~ ^[Yy]$ ]]; then
            rm -f "$CERT_DIR/ca-"*
            generate_ca
        else
            log_info "기존 CA 인증서를 사용합니다."
        fi
    else
        generate_ca
    fi
    
    # 2. Coordinator 인증서 발급
    generate_server_cert "coordinator"
    
    # 3. Node 인증서들 발급 (NODE_IDS 환경 변수 기반 또는 기본값)
    if [[ -n "$NODE_IDS" ]]; then
        IFS=',' read -ra NODE_ID_ARRAY <<< "$NODE_IDS"
        # 각 노드 ID에서 공백과 줄바꿈 문자 제거
        for i in "${!NODE_ID_ARRAY[@]}"; do
            NODE_ID_ARRAY[i]=$(echo "${NODE_ID_ARRAY[i]}" | tr -d '\r\n' | xargs)
        done
        for node_id in "${NODE_ID_ARRAY[@]}"; do
            generate_server_cert "$node_id"
        done
    else
        # 기본값: node1, node2, node3
        generate_server_cert "node1"
        generate_server_cert "node2"
        generate_server_cert "node3"
    fi
    
    log_success "=== 전체 인증서 발급 완료 ==="
    log_info "발급된 FQDN 목록:"
    log_info "  CA: MPC Engine Root CA ($DEPLOY_ENV)"
    log_info "  Coordinator: coordinator$TLS_DOMAIN_SUFFIX"
    
    if [[ -n "$NODE_IDS" ]]; then
        IFS=',' read -ra NODE_ID_ARRAY <<< "$NODE_IDS"
        # 각 노드 ID에서 공백과 줄바꿈 문자 제거
        for i in "${!NODE_ID_ARRAY[@]}"; do
            NODE_ID_ARRAY[i]=$(echo "${NODE_ID_ARRAY[i]}" | tr -d '\r\n' | xargs)
        done
        for node_id in "${NODE_ID_ARRAY[@]}"; do
            log_info "  Node: $node_id$TLS_DOMAIN_SUFFIX"
        done
    else
        log_info "  Nodes: node1$TLS_DOMAIN_SUFFIX, node2$TLS_DOMAIN_SUFFIX, node3$TLS_DOMAIN_SUFFIX"
    fi
    
    show_deployment_guide
}

# 인증서 정보 출력
show_cert_info() {
    if [[ ! -d "$CERT_DIR" ]]; then
        log_error "인증서 발급 디렉토리가 존재하지 않습니다: $CERT_DIR"
        return 1
    fi
    
    log_info "=== 발급된 인증서 목록 ==="
    echo "📁 발급 디렉토리: $CERT_DIR"
    echo "🌍 환경: ${DEPLOY_ENV:-unknown}"
    echo "🏷️  도메인 접미사: ${TLS_DOMAIN_SUFFIX:-unknown}"
    echo "⚠️  임시 폴더입니다. 필요한 곳으로 복사하여 사용하세요."
    echo ""
    
    if [[ -f "$CERT_DIR/ca-cert.pem" ]]; then
        echo "🔐 Root CA:"
        echo "  인증서: ca-cert.pem"
        echo "  개인키: ca-key.pem (암호화됨)"
        
        # CA 인증서 정보 출력
        echo "  만료일: $(openssl x509 -in "$CERT_DIR/ca-cert.pem" -noout -enddate | cut -d= -f2)"
        echo "  CN: $(openssl x509 -in "$CERT_DIR/ca-cert.pem" -noout -subject | sed 's/.*CN = //')"
        echo ""
    fi
    
    # 서버 인증서들 확인
    for cert_file in "$CERT_DIR"/*-cert.pem; do
        if [[ -f "$cert_file" ]]; then
            local server_name=$(basename "$cert_file" -cert.pem)
            echo "🖥️  $server_name 서버:"
            echo "  인증서: $server_name-cert.pem"
            echo "  개인키: $server_name-key.pem"
            echo "  만료일: $(openssl x509 -in "$cert_file" -noout -enddate | cut -d= -f2)"
            echo "  CN: $(openssl x509 -in "$cert_file" -noout -subject | sed 's/.*CN = //')"
            
            # SAN 정보 출력
            local san_info=$(openssl x509 -in "$cert_file" -noout -text | grep -A1 "Subject Alternative Name" | tail -1 | sed 's/^ *//')
            if [[ -n "$san_info" ]]; then
                echo "  SAN: $san_info"
            fi
            echo ""
        fi
    done
    
    # 전체 파일 목록
    echo "📋 전체 파일:"
    ls -la "$CERT_DIR/" | grep -E '\.(pem|crt|key)$' || echo "  인증서 파일이 없습니다."
}

# 인증서 유효성 검증
verify_certificates() {
    log_info "인증서 유효성 검증 중..."
    
    if [[ ! -f "$CERT_DIR/ca-cert.pem" ]]; then
        log_error "CA 인증서가 없습니다."
        return 1
    fi
    
    # CA 자체 검증
    if openssl x509 -in "$CERT_DIR/ca-cert.pem" -noout -text > /dev/null 2>&1; then
        log_success "CA 인증서 유효"
    else
        log_error "CA 인증서 무효"
        return 1
    fi
    
    # 서버 인증서들 검증
    for cert_file in "$CERT_DIR"/*-cert.pem; do
        if [[ -f "$cert_file" ]]; then
            local server_name=$(basename "$cert_file" -cert.pem)
            if openssl verify -CAfile "$CERT_DIR/ca-cert.pem" "$cert_file" > /dev/null 2>&1; then
                log_success "$server_name 인증서 유효"
            else
                log_error "$server_name 인증서 무효"
                return 1
            fi
        fi
    done
    
    log_success "모든 인증서 검증 완료"
}

# 배포 가이드 출력 (환경별)
show_deployment_guide() {
    echo ""
    log_info "=== 인증서 배포 가이드 ($DEPLOY_ENV 환경) ==="
    echo "발급된 인증서를 사용할 환경으로 복사하세요:"
    echo ""
    
    case "$DEPLOY_ENV" in
        "local")
            echo "📋 로컬 개발 환경:"
            echo "  $0 copy certs/local"
            echo ""
            ;;
        "dev")
            echo "📋 개발 서버 환경:"
            echo "  $0 copy certs/dev"
            echo "  scp -r certs/dev/* dev-server:/etc/mpc-engine/certs/"
            echo ""
            ;;
        "staging")
            echo "📋 스테이징 환경:"
            echo "  $0 copy certs/staging"
            echo "  kubectl create secret tls mpc-tls --cert=certs/staging/ --key=certs/staging/"
            echo ""
            ;;
        "prod")
            echo "📋 프로덕션 환경 (보안 주의):"
            echo "  $0 copy /media/usb/mpc-certs/"
            echo "  # USB로 각 서버에 수동 배포"
            echo "  # KMS에 업로드 후 즉시 로컬 삭제"
            echo ""
            ;;
    esac
    
    echo "⚠️  중요: 복사 완료 후 발급 폴더 정리"
    echo "  $0 clean"
    echo ""
    log_warn "발급 폴더($CERT_DIR)는 임시용입니다."
    log_warn "필요한 곳으로 복사 후 보안을 위해 삭제하는 것을 권장합니다."
}

# 인증서 복사 헬퍼 함수
copy_certificates() {
    local target_dir=$1
    
    if [[ -z "$target_dir" ]]; then
        log_error "복사 대상 디렉토리를 지정하세요."
        echo "사용법: copy_certificates <target_directory>"
        return 1
    fi
    
    if [[ ! -d "$CERT_DIR" ]]; then
        log_error "발급된 인증서가 없습니다. 먼저 인증서를 발급하세요."
        return 1
    fi
    
    log_info "인증서 복사 중: $CERT_DIR -> $target_dir"
    mkdir -p "$target_dir"
    cp -r "$CERT_DIR"/* "$target_dir/"
    
    log_success "인증서 복사 완료: $target_dir"
    
    # 환경 정보 파일 생성
    echo "# 인증서 환경 정보" > "$target_dir/cert-env-info.txt"
    echo "DEPLOY_ENV=${DEPLOY_ENV:-unknown}" >> "$target_dir/cert-env-info.txt"
    echo "TLS_DOMAIN_SUFFIX=${TLS_DOMAIN_SUFFIX:-unknown}" >> "$target_dir/cert-env-info.txt"
    echo "GENERATED_AT=$(date)" >> "$target_dir/cert-env-info.txt"
    
    # 복사 후 발급 폴더 정리 여부 확인
    echo ""
    echo -n "발급 폴더($CERT_DIR)를 정리하시겠습니까? [y/N]: "
    read -r answer
    if [[ $answer =~ ^[Yy]$ ]]; then
        rm -rf "$CERT_DIR"
        log_success "발급 폴더 정리 완료"
    fi
}

# 정리 작업
cleanup() {
    # CSR 파일 삭제
    rm -f "$CERT_DIR"/*.csr
    
    # Zone.Identifier 파일 삭제 (Windows 호환성)
    find "$CERT_DIR" -name "*Zone.Identifier*" -delete 2>/dev/null || true
    
    # 공백이 포함된 잘못된 파일명 정리
    find "$CERT_DIR" -name "* -*" -type f 2>/dev/null | while read -r file; do
        if [[ -f "$file" ]]; then
            # 공백 제거한 새 파일명
            new_name=$(echo "$file" | sed 's/ -/-/g')
            if [[ "$file" != "$new_name" ]]; then
                log_warn "파일명 수정: $(basename "$file") -> $(basename "$new_name")"
                mv "$file" "$new_name"
            fi
        fi
    done
}

# 메인 로직
main() {
    if [[ $# -eq 0 ]]; then
        show_usage
        exit 1
    fi
    
    local target=$1
    local environment=${2:-$DEFAULT_DEPLOY_ENV}
    
    # 환경 설정 로드 (copy, clean, info, verify 제외)
    case $target in
        "copy"|"clean"|"info"|"list"|"verify"|"help"|"-h"|"--help")
            # 이 명령들은 환경 로드 불필요
            ;;
        *)
            load_environment_config "$environment"
            setup_directories
            ;;
    esac
    
    case $target in
        "all")
            generate_all
            cleanup
            show_cert_info
            verify_certificates
            ;;
        "ca")
            generate_ca
            cleanup
            show_cert_info
            ;;
        "coordinator")
            generate_coordinator
            cleanup
            show_cert_info
            ;;
        "node1"|"node2"|"node3")
            generate_node "$target"
            cleanup
            show_cert_info
            ;;
        "copy")
            if [[ $# -lt 2 ]]; then
                log_error "복사 대상 디렉토리를 지정하세요."
                echo "사용법: $0 copy <target_directory>"
                echo "예시: $0 copy certs/local"
                exit 1
            fi
            copy_certificates "$2"
            ;;
        "clean")
            if [[ -d "$CERT_DIR" ]]; then
                echo -n "발급 폴더($CERT_DIR)를 삭제하시겠습니까? [y/N]: "
                read -r answer
                if [[ $answer =~ ^[Yy]$ ]]; then
                    rm -rf "$CERT_DIR"
                    log_success "발급 폴더 삭제 완료"
                else
                    log_info "삭제가 취소되었습니다."
                fi
            else
                log_info "발급 폴더가 존재하지 않습니다."
            fi
            ;;
        "info"|"list")
            show_cert_info
            ;;
        "verify")
            verify_certificates
            ;;
        "help"|"-h"|"--help")
            show_usage
            ;;
        *)
            log_error "알 수 없는 대상: $target"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# 스크립트 실행
main "$@"