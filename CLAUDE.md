# Agent instructions

## Reference docs

Datasheets and the Hubble docs are kept locally in `docs/local/`, not committed. Run `docs/fetch_docs.sh` to build or refresh them. Check them before guessing at pinouts, registers or SDK APIs.

- `docs/local/datasheets/*.txt` -- text of every PDF in the repo (XIAO MG24 board datasheet)
- `docs/local/hubble-device-sdk/<ref>/` -- Hubble Device SDK sources: `docs/**/*.rst` (the published docs), `include/hubble/` (API headers, which are the API reference), `port/`, `samples/`
  - `release_3.1-branch` matches the published 3.1 docs
  - `main` adds the Silicon Labs port (`port/sisdk`) and the Silabs quickstart/integration guides
- `docs/local/hubble.com/llms-*.txt` -- hubble.com/docs as plain text (network, device SDK, cloud API, guides). `llms.txt` is the table of contents

`docs/local/` is gitignored, so ripgrep-based search skips it unless you point at it explicitly (pass the path, or `--no-ignore`).

## Firmware toolchain

The Zephyr workspace (Zephyr, HALs, Hubble SDK, toolchain, venv) lives outside the repo, in `xiao-ws/` next to the main checkout, shared by all worktrees. `west.yml` and `requirements.txt` pin it (no Homebrew); `scripts/setup.sh` builds it.

- `source <xiao-ws>/env.sh` before any `west` command
- Change versions by editing the pins and re-running `scripts/setup.sh`, never by hand in the workspace

### Editing the Hubble SDK

The SDK comes from our fork, `yumibyte/hubble-device-sdk`. Its checkout at `<xiao-ws>/modules/lib/hubblenetwork-sdk` has remotes `fork` (ours) and `upstream` (HubbleNetwork).

- Work on a branch there (`xiao-mg24`, based on v3.1.0), commit, and push to `fork`
- Then bump `revision:` in `west.yml` to that commit in a PR here; the build only uses what `west.yml` pins
- `west update` checks out the pinned commit detached, leaving the branch and its commits in place

## Git

- Never force-push (`git push --force`, `--force-with-lease`, or equivalent)

## Generated outputs

Git keeps every committed binary forever, so regenerable outputs bloat the repo for everyone.

- Do not commit datasheets, vendor PDFs, or anything `docs/fetch_docs.sh` produces -- commit the script that makes them instead
- Do not commit build outputs, logs, or captures that a script or tool produces
- Write outputs to a gitignored directory (e.g. `docs/local/`) or a folder outside the repo
- Do not commit new large raw data captures without asking first; point scripts at the local copy by path

## Pull requests

When writing PR titles or bodies, describe intent and effect — not a tour of identifiers.

- Prefer what changed and why (behavior, structure, risk)
- Do not verbosely restate variable, function, type, or file names that the diff already shows
- Add at least one fitting label from the repo's label set (`bug`, `enhancement`, `documentation`, ...) -- `gh pr create --label <name>`
- Assign the PR to the person it is opened for (`--assignee @me` when working on the user's behalf)

## Attribution

- Do not credit Claude on commits or PRs: no `Co-Authored-By: Claude ...` trailers, no "Generated with Claude Code" lines -- the human author owns the change

## Comments (code and review)

- Explain intention, invariants, and non-obvious constraints
- Do not narrate names (`// increment count`, `// set foo to bar`)

## PR size

- Keep each PR's diff as small as possible: one focused change per PR
- Split unrelated or separable work (guidance, refactors, new features, data) into separate PRs, even if they come up in the same session

## Stacked PRs

When a PR depends on another open PR, make it part of a GitHub stack (`gh stack`, from the `github/gh-stack` extension) instead of a loose PR against the parent's branch.

- Open each PR with its base set to the branch below it, then link them bottom to top by PR number: `gh stack link <bottom-pr> <next-pr> ...` (PR numbers push nothing)
- Merge with `gh stack merge`, which merges the stack in one all-or-nothing step
- Do not use `gh stack sync`, `push`, `rebase` or `submit` -- they push with `--force-with-lease`, which is a force-push (see Git). Push with plain `git push`, and bring a branch up to date by merging its parent, not rebasing
