# Upstream Merge Notes

How to merge a new upstream OpenSSH release into this Win32 fork. Written from
the 10.3p1, 10.4p1 and 10.5p1 merges; update this file as each new merge
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
- Occasional updates to `win32/latestw_all` (PowerShell/openssh-portable).
  Infrequent; last moved 2026-08-03. Worth a look before each merge but not
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
  Fastest way to catch these: extract `LIBSSH_OBJS` from `Makefile.in`, map
  `.o`→`.c`, and diff against the `ClCompile` entries in `libssh.vcxproj`.
  Four names always show up as missing and are absent **by design** — don't
  "fix" them: `ttymodes.c` (superseded by `win32compat/ttymodes_windows.c`),
  `umac128.c` (aliased to `umac.c` via `#define`s in `config.h.vs`),
  `ed25519-openssl.c` (gated on `OPENSSL_HAS_ED25519`, which we don't define),
  and `sftp-realpath.c` (Windows has its own `realpath` in `win32compat`).
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
  `#include <wincrypt.h>`. Autoconf builds never hit any of this: `includes.h`
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
- **`ssh_packet_set_interactive` call sites** — the signature changed in
  10.3p1; Win32 compat code was updated to match. Recheck on signature
  changes.
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
  fix needs to follow.
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
LibreSSL bump touches five things: both `vcpkg.json` files, the `SHA512` and the
`PATCHES` list in `portfile.cmake`, and the hardcoded `FILEVERSION 4,x,y,0`
inside `add-version-file.patch`. That last one becomes the shipped
`libcrypto.dll`'s PE version and nothing cross-checks it, so it lies silently.
Dry-run every patch (`git apply --check`) against the new tarball first —
upstream absorbs them over time, and one that now fails as "already exists" is
done, not broken (4.3.2 retired `aarch64-windows.diff` this way). `README.md`
carries the version list too. Delete `vcpkg_installed/` before rebuilding, or
the old artifacts are silently reused and the build proves nothing.

## Version floor

The Win32 base (`win32/latestw_all`) is at 10.0p2. Every upstream release
after that merges through us, not through PowerShell — expect the Win32
compat layer to need more adjustment the further upstream moves ahead
of `latestw_all`.

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
