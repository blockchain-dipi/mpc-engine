# generate_certs.sh 사용법

MPC Engine의 TLS 인증서 발급 및 관리 스크립트입니다.

## 개요

내부 서버 간 TLS 통신을 위한 Self-signed 인증서를 발급합니다.

**발급되는 인증서:**
- Root CA (1개) - 모든 서버 인증서의 서명용
- Coordinator 서버 인증서 (1개) - HTTPS 서버용  
- Node 서버 인증서 (3개) - TCP over TLS 서버용

**발급 위치:** `./certs/generated/` (임시 폴더)

**새로운 기능:** 환경별 도메인 설정 지원 (`TLS_DOMAIN_SUFFIX` 환경 변수 기반)

## 사용법

### 환경별 인증서 발급 (권장)

```bash
# 로컬 개발 환경 (.local 도메인)
./scripts/generate_certs.sh all local

# 개발 서버 환경 (.dev.mpc-engine.com 도메인)
./scripts/generate_certs.sh all dev

# 스테이징 환경 (.staging.mpc-engine.com 도메인)
./scripts/generate_certs.sh all staging

# 프로덕션 환경 (.mpc-engine.com 도메인)
./scripts/generate_certs.sh all prod
```

### 개별 서버 인증서 발급

```bash
# 환경 지정하여 개별 발급
./scripts/generate_certs.sh coordinator dev
./scripts/generate_certs.sh node1 local
./scripts/generate_certs.sh node2 prod

# CA 인증서만 발급
./scripts/generate_certs.sh ca local
```

### 관리 명령어

```bash
# 발급된 인증서 목록 및 정보 확인
./scripts/generate_certs.sh info

# 인증서 유효성 검증
./scripts/generate_certs.sh verify

# 인증서를 특정 디렉토리로 복사
./scripts/generate_certs.sh copy <target_directory>

# 발급 폴더 정리 (삭제)
./scripts/generate_certs.sh clean

# 도움말 출력
./scripts/generate_certs.sh help
```

## 환경별 설정

### 지원하는 환경

| 환경 | 설정 파일 | 도메인 접미사 | 용도 |
|------|-----------|---------------|------|
| `local` | `./env/.env.local` | `.local` | 로컬 개발 |
| `dev` | `./env/.env.dev` | `.dev.mpc-engine.com` | 개발 서버 |
| `staging` | `./env/.env.staging` | `.staging.mpc-engine.com` | 스테이징 |
| `prod` | `./env/.env.prod` | `.mpc-engine.com` | 프로덕션 |

### 환경 설정 파일 예시

#### local 환경 (./env/.env.local)
```bash
DEPLOY_ENV=local
TLS_DOMAIN_SUFFIX=.local
NODE_IDS=node1,node2,node3
```

#### dev 환경 (./env/.env.dev)
```bash
DEPLOY_ENV=dev
TLS_DOMAIN_SUFFIX=.dev.mpc-engine.com
NODE_IDS=node1,node2,node3
```

#### prod 환경 (./env/.env.prod)
```bash
DEPLOY_ENV=prod
TLS_DOMAIN_SUFFIX=.mpc-engine.com
NODE_IDS=node1,node2,node3
```

## 상세 사용 가이드

### 1. 처음 설정 (환경별)

#### 로컬 개발 환경
```bash
# 1. 로컬 환경용 인증서 발급
./scripts/generate_certs.sh all local

# 2. 로컬 개발용으로 복사
./scripts/generate_certs.sh copy certs/local

# 3. 발급 폴더 정리 (선택사항)
./scripts/generate_certs.sh clean
```

**발급되는 인증서:**
- `coordinator.local`
- `node1.local`, `node2.local`, `node3.local`

#### 개발 서버 환경
```bash
# 1. 개발 서버용 인증서 발급
./scripts/generate_certs.sh all dev

# 2. 개발 서버용으로 복사
./scripts/generate_certs.sh copy certs/dev

# 3. 발급 폴더 정리
./scripts/generate_certs.sh clean
```

