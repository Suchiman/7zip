# 7zQ — Qt port of the 7-Zip file manager

A native Qt 6 build of 7-Zip's graphical file manager.

The port replaces only the presentation layer. Everything below it — the C
codecs, the archive handlers, `CPP/7zip/UI/Common` (extract / update / hash)
and the `CPP/7zip/UI/Agent` "archive as a folder" layer — is 7-Zip's own code,
compiled from these sources. Archives the port writes are produced by the same
encoder the `7zz` command-line program uses.

## Building

```sh
./build-qt.sh                 # re-runs itself in nix-shell if needed
./CPP/7zip/UI/Qt/7zQ          # or: ./CPP/7zip/UI/Qt/7zQ /path/to/open
```

With flakes:

```sh
nix build            # -> ./result/bin/7zQ
nix run              # build and start it
nix run . -- /tmp    # with a folder or archive to open
nix develop          # the toolchain, then ./build-qt.sh
nix flake check      # builds the package
```

### Nix flake
Add the following to your `flake.nix`:
```nix
inputs.sevenzip-qt.url = "github:Suchiman/7zip";
inputs.sevenzip-qt.inputs.nixpkgs.follows = "nixpkgs"; # optional, deduplicates dependencies
```
You can then access the package at: `inputs.sevenzip-qt.packages.${pkgs.stdenv.hostPlatform.system}.default`

The build has two stages:

1. `CPP/7zip/Bundles/Qt7z/makefile.gcc` compiles the platform-independent part
   of 7-Zip into `_o/lib7z.a`, with the same object list and the same flags as
   `Bundles/Alone2` (the `7zz` console program), minus `UI/Console` and plus
   `UI/Agent`.
2. `CPP/7zip/UI/Qt/7zQ.pro` builds the Qt user interface and links that
   library.

The library **must** be linked with `--whole-archive` (the `.pro` does this).
Every codec and archive format registers itself from a static constructor in
its own `*Register.o`, and nothing references those objects by symbol, so
without it the linker drops them and `CCodecs::Load()` finds no formats.

## What was ported

| Original | Qt port |
| --- | --- |
| `UI/FileManager/App.cpp`, `MyLoadMenu.cpp`, `resource.rc` | `MainWindow.{h,cpp}` |
| `UI/FileManager/Panel*.cpp` | `Panel.{h,cpp}`, `PanelModel.{h,cpp}` |
| `UI/FileManager/FSFolder.cpp` | `FsFolder.{h,cpp}` (rewritten for POSIX) |
| `UI/FileManager/RootFolder.cpp`, `FSDrives.cpp` | `RootFolder.{h,cpp}` (mount points) |
| `UI/FileManager/SysIconUtils.cpp` | `IconUtils.{h,cpp}` (XDG icon theme) |
| `UI/FileManager/ProgressDialog2.*` | `ProgressDialog.{h,cpp}` |
| `UI/FileManager/OverwriteDialog.*` | `OverwriteDialog.{h,cpp}` |
| `UI/FileManager/PasswordDialog.*` | `PasswordDialog.{h,cpp}` |
| `UI/FileManager/ComboDialog/CopyDialog/ListViewDialog/SplitDialog/AboutDialog/MessagesDialog` | `SmallDialogs.{h,cpp}` |
| `UI/FileManager/OptionsDialog.cpp` + all six property pages | `OptionsDialog.{h,cpp}` |
| `UI/FileManager/PanelItemOpen.cpp` | `CPanel::OpenItem()` / `OpenAsArc()` |
| `UI/FileManager/ExtractCallback.cpp` | `Callbacks.{h,cpp}` (`CExtractCallbackQt`) |
| `UI/FileManager/PanelSplitFile.cpp` | `SplitFile()` in `Operations.cpp` |
| `UI/GUI/CompressDialog.*` | `CompressDialog.{h,cpp}` |
| `UI/GUI/ExtractDialog.*` | `ExtractDialog.{h,cpp}` |
| `UI/GUI/BenchmarkDialog.*` | `BenchmarkDialog.{h,cpp}` |
| `UI/GUI/ExtractGUI.cpp`, `UpdateGUI.cpp` | `Operations.{h,cpp}` |
| `UI/GUI/UpdateCallbackGUI.cpp` | `Callbacks.{h,cpp}` (`CUpdateCallbackQt`) |
| `UI/Common/ZipRegistry.cpp` (`NWorkDir::CInfo`) | `WorkDirSettings.cpp` (QSettings) |

