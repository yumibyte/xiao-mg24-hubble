# xiao-mg24-hubble

Hubble satellite firmware for the Seeed XIAO MG24, on Zephyr.

`app/` starts from the Hubble SDK's `sat-continuous` sample: it sends a satellite packet every
`CONFIG_SAMPLE_SAT_TX_INTERVAL_SECONDS` (10 s by default).

[docs/satellite-packet.md](docs/satellite-packet.md) explains the satellite packet format, the 13-byte
payload limit and the 1,024-new-packets-per-day rule.

## Setup

```sh
./scripts/setup.sh
```

Needs only `git` and [uv](https://docs.astral.sh/uv/). This builds a shared Zephyr workspace at
`../xiao-ws/` (next to the main checkout; override with `XIAO_WS`) from the pins in `west.yml` and
`requirements.txt`, native to your machine even when launched from a Rosetta shell. It is safe to re-run, and
should be re-run whenever those files change. Every checkout and worktree of this repo uses the same
workspace.

## Build and flash

```sh
source ../xiao-ws/env.sh
west build -b xiao_mg24 app -- -DCONFIG_SAMPLE_HUBBLE_KEY=\"<base64 device key>\"
west flash -r pyocd
```

For the Nordic nRF21540-DK (nRF52840 with the nRF21540 +20 dBm front-end), build for
`nrf21540dk/nrf52840` and flash with [SEGGER J-Link](https://www.segger.com/downloads/jlink/):

```sh
west build -b nrf21540dk/nrf52840 app -- -DCONFIG_SAMPLE_HUBBLE_KEY=\"<base64 device key>\"
west flash -r jlink
```

To transmit only around a predicted pass, power the board on at a planned time and set when to start
and how long to send, relative to power-on. For example, power on at 01:50 for a pass rising at 02:00,
then send one packet a second for 15 minutes from 01:55:

```sh
west build -b nrf21540dk/nrf52840 app -- -DCONFIG_SAMPLE_HUBBLE_KEY=\"<key>\" \
  -DCONFIG_SAMPLE_SAT_TX_INTERVAL_SECONDS=1 \
  -DCONFIG_SAMPLE_SAT_TX_START_DELAY_SECONDS=300 \
  -DCONFIG_SAMPLE_SAT_TX_WINDOW_SECONDS=900
```

Every packet gets a new sequence number, so the build fails if the window would need more than the
1,024 a key gets per day ([docs/satellite-packet.md](docs/satellite-packet.md)).

Register the device key in Hubble as **AES-256** with the **Device uptime** counter source. Without a
key the firmware still builds and transmits, using an all-zero placeholder key.

Logging is off by default, because builds with it on were not received by the satellite network.
For bench debugging, add `-DEXTRA_CONF_FILE=debug.conf` to the build to get logs on the board's USB
serial port, and flash a build without it before testing reception.
