# Roadmap

Open / pending items. Folded into the next release cycle (whenever an upstream
trigger forces one, see `MERGE_NOTES.md`).

## MSI installer (`contrib/win32/install/`)

- **Disable Restart Manager probe** to remove the ~1-2 min pre-UAC delay on
  hosts with AV/EDR hooks.
  Add `<Property Id="MSIRESTARTMANAGERCONTROL" Value="Disable" />` to
  `product.wxs`. Services are still stopped via the existing `ServiceControl`
  entries; only the RM-based "files in use" probe goes away.
- **Interactive install-path picker.** Currently the MSI runs UI-less; the
  default is `C:\Program Files\OpenSSH\` (matches upstream Win32-OpenSSH).
  Adding a folder picker lets users dodge the hvc.exe quirk interactively.
  `WixToolset.UI.wixext` is already referenced in `openssh.wixproj`. Default
  install path stays at `C:\Program Files\OpenSSH\` for upstream parity; the
  picker is the escape hatch.

  **Approach (decided):** trim `WixUI_InstallDir` down to `Welcome →
  InstallDir → Install → Finish` via a few `<Publish>` overrides. No
  `License.rtf` to maintain, no per-feature page. Smallest stock dialog set
  with a folder picker is `WixUI_InstallDir`; trimming via publish
  overrides is the path to fewer clicks.

  **Test plan additions for this change:**
  - Interactive install: pick non-default path, verify files land there.
  - Interactive install: accept default, verify `C:\Program Files\OpenSSH\`.
  - Silent install with `INSTALLFOLDER=C:\X /qn`: verify property honored.
  - Upgrade rev 1 → next rev: verify path preserved and services restart.

## Documentation

- **Promote hvc.exe known-issue note from release notes into `README.md`.**
  Currently lives only in the `v10.3p1-win32.1` release description. Should
  move to the `## Known issues` section in README so it's visible without
  navigating to a release tag.

## Retiring the fork

- **Exit test, check each cycle.** Retire once Microsoft ships two upstream
  releases in a row as non-Preview binaries, each within ~4 weeks of the
  upstream tag (`gh release list --repo PowerShell/Win32-OpenSSH`), and each
  local fix we carry is upstreamed or obsolete. Carried as of 10.6p2:
  same-version MSI upgrades, and our `VirtualAlloc`/`VirtualLock` variant of
  `sshkey_prekey_alloc` (their base now fixes the leak with `calloc`/`free`,
  so ours is hardening, not a fix). Their base has the post-auth connection
  reset fix (checked 2026-11-09). 2026-11-09: their latest is 10.4p1-Preview
  (2026-10-20).

## Upstream merges

- **`win32/latestw_all`** merged through `0172cc6ad` (2026-10-30, 10.5p1).
- **`openssh/openssh-portable`** merged through `V_10_6_P2` (2026-11-05).

## Follow-ups

- **MSI forgets its install folder on upgrade.** Without
  `INSTALLFOLDER=C:\OpenSSH\` an upgrade moves to `C:\Program Files\OpenSSH\`.
  Fold into the install-path picker work above.
- **Dead ECC macros in `config.h.vs`**: `OPENSSL_HAS_ECC` and
  `OPENSSL_HAS_NISTP256/384/521` no longer exist upstream and nothing reads
  them, which also makes the `-NoOpenSSL` branch of
  `OpenSSHBuildHelper.psm1` that strips two of them a no-op. Harmless;
  clean up when that build path is next touched.
