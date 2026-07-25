# Pharos

A custom Linux distribution built up from Linux From Scratch with a custom package manager. The intention is to be completely built from source.

Far from an initial version.

Currently working on a redesign of the migration style in inspiration of go migrate and redoing the style of the sql backend interface.

Some details of the project can be seen further in the `docs/` but is ultimately limited due to how far from an initial version this is.

## Scribe

The package manager. Inspiration from nix has led to the design decision for a tracking of all of the installed packages to be in one place. Implementation will be a bit simpler and will rely heavily on symlinks but keep generation tracking.
