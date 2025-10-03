# HTTPS Library

MPC Engine의 HTTPS 통신 라이브러리

---

## 개요

Coordinator와 Wallet Server 간 안전한 HTTPS 통신을 위한 라이브러리입니다.

**기술 스택:**
- OpenSSL (TLS 1.2+)
- nlohmann/json (JSON 직렬화)

**구조:**
TlsContext → TlsConnection → HttpsClient

---

## 빌드

### 의존성
# Ubuntu/Debian
sudo apt-get install libssl-dev

# macOS
brew install openssl
컴파일
bashmkdir build && cd build
cmake ..
make -j$(nproc)

디렉토리 구조
bashchmod +x setup_https_structure.sh
./setup_https_structure.sh
src/common/https/
├── include/
│   ├── TlsContext.hpp      # SSL_CTX 관리
│   ├── TlsConnection.hpp   # SSL 연결 관리
│   ├── HttpTypes.hpp       # HTTP 메시지 구조
│   └── HttpsClient.hpp     # HTTPS 클라이언트
└── src/
    ├── TlsContext.cpp
    ├── TlsConnection.cpp
    ├── HttpTypes.cpp
    └── HttpsClient.cpp

개발용 인증서
bashchmod +x generate_test_certs.sh
./generate_test_certs.sh
생성 파일:

certs/local/ca-cert.pem - CA 인증서
certs/local/coordinator-cert.pem - 서버 인증서
certs/local/coordinator-key.pem - 서버 개인키

⚠️ 개발 전용 - 프로덕션 사용 금지
검증:
bashopenssl verify -CAfile certs/local/ca-cert.pem certs/local/coordinator-cert.pem

사용 예시
HTTPS 클라이언트
cpp#include "common/https/include/HttpsClient.hpp"

using namespace mpc_engine::https;

// 설정
HttpsClientConfig config;
config.ca_file = "./certs/local/ca-cert.pem";
config.verify_hostname = true;
config.timeout_ms = 30000;

// 클라이언트 생성
HttpsClient client(config);
client.Initialize();

// POST 요청
HttpResponse response;
bool success = client.PostJson(
    "https://localhost:3000/api/v1/sign",
    json_body,
    "Bearer token_12345",
    response
);

if (success && response.IsSuccess()) {
    std::cout << response.body << std::endl;
}
JSON 프로토콜
cpp#include "protocols/coordinator_wallet/include/WalletProtocolTypes.hpp"

using namespace mpc_engine::protocol::coordinator_wallet;

// 요청 생성
WalletSigningRequest request;
request.requestId = "req-123";
request.keyId = "key-abc";
request.transactionData = "0x1234";

std::string json = request.ToJson();

// 응답 파싱
WalletSigningResponse response;
if (response.FromJson(json_response)) {
    std::cout << response.finalSignature << std::endl;
}

API 레퍼런스
TlsContext
cppTlsConfig config;
config.mode = TlsMode::CLIENT;
config.ca_file = "ca-cert.pem";
config.verify_hostname = true;
config.min_tls_version = TLS1_2_VERSION;

TlsContext ctx(config);
ctx.Initialize();
TlsConnection
cppTlsConnection conn;
TlsResult result = conn.Connect(ctx, "localhost", 8443);

if (result == TlsResult::SUCCESS) {
    conn.Send(data, len);
    conn.Receive(buffer, len);
}
HttpsClient
cppHttpHeaders headers;
headers.authorization = "Bearer token";

HttpResponse response;
client.Post(url, headers, body, response);

보안
TLS 설정

최소 버전: TLS 1.2
암호화 스위트: 강력한 알고리즘만
Hostname 검증: 필수
인증서 체인 검증: 필수

권장 사항
cppconfig.min_tls_version = TLS1_2_VERSION;
config.verify_hostname = true;
config.cipher_list = "ECDHE+AESGCM:ECDHE+CHACHA20";

트러블슈팅
OpenSSL 에러
bash# 에러 확인
openssl s_client -connect localhost:8443 -CAfile certs/local/ca-cert.pem

# 라이브러리 확인
ldconfig -p | grep ssl
인증서 권한
bashchmod 600 certs/local/*-key.pem
chmod 644 certs/local/*-cert.pem
VSCode IntelliSense
bashln -sf build/compile_commands.json .
# Ctrl+Shift+P → "C/C++: Reset IntelliSense Database"

테스트
bashcmake .. -DBUILD_TESTS=ON
make
./test_https_client
./test_https_server

참고

OpenSSL Docs : https://docs.openssl.org/master/
RFC 8446 - TLS 1.3 : https://www.rfc-editor.org/rfc/rfc8446
nlohmann/json : https://github.com/nlohmann/json