package main

import (
	"bytes"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"sync"
	"sync/atomic"
	"time"

	pb "load-test/proto"

	"google.golang.org/protobuf/proto"
)

const (
    WALLET_MESSAGE_TYPE_SIGNING_REQUEST = 1001
)

type LoadTestConfig struct {
	CoordinatorURL     string
	CACertPath         string
	NumClients         int
	RequestsPerClient  int
	RequestDelayMs     int
	RequestTimeoutSec  int
	ConnectionPoolSize int
}

type RequestStats struct {
	TotalRequests      atomic.Uint64
	SuccessfulRequests atomic.Uint64
	FailedRequests     atomic.Uint64
	TimeoutRequests    atomic.Uint64
	TotalLatencyMs     atomic.Uint64
	MinLatencyMs       atomic.Uint64
	MaxLatencyMs       atomic.Uint64
	StartTime          time.Time
	EndTime            time.Time
}

func NewRequestStats() *RequestStats {
	stats := &RequestStats{}
	stats.MinLatencyMs.Store(^uint64(0))
	return stats
}

func (s *RequestStats) RecordRequest(success bool, latencyMs uint64, timedOut bool) {
	s.TotalRequests.Add(1)
	if timedOut {
		s.TimeoutRequests.Add(1)
	} else if success {
		s.SuccessfulRequests.Add(1)
	} else {
		s.FailedRequests.Add(1)
	}
	s.TotalLatencyMs.Add(latencyMs)

	for {
		oldMin := s.MinLatencyMs.Load()
		if latencyMs >= oldMin || s.MinLatencyMs.CompareAndSwap(oldMin, latencyMs) {
			break
		}
	}
	for {
		oldMax := s.MaxLatencyMs.Load()
		if latencyMs <= oldMax || s.MaxLatencyMs.CompareAndSwap(oldMax, latencyMs) {
			break
		}
	}
}

func (s *RequestStats) PrintReport() {
	total := s.TotalRequests.Load()
	success := s.SuccessfulRequests.Load()
	failed := s.FailedRequests.Load()
	timeouts := s.TimeoutRequests.Load()
	duration := s.EndTime.Sub(s.StartTime).Seconds()
	throughput := float64(total) / duration
	avgLatency := uint64(0)
	if total > 0 {
		avgLatency = s.TotalLatencyMs.Load() / total
	}

	fmt.Println("\n========================================")
	fmt.Println("  Load Test Report")
	fmt.Println("========================================")
	fmt.Printf("Total Requests:      %d\n", total)
	fmt.Printf("Successful:          %d (%.2f%%)\n", success, float64(success)*100/float64(total))
	fmt.Printf("Failed:              %d\n", failed)
	fmt.Printf("Timeouts:            %d\n", timeouts)
	fmt.Printf("Duration:            %.2f sec\n", duration)
	fmt.Printf("Throughput:          %.2f req/sec\n", throughput)
	fmt.Println("\nLatency Statistics:")
	fmt.Printf("  Average:           %d ms\n", avgLatency)
	fmt.Printf("  Min:               %d ms\n", s.MinLatencyMs.Load())
	fmt.Printf("  Max:               %d ms\n", s.MaxLatencyMs.Load())
	fmt.Println("========================================\n")
}

type WalletClient struct {
	ClientID   int
	HTTPClient *http.Client
	BaseURL    string
	Stats      *RequestStats
}

