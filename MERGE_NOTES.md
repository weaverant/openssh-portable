# Upstream Merge Notes

How to merge a new upstream OpenSSH release into this Win32 fork. Written from
the 10.3p1 merge; update this file as each new merge teaches us something.

## Remotes

Two upstreams, both required:

| Remote    | URL                                                  | Role                             |
| --------- | ---------------------------------------------------- | -------------------------------- |
| `openssh` | https://github.com/openssh/openssh-portable.git      | Canonical OpenSSH releases       |
| `win32`   | https://github.com/PowerShell/openssh-portable.git   | Microsoft's Win32 port base      |
| `origin`  | https://github.com/weaverant/openssh-portable.git    | This fork                        |

One-time setup:

```sh
git remote add openssh https://github.com/openssh/openssh-portable.git
git remote add win32   https://github.com/PowerShell/openssh-portable.git
```

## What we track

- New releases from `openssh/openssh-portable` — tagged `V_<X>_<Y>_P<N>`
  (e.g. `V_10_3_P1`, `V_10_3_P2`). Both feature releases and portable-only
  security patches matter.
- Occasional updates to `win32/latestw_all` (PowerShell/openssh-portable).
  Infrequent; last moved 2026-02-02. Worth a look before each merge but not
  a blocker if nothing changed.

Monitoring is currently a GitHub release watch on `openssh/openssh-portable`.
No automation yet.

## Merge procedure

```sh
git fetch --tags openssh
git fetch win32 latestw_all

# Branch from the tip of the last release merge (current origin/HEAD),
# NOT from latestw_all — we are accumulating our own Win32 fixes.
git checkout -b merge-<X>.<Y>p<N> origin/HEAD

# Optional: if win32/latestw_all has moved since our last base, pull it first.
# git merge win32/latestw_all

git merge V_<X>_<Y>_P<N>    # resolve conflicts — see hotspots below
```

Then build, fix, test, tag:

```sh
# Build all 14 main binaries (Visual Studio solution in contrib/win32/openssh)
# Fix-up commits on top of the merge — do NOT amend the merge commit.
git tag v<X>.<Y>p<N>-win32
```

## Conflict hotspots

These paths conflicted or needed manual follow-up on the 10.3p1 merge.
Expect the same areas next time.

### `contrib/win32/openssh/` — Visual Studio project files

- `libssh.vcxproj`, `sshd-auth.vcxproj`, `sshd-session.vcxproj`,
  `unittest-*.vcxproj` — add/remove source files whenever upstream adds
  or removes `.c` files. 10.3p1 added `ssherr-libcrypto.c`, `misc-agent.c`,
  removed `ssh-dss.c`, `ssh-xmss.c`, `xmss_*.c`, `sshkey-xmss.c`.
- `config.h.vs` — Windows-only `config.h`. Keep our `S_ISSOCK` macro and
  `PLEDGE_EXTRA_INET` additions. Add any new upstream feature macros.
- `paths.targets`, `OpenSSHBuildHelper.psm1` — MSBuild plumbing; rarely
  conflicts but check when upstream changes its build.

### `contrib/win32/win32compat/inc/` — header shims

Upstream sometimes starts `#include`ing BSD headers that do not exist on
Windows. We ship stubs for them here. When upstream adds a new include,
add a matching shim.

Existing shims touched by 10.3p1: `sys/queue.h`, `sys/tree.h`, `sys/stat.h`,
`endian.h`, `glob.h`, `ifaddrs.h`, `netgroup.h`, `nlist.h`, `paths.h`,
`util.h`, `crtheaders.h`.

### `contrib/win32/win32compat/` — source shims

- `w32-doexec.c` — process spawning. Watch for signature changes to
  upstream helpers it mirrors.
- `ssh-agent/keyagent-request.c` — agent request handling. PKCS11 dispatch
  changed in 10.3p1 (keyblob-based, replacing `RSA_METHOD`/`EC_KEY_METHOD`);
  future agent/PKCS11 changes upstream will show up here.
- `pwd.c` — Windows has no `sshd` privsep user; `getpwnam("sshd")` always
  fails. The `lookup_sid` debug message is at level 3 so it only shows
  under `-ddd`. Preserve this.

### `openbsd-compat/`

- `glob.c`, `arc4random.c`, `openbsd-compat.h` — edited on 10.3p1 for
  Windows-specific behavior. Prefer pushing Windows fixes into
  `contrib/win32/win32compat/` when possible to shrink this surface.

### Upstream-file edits to preserve

Some upstream files carry Windows-specific edits. Recurring ones:

- **`sshd-session.c`** — do NOT double-`waitpid` on Windows. On
  `FORK_NOT_SUPPORTED`, `monitor_child_preauth()` already reaps the child;
  a second `waitpid` in `privsep_preauth` returns `ECHILD` and the session
  is killed right after auth. Introduced as a follow-up fix to 10.3p1
  (0d1346ca1) — check this still holds after future upstream changes to
  the privsep path.
- **`ssh_packet_set_interactive` call sites** — the signature changed in
  10.3p1; Win32 compat code was updated to match. Recheck on signature
  changes.

## Conventions during a merge

- **Don't resurrect removed upstream features.** 10.3p1 removed DSA and
  XMSS; we took the removal. Don't keep them alive in Win32 code paths.
- **Don't resurrect PowerShell team CI.** `.azdo/`, `.github/`, and
  `AzDOBuildTools/` were removed; they belonged to Microsoft's release
  pipeline and don't apply here.
- **Keep key defaults aligned with upstream** (ed25519 / ECDSA 256-bit at
  10.3p1). Deviating requires a documented reason.
- **Our README replaces upstream's.** After each merge, restore this
  fork's README (prerequisites, Windows build steps, known issues).
  Upstream's README will clobber it in a fast-forward-heavy merge.

## Post-merge verification checklist

1. **Build all 14 main binaries** from `contrib/win32/openssh` solution.
   Build-clean is the bar; warnings are acceptable.
2. **Build unit tests.** `unittest-misc` and `unittest-win32compat` often
   break first because upstream's test infrastructure churns (new
   `test_*.c` files, new stubs needed). Add missing sources to the
   `.vcxproj`; add stubs for `benchmarks()` etc. to win32compat tests.
3. **Smoke test interactive login** — password auth especially, since
   `FORK_NOT_SUPPORTED` regressions show up here.
4. **Smoke test `scp` and `sftp`** — file transfer path.
5. **Build the MSI** — `contrib/win32/install/`. WiX v6 (migrated in
   5b8de34db); no manual WiX download needed.
6. **Tag** `v<X>.<Y>p<N>-win32` once verification passes.

## Version floor

The Win32 base (`win32/latestw_all`) is at 10.0p2. Every upstream release
after that merges through us, not through PowerShell — expect the Win32
compat layer to need more adjustment the further upstream moves ahead
of `latestw_all`.
