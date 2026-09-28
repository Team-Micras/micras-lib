# Communication protocol

The session layer the robot runs over a byte stream to expose its variables to an application such
as [micras-monitor](https://github.com/Team-Micras/micras-monitor): a schema, streamed groups of
samples, writes, reads and commands. It knows nothing of the transport below the stream; the project
that uses it documents the radio and the wiring it runs over.

The protocol assumes a transport with no flow control, which drops bytes silently when its buffer
fills, as a serial Bluetooth module without `RTS`/`CTS` does. The credit window of the session layer
exists to keep it from filling.

## Framing

Frames are [COBS](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing) encoded and
delimited by a zero byte:

```text
COBS( type | payload | fletcher16 )  0x00
```

The frame check is Fletcher-16 over the type and the payload, before encoding, little endian. It is
not protecting against the radio, which has a CRC-24 and retransmits until acknowledged. It covers
the two hops BLE never sees: the 8N1 serial port with no parity between the microcontroller and the
module, and the module's buffer, which drops runs of bytes with no indication that it did. A
dropped run can join the head of one frame to the tail of the next into something with a plausible
shape, and the two running sums make that about as unlikely as a sixteen bit check can: one over
sixty five thousand, against one in two hundred and fifty five for a single sum of bytes. It is
weaker than a CRC on long bursts, which is what a CRC is uniquely good at, and it is four lines
that live beside the codec they belong to.

Message types, their payloads and the meaning of every field are in
`micras_comm/include/micras/comm/protocol.hpp`. Everything on the wire is little endian, which is
what both the microcontroller and `DataView` in the browser already are.

## The parts worth knowing before reading the code

- **The schema is fetched once per firmware build, not once per connection.** `HELLO_ACK` carries a
  `schema_hash` over every name, type and access flag in order. Identifiers are registration order,
  so adding one variable shifts every later one; comparing the hash against the one a cached schema
  was fetched with is what stops the application from plotting the wrong signal.
- **Samples come in groups, not one variable at a time.** A `SAMPLE` carries several variables
  captured in the same control loop iteration under one timestamp. A response plotted against a
  setpoint captured two iterations later is not a plot of a control loop.
- **The application has to return credit.** The robot may have at most `initial_credit` bytes
  outstanding. `CREDIT` says how many more bytes the application has taken. When the window is
  closed, samples are dropped and the sequence number shows the gap; replies to requests are not
  charged to the window, because they are already bounded by the rate of the requests themselves.
- **Writes are levels and commands are edges.** `WRITE` sets a gain or a flag and is acknowledged
  with a result; the `idle` flag on a variable refuses the dangerous ones while the robot is moving.
  `COMMAND` happens once, when it arrives.
- **The link cannot carry the control loop.** It is between twenty and a hundred times too slow for
  8 kHz, so a group is defined with a period in loop iterations and only every period-th iteration
  is sent. The rate the application asks for is a rate it can actually receive.