**발급되는 인증서:**
- `coordinator.dev.mpc-engine.com`
- `node1.dev.mpc-engine.com`, `node2.dev.mpc-engine.com`, `node3.dev.mpc-engine.com`

#### 프로덕션 환경
```bash
# 1. 프로덕션용 인증서 발급
./scripts/generate_certs.sh all prod

# 2. USB로 안전하게 복사
./scripts/generate_certs.sh copy /media/usb/mpc-certs/

# 3. 보안을 위해 즉시 발급 폴더 삭제
./scripts/generate_certs.sh clean
```

**발급되는 인증서:**
- `coordinator.mpc-engine.com`
- `node1.mpc-engine.com`, `node2.mpc-engine.com`, `node3.mpc-engine.com`

### 2. 출력 예시 (환경별)

#### 로컬 환경 발급 출력
```
[INFO] 환경 설정 로드 중: local
[SUCCESS] 환경 설정 로드 완료
  DEPLOY_ENV: local
  TLS_DOMAIN_SUFFIX: .local
  NODE_IDS: node1,node2,node3

=== 발급된 인증서 목록 ===
📁 발급 디렉토리: ./certs/generated
🌍 환경: local
🏷️  도메인 접미사: .local

🔐 Root CA:
  인증서: ca-cert.pem
  개인키: ca-key.pem (암호화됨)
  만료일: Apr  7 06:15:23 2030 GMT
  CN: MPC Engine Root CA (local)

🖥️  coordinator 서버:
  인증서: coordinator-cert.pem
  개인키: coordinator-key.pem
  만료일: Apr  6 06:15:24 2030 GMT
  CN: coordinator.local
  SAN: DNS:coordinator.local, DNS:localhost, DNS:coordinator, IP:127.0.0.1, IP:::1

🖥️  node1 서버:
  인증서: node1-cert.pem
  개인키: node1-key.pem  
  만료일: Apr  6 06:15:25 2030 GMT
  CN: node1.local
  SAN: DNS:node1.local, DNS:localhost, DNS:node1, IP:127.0.0.1, IP:::1
```

#### 프로덕션 환경 발급 출력
```
[INFO] 환경 설정 로드 중: prod
[SUCCESS] 환경 설정 로드 완료
  DEPLOY_ENV: prod
  TLS_DOMAIN_SUFFIX: .mpc-engine.com

🖥️  coordinator 서버:
  CN: coordinator.mpc-engine.com
  SAN: DNS:coordinator.mpc-engine.com, DNS:localhost, DNS:coordinator.internal, DNS:coordinator-service

🖥️  node1 서버:
  CN: node1.mpc-engine.com
  SAN: DNS:node1.mpc-engine.com, DNS:localhost, DNS:node1.internal, DNS:node1-service
```

### 3. 개별 서버 인증서 재발급 (환경별)

특정 서버의 인증서만 갱신해야 할 때:

```bash
# 개발 환경의 Coordinator 인증서만 재발급
./scripts/generate_certs.sh coordinator dev

# 프로덕션 환경의 특정 Node만 재발급
./scripts/generate_certs.sh node1 prod

# 기존 환경에 복사
./scripts/generate_certs.sh copy certs/dev

# 발급 폴더 정리
./scripts/generate_certs.sh clean
```

**중요한 전제조건:**
개별 서버 인증서를 발급하려면 기존 CA 인증서가 `certs/generated/` 폴더에 있어야 합니다.

**CA 파일이 없을 때 해결법:**
```bash
# 방법 1: 전체 재발급 (CA 포함)
./scripts/generate_certs.sh all dev

# 방법 2: 기존 CA를 generated/ 폴더에 복사
cp certs/dev/ca-* certs/generated/
./scripts/generate_certs.sh coordinator dev

# 방법 3: USB에서 CA 가져오기 (프로덕션)
cp /media/usb/ca-* certs/generated/
./scripts/generate_certs.sh coordinator prod
```

