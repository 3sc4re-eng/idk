# ACE source

This repository contains the maintained ACE source bundles, their extracted C++ sources, build script, regression sources, collision/radar assets and verification notes.

`Ace_a.txt` and `Ace_b.txt` are the source bundles. Each file is enclosed in a complete `:::FILE name` / `:::ENDFILE` block. `build.bat` validates and extracts those blocks, compiles with MinGW C++20 and packages `dist`. Bundles and checked-in `src` must match. The build regenerates `src/generated_build_info.h` and `src/build-info.json`; those generated identity files are not checked in.

Read [CHANGELOG.md](CHANGELOG.md) for the actual implementation state, limitations, verification and next steps. A source upload does not establish that an executable contains that source, or that the software works with live hardware.

Only Deadlock currently has a live backend. Full skeletal rendering, animated mesh effects, remote authentication/licensing and other game backends are not delivered features. The 118-file proposal discussed in the changelog is staged outside this project and is not part of this source snapshot.

Do not run the product or compiled regression binaries on the development PC. Hardware/runtime validation belongs on the user's mini PC. Source inspection and compile-only checks must be distinguished from runtime results.

The build may obtain missing compiler, UI and DMA dependencies. Runtime DLLs, accounts, licenses, local profiles, logs and executables are excluded from this source repository. No CI workflow is configured to run hardware or input-device paths.
