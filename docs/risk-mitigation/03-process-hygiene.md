# Process Hygiene - Digital Fingerprint Minimization

**Pillar 3 of Low-Profile Design**

---

## Table of Contents

1. [Why Your Binary Is a Detection Vector](#why-your-binary-is-a-detection-vector)
2. [Code Signing Strategies](#code-signing-strategies)
3. [String Sanitization](#string-sanitization)
4. [Manifest Hygiene](#manifest-hygiene)
5. [Import Table Minimization](#import-table-minimization)
6. [Build Reproducibility](#build-reproducibility)
7. [Symbol Stripping](#symbol-stripping)

---

## Why Your Binary Is a Detection Vector

### How Anti-Cheats Scan Executables

**Modern anti-cheat systems analyze running processes:**

```cpp
// Pseudo-code for anti-cheat binary analysis
void scanProcess(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    // 1. Check digital signature
    if (!IsCodeSigned(hProcess)) {
        FlagAsS uspicious("Unsigned binary");
    }

    // 2. Scan for suspicious strings
    if (ContainsString(hProcess, "aimbot") || ContainsString(hProcess, "cheat")) {
        Ban("Suspicious strings detected");
    }

    // 3. Check loaded modules
    EnumProcessModules(hProcess, &modules, sizeof(modules), &needed);
    for (auto& mod : modules) {
        if (IsKnownCheatDLL(mod)) {
            Ban("Known cheat module");
        }
    }

    // 4. Check manifest for elevation
    if (RequiresAdministrator(hProcess) && !IsWhitelisted(hProcess)) {
        FlagAsSuspicious("Elevated privileges");
    }
}
```

**Detection Vectors:**
1. **Unsigned binaries** - Legitimate software is code-signed
2. **Suspicious strings** - "Aimbot", "Cheat", "Inject", "Hook"
3. **Import table** - Suspicious APIs (WriteProcessMemory, CreateRemoteThread)
4. **Manifest** - Unnecessary elevation (requireAdministrator)
5. **PDB paths** - Debug symbols embedded in binary
6. **File metadata** - Version info, company name

---

## Code Signing Strategies

### Why Code Signing Matters

**Legitimate software is signed:**
- Microsoft code signs all Windows components
- Games/anti-cheats are code-signed (publishers like EA, Riot, Valve)
- Unsigned binaries are flagged as "potentially harmful" by Windows Defender

**Anti-cheat heuristic:**
```
If (process is unsigned AND accesses game memory):
    Probability(is_cheat) = 0.95
```

### Option 1: Self-Signed Certificate

**Create a self-signed cert:**
```powershell
# Generate self-signed certificate
$cert = New-SelfSignedCertificate `
    -Type CodeSigning `
    -Subject "CN=YourCompany, O=YourCompany, C=US" `
    -KeyExportPolicy Exportable `
    -CertStoreLocation Cert:\CurrentUser\My `
    -NotAfter (Get-Date).AddYears(5)

# Export to PFX
$password = ConvertTo-SecureString -String "YourPassword" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath "CodeSigningCert.pfx" -Password $password
```

**Sign your executable:**
```powershell
# Sign with signtool (from Windows SDK)
signtool sign /f CodeSigningCert.pfx /p YourPassword /t http://timestamp.digicert.com macroman_aimbot.exe
```

**Limitations:**
- Not trusted by default (user must install cert)
- Still shows "Unknown Publisher" warning
- Better than unsigned, but not ideal

### Option 2: Commercial Code Signing Certificate

**Purchase from trusted CA:**
- **Sectigo (Comodo):** $70-200/year
- **DigiCert:** $200-400/year
- **GlobalSign:** $150-300/year

**Benefits:**
- Trusted by Windows by default
- Shows your company name as publisher
- No SmartScreen warnings

**Drawbacks:**
- Requires real identity/company verification (KYC)
- Expensive for personal projects
- Not anonymous

### Option 3: No Signing (MVP Approach)

**For educational/testing:**
- Accept that binary will be flagged as "unsigned"
- Disable Windows Defender during testing
- **Do not use in production**

**Add disclaimer in documentation:**
```
WARNING: This software is unsigned. Windows will show a SmartScreen warning.
For educational use only. Add exception in Windows Defender if needed.
```

---

## String Sanitization

### The Problem with Hardcoded Strings

**Your binary's `.data` section contains all string literals:**

```cpp
// Bad: Strings visible in binary
LOG_INFO("Aimbot initialized");
auto config = loadConfig("aimbot_config.ini");
SetWindowText(hwnd, "Aimbot Control Panel");
```

**Anti-cheat can scan:**
```bash
strings macroman_aimbot.exe | grep -i "aimbot"
# Output:
# Aimbot initialized
# aimbot_config.ini
# Aimbot Control Panel
```

**Result:** Instant detection.

### Solution: Compile-Time String Encryption

**Implementation (from design doc):**
```cpp
template<size_t N>
struct EncryptedString {
    char data[N];
    constexpr EncryptedString(const char (&str)[N]) : data{} {
        for (size_t i = 0; i < N; ++i) {
            data[i] = str[i] ^ 0xAB;  // XOR key
        }
    }

    std::string decrypt() const {
        std::string result(N - 1, '\0');
        for (size_t i = 0; i < N - 1; ++i) {
            result[i] = data[i] ^ 0xAB;
        }
        return result;
    }
};

#define ENCRYPT_STRING(str) (EncryptedString(str).decrypt())
```

**Usage:**
```cpp
// Good: Encrypted at compile-time, decrypted at runtime
LOG_INFO(ENCRYPT_STRING("Aimbot initialized"));
auto config = loadConfig(ENCRYPT_STRING("aimbot_config.ini"));
SetWindowText(hwnd, ENCRYPT_STRING("Aimbot Control Panel").c_str());
```

**Verification:**
```bash
strings macroman_aimbot.exe | grep -i "aimbot"
# Output: (empty)
```

**Advanced: Use skCrypter**

```cpp
#include "skCrypter.h"

LOG_INFO(skCrypt("Aimbot initialized").decrypt());
```

[skCrypter GitHub](https://github.com/skadro-official/skCrypter)

### Strings to Encrypt

**Critical strings:**
- "Aimbot", "Cheat", "Hack", "Bot"
- "Overlay", "Inject", "Hook"
- "ESP", "Wallhack", "Trigger"
- Config file names
- Window titles
- IPC channel names (`MacromanAimbot_Config`)
- Log messages containing sensitive keywords

**Benign strings (safe to leave):**
- Generic error messages ("Failed to open file")
- Library names ("OpenCV", "ImGui")
- Non-descriptive function names

---

## Manifest Hygiene

### The Problem with Elevation

**Manifest requesting admin:**
```xml
<requestedExecutionLevel level="requireAdministrator" uiAccess="false" />
```

**Anti-cheat heuristic:**
```
If (unsigned binary AND requires admin):
    Probability(is_malware) = 0.9
```

**Why cheats historically needed admin:**
- WriteProcessMemory requires PROCESS_VM_WRITE handle
- Injecting DLLs requires elevation
- Reading game memory (PROCESS_VM_READ)

**Why external vision-based aimbots DON'T need admin:**
- Screen capture (WinRT/Desktop Duplication) runs in user-mode
- Mouse input (Arduino) is hardware-level
- No memory access to game process

### Correct Manifest for External Aimbot

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity
      version="1.0.0.0"
      processorArchitecture="amd64"
      name="MacromanAimbot"
      type="win32"
  />
  <description>Educational Computer Vision Application</description>

  <!-- ✅ GOOD: No elevation required -->
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false" />
      </requestedPrivileges>
    </security>
  </trustInfo>

  <!-- DPI awareness (optional) -->
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
    </windowsSettings>
  </application>

  <!-- Compatibility (optional) -->
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>  <!-- Windows 10 -->
      <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>  <!-- Windows 7 -->
    </application>
  </compatibility>
</assembly>
```

**Embed in CMake:**
```cmake
if(MSVC)
    target_sources(macroman_aimbot PRIVATE ${CMAKE_SOURCE_DIR}/app.manifest)
endif()
```

---

## Import Table Minimization

### Suspicious API Imports

**Anti-cheats flag processes that import:**
- `kernel32.dll::WriteProcessMemory` (memory injection)
- `kernel32.dll::CreateRemoteThread` (DLL injection)
- `ntdll.dll::NtReadVirtualMemory` (memory reading)
- `user32.dll::SetWindowsHookEx` (keyboard/mouse hooks)

**Our external aimbot should ONLY import:**
- `user32.dll::CreateWindowEx` (for overlay)
- `kernel32.dll::CreateFileW` (for config files)
- `ws2_32.dll::socket` (if networking is needed)
- OpenCV/ImGui/ONNX Runtime APIs

### Check Your Imports

**Using Dependency Walker:**
```powershell
# Download: https://www.dependencywalker.com/
depends.exe macroman_aimbot.exe
```

**Or use PowerShell:**
```powershell
dumpbin /imports macroman_aimbot.exe
```

**Look for red flags:**
- WriteProcessMemory
- ReadProcessMemory
- CreateRemoteThread
- OpenProcess (with PROCESS_VM_WRITE)

### Minimize Imports

**Delay-load unnecessary DLLs:**
```cmake
# CMakeLists.txt
if(MSVC)
    set_target_properties(macroman_aimbot PROPERTIES LINK_FLAGS "/DELAYLOAD:opencv_world4100.dll")
endif()
```

**Effect:** DLL not loaded until first use, reducing initial import table size.

---

## Build Reproducibility

### Why Unique Builds Are Detectable

**Static hash-based detection:**
```cpp
// Anti-cheat maintains database of known cheat hashes
if (GetFileHash("macroman_aimbot.exe") == KNOWN_CHEAT_HASH) {
    Ban("Known cheat detected");
}
```

**Solution:** Every build should have a unique hash.

### Polymorphic Build IDs

**Generate random GUID per build (from design doc):**

```cmake
# CMakeLists.txt
string(TIMESTAMP BUILD_TIME "%Y%m%d_%H%M%S")
string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" BUILD_GUID)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/BuildInfo.h.in"
    "${CMAKE_BINARY_DIR}/generated/BuildInfo.h"
)

add_definitions(-DBUILD_GUID="${BUILD_GUID}")
```

**BuildInfo.h.in:**
```cpp
#pragma once
#define BUILD_GUID "@BUILD_GUID@"
#define BUILD_TIME "@BUILD_TIME@"
```

**Usage:**
```cpp
#include "BuildInfo.h"

void logBuildInfo() {
    LOG_INFO("Build ID: {}", BUILD_GUID);
    LOG_INFO("Build Time: {}", BUILD_TIME);
}
```

**Result:** Every compilation produces a unique binary hash, preventing static signature matching.

---

## Symbol Stripping

### The Problem with Debug Symbols

**Debug builds embed:**
- PDB file path (C:\Users\YourName\source\repos\macroman_aimbot\...)
- Function names
- Variable names
- Line numbers

**Example:**
```bash
strings macroman_aimbot.exe | grep -i "pdb"
# C:\Users\Macroman\source\repos\macroman_aimbot\out\build\x64-debug\macroman_aimbot.pdb
```

**This reveals:**
- Your username
- Project structure
- Debug information for reverse engineering

### Strip Symbols in Release Builds

**CMake configuration:**
```cmake
# For MSVC
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")  # Generate PDB
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

    # Strip PDB path from binary
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /PDBALTPATH:%_PDB%")
endif()
```

**For GCC/Clang:**
```cmake
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")  # Strip symbols
endif()
```

**Manual stripping:**
```bash
# Windows (using MSVC tools)
editbin /RELEASE macroman_aimbot.exe

# Linux
strip --strip-all macroman_aimbot
```

**Verification:**
```bash
strings macroman_aimbot.exe | grep -i "pdb"
# Output: (empty)
```

---

## Version Info Spoofing

### Mimicking Benign Software

**Windows executables have version info metadata:**
```
Right-click EXE → Properties → Details
```

**Default (bad):**
- File Description: macroman_aimbot
- Company Name: (empty)
- Product Name: macroman_aimbot

**Benign-looking (better):**
- File Description: System Configuration Utility
- Company Name: Microsoft Corporation
- Product Name: Windows System Tools

**Create version resource:**

**app.rc:**
```rc
1 VERSIONINFO
FILEVERSION 1,0,0,1
PRODUCTVERSION 1,0,0,1
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x40004L
FILETYPE 0x1L
FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "Educational Software Foundation"
            VALUE "FileDescription", "Computer Vision Research Tool"
            VALUE "FileVersion", "1.0.0.1"
            VALUE "InternalName", "macroman_aimbot"
            VALUE "LegalCopyright", "Copyright (C) 2025"
            VALUE "OriginalFilename", "macroman_aimbot.exe"
            VALUE "ProductName", "CV Research Suite"
            VALUE "ProductVersion", "1.0.0.1"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
```

**Add to CMake:**
```cmake
if(MSVC)
    target_sources(macroman_aimbot PRIVATE ${CMAKE_SOURCE_DIR}/app.rc)
endif()
```

**Effect:** Binary appears as "Computer Vision Research Tool" instead of "macroman_aimbot".

---

## Process Hygiene Checklist

**Before Release:**
- [ ] Code sign binary (self-signed minimum, commercial ideal)
- [ ] Encrypt all sensitive strings (ENCRYPT_STRING macro)
- [ ] Manifest: `level="asInvoker"` (no admin)
- [ ] Verify imports: NO WriteProcessMemory, CreateRemoteThread, etc.
- [ ] Polymorphic build ID (unique GUID per build)
- [ ] Strip debug symbols (no PDB paths in binary)
- [ ] Version info: Benign company/product names
- [ ] Test with `strings` command: No "aimbot", "cheat", etc.

**Validation Commands:**
```powershell
# Check if signed
Get-AuthenticodeSignature macroman_aimbot.exe

# Check strings
strings macroman_aimbot.exe | Select-String -Pattern "aimbot|cheat|hack"

# Check imports
dumpbin /imports macroman_aimbot.exe | Select-String -Pattern "WriteProcess|CreateRemote"

# Check manifest
mt.exe -inputresource:macroman_aimbot.exe -out:manifest.xml
cat manifest.xml | Select-String -Pattern "requireAdministrator"
```

---

## Conclusion

**Your binary is the first line of defense. Harden it.**

**Key Takeaways:**
- Code signing prevents "untrusted binary" flags
- String encryption hides sensitive keywords
- Manifest must NOT request elevation
- Import table should contain ZERO memory-access APIs
- Polymorphic builds prevent hash-based detection
- Symbol stripping removes debug info
- Version info should mimic benign software

**Priority Order:**
1. **String encryption** (immediate, critical)
2. **Manifest hygiene** (asInvoker, no admin)
3. **Import table validation** (no suspicious APIs)
4. **Symbol stripping** (remove PDB paths)
5. **Code signing** (long-term, post-MVP)

**Next Steps:**
1. Implement ENCRYPT_STRING macro
2. Audit all hardcoded strings
3. Verify manifest (asInvoker)
4. Check imports with dumpbin
5. Test release build with strings/depends

**Related Documents:**
- [Main Architecture](../plans/2025-12-29-modern-aimbot-architecture-design.md) - Section 10.3: Static Signature Mitigation
- [Read-Only Philosophy](./04-read-only-philosophy.md) - Why we don't need memory access

---

**Last Updated:** 2025-12-29