### 4. 환경별 배포 워크플로우

#### 로컬 개발 시작
```bash
# 1. 로컬 환경 인증서 발급
./scripts/generate_certs.sh all local

# 2. 로컬 환경으로 복사  
./scripts/generate_certs.sh copy certs/local

# 3. 발급 폴더 정리
./scripts/generate_certs.sh clean

# 4. 개발 시작
./build/coordinator local
```

#### 개발 서버 배포
```bash
# 1. 개발 서버용 인증서 발급
./scripts/generate_certs.sh all dev

# 2. 개발 서버로 복사
./scripts/generate_certs.sh copy certs/dev

# 3. 서버에 배포 (scp, rsync 등)
scp -r certs/dev/* dev-server:/etc/mpc-engine/certs/

# 4. 발급 폴더 정리
./scripts/generate_certs.sh clean
```

#### 스테이징 환경 배포
```bash
# 1. 스테이징용 인증서 발급
./scripts/generate_certs.sh all staging

# 2. 스테이징으로 복사
./scripts/generate_certs.sh copy certs/staging

# 3. Kubernetes Secret 생성
kubectl create secret tls mpc-tls \
  --cert=certs/staging/coordinator-cert.pem \
  --key=certs/staging/coordinator-key.pem

# 4. 발급 폴더 정리
./scripts/generate_certs.sh clean
```

#### 프로덕션 환경 배포
```bash
# 1. 프로덕션용 인증서 발급
./scripts/generate_certs.sh all prod

# 2. USB로 안전하게 복사
./scripts/generate_certs.sh copy /media/usb/mpc-certs/

# 3. KMS에 업로드 후 즉시 로컬 삭제
aws kms put-secret --secret-id prod-coordinator-cert --secret-value file://certs/prod/coordinator-cert.pem
rm -rf certs/prod

# 4. 발급 폴더 정리
./scripts/generate_certs.sh clean
```

### 5. 환경별 인증서 갱신

#### 개별 서버 인증서만 갱신
```bash
# 1. 환경 지정하여 특정 서버만 갱신
./scripts/generate_certs.sh coordinator dev

# 2. 운영 환경에 배포
./scripts/generate_certs.sh copy certs/dev

# 3. 발급 폴더 정리
./scripts/generate_certs.sh clean

# 4. 해당 서버만 재시작
systemctl restart coordinator
```

#### 전체 인증서 갱신 (CA 포함)
```bash
# 1. 모든 서버 중단
systemctl stop mpc-engine

# 2. 기존 인증서 백업
cp -r certs/dev certs/backup.$(date +%Y%m%d)

# 3. 환경별 전체 갱신 (CA + 모든 서버)
./scripts/generate_certs.sh all dev

# 4. 새 인증서 배포
./scripts/generate_certs.sh copy certs/dev

# 5. 발급 폴더 정리
./scripts/generate_certs.sh clean

# 6. 모든 서버 재시작
systemctl start mpc-engine
```

### 6. 인증서 검증 (환경 정보 포함)

발급 후 또는 문제 발생 시 인증서 유효성 확인:

```bash
./scripts/generate_certs.sh verify
```

**출력 예시:**
```
[INFO] 인증서 유효성 검증 중...
[SUCCESS] CA 인증서 유효
[SUCCESS] coordinator 인증서 유효
[SUCCESS] node1 인증서 유효
[SUCCESS] node2 인증서 유효
[SUCCESS] node3 인증서 유효
[SUCCESS] 모든 인증서 검증 완료
```

## 파일 구조

### 발급 폴더 (임시)
```
certs/generated/             # 발급 전용 임시 폴더
├── ca-cert.pem              # Root CA 인증서 (공개)
├── ca-key.pem               # Root CA 개인키 (암호화)
├── coordinator-cert.pem     # Coordinator 서버 인증서
├── coordinator-key.pem      # Coordinator 개인키
├── node1-cert.pem          # Node 1 서버 인증서
├── node1-key.pem           # Node 1 개인키
├── node2-cert.pem          # Node 2 서버 인증서  
├── node2-key.pem           # Node 2 개인키
├── node3-cert.pem          # Node 3 서버 인증서
├── node3-key.pem           # Node 3 개인키
└── cert-env-info.txt       # 환경 정보 (자동 생성)
```