func NewWalletClient(clientID int, config *LoadTestConfig, stats *RequestStats) (*WalletClient, error) {
	// CA 인증서 로드
	caCert, err := os.ReadFile(config.CACertPath)
	if err != nil {
		return nil, fmt.Errorf("failed to read CA cert: %w", err)
	}

	caCertPool := x509.NewCertPool()
	caCertPool.AppendCertsFromPEM(caCert)

	// 클라이언트 인증서로 coordinator 인증서 사용
	clientCert, err := tls.LoadX509KeyPair(
		"../../certs/local/coordinator-cert.pem",
		"../../.kms/coordinator-key.pem",
	)
	if err != nil {
		return nil, fmt.Errorf("failed to load client cert: %w", err)
	}

	httpClient := &http.Client{
		Timeout: time.Duration(config.RequestTimeoutSec) * time.Second,
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{
				RootCAs:      caCertPool,
				Certificates: []tls.Certificate{clientCert},  // mTLS 인증서
			},
			MaxIdleConns:        config.ConnectionPoolSize,
			MaxIdleConnsPerHost: config.ConnectionPoolSize / config.NumClients,
			IdleConnTimeout:     90 * time.Second,
			DisableKeepAlives:   false,
		},
	}

	return &WalletClient{
		ClientID:   clientID,
		HTTPClient: httpClient,
		BaseURL:    config.CoordinatorURL,
		Stats:      stats,
	}, nil
}

func (c *WalletClient) SendSigningRequest(requestID string) (bool, uint64, bool) {
	start := time.Now()

	request := &pb.WalletCoordinatorMessage{
	    MessageType: WALLET_MESSAGE_TYPE_SIGNING_REQUEST,
	    Payload: &pb.WalletCoordinatorMessage_SigningRequest{
	        SigningRequest: &pb.WalletSigningRequest{
	            Header: &pb.WalletRequestHeader{
	                MessageType:   WALLET_MESSAGE_TYPE_SIGNING_REQUEST,
	                RequestId:     requestID,  // ← i가 아니라 requestID
	                Timestamp:     time.Now().Format(time.RFC3339),
	                CoordinatorId: "test-coordinator",
	            },
	            KeyId:           "test-key-123",
	            TransactionData: "0xabcdef1234567890",
	            Threshold:       2,
	            TotalShards:     3,
	            RequiredShards:  []string{"shard0", "shard1", "shard2"},
	        },
	    },
	}

	data, err := proto.Marshal(request)
	if err != nil {
		log.Printf("[Client %d] Marshal failed: %v\n", c.ClientID, err)
		return false, 0, false
	}

	resp, err := c.HTTPClient.Post(
		c.BaseURL+"/api/v1/sign",
		"application/protobuf",
		bytes.NewReader(data),
	)

	latency := time.Since(start).Milliseconds()

	if err != nil {
		if os.IsTimeout(err) {
			return false, uint64(latency), true
		}
		log.Printf("[Client %d] Request failed: %v\n", c.ClientID, err)
		return false, uint64(latency), false
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		log.Printf("[Client %d] Bad status %d: %s\n", c.ClientID, resp.StatusCode, body)
		return false, uint64(latency), false
	}

	respData, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Printf("[Client %d] Read failed: %v\n", c.ClientID, err)
		return false, uint64(latency), false
	}

	var responseMsg pb.WalletCoordinatorMessage
	if err := proto.Unmarshal(respData, &responseMsg); err != nil {
		log.Printf("[Client %d] Unmarshal failed: %v\n", c.ClientID, err)
		return false, uint64(latency), false
	}

	signingResp := responseMsg.GetSigningResponse()
	success := signingResp != nil && signingResp.Header.Success

	return success, uint64(latency), false
}

func (c *WalletClient) RunLoadTest(config *LoadTestConfig) {
	log.Printf("[Client %d] Starting %d requests...\n", c.ClientID, config.RequestsPerClient)

	for i := 0; i < config.RequestsPerClient; i++ {
		requestID := fmt.Sprintf("client_%d_req_%d", c.ClientID, i)
		success, latency, timedOut := c.SendSigningRequest(requestID)
		c.Stats.RecordRequest(success, latency, timedOut)

		if config.RequestDelayMs > 0 && i < config.RequestsPerClient-1 {
			time.Sleep(time.Duration(config.RequestDelayMs) * time.Millisecond)
		}
	}

	log.Printf("[Client %d] Completed\n", c.ClientID)
}

