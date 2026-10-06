# Upstream Merge Notes

How to merge a new upstream OpenSSH release into this Win32 fork. Written from
the 10.3p1, 10.4p1, 10.5p1 and 10.6p1 merges; update this file as each new merge
teaches us something.

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

**Check the remotes actually exist before starting.** On the 10.5p1 merge both
were missing from the working clone while stale `remotes/win32/*` tracking refs
survived — so `git log win32/latestw_all` answered from a months-old snapshot
and only `git fetch` revealed the remote was gone. A tracking ref is not proof
of a configured remote.

## What we track

- New releases from `openssh/openssh-portable` — tagged `V_<X>_<Y>_P<N>`
  (e.g. `V_10_3_P1`, `V_10_3_P2`). Both feature releases and portable-only
  security patches matter.
- Updates to `win32/latestw_all` (PowerShell/openssh-portable). Quiet for
  months, then Microsoft merged 10.1p1, 10.2p1 and 10.3p1 within ten days
  (last moved 2026-09-25). Check before each merge; when it has moved, see
  "When the base has merged releases we already carry" below.

Monitoring is currently a GitHub release watch on `openssh/openssh-portable`.
No automation yet.

## Merge procedure

```sh
git fetch --tags openssh
git fetch win32 latestw_all

# Branch from the tip of the last release merge (current origin/HEAD),
# NOT from latestw_all — we are accumulating our own Win32 fixes.
# origin/HEAD is a local pointer that never follows the GitHub default by
# itself: refresh it first, or this branches from wherever it pointed when the
# clone was made. --no-track keeps the previous release branch from becoming
# the new branch's upstream.
git remote set-head origin -a
git checkout --no-track -b merge-<X>.<Y>p<N> origin/HEAD

# If win32/latestw_all has moved since our last base, merge it FIRST, as its
# own commit. Doing it after the release tag means resolving the same
# contrib/win32/openssh project files twice, in the harder direction.
# git merge win32/latestw_all

git merge V_<X>_<Y>_P<N>    # resolve conflicts — see hotspots below
```

Merging `latestw_all` first gives the release merge **two merge bases**, so git
builds a virtual base and the generated release artifacts (`configure`,
`config.h.in`, `ChangeLog`, the `*.0` man pages) arrive as `add/add` conflicts
instead of clean merges. Take upstream for all of them — they are generated,
we never hand-edit them.

### When the base has merged releases we already carry

From the 2026-10 base merge (Microsoft at 10.3p1, us at 10.5p1). Both sides had
merged the same upstream tags, so this merge has two merge bases as well.

- **Generated files conflict the other way round.** `configure`, `config.h.in`,
  `ChangeLog` and the `*.0` pages: keep ours, we hold the newer release.
- **Read the Windows deltas, not the conflict hunks.** With two merge bases the
  markers nest and are close to unreadable. Compare
  `git diff V_<their release> win32/latestw_all -- <file>` with
  `git diff V_<our release> HEAD -- <file>`: what each side adds to upstream.
  That found three defects in our own earlier resolutions, all fixed by taking
  Microsoft's side: the Windows copy of the certificate principal match in
  `sshkey.c` still had the argument order upstream reversed in 10.3p1, the
  banner-exchange telemetry call was lost when `kex_exchange_identification`
  moved to `sshd-auth.c`, and `ssh-sk-client.c` created a pipe on Windows that
  only the non-Windows path closed.