### 환경별 관리 폴더
```
certs/
├── generated/              # 발급 임시 폴더 (Git 제외)
├── local/                  # 로컬 개발용 (Git 포함)
├── dev/                    # 개발 서버용 (Git 제외)
├── staging/                # 스테이징용 (Git 제외)
└── prod/                   # 프로덕션용 (Git 제외)
```

### 환경 정보 파일 (cert-env-info.txt)
```bash
# 인증서 환경 정보
DEPLOY_ENV=dev
TLS_DOMAIN_SUFFIX=.dev.mpc-engine.com
GENERATED_AT=2024-10-09 15:30:25
```

### 자동 생성되는 .gitignore
```bash
# 발급 임시 폴더 제외
generated/

# 환경별 인증서 관리
# local/ - Git 포함 (개발용)
# dev/ - Git 제외 (개발 서버용)
dev/
# staging/ - Git 제외 (스테이징용)
staging/
# prod/ - Git 제외 (프로덕션용)
prod/
```

### 환경별 인증서 정보

| 환경 | CN 예시 | SAN 예시 | 유효기간 |
|------|---------|----------|----------|
| **local** | coordinator.local | localhost, coordinator | 5년 |
| **dev** | coordinator.dev.mpc-engine.com | localhost, coordinator.internal, coordinator-service | 5년 |
| **staging** | coordinator.staging.mpc-engine.com | localhost, coordinator.internal, coordinator-service | 5년 |
| **prod** | coordinator.mpc-engine.com | localhost, coordinator.internal, coordinator-service | 5년 |

**공통 인증서 설정:**
- **Root CA 유효기간**: 10년 (3650일)
- **암호화**: RSA 2048bit, SHA256
- **CA 패스워드**: `mpc-engine-ca-key`

## 운영 워크플로우 (환경별)

### 멀티 환경 개발 워크플로우

```bash
# 1. 로컬 개발
./scripts/generate_certs.sh all local
./scripts/generate_certs.sh copy certs/local
./build/coordinator local

# 2. 개발 서버 테스트
./scripts/generate_certs.sh all dev
./scripts/generate_certs.sh copy certs/dev
deploy-to-dev.sh

# 3. 스테이징 검증
./scripts/generate_certs.sh all staging  
./scripts/generate_certs.sh copy certs/staging
deploy-to-staging.sh

# 4. 프로덕션 배포
./scripts/generate_certs.sh all prod
./scripts/generate_certs.sh copy /media/usb/
deploy-to-production.sh
```

### 환경별 정기 점검

```bash
# 모든 환경의 인증서 만료일 확인
for env in local dev staging prod; do
  if [[ -d "certs/$env" ]]; then
    echo "=== $env 환경 ==="
    openssl x509 -in "certs/$env/coordinator-cert.pem" -noout -enddate
  fi
done

# 만료 임박 시 갱신
./scripts/generate_certs.sh all staging
./scripts/generate_certs.sh copy certs/staging
```

## 문제 해결

### Q: 환경 파일이 없다는 에러
```bash
[WARN] 환경 파일이 없습니다: ./env/.env.dev
[INFO] 기본값 사용: DEPLOY_ENV=local, TLS_DOMAIN_SUFFIX=.local
```

**해결책:**
```bash
# 환경 파일 생성
cp env/.env.local env/.env.dev
# TLS_DOMAIN_SUFFIX를 적절히 수정
```

### Q: 잘못된 환경으로 인증서를 발급했음
```bash
# 올바른 환경으로 재발급
./scripts/generate_certs.sh clean
./scripts/generate_certs.sh all dev  # 올바른 환경 지정
```

