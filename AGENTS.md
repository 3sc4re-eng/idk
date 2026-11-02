# Project update record

The user requires one persistent running record in `CHANGELOG.md`.

- Update it with every project change, including documentation and build/packaging changes.
- State what changed, its actual implementation state, verification performed, limitations and next planned work with an intended approach.
- Distinguish planned work, scratch proposals, applied bundled source, compiled artifacts and mini-PC verification. Never claim that a compile proves hardware operation or fixes the reported reader rate.
- Keep entries in reverse chronological order and retain older entries as history. Record subsequent corrections explicitly.
- Preserve the user's edits. Source is delivered in `Ace_a.txt` and `Ace_b.txt`; replace complete `:::FILE ... :::ENDFILE` blocks and keep generated `src` consistent whenever source is changed.
- Do not launch ACE, live DMA/input-device paths, or compiled test binaries on this main PC. State exactly which compilation, static inspection or runtime validation was performed. Open the menu preview only when requested by the user.
- The user authorized uploading all project source and future completed updates to `https://github.com/3sc4re-eng/idk`, identified from the user's Chrome GitHub sign-in. The older connected-app account `tdscare` is not this upload destination. Preserve existing remote files and history; do not force-push. Keep source bundles and extracted `src` synchronized. Exclude runtime accounts, licenses, credentials, local profiles, logs and compiled executables from source uploads. Record the actual upload result in the changelog; never claim an upload that did not complete.