- **A clean auto-merge can still duplicate.** Both sides had added the same
  thing in different spellings and git reported no conflict: `misc-agent.c`
  twice in `sshd-auth.vcxproj` and `sshd-session.vcxproj`
  (`$(OpenSSH-Src-Path)` against `..\..\..\`), two `test_*.c` entries in
  `unittest-misc.vcxproj`, a second `benchmarks()` in the win32compat tests, a
  second `PLEDGE_EXTRA_INET` in `config.h.vs`. Read the whole staged diff
  against our branch, not only the conflicted files.
- **Taking one side per hunk can unbalance `#ifdef` nesting** where both sides
  restructured the same function (`ssh-sk-client.c`). Diff the result against
  each whole side afterwards.
- Dropped under the CI convention below: Microsoft's agent tooling in
  `.github/`, `.vscode/mcp.json` and `contrib/win32/openssh/code_coverage/`.

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
  10.4p1 added `libcrux-mlkem-mldsa.c` and `ssh-mldsa-eddsa.c` to
  `libssh.vcxproj`. mlkem768's implementation moved out of the header-only
  `libcrux_mlkem768_sha3.h` (deleted) into the new `libcrux-mlkem-mldsa.c`
  translation unit; since the Win32 build enables `USE_MLKEM768X25519`,
  omitting that `.c` fails the link with unresolved `crypto_kem_mlkem768_*`.
  10.5p1 added `kexmlkem768ecdh.c` (mlkem768nistp256-sha256).
  10.6p1 added nothing the Win32 build needs and dropped `sshpty.o` from
  `SSHDOBJS`, so `win32_sshpty.c` left `sshd.vcxproj`.
  Fastest way to catch these: extract `LIBSSH_OBJS` from `Makefile.in`, map
  `.o`→`.c`, and diff against the `ClCompile` entries in `libssh.vcxproj`.
  Three names always show up as missing and are absent **by design**, so don't
  "fix" them: `ttymodes.c` (superseded by `win32compat/ttymodes_windows.c`),
  `umac128.c` (aliased to `umac.c` via `#define`s in `config.h.vs`) and
  `sftp-realpath.c` (Windows has its own `realpath` in `win32compat`).
  `ed25519-openssl.c` is listed since the 2026-10 base merge but compiles to
  nothing: it is gated on `OPENSSL_HAS_ED25519`, which we don't define.
- `config.h.vs` — Windows-only `config.h`. Keep our `S_ISSOCK` macro and
  `PLEDGE_EXTRA_INET` additions. Add any new upstream feature macros.
  10.4p1 added `USE_MLDSA` (matches upstream's default; registers the
  `mldsa44-ed25519` key type and pulls in `ssh-mldsa-eddsa.c`). Upstream
  gates the three PQ algs — `USE_SNTRUP761X25519`, `USE_MLKEM768X25519`,
  `USE_MLDSA` — together; keep them enabled together. With `USE_MLDSA` off,
  `unittest-sshkey` fails at `KEY_MLDSA44_ED25519` (`sshkey_new` returns
  NULL for the unregistered type).
  10.5p1 made ECC mandatory and deleted `OPENSSL_HAS_ECC` and
  `OPENSSL_HAS_NISTP256/384/521` from `config.h.in`; nothing in the tree
  reads them any more, so our `config.h.vs` defines are now dead — as is the
  `-NoOpenSSL` branch in `OpenSSHBuildHelper.psm1` that strips two of them.
  Harmless, but don't add them back. Upstream's new `USE_BRAINPOOLP256R1` is
  equally vestigial: they removed `mlkem768brainpoolp256r1-sha256` before
  release, so nothing references it. Don't define it.
- `paths.targets`, `OpenSSHBuildHelper.psm1` — MSBuild plumbing; rarely
  conflicts but check when upstream changes its build. We keep
  `WindowsSDKVersion` out of the tracked file (generated per-machine into
  gitignored `paths.local.targets`), but `ZLibName` must stay: win32 #849
  replaced the hardcoded `zlib.lib` in eight vcxproj files with
  `$(ZLibName)`, so dropping the definition silently expands it to empty and
  zlib stops linking.
- `version.rc` — Windows PE resource block. Bump `FILEVERSION`,
  `PRODUCTVERSION`, `FileVersion`, and `ProductVersion` strings to match
  the upstream release. Easy to miss because `version.h` already carries
  the correct `SSH_WINDOWS_VERSION` / `SSH_PORTABLE` macros, so the SSH
  protocol banner is right even when `version.rc` is stale — only
  Explorer's file-properties / WER reports / `!analyze` expose the lie.
  Missed on the 10.3p1 merge; installed binaries showed "10.0p2" in
  WER dumps despite being built from 10.3p1 source.

### `contrib/win32/win32compat/inc/` — header shims

Upstream sometimes starts `#include`ing BSD headers that do not exist on
Windows. We ship stubs for them here. When upstream adds a new include,
add a matching shim.

Existing shims touched by 10.3p1: `sys/queue.h`, `sys/tree.h`, `sys/stat.h`,
`endian.h`, `glob.h`, `ifaddrs.h`, `netgroup.h`, `nlist.h`, `paths.h`,
`util.h`, `crtheaders.h`.

Since the 2026-10 base merge Microsoft ships its own `sys/queue.h`,
`sys/tree.h`, `sys/mount.h` and `glob.h` (the last in `win32compat/`, not
`inc/`); ours were replaced by theirs. Microsoft also wraps the includes of
missing headers inside upstream files (`#ifdef HAVE_PATHS_H`, `HAVE_UTIL_H`,
`HAVE_IFADDRS_H`, `HAVE_ENDIAN_H`, `HAVE_NETGROUP_H`, `HAVE_NLIST`, two dozen
files). We took those, so an upstream edit next to one of these includes now
conflicts.

### `contrib/win32/win32compat/` — source shims

- `w32-doexec.c` — process spawning. Watch for signature changes to
  upstream helpers it mirrors.
- `ssh-agent/keyagent-request.c` — agent request handling. PKCS11 dispatch
  changed in 10.3p1 (keyblob-based, replacing `RSA_METHOD`/`EC_KEY_METHOD`);
  future agent/PKCS11 changes upstream will show up here.
- `pwd.c` — Windows has no `sshd` privsep user; `getpwnam("sshd")` always
  fails. The `lookup_sid` debug message is at level 3 so it only shows
  under `-ddd`. Preserve this.
- `ssh-agent/agent.h` — **the include order is load-bearing in both
  directions; read the comment there before reordering anything.**
  `config.h` must come *before* `Windows.h` (it defines `WIN32_LEAN_AND_MEAN`,
  without which Windows.h pulls winsock 1 and collides with the WinSock2.h
  that config.h's own `signal.h` shim brings in) and *before* every OpenSSH
  header (10.5's `sshbuf.h` defines `BIGNUM`/`EC_KEY`/`EC_GROUP`/`EC_POINT`/
  `EVP_PKEY` to `void` when `WITH_OPENSSL` is unset and, unlike `sshkey.h`,
  never undefines them — so a later `<openssl/*.h>` gets
  `typedef struct evp_pkey_st void;`). Because `WIN32_LEAN_AND_MEAN` also
  drops `wincrypt.h`, `DATA_BLOB`/`CryptProtectData` need an explicit
  `#include <wincrypt.h>`, and `agent-main.c` needs `<stdlib.h>` for
  `_set_invalid_parameter_handler`. Autoconf builds never hit any of this: `includes.h`
  puts `config.h` first in every translation unit. We also added the missing
  `#undef` block to `sshbuf.h` as a belt-and-braces fix — worth pushing
  upstream, and worth re-checking whether upstream fixed it themselves.

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
- **`ssh_packet_set_interactive`**: no Win32 call sites remain. Upstream
  dropped the calls from the exec paths in `session.c`, and `w32-doexec.c`
  mirrors that file, so the 2026-10 base merge removed them there too.
- **`sshkey.c`: the `#ifdef WINDOWS` copy of the certificate principal
  match.** It duplicates upstream's loop to compare case-insensitively, so it
  never conflicts when upstream edits the original. 10.3p1 swapped the
  `match_pattern` arguments and our copy kept the old order until the 2026-10
  base merge. Diff the two branches of that `#ifdef` after every merge.
- **`readconf.c`: `ssh_valid_ruser` keeps `\` legal on Windows.** 10.6p1
  refuses `$` and `\` in a username given on the command line. `DOMAIN\user`
  is the native account syntax, so our `#ifdef WINDOWS` branch refuses `$` but
  allows `\` except in last position (the rule upstream had until 10.5p1).
  A deliberate deviation; mirror any character upstream adds to the list.
- **`defines.h`: Windows is exempt from `SKIP_PRIVDROP`.** `config.h.vs`
  defines `DISABLE_FD_PASSING`, which since 10.6p1 would make sshd force
  `GatewayPorts no` and `AllowStreamLocalForwarding no` and refuse `-R` below
  port 1024. Upstream means platforms whose session process keeps root; ours
  spawns the post-auth child as the logged-in user (`privsep_postauth` under
  `FORK_NOT_SUPPORTED`), so `!defined(WINDOWS)` sits next to the Cygwin
  exemption. Keep it when upstream touches that condition. It also hides an
  upstream defect: the `SKIP_PRIVDROP` block in `channels.c`
  (`check_rfwd_permission`) names `allowed_open`, which is not in scope, so
  10.6p1 does not compile wherever the macro is set. Not reported upstream
  yet.
- **`sshkey.c` — `sshkey_prekey_alloc` `#ifdef WINDOWS` gate.** Upstream
  nests the Windows branch inside `#if HAVE_MMAP`, which is never defined
  on Windows, so alloc falls through to `calloc()`. Meanwhile
  `sshkey_prekey_free`'s `#ifdef WINDOWS` branch is at the top level and
  calls `VirtualFree()` on that calloc pointer — fails silently, memory
  never released. Result: a 16 KB `SSHKEY_SHIELD_PREKEY_LEN` buffer is
  leaked on every host-key re-shield, which on Windows happens on every
  accept via `pack_hostkeys`. Heap-pressure crashes were observed within
  ~38 seconds under moderate connection load (SSH.NET smoke tests). Fix
  moves the Windows branch to the outer `#ifdef` so both allocate and
  free use `VirtualAlloc`/`VirtualFree` consistently. Watch this function
  on every merge — if upstream reorganises the preprocessor gates, our
  fix needs to follow. Microsoft's base now fixes the same leak with
  `calloc`/`free`; we keep `VirtualAlloc`/`VirtualLock`, so this function
  conflicts on every base merge.
- **`servconf.c` — `#ifdef WINDOWS` around `sshd_session_path` /
  `sshd_auth_path`.** Windows uses `derelativise_path()` where upstream
  uses `xstrdup()`; keep the guard. 10.4p1 also dropped the
  `refuse_connection == -1` default in `fill_default_server_options` —
  upstream now zero-inits via `memset` in `initialize_server_options` and
  removed the `-1` sentinel, so take the removal (our leftover default was
  dead code).
- **`servconf.h` — take upstream `--theirs` on wholesale reorganisation.**
  10.4p1 made `ServerOptions` macro-based (`SSHCONF_STRARRAY(...)`); our
  only edit was an obsolete `uint`→`u_int` typo, so upstream's version was
  correct outright. Hand-merging a struct reorg risks duplicate or missing
  members.
- **`readconf.c` — `= NULL` init on `def_*` in `fill_default_options`.**
  Our defensive init (not in pristine upstream); keep it. 10.4p1 changed
  `ret = 0` to `ret = -1` for a new `goto fail` cleanup path — take
  upstream's `ret = -1`.

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
- **Restoring the README is not the same as updating it.** The 10.4p1 merge
  kept our README and left every version string saying 10.3p1, so the GitHub
  landing page advertised the wrong release for a full cycle. Grep it for the
  old version and fix the title, the release link, the feature list, the
  `-p:ProductVersion=` example, the `V_<X>_<Y>_P<N>` tag reference, and the
  vcpkg dependency versions. The repo **About** blurb is GitHub metadata, not
  a file — it goes stale invisibly and needs `gh repo edit --description`.

## Post-merge verification checklist

1. **Build all 14 main binaries** from `contrib/win32/openssh` solution.
   Build-clean is the bar; warnings are acceptable.
2. **Build and run unit tests.** `unittest-misc` and `unittest-win32compat`
   often break first because upstream's test infrastructure churns (new
   `test_*.c` files, new stubs needed). Add missing sources to the
   `.vcxproj`; add stubs for `benchmarks()` etc. to win32compat tests.
   Running them: `unittest-sshkey` / `unittest-hostkeys` need `-d testdata`;
   `unittest-win32compat`'s symlink tests (from ~test #39) need elevation or
   Developer Mode — unelevated they fail on `symlink() = -1`, which is
   environmental, not a regression. 10.4p1 added `regress/unittests/crypto`
   and `regress/unittests/servconf` upstream; neither is wired into the
   Win32 solution.
3. **Checks that need neither elevation nor credentials** — worth running
   first, because they catch most of what a merge breaks:
   `ssh -V` and the PE metadata (`(Get-Item ssh.exe).VersionInfo`) must both
   show the new version; `ssh -Q kex` / `-Q key` must list the PQ algorithms
   (proof the new `kex*.c` actually linked); `ssh-keygen` generate +
   `-Y sign`/`-Y verify` round-trip per key type, including a deliberately
   tampered payload that must be rejected; `ssh-keyscan -p 22 127.0.0.1`
   against any live sshd for a full KEX handshake; and `sshd -t -f <config>`.
   Note `ssh-keygen -Y verify` reads the signed data from **stdin** — without
   a redirect it hangs, looking like a broken binary.
4. **Smoke test interactive login** — password auth especially, since
   `FORK_NOT_SUPPORTED` regressions show up here. Install the MSI and test
   the real service: a hand-started sshd fails password auth at
   `CreateProcessAsUserW ... 1314` no matter how elevated, because
   `get_user_token()` only takes the privileged LSA path as SYSTEM.
5. **Smoke test `scp` and `sftp`** — file transfer path.
6. **Build the MSI** — `contrib/win32/install/`. WiX v6 (migrated in
   5b8de34db); no manual WiX download needed. Pass the version explicitly
   (`-p:ProductVersion=<X>.<Y>.<N>`, matching `version.rc`'s `p<N>` field);
   the wixproj defaults to `1.0.0`. Use `-t:Rebuild` — a changed property
   alone does not rebuild the MSI.
7. **Tag** `v<X>.<Y>p<N>-win32` once verification passes.
8. **Move the repo default branch** to the new `merge-<X>.<Y>p<N>`:
   `gh repo edit weaverant/openssh-portable --default-branch merge-<X>.<Y>p<N>`.
   Each release lives on its own branch, so a default left behind serves an
   old README as the landing page however correct the new one is — found in
   Aug 2026 with the default still on `merge-10.4p1`, whose README said
   10.3p1. Editing README text never catches this; only the setting does.

Scripting these from Git Bash: drive the binaries from **bash, not
PowerShell**. PowerShell's native empty-argument passing swallows
`-N ""`, so `ssh-keygen` sits waiting for a passphrase that never comes and
the whole run looks like a hang. Also give `sshd -t` an absolute Windows
path — a POSIX-style relative `-f` path gets mangled and reports
"No such file or directory".

## Reshipping the same upstream release

A rebuild of an already-published version — dependency bump, Windows-only fix —
ships as `v<X>.<Y>p<N>-win32.<rev>`, titled "... (rev <rev>)". **The MSI version
does not change**: it stays `<X>.<Y>.<N>`, because the next slot belongs to
upstream's own `p<N+1>`. That only upgrades in place because `product.wxs` sets
`AllowSameVersionUpgrades="yes"` — without it `MajorUpgrade` ignores a
same-version MSI and the old install stays. A fourth field cannot substitute;
MSI compares only the first three. Don't read `v10.3p1-win32.1` as precedent for
the version number — that rev happened to bump 10.0.0.0 to 10.3.1.0 because it
was fixing stale metadata.

## Bumping the vcpkg dependencies

`contrib/win32/openssh/vcpkg_overlay_ports/` holds our LibreSSL and libfido2
ports; `vcpkg.json` pins the versions and a `builtin-baseline` freezes the
registry, so upstream vcpkg moves never reach this build on their own. A
LibreSSL bump touches four things: both `vcpkg.json` files, and the `SHA512`
and the `PATCHES` list in `portfile.cmake`. `libcrypto.dll`'s PE version needs
no edit: the portfile generates `crypto/version.rc` from `version.rc.in` and the
port version. Dry-run the patches against the new tarball first, the way vcpkg
applies them: in portfile order with
`git apply --ignore-whitespace --whitespace=nowarn`. A plain
`git apply --check` reports failures that are not real, because the patch files
check out with CRLF and the tarball is LF. Upstream absorbs patches over time,
and one that now fails as "already exists" is done, not broken (4.3.2 retired
`aarch64-windows.diff` this way); one that fails on context needs the context
refreshed (4.3.3, `modify-cmakelists.patch`). `README.md` carries the version
list too. Delete `vcpkg_installed/` before rebuilding, or the old artifacts are
silently reused and the build proves nothing. Afterwards
`(Get-Item bin\x64\Release\libcrypto.dll).VersionInfo` must show the new
version.

## Version floor

The Win32 base (`win32/latestw_all`) is at 10.3p1, merged here 2026-10-06.
Every upstream release after that merges through us, not through PowerShell:
expect the Win32 compat layer to need more adjustment the further upstream
moves ahead of `latestw_all`.

## Windows runtime quirks

### Signal numbers in log output are not POSIX

The Win32 compat layer renumbers signals in
`contrib/win32/win32compat/inc/signal.h` — for example `SIGTERM=8`,
`SIGFPE=15` (inverted from POSIX). Upstream log messages such as

    Received signal 8; terminating.

come from `sshd.c:977` (upstream OpenBSD, introduced 2021-06-04) and
print the raw integer with `%d`. On this Win32 build "signal 8" means
SIGTERM, not SIGFPE. Typical triggers: `CTRL_LOGOFF_EVENT` at user
logoff, `CTRL_SHUTDOWN_EVENT` at reboot, or the SCM stopping the
service (see `native_sig_handler` in
`contrib/win32/win32compat/signal.c`). This is a clean shutdown — the
SCM restarts the service a few seconds later. Not a crash.

Don't patch the upstream `logit` call to "fix" this — it costs merge
friction for a cosmetic change. Document here instead.

### No Unix-domain server sockets

`w32_bind`, `w32_listen` and `w32_accept` in
`contrib/win32/win32compat/w32fd.c` return `ENOTSUP` for `AF_UNIX`. Agent
forwarding *into* our sshd therefore cannot work (the client sees "Agent
forwarding disabled: couldn't create listener socket"), nor can
`ControlMaster`. Found on the 10.6p1 install test, but as old as the port.
Testing `ssh -A localhost ssh-add -l` on one machine proves nothing: with no
forwarded socket, `ssh-add` reaches the local agent service directly. Look at
`echo %SSH_AUTH_SOCK%` in the session instead.

### `mkdir_path()` takes a drive letter for a relative name

New in 10.6p1 (`misc.c`). Its fallback for platforms without `openat`, the one
Windows compiles, treats every path not starting with `/` as relative, so
`D:/x/y` becomes `<cwd>/D:/x/y` and fails with "Invalid argument". Reachable
through `lmkdir -p` in `sftp`; relative paths and the remote `mkdir -p` work.
Not fixed: it would mean patching an upstream helper for one niche command.

### What the session process runs as

`tasklist /v /fi "imagename eq sshd-session.exe"`, run elevated or inside an
SSH session, lists two processes per login: the monitor as
`NT AUTHORITY\SYSTEM` and its child as the logged-in user (checked on 10.6.1,
2026-10-06). The `SKIP_PRIVDROP` exemption in `defines.h` rests on this; if a
future merge changes it, the exemption has to go.