func RunLoadTest(config *LoadTestConfig) bool {
	stats := NewRequestStats()

	fmt.Println("\n========================================")
	fmt.Println("  Starting Load Test")
	fmt.Println("========================================")
	fmt.Printf("Coordinator URL:     %s\n", config.CoordinatorURL)
	fmt.Printf("Wallet Clients:      %d\n", config.NumClients)
	fmt.Printf("Requests/Client:     %d\n", config.RequestsPerClient)
	fmt.Printf("Total Requests:      %d\n", config.NumClients*config.RequestsPerClient)
	fmt.Printf("Request Delay:       %d ms\n", config.RequestDelayMs)
	fmt.Printf("Connection Pool:     %d\n", config.ConnectionPoolSize)
	fmt.Println("========================================\n")

	stats.StartTime = time.Now()

	var wg sync.WaitGroup
	for i := 0; i < config.NumClients; i++ {
		wg.Add(1)
		go func(clientID int) {
			defer wg.Done()
			client, err := NewWalletClient(clientID, config, stats)
			if err != nil {
				log.Printf("Failed to create client %d: %v\n", clientID, err)
				return
			}
			client.RunLoadTest(config)
		}(i)
	}

	wg.Wait()
	stats.EndTime = time.Now()
	stats.PrintReport()

	successRate := float64(stats.SuccessfulRequests.Load()) * 100 / float64(stats.TotalRequests.Load())
	return successRate >= 99.0
}

func main() {
	fmt.Println("\n========================================")
	fmt.Println("  Coordinator HTTPS Load Test Suite")
	fmt.Println("========================================\n")

	allPassed := true

	// Light Load
	fmt.Println("\n[Test 1] Light Load Test")
	if !RunLoadTest(&LoadTestConfig{
		CoordinatorURL:     "https://127.0.0.1:9080",
		CACertPath:         "../../certs/local/ca-cert.pem",
		NumClients:         5,
		RequestsPerClient:  20,
		RequestDelayMs:     50,
		RequestTimeoutSec:  30,
		ConnectionPoolSize: 50,
	}) {
		fmt.Println("✗ Light load test failed!")
		allPassed = false
	} else {
		fmt.Println("✓ Light load test passed!")
	}

	// Medium Load
	fmt.Println("\n[Test 2] Medium Load Test")
	if !RunLoadTest(&LoadTestConfig{
		CoordinatorURL:     "https://127.0.0.1:9080",
		CACertPath:         "../../certs/local/ca-cert.pem",
		NumClients:         20,
		RequestsPerClient:  50,
		RequestDelayMs:     10,
		RequestTimeoutSec:  30,
		ConnectionPoolSize: 100,
	}) {
		fmt.Println("✗ Medium load test failed!")
		allPassed = false
	} else {
		fmt.Println("✓ Medium load test passed!")
	}

	// High Load
	fmt.Println("\n[Test 3] High Load Test")
	if !RunLoadTest(&LoadTestConfig{
		CoordinatorURL:     "https://127.0.0.1:9080",
		CACertPath:         "../../certs/local/ca-cert.pem",
		NumClients:         50,
		RequestsPerClient:  100,
		RequestDelayMs:     0,
		RequestTimeoutSec:  30,
		ConnectionPoolSize: 200,
	}) {
		fmt.Println("✗ High load test failed!")
		allPassed = false
	} else {
		fmt.Println("✓ High load test passed!")
	}

	// Stress Test
	fmt.Println("\n[Test 4] Stress Test")
	if !RunLoadTest(&LoadTestConfig{
		CoordinatorURL:     "https://127.0.0.1:9080",
		CACertPath:         "../../certs/local/ca-cert.pem",
		NumClients:         100,
		RequestsPerClient:  50,
		RequestDelayMs:     0,
		RequestTimeoutSec:  30,
		ConnectionPoolSize: 300,
	}) {
		fmt.Println("✗ Stress test failed!")
		allPassed = false
	} else {
		fmt.Println("✓ Stress test passed!")
	}

	fmt.Println("\n========================================")
	fmt.Println("  Final Test Results")
	fmt.Println("========================================")

	if allPassed {
		fmt.Println("✓ All load tests PASSED! 🎉")
		os.Exit(0)
	} else {
		fmt.Println("✗ Some tests FAILED!")
		os.Exit(1)
	}
}
