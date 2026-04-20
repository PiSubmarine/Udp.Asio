# Udp.Asio

`PiSubmarine.Udp.Asio` implements `Udp.Api::ISocket` using `boost::asio`.

## Design goals

- no callbacks
- no background threads
- non-blocking socket operations only
- explicit polling by the caller
- bounded internal receive buffering

## Receive path

`Socket::Poll()` repeatedly calls non-blocking `receive_from(...)` until:

- no more datagrams are available
- or the internal receive ring buffer is full

Received datagrams are stored in a circular buffer and later drained with
`TryReceive()`.

## Send path

`Send(...)` performs an immediate non-blocking `send_to(...)`. If the socket is
not ready or the destination is invalid, the call returns an error.
