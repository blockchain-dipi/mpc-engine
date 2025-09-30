==========================================
  NodeTcpServer Integration Tests
==========================================

=== Test 1: Basic Connection ===
Receive thread started
Send thread started
Handler processing message type: 0
Client connected: 127.0.0.1:xxxxx
Client disconnected: 127.0.0.1:xxxxx
✓ Basic connection test passed

=== Test 2: Multiple Messages ===
Received: Response_1
Received: Response_2
...
Stats:
  Messages received: 10
  Messages sent: 10
  Pending in send queue: 0
✓ Multiple messages test passed

=== Test 3: Concurrent Processing ===
Max concurrent handlers: 8
✓ Concurrent processing test passed

=== Test 4: Security Rejection ===
[SECURITY] Rejected connection from 127.0.0.1:xxxxx
✓ Security rejection test passed

=== Test 5: Graceful Shutdown ===
Stopping server...
Completed messages: 3
✓ Graceful shutdown test passed

==========================================
  All tests passed! ✓
==========================================