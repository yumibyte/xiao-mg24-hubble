# Hubble satellite packet format

What a Hubble satellite packet looks like on the air, and why it limits how much data a device can
send.

Hubble documents the BLE packet ([Advertising Packet](https://hubble.com/docs/device-sdk/terrestrial/advertising-packet))
but not the satellite one, so this page is worked out from the SDK source at v3.1.0
([`src/hubble_sat_packet.c`](https://github.com/HubbleNetwork/hubble-device-sdk/blob/f45e1c7ad04f2c536debb93cbf253c521db22e28/src/hubble_sat_packet.c)).
Treat it as our reading of the code, not a Hubble spec. The receiving side (satellite and Hubble
cloud) expects exactly this format, so none of it can be changed except the payload.

## Layer 1: the Hubble message

Built by `hubble_sat_packet_get()`.

```
┌─────────┬──────────────┬──────────────┬──────────────┬──────────────────────────┐
│ version │ packet number│  device ID   │ tamper check │      YOUR DATA           │
│ 2 bits  │   10 bits    │   32 bits    │   32 bits    │  0, 4, 9 or 13 bytes     │
└─────────┴──────────────┴──────────────┴──────────────┴──────────────────────────┘
   Hubble's header: 76 bits, fixed                         ↑ encrypted with the device key
```

| Field | Bits | Purpose |
|---|---|---|
| Version | 2 | Payload protocol version, currently `0`. Leaves room for up to four formats |
| Packet number (sequence number) | 10 | Counts 0–1023, then wraps. The cloud needs it to decrypt (see [the 1,024 limit](#the-1024-new-packets-per-day-limit)) |
| Device ID (EID) | 32 | Tells the cloud which device sent the packet. Changes daily so the device can't be tracked by a fixed ID |
| Tamper check (auth tag) | 32 | AES-CMAC over the encrypted data: proves the packet came from this key and wasn't altered |
| Payload | 0 / 32 / 72 / 104 | Application data, AES-CTR encrypted. Only these four sizes are accepted |

The packet number and the time the cloud receives the packet come with every packet, so the payload doesn't
need its own counter or timestamp.

## Layer 2: error correction

The message is cut into 6-bit symbols, and Reed-Solomon error-correction symbols are appended so the
receiver can repair bits damaged on the way to orbit. Bigger payloads get more correction.

| Payload | Message symbols | Error-correction symbols | Total data symbols |
|---|---|---|---|
| 0 bytes | 13 | 10 | 23 |
| 4 bytes | 18 | 12 | 30 |
| 9 bytes | 25 | 14 | 39 |
| 13 bytes | 30 | 16 | 46 |

The satellite only recognises these four totals. That's why the SDK rejects any other payload length.

## Layer 3: what goes on the air

Built by `hubble_sat_packet_frames_get()`.

```
frame 0 (channel A)                          frame 1 (B)        frame 2 (C)        frame 3 (D)
┌──────────┬────────┬────────┬───────────┐   ┌──────────────┐   ┌──────────────┐   ┌──────────┐
│ preamble │ PHY hdr│ PHY ECC│ data …    │ → │ data …       │ → │ data …       │ → │ data …   │
│ 8 sym    │ 2 sym  │ 4 sym  │ 2 sym     │   │ 16 sym       │   │ 16 sym       │   │ 12 sym   │
└──────────┴────────┴────────┴───────────┘   └──────────────┘   └──────────────┘   └──────────┘
  (13-byte payload: 14 + 46 = 60 symbols in 4 frames)
```

- **Preamble:** a fixed pattern (`63, 0, 63, 0, 63, 0, 63, 63`) the receiver uses to find the start
  of the packet.
- **PHY header:** 12 bits giving the PHY version (`1`), which of the four sizes follows, the hopping
  sequence and the starting channel. It has its own 4 error-correction symbols.
- **Data:** the Layer 2 symbols, whitened (scrambled with a channel-seeded pattern so long runs of the
  same value don't occur), then split into frames of up to 16 symbols. Each frame goes out on a
  different channel.
- **Symbols:** each 6-bit symbol (0–63) is sent as one carrier tone at one of 64 frequency offsets,
  for 8 ms with a 0.8 ms gap (`HUBBLE_WAIT_SYMBOL_US`, `HUBBLE_WAIT_SYMBOL_OFF_US`).

## Time on air

| Payload | Symbols on air | Time on air (× 8.8 ms) |
|---|---|---|
| 0 bytes | 37 | ~326 ms |
| 4 bytes | 44 | ~387 ms |
| 9 bytes | 53 | ~466 ms |
| 13 bytes | 60 | ~528 ms |

On the XIAO MG24 bench board, the first packet's "sent" log appeared at 0.331 s with an empty payload
and at 0.534 s with a 13-byte one, which matches.

## The 1,024 new packets per day limit

Hubble's docs: "You may generate between 1 and 1,024 new payloads per day … The 1,024 limit is set by
the sequence number field size. Exceeding it risks nonce reuse"
([Custom Payload](https://hubble.com/docs/guides/device-integration/custom-payload), FAQ 4; also
[Timing Management](https://hubble.com/docs/guides/device-integration/timing-management)). The docs
say it about BLE advertisements, but the satellite packet uses the same counter.

Put simply:

1. Encryption mixes the payload with a pattern made from the key and a **one-time number**. Using the
   same one-time number for two different messages lets someone who captures both start working out
   their contents.
2. The one-time number comes from the **day number** (uptime ÷ 86,400 s with the device-uptime
   counter) and the **packet number**.
3. The packet number has 10 bits, so it counts 0–1023 and then wraps
   ([`hubble_crypto.c`](https://github.com/HubbleNetwork/hubble-device-sdk/blob/f45e1c7ad04f2c536debb93cbf253c521db22e28/src/hubble_crypto.c#L55-L65)).
   After 1,024 new packets in a day, numbers repeat.
4. A new day gives a new day number and a fresh 1,024.

**Only building a new packet uses a number.** Transmitting the same packet again doesn't, so a
packet can be resent as often as power and regulations allow.

What happens on packet 1,025 in a day depends on `CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK`:

| Nonce check | Result |
|---|---|
| On (SDK default) | `hubble_sat_packet_get()` refuses (`-EPERM`) until the day rolls over |
| Off (Hubble's `sat-continuous` sample) | The number silently wraps and nonces are reused |

The day length can't be changed to get more numbers: `CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC` is fixed at
86,400. The limit is per device key.

## What this means for throughput

- **Per packet:** at most 13 bytes of data.
- **Per day, securely:** at most 1,024 new packets, so 13,312 bytes per device.
- **In practice, passes limit you first.** Only packets sent while a satellite is overhead can arrive.
  With two ~2-minute passes a day and ~0.53 s per 13-byte packet, back-to-back sending fits roughly
  450 new packets (~5.8 KB) into the passes, well under 1,024. That's an upper bound that assumes
  back-to-back sending is allowed and every packet is received.
- **New packets built outside passes waste numbers.** A device that builds a new packet every 10 s all
  day makes ~8,000 a day: only ~20 are sent during passes, and the counter wraps about 8 times.

To get more data through, pack the 13 bytes efficiently: bit-pack values, send differences rather than
absolute readings, and don't repeat what the cloud already provides.
