# Udp.Asio

`PiSubmarine.Udp.Asio` implements `Udp.Api::ISocket` using `boost::asio`.

## Design goals

- no callbacks
- no background threads
- non-blocking socket operations only
- ticking inside the concrete implementation
- bounded internal receive buffering

## Receive path

`Socket::Tick(...)` repeatedly calls non-blocking `receive_from(...)` until:

- no more datagrams are available
- or the current tick finishes draining the socket

Received datagrams are stored in a circular buffer and later drained with
`TryReceive()`. If the ring buffer is full, the oldest buffered datagram is
dropped so the newest datagram can be retained. This matches the "latest packet
wins" semantics expected by control and telemetry flows.

## Send path

`Send(...)` performs an immediate non-blocking `send_to(...)`. If the socket is
not ready or the destination is invalid, the call returns an error.

Higher-level modules should depend on `Udp.Api::ISender` / `IReceiver`, while
composition code may keep a reference to `Time::ITickable` for the same socket.
