# Klogg Total Commander Lister Plugin

This document explains how Klogg integrates with Total Commander via the WLX
(Lister) plugin API. It covers the exported entry points, the embedded viewer
widget that backs the plugin, packaging guidance, and usage notes for end users
and testers.

## Lister API overview

Total Commander calls into the plugin through a well-defined C-style interface.
The implementation in `klogg_lister.dll` provides the following entry points:

| Function | Purpose |
| --- | --- |
| `HWND ListLoad(HWND ParentWin, char* FileToLoad, int ShowFlags)` | Create a new viewer window for the supplied file using ANSI paths. |
| `HWND ListLoadW(HWND ParentWin, wchar_t* FileToLoad, int ShowFlags)` | Unicode variant of `ListLoad`. |
| `int ListLoadNext(HWND ParentWin, HWND PluginWin, char* FileToLoad, int ShowFlags)` | Reuse an existing viewer window to display a different file (ANSI). Returns `0` on success, `1` on failure. |
| `int ListLoadNextW(HWND ParentWin, HWND PluginWin, wchar_t* FileToLoad, int ShowFlags)` | Unicode variant of `ListLoadNext`. |
| `void ListCloseWindow(HWND ListWin)` | Destroy the hosted widget and release resources associated with the window handle. |
| `int ListSearchText(HWND ListWin, char* SearchString, int SearchParameter)` | Execute a search within the current file using ANSI search terms. Returns `0` if the request was dispatched. |
| `int ListSearchTextW(HWND ListWin, wchar_t* SearchString, int SearchParameter)` | Unicode variant of `ListSearchText`. |
| `int ListSendCommand(HWND ListWin, int Command, int Parameter)` | Handle keyboard commands (copy, select all, refresh, capability queries, etc.). |
| `int ListNotificationReceived(HWND ListWin, int Message, WPARAM wParam, LPARAM lParam)` | Currently returns `-1` (not implemented); provided for completeness. |

The `SearchParameter` and `Command` flags follow Total Commander’s documented
bit layout. The plugin currently honours:

* `lcs_matchcase`, `lcs_backwards`, `lcs_repeatsearch`, `lcs_regex`, and
  `lcs_wholewords` (treated as a literal search).
* `lc_copy`, `lc_selectall`, `lc_refresh`, and `lc_getviewercaps` commands. The
  capability query advertises text search, multiple-file reuse, and standard
  clipboard operations.

Unsupported flags or commands return `LISTPLUGIN_NOTIMPLEMENTED` so that Total
Commander can fall back to its default behaviour.

## Embedded viewer widget

`ListerViewerWidget` lives in `src/plugins/total_commander_lister`. It reuses the
existing `CrawlerWidget` stack from the desktop application while hiding the
main-window chrome:

* A `Session` instance provides shared `LogData`/`LogFilteredData` objects.
* `QuickFindMux` and `QuickFindWidget` are embedded so that Total Commander
  search commands map directly to Klogg’s incremental search pipeline.
* Viewer configuration is injected at runtime through `applyShowFlags`, keeping
  the widget free from global UI assumptions (menus, toolbars, etc.).

All state stays within the plugin DLL so that multiple lister windows can operate
in parallel without leaking global state into a running desktop instance.

## Configuration and persistence

`PersistentInfo::overrideApplicationKeys()` and `overridePortableMode()` redirect
settings to `klogg_lister.conf` and `klogg_lister_session.conf`, preventing
collisions with the standalone application profile. The plugin expects the
Microsoft VC++ Redistributable to be installed and bundles the Qt runtime in the
release ZIP (Packaging strategy “Alternative 2”).

## Building the plugin

1. Configure the build with the existing Windows presets or a custom generator,
   ensuring `-DBUILD_SHARED_LIBS=OFF` (default) and the Qt SDK are available.
2. Build the new target:
   ```powershell
   cmake --build build --target klogg_lister_plugin --config RelWithDebInfo
   ```
3. The resulting `klogg_lister.dll` is written to
   `build/output/klogg_lister.dll` alongside the main executables.

The plugin target automatically participates in the `ci_build` meta-target so it
is produced during regular Windows CI runs.

## Packaging

`packaging/windows/prepare_release.cmd` now stages the plugin under
`release/totalcmd/plugins/wlx/klogg_lister` and gathers the required Qt runtime
modules (`QtCore`, `QtGui`, `QtWidgets`, `QtConcurrent`, `QtNetwork`, `QtXml`,
`Qt5Compat`, plus the `platforms` and `styles` plugins). The script also copies
`docs/total_commander_lister.md` as `README.md` inside the plugin bundle,
writes a `pluginst.inf` manifest at the root of the bundle so Total Commander
recognises the ZIP as an auto-install package, and produces an archive named:

```
klogg-totalcmd-lister-<version>-<arch>-<qt>.zip
```

Total Commander users can install the ZIP directly (press `Enter` on the
archive inside Total Commander) or extract it manually into
`%COMMANDER_PATH%\plugins\wlx\klogg_lister`. The CI workflow uploads this
archive as a release asset so it appears on the GitHub **Releases** page in
addition to the workflow artifacts.

## Installation & usage

1. Extract the plugin ZIP to a folder (recommended path:
   `%COMMANDER_PATH%\plugins\wlx\klogg_lister`).
2. In Total Commander, open **Configuration → Options → Plugins → Lister
   plugins** and add `klogg_lister.dll` from the extracted directory.
3. Enable the plugin for relevant file masks (e.g. `*.log;*.txt`).
4. Press `F3` on a log file to launch the Klogg viewer inside Total Commander.

Keyboard shortcuts align with Total Commander conventions (`Ctrl+C`/`Ctrl+A`
handled by the host, `Ctrl+F` mapped to the quick-find bar). The overview pane,
search history, and highlighting behave identically to the standalone build.

## Testing checklist

* Automated: run `ctest` (unchanged) after building. The plugin target does not
  add new automated tests but should compile cleanly in CI.
* Manual Total Commander test plan:
  1. Install the plugin into a test copy of Total Commander.
  2. Open a large log file via `F3`; ensure the file loads and the overview pane
     renders.
  3. Trigger `n`, `Shift+n`, `Ctrl+F`, and the search bar to verify forward and
     backward searches.
  4. Use `Ctrl+R` (mapped through `lc_refresh`) to force a reload of a tailing
     log file.
  5. Close the lister window and reopen another file to confirm `ListLoadNext`
     works without orphaned processes.

Report any issues via the main Klogg issue tracker, noting that the plugin is in
technical preview during initial releases.

## Known limitations

* Advanced Total Commander commands such as custom print requests or preview
  bitmap generation are not yet implemented and return
  `LISTPLUGIN_NOTIMPLEMENTED`.
* `lcs_wholewords` currently performs a literal search without enforcing word
  boundaries (planned enhancement).
* Only the Windows build path is supported; the plugin does not load on Linux
  or macOS Total Commander alternatives.

