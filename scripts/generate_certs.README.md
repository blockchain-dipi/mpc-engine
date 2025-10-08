# generate_certs.sh 사용법

MPC Engine의 TLS 인증서 발급 및 관리 스크립트입니다.

## 개요

내부 서버 간 TLS 통신을 위한 Self-signed 인증서를 발급합니다.

**발급되는 인증서:**
- Root CA (1개) - 모든 서버 인증서의 서명용
- Coordinator 서버 인증서 (1개) - HTTPS 서버용  
- Node 서버 인증서 (3개) - TCP over TLS 서버용

**발급 위치:** `./certs/generated/` (임시 폴더)

## 사용법

### 기본 명령어

```bash
# 전체 인증서 발급 (처음 사용 시)
./scripts/generate_certs.sh all

# 개별 서버 인증서 발급
./scripts/generate_certs.sh coordinator
./scripts/generate_certs.sh node1
./scripts/generate_certs.sh node2  
./scripts/generate_certs.sh node3

# CA 인증서만 발급
./scripts/generate_certs.sh ca
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

## 상세 사용 가이드

### 1. 처음 설정 (개발 환경)

```bash
# 1. 전체 인증서 발급
./scripts/generate_certs.sh all

# 2. 로컬 개발용으로 복사
./scripts/generate_certs.sh copy certs/local

# 3. 발급 폴더 정리 (선택사항)
./scripts/generate_certs.sh clean
```

**출력 예시:**
```
🔐 Root CA:
  인증서: ca-cert.pem
  개인키: ca-key.pem (암호화됨)
  만료일: Apr  7 06:15:23 2030 GMT

🖥️  coordinator 서버:
  인증서: coordinator-cert.pem
  개인키: coordinator-key.pem
  만료일: Apr  6 06:15:24 2030 GMT

🖥️  node1 서버:
  인증서: node1-cert.pem
  개인키: node1-key.pem  
  만료일: Apr  6 06:15:25 2030 GMT
```

### 2. 환경별 배포

#### 로컬 개발 환경
```bash
# 방법 1: 스크립트 헬퍼 사용
./scripts/generate_certs.sh copy certs/local