### Q: 환경별 도메인이 코드와 일치하지 않음
```bash
# 환경 설정 확인
grep TLS_DOMAIN_SUFFIX env/.env.dev

# 인증서 CN 확인
openssl x509 -in certs/dev/coordinator-cert.pem -noout -subject

# 불일치 시 재발급
./scripts/generate_certs.sh all dev
```

### Q: 새로운 환경 추가
```bash
# 1. 환경 설정 파일 생성
cp env/.env.local env/.env.k8s
# TLS_DOMAIN_SUFFIX=.svc.cluster.local 설정

# 2. 인증서 발급
./scripts/generate_certs.sh all k8s

# 3. 배포
./scripts/generate_certs.sh copy certs/k8s
```

### Q: 동적 Node 개수 지원
환경 설정 파일에서 `NODE_IDS`를 수정:
```bash
# env/.env.custom
NODE_IDS=node1,node2,node3,node4,node5

# 발급
./scripts/generate_certs.sh all custom
```

## 보안 고려사항

### ✅ 환경별 보안 강화
- **환경 분리**: 각 환경별로 독립적인 CA 및 인증서
- **도메인 검증**: 환경별 정확한 FQDN 매칭
- **SAN 확장**: 환경에 맞는 추가 도메인 지원
- **환경 정보 추적**: cert-env-info.txt로 발급 환경 기록

### ⚠️ 주의사항  
- **환경 일치**: 코드의 TLS_DOMAIN_SUFFIX와 인증서 CN이 정확히 일치해야 함
- **CA 보안**: 환경별로 다른 CA 사용 권장 (프로덕션 vs 개발)
- **임시 폴더 관리**: 발급 후 반드시 정리

### 🚫 프로덕션 보안 원칙
- **오프라인 CA**: 프로덕션 CA는 오프라인 시스템에서 관리
- **USB 전송**: 물리적 매체로 안전하게 전송
- **즉시 삭제**: KMS 업로드 후 로컬 파일 즉시 삭제
- **접근 제한**: 프로덕션 인증서는 최소 권한 원칙

## 권장 사항

### 환경별 접근 전략

#### 개발 환경 (local, dev)
- `certs/local/` Git 포함 (팀 공유)
- `certs/dev/` Git 제외 (서버별 개별 관리)
- 1년 단위 갱신

#### 테스트 환경 (staging)  
- `certs/staging/` Git 제외
- 자동화된 배포 파이프라인 연동
- 6개월 단위 갱신

#### 프로덕션 환경 (prod)
- USB 또는 보안 저장소만 사용
- Cloud KMS와 연동
- 3개월 단위 갱신
- 수동 배포 및 검증

### 모니터링 및 알림
```bash
# 만료일 모니터링 스크립트
#!/bin/bash
for env in dev staging prod; do
  if [[ -f "certs/$env/coordinator-cert.pem" ]]; then
    days_left=$(( ($(date -d "$(openssl x509 -in "certs/$env/coordinator-cert.pem" -noout -enddate | cut -d= -f2)" +%s) - $(date +%s)) / 86400 ))
    if [[ $days_left -lt 30 ]]; then
      echo "WARNING: $env 환경 인증서가 $days_left 일 후 만료됩니다!"
    fi
  fi
done
```

## 업데이트 이력

- **v1.0**: 초기 버전, 기본 인증서 발급 기능
- **v1.1**: 발급/배포 분리, 임시 폴더 사용
- **v2.0**: 환경 변수 기반 도메인 설정 지원
- **v2.1**: 환경별 SAN 설정, 환경 정보 추적
- **향후 계획**: Node 동적 추가, 클라우드 KMS 완전 연동

---

**관련 문서:**
- [TLS 구현 계획](../docs/tls-implementation-plan.md)  
- [환경 설정 가이드](../env/README.md)
- [보안 정책](../docs/security-policy.md)
- [배포 가이드](../docs/deployment-guide.md)