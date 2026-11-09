# TCP Server Implementation Plan

## Goal

Add a FreeRTOS TCP server under `applications/tcp/` using the lwIP
BSD-socket API. The server must support multiple simultaneous clients without
blocking one client while another is idle. Each client exposes pointer-based
read and send-buffer functions so application code can inspect received bytes
and queue outgoing bytes without copying through an extra API buffer.
The server must never have more than four active client connections.

## Proposed architecture

- One server task owns the listening socket, all client sockets, and client
  state. No other task calls `recv()`, `send()`, `close()`, or changes socket
  state directly.
- Use `lwip_socket()`, `lwip_bind()`, `lwip_listen()`, `lwip_accept()`,
  `lwip_select()`, `lwip_recv()`, `lwip_send()`, and `lwip_close()` consistently.
- Set the listener and every accepted client non-blocking with
  `lwip_ioctl(fd, FIONBIO, &enabled)`.
- Use `lwip_select()` with a bounded timeout to wait for listener readability,
  client readability, and client writability when queued transmit bytes exist.
  Keep the timeout finite so the task can process shutdown requests and timers.
- Drain pending accepts until `EAGAIN`/`EWOULDBLOCK`; one accept per loop can
  leave queued connections waiting unnecessarily.
- Bound clients and per-client buffers with compile-time configuration. The
  hard maximum is four active clients; any configured client limit must be in
  the range 1 through 4. Check descriptors against lwIP's `FD_SETSIZE` before
  adding them to a select set.
- Keep network operations in task context, not in ISRs or lwIP callbacks.

## Client state and pointer contract

Each client record should contain at least:

- socket descriptor and lifecycle state;
- remote address for logging/diagnostics;
- fixed-size RX and TX ring buffers;
- RX read position and received-data write position;
- TX queued-data read position and application write position;
- optional last-activity tick for idle timeout handling.

Provide the client API:

```c
const uint8_t *tcp_client_read_ptr(tcp_client_t *client,
                                   size_t *contiguous_length);
bool tcp_client_read_consume(tcp_client_t *client, size_t length);
uint8_t *tcp_client_send_ptr(tcp_client_t *client,
                             size_t *contiguous_capacity);
bool tcp_client_send_commit(tcp_client_t *client, size_t length);
```

- `read_ptr` returns the next contiguous unread bytes; `read_consume` advances
  the RX read position after the application has handled those bytes.
- `send_ptr` returns contiguous free TX-buffer space; the application writes
  there and calls `send_commit` to publish those bytes to the server task.
- Pointer lengths never extend across the physical end of a ring buffer.
  Callers loop through contiguous spans to handle wrap-around.
- A returned pointer is borrowed, not owned. It remains valid only until the
  matching consume/commit call or a client close/reset. Callers must not retain
  it after that point.
- Only the server task advances socket receive/transmit positions. If other
  tasks use the pointer API, protect reservation/commit operations with a
  mutex or route all operations through a server-task message queue. Do not
  allow concurrent users to reserve the same span.
- The pointer API does not imply message boundaries. TCP is a byte stream; the
  application protocol must frame messages (for example with a fixed header,
  length prefix, or delimiter).

## Socket event loop

1. Create, configure, bind, and listen on the server socket. Make it
   non-blocking and check every syscall result.
2. Build the read set from the listener and active clients. Add a client to the
   write set only when its TX ring contains queued bytes.
3. Call `lwip_select()` with a finite timeout. Handle `EINTR` by rebuilding
   sets; treat other select errors as a logged server error and recover or
   restart the listener.
4. Accept all pending clients when the listener is readable. Initialize a free
   client slot before publishing the client to application code. If the
   configured client limit is reached (never greater than four), close the
   newly accepted socket and log the rejection. A rejected connection must not
   consume a client slot or change the active-client count.
5. On client readability, receive into the contiguous free RX span. Repeat
   while data is available and capacity remains. A positive read advances the
   RX write position; `EAGAIN`/`EWOULDBLOCK` means the client is still open;
   zero means orderly peer disconnect; other errors close that client.
6. On client writability, send from the contiguous queued TX span. Advance the
   TX read position only by the number of bytes actually sent. Preserve any
   unsent tail for the next writable event; handle partial writes and
   `EAGAIN`/`EWOULDBLOCK` without dropping data.
7. Notify application code that data is available or send capacity has changed
   using a callback or queue event. Keep callbacks bounded and non-blocking.
8. On disconnect/error, remove the descriptor from all sets, close it once,
   reset buffer positions, and notify the application once before reusing the
   slot.

## Public module surface

Create:

- `applications/tcp/tcp_server.h` — opaque server type, configuration,
  lifecycle functions, active-client count, and event callback declarations.
- `applications/tcp/tcp_client.h` — opaque client type, remote endpoint lookup,
  and pointer accessors with consume/commit calls.
- `applications/tcp/tcp_server.c` — server task, socket setup, select
  loop, accept/read/write handling, client lifecycle, and notifications.
- `applications/tcp/tcp_client.c` — fixed-size client RX/TX ring buffers and
  their public accessors.
- `applications/tcp/CMakeLists.txt` — an application library linked
  with the existing lwIP and FreeRTOS targets.

Use a 512-byte RX and TX ring per client. Keep listen address/port, maximum
client count (1 through 4), listen backlog, select timeout, task stack, and
task priority in the configuration. Use statically allocated server state,
task control block, 768-word task stack, and binary semaphores; permit only one
active server. Default a zero select timeout to 100 ms and cap it at 1000 ms
so shutdown remains bounded. Reject invalid client, priority, and timeout
configurations.

## Failure handling and resource ownership

- Check socket creation, bind, listen, non-blocking configuration, select,
  accept, receive, send, and close paths; log the failing operation and lwIP
  error.
- Close partially initialized resources on every startup failure.
- Define server stop behavior: request stop through a queue/event, wake the
  select loop (or use its bounded timeout), close clients and listener in the
  owning task, then report task completion.
- Do not spin on non-blocking `EAGAIN`; return to `select()` or yield.
- Decide whether a full RX buffer pauses reads (backpressure) or closes clients;
  prefer pausing reads until application code consumes data.
- Define TX-full behavior explicitly: return `false` from `send_commit` if the
  requested length exceeds available space; never overwrite queued bytes.

## Validation plan

1. Unit-test ring-buffer pointer spans, wrap-around, consume/commit bounds,
   and full/empty states without lwIP.
2. Add a host socket test or mocked lwIP layer for multiple clients up to the
   configured limit, partial sends, `EAGAIN`, peer close, and rejection of an
   attempted fifth connection while four clients are active. Confirm rejected
   connections never increase the active-client count and verify listener
   recovery.
3. Build the firmware target and confirm the server library links only against
   the existing lwIP/FreeRTOS configuration.
4. On hardware, connect multiple clients, leave one idle while another
   exchanges data, exercise RX/TX wrap-around, then test disconnect/reconnect
   and server shutdown.

## Implementation sequence

1. Confirm the buffer RAM budget for STM32H563 and enforce the four-client hard
   maximum in the configuration and accept path.
2. Implement and unit-test the per-client RX/TX ring-buffer API.
3. Add listener setup, non-blocking configuration, and bounded select loop.
4. Add accept, receive, send, and disconnect handling.
5. Add application notifications and lifecycle integration.
6. Add tests, build validation, and an on-device multi-client test procedure.