# OpenSSH for Windows 10.3p1

A Windows build of [OpenSSH 10.3p1](https://www.openssh.com/txt/release-10.3) based on the [PowerShell/openssh-portable](https://github.com/PowerShell/openssh-portable) Win32 fork.

The official Win32-OpenSSH release is currently at 10.0p2. This project merges upstream OpenSSH 10.3p1 into the Win32 fork to bring post-quantum cryptography support and other improvements to Windows ahead of the official release.

## What's included

- **OpenSSH 10.3p1** features merged into the Win32 fork
- **Post-quantum key exchange**: ML-KEM (mlkem768x25519-sha256) offered by default
- **PQC negotiation warnings** when connecting to servers that don't support post-quantum key exchange
- **DSA and XMSS support removed** (aligned with upstream)
- **Key defaults aligned with upstream**: ed25519 default key type, 256-bit ECDSA
- **PKCS#11 rewrite**: keyblob-based dispatch replacing legacy RSA_METHOD/EC_KEY_METHOD

All 14 binaries build and link: `ssh`, `sshd`, `sshd-auth`, `sshd-session`, `scp`, `sftp`, `sftp-server`, `ssh-agent`, `ssh-add`, `ssh-keygen`, `ssh-keyscan`, `ssh-shellhost`, `ssh-sk-helper`, `ssh-pkcs11-helper`.

## Building

### Prerequisites

- **Visual Studio 2022** with the C++ desktop development workload
- **MSVC v143 Spectre-mitigated libs** (install via VS Installer, Individual Components)
- **Windows SDK 10.0.22621.0** or later
- **[vcpkg](https://github.com/microsoft/vcpkg)** -- clone, run `bootstrap-vcpkg.bat`, then `vcpkg integrate install`

vcpkg handles all library dependencies automatically: LibreSSL 4.2.0, zlib 1.3.1, libfido2 1.16.0, libcbor 0.13.0.

### Build

```powershell
cd <repo-root>
Import-Module .\contrib\win32\openssh\OpenSSHBuildHelper.psm1 -Force
Start-OpenSSHBuild -Configuration Release -NativeHostArch x64
```

Binaries are written to `bin\x64\Release\`.

### Package

```powershell
Start-OpenSSHPackage -Configuration Release -NativeHostArch x64
```

Produces `bin\x64\Release\OpenSSH-Win64.zip` and a symbols archive.

### MSI (optional)

Requires [WiX Toolset v3.14](https://github.com/wixtoolset/wix3/releases) (the binaries zip, not an installer).

```powershell
msbuild contrib\win32\install\openssh.wixproj `
  /p:Platform=x64 /p:Configuration=Release `
  /p:ProductVersion=10.3.0.0 `
  /p:WixToolPath="<path-to-wix314>\" `
  /p:SuppressIces="ICE18"
```

## Origin

This is a merge of:

- **Base**: [PowerShell/openssh-portable](https://github.com/PowerShell/openssh-portable) `latestw_all` branch (OpenSSH 10.0p2 Win32 fork)
- **Upstream**: [openssh/openssh-portable](https://github.com/openssh/openssh-portable) tag `V_10_3_P1`

Win32-specific changes (Windows service integration, process spawning via `posix_spawn`, Windows authentication, path handling) are preserved. Upstream changes to shared code (SOCKS parsing, certificate validation, PKCS#11 dispatch, key exchange) are taken from 10.3p1.

### Windows compat layer additions

The merge required new compat headers in `contrib/win32/win32compat/inc/` to satisfy upstream includes that assume Unix system headers: `sys/queue.h`, `sys/tree.h`, `endian.h`, `glob.h`, `ifaddrs.h`, `netgroup.h`, `nlist.h`, `paths.h`, `util.h`. These are either redirectors to existing `openbsd-compat` implementations or empty stubs for functionality guarded by `#ifdef`.

## Known issues

- Unit tests `unittest-misc` and `unittest-win32compat` have linker errors from upstream test infrastructure changes. All main binaries are unaffected.
- The PKCS#11 client (`ssh-pkcs11-client.c`) uses the new upstream keyblob-based dispatch. The Windows agent's PKCS#11 key management in `keyagent-request.c` provides backward-compatible local key tracking.

## License

OpenSSH is released under a [BSD license](LICENCE).

## Security

Security issues in OpenSSH should be reported to [openssh@openssh.com](mailto:openssh@openssh.com). See [OpenSSH Security](https://www.openssh.com/security.html).