# 방법 2: 직접 복사
mkdir -p certs/local
cp -r certs/generated/* certs/local/
```

#### 개발 서버 환경
```bash
./scripts/generate_certs.sh copy certs/dev

# 또는
mkdir -p certs/dev
cp -r certs/generated/* certs/dev/
```

#### 프로덕션 (USB 복사)
```bash
# USB 마운트 후
./scripts/generate_certs.sh copy /media/usb/mpc-certs/

# 또는 직접 복사
cp -r certs/generated/* /media/usb/mpc-certs/

# 보안을 위해 즉시 발급 폴더 삭제
./scripts/generate_certs.sh clean
```

### 3. 개별 서버 인증서 재발급

특정 서버의 인증서만 갱신해야 할 때:

```bash
# Coordinator 인증서만 재발급
./scripts/generate_certs.sh coordinator

# 기존 환경에 복사
./scripts/generate_certs.sh copy certs/local

# 발급 폴더 정리
./scripts/generate_certs.sh clean
```

**중요한 전제조건:**
개별 서버 인증서를 발급하려면 기존 CA 인증서가 `certs/generated/` 폴더에 있어야 합니다.

**이유:**
```bash
# 새 서버 인증서를 발급할 때 CA 개인키로 서명이 필요함
openssl x509 -req -in coordinator.csr \
    -CA ca-cert.pem \      # ← CA 인증서 필요
    -CAkey ca-key.pem \    # ← CA 개인키로 서명 필요
    -out coordinator-cert.pem
```

**CA 파일이 없을 때 해결법:**
```bash
# 방법 1: 전체 재발급 (CA 포함)
./scripts/generate_certs.sh all

# 방법 2: 기존 CA를 generated/ 폴더에 복사
cp certs/local/ca-* certs/generated/
./scripts/generate_certs.sh coordinator

# 방법 3: USB에서 CA 가져오기 (프로덕션)
cp /media/usb/ca-* certs/generated/
./scripts/generate_certs.sh coordinator
```

### 4. CA 재발급 (전체 갱신)

모든 인증서를 새로 발급해야 할 때:

```bash
# 기존 CA 삭제하고 전체 재발급
./scripts/generate_certs.sh all
# → "기존 CA를 삭제하고 새로 발급하시겠습니까? [y/N]:" → y 입력

# 환경별로 배포
./scripts/generate_certs.sh copy certs/local
./scripts/generate_certs.sh copy certs/dev

# 발급 폴더 정리
./scripts/generate_certs.sh clean
```

### 5. 인증서 갱신 가이드

인증서 만료 전 또는 보안상 이유로 인증서를 갱신해야 할 때:

#### 만료일 확인
```bash
# 전체 인증서 만료일 확인
./scripts/generate_certs.sh info

# 개별 인증서 만료일 확인
openssl x509 -in certs/local/coordinator-cert.pem -noout -enddate
```

#### 개별 서버 인증서만 갱신 (일반적)
```bash
# 1. 기존 CA를 그대로 유지하면서 특정 서버만 갱신
./scripts/generate_certs.sh coordinator

# 2. 운영 환경에 배포
./scripts/generate_certs.sh copy certs/local

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
cp -r certs/local certs/backup.$(date +%Y%m%d)

# 3. 전체 갱신 (CA + 모든 서버)
./scripts/generate_certs.sh all

# 4. 새 인증서 배포
./scripts/generate_certs.sh copy certs/local

# 5. 발급 폴더 정리
./scripts/generate_certs.sh clean

# 6. 모든 서버 재시작
systemctl start mpc-engine
```

#### 프로덕션 환경 갱신 (오프라인 CA)
```bash
# 1. USB에서 CA 인증서 가져오기
cp /media/usb/ca-* certs/generated/

# 2. 필요한 서버 인증서만 갱신
./scripts/generate_certs.sh coordinator
./scripts/generate_certs.sh node1

# 3. KMS에 새 인증서 업로드
./scripts/generate_certs.sh copy production-kms/

# 4. CA 개인키 즉시 삭제 (보안)
rm certs/generated/ca-key.pem

# 5. 발급 폴더 정리
./scripts/generate_certs.sh clean
```

### 6. 인증서 검증

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
└── node3-key.pem           # Node 3 개인키
```

### 환경별 관리 폴더
```
certs/
├── generated/              # 발급 임시 폴더 (Git 제외)
├── local/                  # 로컬 개발용 (Git 포함)
├── dev/                    # 개발 서버용 (Git 제외)
└── staging/                # 스테이징용 (Git 제외)
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
```

### 인증서 정보

| 항목 | 값 |
|------|-----|
| **Root CA** | |
| 유효기간 | 10년 (3650일) |
| 암호화 | RSA 2048bit, SHA256 |
| 패스워드 | `mpc-engine-ca-key` |
| **서버 인증서** | |
| 유효기간 | 5년 (1825일) |  
| 암호화 | RSA 2048bit, SHA256 |
| SAN | localhost, 127.0.0.1, ::1 |
| CN | `{server}.local` (예: coordinator.local) |

## 운영 워크플로우

### 로컬 개발 시작
```bash
# 1. 인증서 발급
./scripts/generate_certs.sh all

# 2. 로컬 환경으로 복사  
./scripts/generate_certs.sh copy certs/local

# 3. 발급 폴더 정리
./scripts/generate_certs.sh clean

# 4. 개발 시작
./build/coordinator local
```

### 개발 서버 배포
```bash
# 1. 인증서 발급
./scripts/generate_certs.sh all

# 2. 개발 서버로 복사
./scripts/generate_certs.sh copy certs/dev

# 3. 서버에 배포 (scp, rsync 등)
scp -r certs/dev/* server:/path/to/certs/

# 4. 발급 폴더 정리
./scripts/generate_certs.sh clean
```

### 정기 점검 시 인증서 갱신
```bash
# 1. 서비스 중단
systemctl stop mpc-engine

# 2. 새 인증서 발급
./scripts/generate_certs.sh all

# 3. 기존 인증서 백업
cp -r certs/local certs/local.backup.$(date +%Y%m%d)

# 4. 새 인증서 배포
./scripts/generate_certs.sh copy certs/local

# 5. 발급 폴더 정리
./scripts/generate_certs.sh clean

# 6. 서비스 재시작
systemctl start mpc-engine
```

## 문제 해결

### Q: "CA 인증서가 없습니다" 에러
```bash
# CA부터 발급
./scripts/generate_certs.sh ca
# 또는 전체 재발급
./scripts/generate_certs.sh all
```

### Q: 기존 CA를 유지하고 서버 인증서만 재발급하고 싶음
```bash
# CA는 그대로 두고 개별 서버만 재발급
./scripts/generate_certs.sh coordinator
./scripts/generate_certs.sh node1
```

### Q: "인증서 무효" 에러가 발생함
```bash
# 인증서 체인 확인
openssl verify -CAfile certs/generated/ca-cert.pem certs/generated/coordinator-cert.pem

# 전체 재발급이 필요한 경우
./scripts/generate_certs.sh all
```

### Q: 인증서 내용을 자세히 확인하고 싶음
```bash
# 인증서 상세 정보 출력
openssl x509 -in certs/generated/coordinator-cert.pem -text -noout

# 만료 날짜만 확인
openssl x509 -in certs/generated/coordinator-cert.pem -noout -enddate
```

### Q: 발급 폴더가 계속 남아있음
```bash
# 수동 정리
rm -rf certs/generated

# 또는 스크립트 사용
./scripts/generate_certs.sh clean
```

### Q: 새로운 Node 추가 시 인증서 발급
현재는 node1,2,3만 지원합니다. 새로운 Node 추가 시 스크립트 수정이 필요합니다.

```bash
# 스크립트 수정 위치
# generate_server_cert "node4" 추가
# case 문에 "node4" 추가
```

## 보안 고려사항

### ✅ 안전한 요소
- CA 개인키 암호화 (패스워드 보호)
- 적절한 Key Usage 설정
- SAN 확장으로 다양한 접근 경로 지원
- 발급과 배포 분리 (임시 폴더 사용)
- Git에서 발급 폴더 자동 제외

### ⚠️ 주의사항  
- **개발 전용**: 현재 인증서는 로컬 개발용입니다
- **패스워드 노출**: 스크립트에 CA 패스워드가 하드코딩되어 있습니다
- **임시 폴더 관리**: 발급 후 반드시 정리하세요

### 🚫 프로덕션에서 금지
- 이 스크립트로 생성된 인증서를 프로덕션에서 사용 금지
- 프로덕션은 별도 보안 절차를 통해 인증서 관리
- 하드코딩된 패스워드 사용 금지

## 권장 사항

### 개발 환경
- `certs/local/` 사용
- Git에 포함해도 무방 (Private repo)
- 팀 전체가 동일한 인증서 사용

### 테스트 환경  
- `certs/dev/` 또는 `certs/staging/` 사용
- Git에서 제외 (.gitignore)
- 서버별로 개별 복사

### 프로덕션 환경
- USB 또는 보안 저장소 사용
- Cloud KMS와 연동
- 수동 배포 및 관리

## 업데이트 이력

- **v1.0**: 초기 버전, 기본 인증서 발급 기능
- **v1.1**: 발급/배포 분리, 임시 폴더 사용
- **향후 계획**: Node 동적 추가, 클라우드 KMS 연동

---

**관련 문서:**
- [TLS 구현 계획](../docs/tls-implementation-plan.md)  
- [환경 설정 가이드](../env/README.md)
- [보안 정책](../docs/security-policy.md)