Behaviour that was kept deliberately identical rather than "modernised":

* Which columns a fresh panel shows, and how wide they are:
  `GetColumnVisible()` and `GetColumnWidth()` from `PanelItems.cpp` are ported,
  so a file-system folder hides Accessed, Attributes, Packed Size, iNode and
  Links exactly as 7-Zip does. The posix-only properties this port adds are
  folded into the same groups (`kpidPosixAttrib` with `kpidAttrib`,
  `kpidSymLink` with `kpidNtReparse`), while owner and group stay visible
  because they are what an attribute column means here. Right-clicking the
  header brings any of them back.
* Column set, captions and value formatting. `PropertyNameTable.h` is generated
  from `PropertyName.rc` (`tools/gen_prop_names.sh`), sizes use 7-Zip's
  space-grouped digits, and values go through 7-Zip's own
  `ConvertPropertyToString2()`, so attributes read `D....A` / `drwxr-xr-x`.
* Sort order, including folders-first and the three-stage name/prefix fallback
  from `PanelSort.cpp`, and the natural-number name comparison.
* The Add dialog's format/method/level/dictionary/word-size/solid tables, and
  the `-mx=`, `-m0=`, `-md=`, `-mfb=`, `-ms=`, `-mmt=`, `-mhe=` property
  mapping from `UpdateGUI.cpp`.
* Keyboard: Enter, Backspace, Space, Insert, F2 rename, F3 view, F4 edit,
  F5 copy, F6 move, F7 create folder, F9 two panels, Del, Ctrl+A, Ctrl+PgDn,
  Shift+Enter, `*`, `+`/`-`, Alt+Enter.
* The address bar accepts a path that reaches into archives, including nested
  ones: typing `/tmp/a.7z/inner.zip/dir` walks all three levels.
* Browsing into an archive goes through `IFolderManager::OpenFolderFile`, so
  the list code cannot tell a `.7z` apart from a directory.
* Nested archives: opening an item inside an archive first asks the folder for
  a sub-stream (`IInArchiveGetStream`) and opens that, exactly as
  `CPanel::OpenItemInArchive()` does, so `outer.7z/inner.zip/` or
  `x.tar.gz/x.tar/` is browsed without unpacking anything. Handlers that cannot
  produce a sub-stream (a solid 7z block has to be decoded first) fall back to
  unpacking the one item into a temporary folder, again as in 7-Zip.
* Opening a non-archive out of an archive unpacks it to a temporary folder and
  hands that copy to the desktop, instead of refusing.
* `DoItemAlwaysStart()` and its extension list are ported verbatim, so a double
  click on `.exe`, `.pdf`, `.docx`, ... runs/opens the file while "Open Inside"
  (Ctrl+PgDn) still browses it as an archive. "Open Inside *" and
  "Open Inside #" pass 7-Zip's `*` and `#` open-type strings.
* The panel context menu is built from the same command list as the File menu
  (`CPanel::CreateFileMenu()` reuses the File menu template), with the 7-Zip
  commands in front of it, and the enable/disable rules of `CFileMenu::Load()`.

## The Options dialog

All six property pages are present, in `OptionsDialog.cpp`'s order: System,
7-Zip, Folders, Editor, Settings, Language. The two that are registry work on
Windows do the local equivalent instead:

* **System** — the association list is the same list of formats, but ticking a
  box writes an XDG default-application entry to `~/.config/mimeapps.list`
  rather than a `HKCR` key.
* **7-Zip** — "Integrate to shell context menu" has nothing to integrate with
  here, so it is dropped; "Cascaded context menu", "Icons in context menu",
  "Eliminate duplication of root folder" and the "Context menu items" list all
  work and drive this program's own panel context menu, using the flag values
  of `UI/Explorer/ContextMenuFlags.h`.
* **Settings** — the RAM limit is wired to `IArchiveRequestMemoryUseCallback`,
  so an archive that wants a larger dictionary raises the same question 7-Zip's
  `CMemDialog` asks. "Use large memory pages" is dropped (Windows-only API).
* **Language** — lists `7zQ_<code>.qm` files found next to the program or under
  the user's data directory. The port's strings go through Qt's translation
  system, not 7-Zip's `Lang/*.txt` resource-id files, so those cannot be used
  as-is.

## Additions that 7-Zip does not have

