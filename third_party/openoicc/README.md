# OpenOICC selected snapshot

Generator sources, constructor helper, three contract/user documents and selected integration regressions. See NEOCC_SOURCE.json for the pinned revision and file hashes, and LICENSE for MIT terms.

The build and selected tests reuse ../neoioic; no duplicate runtime or legacy inheritance fixture is included. Tests tied to the omitted legacy fixture are removed from the imported runner. Windows directory traversal uses Win32 APIs and exclusive file creation uses O_EXCL so the tool can be linked with NeoCC's MSVCRT target. See NEOCC_SOURCE.json for local changes, tests/README.md for scope, and ../../docs/testing.md for staged compiler validation and its limits.
