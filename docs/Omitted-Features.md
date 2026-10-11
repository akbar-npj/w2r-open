# Omitted Features

W2R Open is a **clean-room, offline** reimplementation. Everything that runs locally is
implemented; the parts of the original app that depend on the vendor's servers, an account, or
DRM are **deliberately not implemented**. This page explains what each of those features is, why
it is out of scope, and what — if anything — replaces it.

The details below were recovered from the original binary's strings and are documented here for
interoperability/understanding only. None of the vendor's code or data is included.

## Sign-in, accounts and licensing

The original app is a **client to a vendor account system**:

- **Sign-in** — *"Sign In"*, *"Create account"*, *"Reset password"*, *"Sign in with Google"*.
  Authentication is backed by **AWS Cognito** (SSO) with Google OAuth, cached identity/access
  tokens, and an `activate` step (`/api/user/activate`; error *"ACCOUNT_NOT_ACTIVATED"*,
  *"Account Not Activated"*).
- **Plans / licence check** — the app shows a plan badge and restricts features by plan
  (`FREE_PLAN_RESTRICTION`, *"Free Plan Access Restricted"*, *"Free plan users"*,
  `INVALID_PLAN`). On start it performs a *"Checking your license with the server"* round-trip and
  requires the account to be activated; expired sessions prompt *"Authentication expired. Please
  log in again."*.
- **Backend** — a set of AWS services behind API Gateway endpoints
  (`…execute-api.us-east-1.amazonaws.com/prod`), Cognito identity, and the vendor website
  (`w2rsolutions.in`) with pages such as `user/signin.php` and `user/app_dashboard.php`. Requests
  identify as `Way2Repair-Client/1.0`.

**Why omitted:** this is a login + paid-licence gate around the vendor's servers. It has no
meaning offline and is exactly what the clean-room port removes. **Replacement:** none — W2R Open
has no accounts and no licence checks; every feature is always available.

## CPU Finder

**CPU Finder** (`tools.cpuFinder`) is a database tool: *"Browse CPU, storage and board-part
databases"*. You search for a phone's CPU/chipset (e.g. *"Main CPU (Snapdragon 765G)"*) and it
maps parts to boards.

- The local data files are **`CPU_DB.enc`** (an **encrypted** database), plus
  `CPU_DB.json`, `pcd_all.json` (board parts) and `storage_chips.json`.
- It also **syncs with the server** (*"AuthService: Failed to sync CPU data"*), so the catalogue
  is vendor-maintained and can be updated remotely.

**Why omitted:** the authoritative data is encrypted (`CPU_DB.enc`) and partly server-side; it
cannot be shipped or reproduced from a clean-room implementation, and it is gated behind the
account system. **Replacement:** none.

## Solutions / Way2Solutions

**Solutions** is the original app's **community repair-solutions database**. For a given board it
shows *"W2R Official Solutions"* and user-submitted solutions — repair notes, traced nets, and
overlays drawn on the boardview (`SolutionViewerWidget`, `SolutionCanvas`,
`SolutionEditorDialog`, `SolutionItemDelegate`, `SolutionApiClient`).

- Solutions can be **created, edited, imported/exported** (`Import Solution`, *"Not a W2RS
  solution file"*), and **shared**: *"Could not save your solution to the community DB"*,
  *"Add Net '%1' to Solution"*, *"Copy sections from another open model's solution into this
  one"*.
- There is a visual **"Solution glow & signal particles"** animation and a *"Hardware solution
  available"* badge.

**Why omitted:** the feature is **server-backed** — its value is the shared community/official
database hosted by the vendor, reached through the same account backend. A purely local copy
would be an empty shell. **Replacement:** none. (If you keep repair notes for a board, store them
in your own documents; W2R Open does not have a solutions store.)

## Telemetry and crash reporting

The original reports data back to the vendor:

- **Crash reporting** posts to `https://w2rsolutions.in/api/crash-report.php`, via a separate
  `CrashDialog.exe` that collects a backtrace (symbolised with `DbgHelp.dll`).
- **Auto-update** checks `https://w2rsolutions.in/downloads/w2rsolutions_latest.exe`.
- The bundled AWS SDK includes telemetry plumbing (`TelemetryProvider`).

**Why omitted:** it is network telemetry/phone-home, which conflicts with the project's
"no network access" goal. **Replacement:** none — W2R Open never contacts the network.

## Anti-analysis and DRM

The original ships **anti-analysis measures** (a DRM/protection feature):

- A **process blacklist** that the app checks for at runtime, covering common reverse-engineering
  and debugging tools — e.g. `frida.exe`, `frida-server.exe`, `frida-trace.exe`, `ghidra.exe`,
  `x64dbg` / `x32dbg.exe`, `cheatengine(.exe|-x86_64.exe)`, `ollydbg` / `llydbg.exe`,
  `immunity debugger.exe`, `processhacker.exe`, `scylla(.exe|_x86.exe)`, `wireshark.exe`,
  `httpdebugger.exe`, `indbg.exe`, `windbg.preview.exe`.
- **Anti-debug checks** (`IsDebuggerPresent`, `CheckRemoteDebuggerPresent`).

**Why omitted:** these are protection measures aimed at preventing exactly the kind of
interoperability work this project does. They are dropped entirely — W2R Open has no process
blacklist, no debugger checks and no obfuscation.

## Summary

| Original feature | Depends on | In W2R Open |
|------------------|-----------|-------------|
| Sign-in / accounts | Vendor servers, AWS Cognito | Removed — no accounts |
| Plans / licence check | Vendor servers | Removed — no licence checks |
| CPU Finder | Encrypted `CPU_DB.enc` + server sync | Not implemented |
| Solutions / Way2Solutions | Vendor community database | Not implemented |
| Telemetry / crash reporting | `w2rsolutions.in` | Removed — never phones home |
| Auto-update | `w2rsolutions.in` | Removed |
| Anti-analysis / anti-debug | — | Removed |

Everything **local** — boardview rendering, schematic viewing, cross-probing, diode readings, the
device library, themes, animations, the language menu and the RFFE probe — is implemented. See the
rest of this wiki for each of those.
