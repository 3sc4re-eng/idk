# Current verification notice — 2 November 2026

Use [CHANGELOG.md](CHANGELOG.md) for the current implementation state, limitations and next steps. The dated reports below are historical; their successful checks and executable hashes do not verify subsequent source changes.

At this update, the project has 114 bundled source blocks. `dist\loader.exe` and its passed receipt still belong to build `20261102T093519Z-5f851ea792` with 108 blocks. The newer 118-file integration is staged outside the project and has not been delivered. No live reader-performance fix is verified. Only Deadlock has a live backend; authentication is local, and Heroes automation and automatic melee parry have since been removed from the active menu.

# Historical: DMA test checkpoint, 1 November 2026

The test package is compiled and available as ACE-DMA-test.zip. Extract the complete package on the mini PC and follow ACE-DMA-TEST-README.txt.

Build: passed (compiler exit code 0). Eight regression suites passed. All 70 source blocks match the extracted sources. ZIP integrity was checked.

Loader SHA-256: `9869a330217c6c49199b8e19323aa88d02b6ab737b8bb53a3f7d7cb03a6f9c81`.

Implemented in this checkpoint:

- Live weapon-definition and modifier-cache reader, with item-adjusted bullet speed, gravity and validity checks. Shared intercept math handles distinct speeds, moving targets, inherited shooter velocity, delay, beams and unreachable trajectories; aim, trigger and magnet consume it.
- Unsupported charged/curved variants and missing data do not receive a guessed lead. Dedicated support for all entity-based ability projectiles is incomplete. Height-offset aim regions remain approximations rather than verified skeletal coordinates.
- Added input delay, separate command spacing, explicit connection/move/click tests, automatic-input switch and legacy config migration. No diagnostic input was sent to hardware here.
- Shared real game-model builder, draggable labels with right-click appearance options, configurable label fonts/outlines, keybind HUD and independent minion controls.
- Ultimate-ready display and cooldown icons using all 176 installed signature-ability images. Enemy cooldown replication still needs in-game confirmation.
- Targeted-item hero/soul filters and selected-movement-ready conditions, including Slowing Hex. Unit-target item actions require the chosen hero under the reticle and a quick-cast game binding; they do not steer toward a future world position.
- Autosave preference, manual save, recache key/action and setting help text.

Tests cover packet/API contracts, configuration/migration, motion bounds at 60/144/240 Hz, actual D3D11 model rendering and click interaction, map collision, snapshot/ability conditions, prediction math and synthetic native speed-cache changes. These are software checks. DMA attachment, live signatures, enemy cooldown replication and physical mouse delivery have not been verified on the mini PC.

The supplied profile uses Ctrl for the menu, End to pause and Mouse 1 for aim. Automatic input starts off. If weapon prediction reports unavailable data, disable Projectile lead temporarily for the baseline aiming/input test and enable the ballistics debug display to inspect the reason.

This is an external DMA overlay: no DLL injection or game-memory-write action is implemented. Other games remain unavailable layouts. Full execute helpers, a separate Flick mode, objective reminders, remote licensing/account management and public radar relay/hosting remain incomplete. New feature work was stopped to prioritize this hardware test.

See ACE-test-results.txt and ACE-build-results.txt for the verification logs. Changes are local; nothing was pushed to GitHub.

## Hero helper update, 1 November 2026

The project folder is now `C:\Users\kieran\Desktop\ace`. The next step adds Mo & Krill's Combo as an opt-in targeted helper. It requires a ready ability, a visible enemy under the crosshair, the configured number of living teammates close to that enemy, line of sight when live geometry is available, and a verified output binding. It does not estimate lethal damage or move the reticle.

Area and targeted action checks now use the installed game's base range plus a conservative lower bound for owned Greater Expansion and a read/confirmed Mystic Expansion imbue. The range slider is a user cap; it defaults high enough to allow those extensions. Other temporary and conditional range modifiers, exact stacking, and the live imbue vector remain unverified on hardware. They are not counted by guessing. The new source bundle has 71 blocks; the software regression suite passed after this change, but the mini-PC test is still required.

## Connection and menu diagnostic update, 1 November 2026

The user tested the prior package on the mini PC and reported that neither the DMA reader nor input device ever connected. The hardware has worked with other products, so the ACE connection path needs proof. Before a reader thread starts, the old menu showed only `not attached`; it now displays the current DMA or module-stage result. ACE requests extended error information from `VMMDLL_InitializeEx`, uses a blank argv[0] as documented by MemProcFS, and logs a specific failure message when available. COM enumeration now grows its Windows buffer, probes 1–32 directly if enumeration returns empty, and accepts both version and legacy ping responses. Hardware success remains unverified until the user tests this build.

Keybind HUD is text only and draggable while the menu is open; click routing includes its bounds. Punctuation keys have readable names. The ESP-label popup has a fixed readable width, builder drawings are clipped to the canvas, the preview label says the hero name alone, and the menu shows save status. `input.block_output` is labeled `Pause ACE output` because it does not block the physical mouse. Input calibration controls are under an advanced expander; the keyboard test key appears only if the device exposes a keyboard API. The Items page is labeled Auto-use owned items. Haze model replacement, aim-control simplification, Counterspell eligibility corrections, and the other audit items remain outstanding.