7-Zip's panel has only "Up One Level" and a folder-history *list*; it has no
Back/Forward and ignores the side buttons of a mouse. This port keeps a
per-panel history so that it behaves like the other file browsers on this
desktop:

* **Back / Forward** in the View menu, on Alt+Left and Alt+Right.
* A **default window size taken from the screen** rather than a fixed pixel
  count: `DefaultMainWindowSize()` gives three quarters of the work area,
  bounded to 720x480 at the low end and 1600x1000 at the high end, so it is
  usable on a 1024x600 netbook and does not open as a small box in the corner
  of a 3440x1440 desktop. 7-Zip has a fixed default and remembers whatever the
  window was last; this port still remembers, and only computes a size on the
  first run.
* A **column chooser** on the header's right-click menu.
* The **mouse side buttons** (`Qt::BackButton` / `Qt::ForwardButton`) do the
  same, anywhere over the panel. The click is consumed, so it never selects or
  opens an item.

The history records archives as well as directories, so going back out of
`a.7z/inner.zip/` and forward into it again works.

Navigation reuses whatever is already open: `CPanel::NavigateTo()` closes or
opens only the part of the chain that actually differs from the target, the
same way `..` does. Moving around inside an archive that is already open
therefore costs nothing, instead of resolving the path from disk again — on a
33 GB NTFS image that is the difference between 0 ms and a ~3 s progress
dialog.

Leaving an archive entirely still frees its index, and stepping back into it
rebuilds it. That is deliberate: the index for a large volume is ~250 MB, and
holding it so that one Forward press is fast is not a good trade.

## Deliberately not ported

* **"Integrate 7-Zip to shell context menu"** and the plugin-DLL parts of the
  7-Zip options page — there is no shell to integrate with and no external
  format plugins in this build.
* **SFX archives** — there is no Linux SFX module in this tree.
* **Alternate data streams**, **NT security**, **hard links**, **"Compress
  shared files"** — NTFS/Win32 concepts.
* **Large icons / Small icons / List view modes** — only Details is
  implemented, which is the mode the file manager ships with.
* **Windows shell context menu** and drag & drop between applications.

## Changes to the 7-Zip sources

Small, `#ifdef`-guarded portability fixes so the shared code compiles on Linux:

* `UI/Agent/Agent.{h,cpp}`, `AgentOut.cpp`, `ArchiveFolderOut.cpp`,
  `ArchiveFolderOpen.cpp`, `UpdateCallbackAgent.cpp` — `FILE_ATTRIBUTE_*`,
  alternate-stream helpers, `CFiTime` vs `FILETIME`, `HMODULE` icon loading and
  `UINT64` spellings guarded or given POSIX equivalents.
* `UI/Agent/Agent.cpp` — `LoadGlobalCodecs()` called `FreeGlobalCodecs()` while
  already holding its own lock on the "no formats" path. A Windows
  `CRITICAL_SECTION` is recursive so this is invisible there; on POSIX it
  deadlocks. Split into a locking and a non-locking variant.
* `UI/Agent/AgentProxy.cpp` — `CProxyArc2::Load()` read an item name out of a
  `CPropVariant` that had already gone out of scope, so `AllocStringAndCopy()`
  copied from a freed BSTR. `prop` is now declared in the enclosing scope.

  This one is worth spelling out, because it is a real use-after-free rather
  than a portability detail. It affects every handler that exposes
  `IArchiveGetRawProps` (NTFS, HFS, FAT, ext, APFS, XAR, 7z, WIM), but whether
  the damage is visible depends on the allocator: when the freed block is
  handed straight back for the copy the data survives untouched, which is why
  `.7z` listings look fine. On NTFS the BSTR and the copy are different sizes,
  glibc writes its free-list bookkeeping over the first bytes, and every name
  in the listing loses its first two or three characters.

## Known limitations

* The panel does not watch the file system; press `Ctrl+R` (View → Refresh)
  after changes made by other programs, as `IFolderWasChanged` is only polled
  on demand.
* Options → Folders sets the working directory used when an archive is updated
  in place; the "use for removable drives only" flag has no meaning here.
* A file opened or edited out of an archive is unpacked into a temporary folder
  and opened read-only. 7-Zip watches that copy and offers to write changes
  back into the archive; this port does not, so edit the file on disk and add
  it again instead.
* The temporary folders live until the program exits, as they do in 7-Zip.
