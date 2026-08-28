# AGENTS.md

zathura: GTK4/girara document viewer in C23 (meson + ninja). Document format support lives in separate plugin repos (poppler, mupdf, ...), not here — this repo provides the plugin API (`zathura/plugin-api.h`, headers installed under `zathura/`).

## Build & test

- Build dir is already configured: use `ninja -C build`. If missing: `meson setup build` (needs recent girara >= 2026.07.07; distro packages often too old — CI builds girara from pwmt/girara git first).
- Local install to `~/.local/bin`: `meson configure build --prefix $HOME/.local && ninja -C build install` (or for a fresh build: `meson setup build --prefix $HOME/.local`). This installs `zathura`/`zathura-sandbox` to `~/.local/bin` and manpages/headers under `~/.local`. For a clean completion install use `-Dshell-completions=disabled` to avoid system `/usr/share/bash-completion` writes.
- All tests: `meson test -C build`. Single test: `meson test -C build <name>` where `<name>` ∈ {document, types, utils, xvfb_session, xvfb_config, xvfb_setting, weston_session, ...}.
- Only `document`, `types`, `utils` run headless. `session`/`config`/`setting` tests require `xvfb-run` or `weston`, detected at `meson setup` time — if neither was installed then, those test binaries aren't even built. Sandbox test additionally needs weston + seccomp/landlock.
- CI also compiles a second build with `-Dsynctex=disabled -Dseccomp=disabled -Dlandlock=disabled`; keep code compilable without optional features (`#ifdef WITH_SYNCTEX/WITH_SANDBOX/WITH_SECCOMP/WITH_LANDLOCK`).

## Gotchas

- **No source globbing**: every new `.c` file must be added explicitly to `sources = files(...)` in the root `meson.build`. New non-code resources go into `data/zathura.gresource.xml`.
- **clang-format is CI-enforced**: run `ninja -C build clang-format-check` before pushing (or `ninja -C build clang-format` to fix). Config: `.clang-format` (120 columns).
- New/changed config options must be documented in `doc/man/zathurarc.5.rst` (Sphinx RST; man pages only build if sphinx-build was available at setup time).
- Changing `zathura/document.h`, `page.h`, `links.h`, or function signatures in `plugin-api.h` requires bumping `plugin_api_version`/`plugin_abi_version` in root `meson.build` (rules commented there).

## Conventions

- **TDD**: when implementing features/changes or fixing bugs, always write failing tests first, then make them pass. Add cases to the existing glib test binaries (`tests/test_utils.c` etc.) — headless ones (`utils`, `types`, `document`) are cheapest; UI behavior needs the xvfb/weston binaries.
- Default branch is `develop`, not main/master.
- Warning level 3 plus `-Werror=implicit-function-declaration`, `-Werror=vla`, `-Werror=int-conversion`, `-Werror=maybe-uninitialized`; code uses GLib/GObject idioms and `girara_*` helpers throughout — prefer those over libc/GTK raw equivalents.
- Two binaries get built: `zathura` and `zathura-sandbox` (same sources + sandbox backends).
