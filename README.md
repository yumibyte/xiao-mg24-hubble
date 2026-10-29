# xiao-mg24-hubble

Hubble satellite firmware for the Seeed XIAO MG24, on Zephyr.

## Setup

```sh
./scripts/bootstrap.sh
```

Needs only `git` and [uv](https://docs.astral.sh/uv/). This builds a shared Zephyr workspace at
`../xiao-ws/` (next to the main checkout; override with `XIAO_WS`) from the pins in `west.yml` and
`requirements.txt`, native to your machine even when launched from a Rosetta shell. It is safe to re-run, and
should be re-run whenever those files change. Every checkout and worktree of this repo uses the same
workspace.

