# OpenSSH for Windows 10.5p1

A Windows build of [OpenSSH 10.5p1](https://www.openssh.com/txt/release-10.5) based on the [PowerShell/openssh-portable](https://github.com/PowerShell/openssh-portable) Win32 fork.

The official Win32-OpenSSH release is currently at 10.0p2. This project merges upstream OpenSSH 10.5p1 into the Win32 fork to bring post-quantum cryptography support and other improvements to Windows ahead of the official release.

## What's included

- **OpenSSH 10.5p1** features merged into the Win32 fork
- **Post-quantum key exchange**: ML-KEM (mlkem768x25519-sha256) offered by default, plus mlkem768nistp256-sha256
- **Post-quantum signatures**: `mldsa44-ed25519` host and user keys built in (upstream pulled it from the default algorithm list for the 10.5 release; it returns in a later one)
- **PQC negotiation warnings** when connecting to servers that don't support post-quantum key exchange
- **DSA and XMSS support removed** (aligned with upstream)
- **Key defaults aligned with upstream**: ed25519 default key type, 256-bit ECDSA
- **PKCS#11 rewrite**: keyblob-based dispatch replacing legacy RSA_METHOD/EC_KEY_METHOD

All 14 binaries and all unit tests build cleanly: `ssh`, `sshd`, `sshd-auth`, `sshd-session`, `scp`, `sftp`, `sftp-server`, `ssh-agent`, `ssh-add`, `ssh-keygen`, `ssh-keyscan`, `ssh-shellhost`, `ssh-sk-helper`, `ssh-pkcs11-helper`.

## Building

### Prerequisites

- **Visual Studio 2022** with the C++ desktop development workload
- **MSVC v143 Spectre-mitigated libs** (install via VS Installer, Individual Components)
- **Windows SDK 10.0.22621.0** or later
- **[vcpkg](https://github.com/microsoft/vcpkg)** -- clone anywhere, run `bootstrap-vcpkg.bat`, then `vcpkg integrate install`
- **[.NET SDK](https://dotnet.microsoft.com/download) 6.0 or later** (only needed for the MSI installer)

vcpkg handles all library dependencies automatically: LibreSSL 4.2.0, zlib 1.3.2, libfido2 1.16.0, libcbor 0.14.0.

### Build

```powershell
cd <repo-root>
Import-Module .\contrib\win32\openssh\OpenSSHBuildHelper.psm1 -Force
Start-OpenSSHBuild -Configuration Release -NativeHostArch x64
```

Binaries are written to `bin\x64\Release\`.

### Package (ZIP)

```powershell
Start-OpenSSHPackage -Configuration Release -NativeHostArch x64
```

Produces `bin\x64\Release\OpenSSH-Win64.zip` and a symbols archive.

### Package (MSI)

WiX v6 and its extensions are pulled in automatically via NuGet -- no separate WiX install needed.

```powershell
dotnet build contrib\win32\install\openssh.wixproj -t:Rebuild `
  -p:Platform=x64 -p:Configuration=Release `
  -p:ProductVersion=10.5.1
```

Produces `contrib\win32\install\bin\x64\Release\openssh.msi`.

## Origin

This is a merge of:

- **Base**: [PowerShell/openssh-portable](https://github.com/PowerShell/openssh-portable) `latestw_all` branch (OpenSSH 10.0p2 Win32 fork)
- **Upstream**: [openssh/openssh-portable](https://github.com/openssh/openssh-portable) tag `V_10_5_P1`

Win32-specific changes (Windows service integration, process spawning via `posix_spawn`, Windows authentication, path handling) are preserved. Upstream changes to shared code (SOCKS parsing, certificate validation, PKCS#11 dispatch, key exchange) are taken from 10.5p1.

### Windows compat layer additions

The merge required new compat headers in `contrib/win32/win32compat/inc/` to satisfy upstream includes that assume Unix system headers: `sys/queue.h`, `sys/tree.h`, `endian.h`, `glob.h`, `ifaddrs.h`, `netgroup.h`, `nlist.h`, `paths.h`, `util.h`. These are either redirectors to existing `openbsd-compat` implementations or empty stubs for functionality guarded by `#ifdef`.

## Known issues

- The PKCS#11 client (`ssh-pkcs11-client.c`) uses the new upstream keyblob-based dispatch. The Windows agent's PKCS#11 key management in `keyagent-request.c` provides backward-compatible local key tracking.

## Disclaimer

This is an unofficial build and is not affiliated with or endorsed by the OpenSSH project or Microsoft. Use at your own risk. For production environments, evaluate thoroughly before deployment. Security issues in the underlying OpenSSH code should be reported to the upstream project.

## License

OpenSSH is released under a [BSD license](LICENCE).

## Security

Security issues in OpenSSH should be reported to [openssh@openssh.com](mailto:openssh@openssh.com). See [OpenSSH Security](https://www.openssh.com/security.html).
