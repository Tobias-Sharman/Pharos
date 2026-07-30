# Scribe

Scribe is in an incomplete state that does not yield to having a proper readme since its internals are being reworked.

The direction is however roughly planned out so this will be amended once the base has been finished reworking.

## Progression

So far the old db abstraction idea that was eking towards some cursed ORM form has been removed and a script has been
made for generating queries based on the sql in inspiration of sqlc

### TODO:
- Properly linking db creation to the init command rather than using in throwaway tests
- Finalise table design, moreso fixing the normal form and interlinking of operations tables to the main tables
    - Includes stuff for user, generations, dependencies, version control and tracking, operations tracking to cleanly support failed download and installation
- Installation path, read a scroll, download and build to a tmp directory, move to intended install place and establish symlinks
- Add deletion
- Add update for scribe and all other packages -> have init bootstrap scribe in as first package
- Clean up todos in code
- Complete Pharos LFS project as a test

Once in a position of being functional enough to use for a from source operating system later support for things like software that is not open source
