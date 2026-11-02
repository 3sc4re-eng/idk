ACE — NATIVE POINTS, WORLD CONTROLS AND PAUSE SETTINGS TEST
Built 2 Nov 2026, 00:33:50 GMT
Build ID: 20261102T003350Z-f11d0af888

MINI-PC INSTALLATION
Leave Deadlock open. Close ACE only and extract this ZIP over the same permanent
mini-PC ACE folder, retaining your configs. Start loader.exe there. Do not run the
live DMA product on the main/gaming PC.

The private ACE-Auto-Test-Link.cmd is delivered separately on the Desktop. The
reviewed automatic-connection version needs no repeated ENABLE within its current
authorization window. It must run on the mini PC; it refuses the receiver PC.
An acknowledged mini-PC session is still needed before owner-queued update,
restart or capture can happen. This product ZIP contains no connection credentials.

CHANGES
- Independent ACE-menu, dead, scoreboard and spectator pause settings now share
  one policy at Aim, Trigger and final output. Defaults remain on. The retired
  legacy raw-UI pause cannot be re-enabled by an old saved profile. Raw UI is an
  explicit diagnostic signal only. Snapshot/input freshness, match and applicable
  weapon/cast requirements stay separate from these preferences.
- World categories have real live-renderer controls for enable, name, icon,
  distance, color, text/icon size and range. Custom appearance can inherit or copy
  shared style and persists through the existing per-game config/share path.
  Imported custom appearance survives initial off/on toggling.
- Native Head/Neck WORLD translations are sampled using guarded, batched host
  reads, current attachment identities and bounded host sample age. Demand follows
  enabled live consumers. Existing cached poses are not silently reused as fresh.
  Only Head/Neck are supported by this native-point test; they are joint origins,
  not established hitbox centers. Body/NPC point mapping is unfinished.
  This is the Deadlock native-point test; other game menus remain unsupported
  previews. Trigger and Magnet share Head/Neck priorities from Aim / Targeting.
  Old body-only profiles stay inactive until you select a supported point; the
  update does not silently change their saved choices.
- Trigger and Magnet have separate range controls. Zero retains the previous Aim
  range; positive values change their own live target selection and read demand.
- Prior menu/radar/framebuffer-capture, whole-health-bar color and capped dead-zone
  changes are retained. The installed Midtown radar map still needs live alignment.

LIMITS AND TEST STATUS
No live mini-PC aiming or native-point alignment has been verified by Codex.
Native engine cache age is unknown; matching fresh reads do not establish an
atomic native frame, and animated poses can be rejected when the reads differ.
The sampler adds measurable work when demanded; it does not promise 500 Hz.
Animated boxes, a wired skeleton, remaining body/NPC points, non-ultimate cooldowns,
automatic map identity, complete hero automation and full runtime initialization
are unfinished. Camera visibility does not establish weapon clearance. No new
game backend is represented as working. Gaming-PC foreground-focus gating is a
documented requirement with no verified producer; it is not implemented here.

VERIFICATION
Product build and full compile-only regression runner exited 0. Eleven regression
programs compiled, with nineteen additional source/compile-time checks. Static
assertions were evaluated by the compiler; runtime fixture functions were not run.
All 104 complete source blocks match extraction. Build identity, source/loader/asset
hashes and ZIP CRC were verified. Source is committed locally; no GitHub push.
No ACE/test executable, game scan, DMA access or hardware input ran on this PC.
