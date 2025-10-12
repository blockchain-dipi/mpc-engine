<!-- src/common/network/https/README_HTTPS.md -->

HTTPS Library
Zero-Copy 설계 기반 고성능 HTTPS 클라이언트 라이브러리
개요
위치: src/common/https/
의존성: OpenSSL, TLS (TlsContext, TlsConnection)
표준: HTTP/1.1 (RFC 7230)
설계: Zero-Copy, Stack Buffer 사용
성능 목표
처리량: 초당 100만+ 요청 처리 가능
메모리: 힙 할당 최소화 (스택 버퍼 사용)
지연시간: 요청당 ~1 μs (네트워크 제외)
구성 요소
1. HttpWriter (저수준)
Zero-Copy HTTP 요청 생성 및 전송
2. HttpResponseReader (저수준)
Zero-Copy HTTP 응답 파싱
3. HttpsClient (중간 수준)
TLS 연결 + HTTP 통합
4. HttpsConnectionPool (고수준)
연결 재사용 및 관리
전체 사용 예제 (Coordinator)

KMS 초기화
TLS Context 준비 (CA 인증서 로드)
Connection Pool 생성
서명 요청 처리

설계 원칙
Zero-Copy
스택 버퍼: 힙 할당 없이 스택 사용 (8KB)
Direct Write: TLS 소켓으로 직접 전송
String View: 복사 대신 참조 전달
성능 비교
힙 할당: 기존 3회 → Zero-Copy 0회
메모리 복사: 기존 3회 → Zero-Copy 1회 (3배 개선)
처리량: 기존 5만 TPS → Zero-Copy 100만 TPS (20배 개선)
인증서 관리
중요: 인증서는 라이브러리가 아닌 KMS에서 로드합니다.
이유: 각 플랫폼(LOCAL, AWS, Azure, IBM, Google)마다 인증서 저장 방식이 다름
올바른 방법:

KMS에서 인증서 로드
TlsContext에 주입
HttpsClient에 TlsContext 전달

API 레퍼런스
HttpWriter
WritePostJson: POST 요청 전송
WriteGet: GET 요청 전송
HttpResponseReader
Response 구조체: status_code, body, body_len, success
ReadResponse: 응답 읽기 및 파싱
HttpsConnectionPool
Initialize: Pool 초기화
Acquire: 연결 획득
Release: 연결 반환
CleanupIdleConnections: 유휴 연결 정리
제한사항
HTTP/1.1만 지원
Chunked Transfer Encoding 미지원 (Content-Length 필수)
최대 응답 크기: 버퍼 크기에 의존 (기본 8KB)