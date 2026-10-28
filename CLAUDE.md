# Agent instructions

## Reference docs

Datasheets and the Hubble docs are kept locally in `docs/local/`, not committed. Run `docs/fetch_docs.sh` to build or refresh them. Check them before guessing at pinouts, registers or SDK APIs.

- `docs/local/datasheets/*.txt` -- text of every PDF in the repo (XIAO MG24 board datasheet)
- `docs/local/hubble-device-sdk/<ref>/` -- Hubble Device SDK sources: `docs/**/*.rst` (the published docs), `include/hubble/` (API headers, which are the API reference), `port/`, `samples/`
  - `release_3.1-branch` matches the published 3.1 docs
  - `main` adds the Silicon Labs port (`port/sisdk`) and the Silabs quickstart/integration guides
- `docs/local/hubble.com/llms-*.txt` -- hubble.com/docs as plain text (network, device SDK, cloud API, guides). `llms.txt` is the table of contents

`docs/local/` is gitignored, so ripgrep-based search skips it unless you point at it explicitly (pass the path, or `--no-ignore`).
