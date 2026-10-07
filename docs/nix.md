# Nix packaging

The flake builds the current checkout using Qt 6 and
[gxde-nix/gxde-dtk6](https://github.com/gxde-nix/gxde-dtk6). Both dependency sources
are pinned in `flake.lock`; DTK and the terminal share one nixpkgs/Qt version.

```sh
nix build path:.
./result/bin/gxde-terminal
# Or build and launch directly:
nix run path:.
```

Enable `nix-command` and `flakes` in your Nix configuration, or pass
`--extra-experimental-features 'nix-command flakes'` to Nix commands.
`path:.` also includes new, untracked packaging files when testing local changes.
After committing them, the usual `nix build .` works too.

The flake exposes `packages.<system>.gxde-terminal` (also `default`), a default
app, a development shell and `overlays.default`. Linux x86_64 and aarch64 outputs
are provided. `nix flake check path:.` builds the package for the current system
and checks installed executable, desktop entry, attribution, keyboard layouts
and translations. It does not run the application's complete GUI unit suite.

For a NixOS configuration, add this flake as an input and use
`inputs.gxde-terminal.packages.${pkgs.stdenv.hostPlatform.system}.default` in
`environment.systemPackages`. Alternatively, add its `overlays.default` to
`nixpkgs.overlays` and install `pkgs.gxde-terminal`.

## Development

```sh
nix develop path:.
cmake -S . -B build-nix-dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DTERM_RPATH=OFF
cmake --build build-nix-dev
```

`nix/package.nix` is also usable with `pkgs.callPackage`, passing a `dtk6widget`
derivation built against the same `pkgs.qt6`. The flake handles that dependency
wiring automatically. Update pinned dependencies deliberately with
`nix flake update`; rebuilding DTK may be required.

The package retains store RPATHs, wraps Qt plugin/data paths, and provides
`expect`, `zssh` and `ssh` on the application's fallback PATH for remote sessions.
It does not install or configure a desktop session or change the user's shell.
On a non-NixOS host, graphics drivers may require the host's usual Nix GUI setup.
