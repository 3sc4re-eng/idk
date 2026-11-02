#pragma once
#include <string>
#include <map>
namespace setting_help{
inline std::string describe(std::string p,const std::string& name){
    if(p.rfind("team.",0)==0)p=p.substr(5);
    static const std::map<std::string,std::string> help={
        {"input.disable_in_menu","Retired compatibility value. Kept in JSON/share data but ignored by every action gate and hidden from controls/search."},
        {"dbg.pause_raw_ui_signal","Diagnostic opt-in: pause automatic input when the current raw UI byte is readable and bit 0 or 1 is set. This does not detect game-menu visibility and may pause throughout normal play. Off by default; legacy profile choices never enable it. Mandatory snapshot, input, freshness and match checks remain independent."},
        {"input.disable_in_overlay","Pause Aim, Trigger, magnet and automatic item/defensive input while the actual ACE menu is open. Disable only if you want those actions to continue while editing ACE settings. The other pause preferences and mandatory fresh-data/input/match checks remain independent."},
        {"input.disable_when_dead","Pause generated actions while your local player is dead. Also pauses if life data is unavailable. Turning this off removes that behavior pause; fresh snapshots, valid inputs and action-specific weapon/cast requirements still apply."},
        {"input.disable_in_scoreboard","Pause generated actions while the sampled scoreboard state is active. Independent of the ACE-menu and spectator options."},
        {"input.disable_when_spectator","Pause generated actions while the sampled player state reports spectating. Independent of other pause options; unsupported action data is never fabricated."},
        {"visuals.enabled","Master switch for enemy, teammate, minion, world-object and projectile ESP. Aim, Trigger, automatic actions and radar keep their own switches. Turning this off skips display-only NPC, world and projectile requests; required action and collision data continues."},
        {"system.autosave","Save edited settings to the active profile after a short pause. Turn this off to save only with Save settings now or the profile Save button."},
        {"system.recache_key","Refresh the DMA memory caches and rebuild the entity list. Automatic actions pause until fresh game data is available."},
        {"system.menu_key","Open or close the menu with this key on the PC running ACE. Game-PC activation is a separate opt-in below."},
        {"system.game_menu_hotkey","Also listen for the same menu key in the game-PC key data. Off by default; the local host hotkey always remains enabled."},
        {"aim.deadzone_fov","Full-cone angle of the crosshair-centered manual handoff ring, using the same conversion as acquisition FOV. Assistance stops while the final aim point is inside and resumes outside."},
        {"trigger.magnet_deadzone_fov","Full-cone angle of the magnet manual handoff region centered on the crosshair. This uses the same angular conversion as Aim and acquisition FOV."},
        {"system.panic_key","Pause generated input and hide gameplay drawings. Press the key again to resume."},
        {"system.overlay_mode","Fuser off uses a transparent Windows desktop overlay. Fuser draws on an opaque black background for hardware composition."},
        {"system.fps_uncapped","Remove the overlay FPS cap and turn VSync off. Turning Uncapped FPS off does not turn VSync back on. Graphics-driver limits and rendering work can still limit the measured rate."},
        {"system.max_fps","Upper limit for overlay updates. Lower this on a slower mini PC. VSync can impose a lower monitor limit."},
        {"system.overlay_vsync","Match overlay presentation to the display refresh cycle. Uncapped FPS turns this off and disables this control. With Uncapped FPS off, both VSync and Maximum overlay FPS can limit the rate."},
        {"system.custom_resolution","Use the width and height below instead of this monitor's resolution. Rendering and aiming use the same dimensions."},
        {"system.width","Width in pixels of the overlay and its aiming coordinate space. Applies when custom resolution is enabled."},
        {"system.height","Height in pixels of the overlay and its aiming coordinate space. Applies when custom resolution is enabled."},
        {"system.map","Choose the collision map for wall checks. Automatic uses the loaded world model name. Unknown or missing geometry blocks visibility-dependent actions."},
        {"system.scoreboard_key","The scoreboard binding used on the game PC. Generated gameplay input pauses while it is held."},
        {"input.mode","Select the serial input-device protocol. Output disabled prevents device output. This does not provide a game-memory-write backend."},
        {"input.baud","Automatic tries 4,000,000 and 115,200 baud. Select a fixed speed if automatic probing cannot identify a serial device. Reconnect after changing it."},
        {"reader.max_hz","Upper limit for completed reader updates. Actual rate depends on DMA work. This does not start a reader before the game attaches."},
        {"reader.discovery_ms","How often to discover newly created entities. Higher values reduce discovery work but delay new entities appearing. Player positions still update in each snapshot."},
        {"reader.world_ms","Refresh interval for cached world objects. Higher values reduce world reads but delay changes to those objects."},
                {"input.com","Use a specific COM port, or 0 to scan available non-Bluetooth serial ports. Reconnect after changing the port."},
        {"input.autoconnect","Try to connect the configured input device when the application starts."},
        {"aim.prediction","Uses live weapon speed, item speed modifiers, gravity, target velocity and input delay to estimate the projectile intercept. Waits when verified shot data or a clear trajectory is unavailable."},
        {"trigger.prediction","Fires when the reticle is on the predicted projectile intercept rather than the target's current position. Requires live ballistics."},
        {"trigger.magnet_prediction","Uses the same live projectile model as Aim, with the magnet's own movement settings."},
        {"input.latency_ms","Adds this delay before each automatic mouse movement, click or ability action, even when the device has been idle. Button releases keep their normal hold duration. Higher values reduce responsiveness."},
        {"input.delay_ms","Minimum interval in milliseconds between generated input commands. Higher values add latency and reduce command rate."},
        {"input.auto_sensitivity","Read sensitivity and zoom adjustment from the current local pawn. Unknown values stop aiming rather than assuming a conversion."},
        {"input.sensitivity","Sensitivity used when automatic reading is disabled. Match the game's sensitivity setting."},
        {"input.yaw","Horizontal degrees moved by one mouse count at sensitivity 1. Use the game's value when calibrating mouse movement."},
        {"input.pitch","Vertical degrees moved by one mouse count at sensitivity 1. Use the game's value when calibrating mouse movement."},
        {"input.test_key","Choose an unused keyboard key for an explicit delivery test. A mouse-only connection may not support it."},
        {"hud.keybinds","Draw bindings, hold/toggle modes and current activation states on screen. Green means the binding is engaged; the row shows whether its activation is engaged."},
        {"aim.enabled","Enable the main aiming profile. It still needs its activation setting, a viable target, fresh game data and a connected input device before generating movement."},
        {"aim.key","Activation binding for the main aiming profile. Mouse buttons are supported."},
        {"aim.mode","Hold works only while the key is down. Toggle changes state on each press. Always keeps this profile armed while enabled."},
        {"aim.fov","Acquisition angle in degrees. A new target must be inside this cone. Hard Lock can retain the same visible target outside it."},
        {"aim.draw_fov","Draw the main acquisition circle on the real overlay. Its radius uses the current projection scale."},
        {"trigger.magnet_smooth","1% is fast; 100% is very slow. This applies only to the trigger magnet, independently of Aim smoothing."},
        {"aim.smooth","1% gives fast correction. 100% gives very slow, gentle correction. Your own mouse movement remains active."},
        {"aim.curve","Change how correction speed develops during acquisition. Bell curve uses a smooth asymmetric speed envelope. All styles retain speed and acceleration limits."},
        {"aim.strength","Scale assistance from 0 to 100 percent. Low values produce a gentle pull; 0 sends no aim movement."},
        {"aim.hard_lock","Keep the acquired target while activation remains held, including outside the acquisition FOV. Lost visibility, invalid data, death or release stop tracking."},
        {"aim.start_delay_ms","Time after pressing the aim key before the assist begins. The separate reaction range applies again when a new target is acquired."},
        {"aim.retarget_ms","Wait this long before accepting a different target. A replacement must remain the current candidate during this delay."},
        {"aim.priority","Choose between crosshair proximity, distance and low health when several viable targets are inside the acquisition cone."},
        {"aim.skip_fast","Do not assist targets whose measured speed exceeds the limit below."},
        {"aim.max_speed","Maximum allowed target velocity in Source world units per second. Applies when avoiding fast targets is enabled."},
        {"aim.max_dist","Maximum target distance in metres. Targets beyond this are excluded from acquisition and tracking."},
        {"aim.visible_ms","Require continuous on-screen visibility for this long before acquiring the target."},
        {"target.team_check","Exclude teammates from automatic aiming and firing."},
        {"trigger.enabled","Enable automatic firing while its key is armed and the crosshair is over an eligible region."},
        {"trigger.key","Hold or toggle this binding to arm firing. It can share a key with Flick, whose movement is then used before firing."},
        {"trigger.activation","Hold requires the key to stay down. Toggle arms or disarms on each press."},
        {"trigger.max_dist","Maximum automatic-firing distance in metres. Zero uses the Aim range, preserving older profiles. A positive value sets a separate range for Trigger; other eligibility checks still apply."},
        {"trigger.magnet_max_dist","Maximum Magnet targeting distance in metres. Zero uses the Aim range; a positive value is independent of both Aim and Trigger firing range."},
        {"trigger.hitchance_min","Minimum targeting-confidence estimate required before a shot. This is a geometric/movement estimate, not a guaranteed hit probability."},
        {"trigger.skip_fast","Do not automatically fire at targets above the configured velocity limit."},
        {"trigger.max_speed","Maximum target speed in world units per second for automatic firing."},
        {"trigger.magnet","Use this tab's separate aiming controls while the trigger binding is armed. Without magnet, the trigger sends no mouse movement."},
        {"auto.active_reload","When the owned Active Reload item is ready, press the configured reload binding once in the native success window."},
        {"auto.reload_key","The game's reload binding. With mouse-only input it must be a matching spare mouse button."},
        {"esp.ultimate","Show ULT READY above the player's label when the read ultimate is trained and available."},
        {"esp.abilities","Draw the four real signature-ability icons above each player, with availability and optional cooldown seconds."},
        {"esp.ability_size","Size in overlay pixels for each ability icon above a player."},
        {"esp.ability_seconds","Show remaining cooldown seconds over ability icons. Unknown or untrained states are marked separately."},
        {"ui.menu_size","Change the menu window size. The display limits its maximum size; Menu font size is separate. At 50%, the menu fits a 600 by 420 pixel area when the display has room."},
        {"ui.splash","Show the AceSoftware startup logo while the launcher or game menu opens."},
        {"radar.listen","This PC only accepts loopback connections. Local network allows other devices on the same network to connect using the PIN."},
        {"radar.port","TCP port for the radar HTTP server. Stop the radar before changing it."},
        {"projectiles.marker","Draw a diamond at each already available projectile position. Independent of player boxes, health, tracers and head circles; gameplay ESP, World & projectile ESP and Projectiles must also be enabled."},
        {"projectiles.name","Show the generic Projectile label for available projectile points. This does not infer an ability, owner or spell name."},
        {"projectiles.distance","Show distance in metres from the local position in the same captured frame. Works with or without the marker or name."},
        {"projectiles.marker_size","Width and height of the projectile diamond in overlay pixels. This changes presentation only, not collision bounds or danger radius."},
        {"projectiles.font_size","Text size in overlay pixels for projectile name and distance. Independent of player and world text sizes."},
        {"projectiles.max_dist","Maximum projectile display distance in metres, independent of player and world ranges. This cannot create unread or unavailable projectile records."},
        {"minions.enabled","Draw the separately configured minion overlay. Player ESP settings do not control these drawings."}
    };
    auto found=help.find(p);if(found!=help.end())return found->second;
    if(p=="world.enabled")return "Master for world-object and projectile ESP, subordinate to Show gameplay ESP. Each category and projectile switch still applies.";
    if(p=="cls.world")return "Show the enabled verified world-object categories. Player, minion and projectile drawings keep their own switches.";
    if(p=="esp.color_world")return "Shared world-object color. Categories with Custom style enabled use their own marker/text color.";
    if(p.rfind("world.style.",0)==0){
        if(p.ends_with(".enabled"))return "Draw this category only when gameplay ESP, World & projectile ESP and World objects are enabled. Other categories are unaffected.";
        if(p.ends_with(".custom"))return "Use separate name, icon, distance, color, size and range settings for this category. Off follows shared world color, player-name font and the shared distance settings. The first edit copies current shared values; later toggles retain the custom style.";
        if(p.ends_with(".label"))return "Show this category's verified name. This does not invent unverified model or loot subtypes.";
        if(p.ends_with(".icon"))return "Draw this category's existing class-derived shape icon, independently of its name and distance text.";
        if(p.ends_with(".distance"))return "Show distance in metres for this category. It can be shown with or without the name or icon.";
        if(p.ends_with(".color"))return "Color and opacity for this category's live icon and text only.";
        if(p.ends_with(".icon_size"))return "Scale this category's shape icon in overlay pixels. This is independent of world collision bounds or pickup radius.";
        if(p.ends_with(".text_size"))return "Text size in overlay pixels for this category's live name/distance label.";
        if(p.ends_with(".draw_distance"))return "Maximum distance in metres for this category only. The value cannot make unread or unavailable objects appear.";
    }
    if(p.rfind("trigger.magnet_",0)==0){auto suffix=p.substr(15);if(suffix=="fov")suffix="fov";return "For trigger magnet only. "+describe("aim."+suffix,name);}
    if(p.rfind("flick.",0)==0)return "For the separate Flick profile. "+describe("aim."+p.substr(6),name);
    if(p.find("act_delay_")!=std::string::npos)return "Random reaction delay after acquiring a new target. Minimum and maximum are milliseconds; the chosen delay stays fixed for that acquisition.";
    if(p.find("first_shot_delay_")!=std::string::npos)return "Random delay in milliseconds after arming the trigger before the first shot. The value is chosen between the configured minimum and maximum.";
    if(p.find("followup_delay_")!=std::string::npos)return "Random interval in milliseconds between generated shots, chosen between the configured minimum and maximum.";
    if(p.find("bone_")!=std::string::npos)return "Choose the relative preference of this aiming region. Higher-priority regions are favoured, and nearer viable regions are more likely. Ignore excludes it.";
    if(p.find("watch.")!=std::string::npos)return "Allow the selected action to respond to "+name+" only when its live condition is detected. Readiness, range, targeting and visibility requirements still apply.";
    if(p.rfind("auto.",0)==0){
        if(p.ends_with("enabled"))return "Use this action automatically only when owned, ready, correctly bound and all configured conditions are satisfied.";
        if(p.ends_with("key"))return "Output binding for this action. It must match the in-game binding; mouse-only input requires a mouse button.";
        if(p.ends_with("health"))return "Use only when your current health percentage is at or below this value.";
        if(p.ends_with("friend_range"))return "Distance in metres within which an alive teammate is considered nearby. The current rule also requires a clear path.";
        if(p.ends_with("friends"))return "Minimum nearby alive teammates required. Zero removes this teammate-count requirement.";
        if(p.ends_with("enemies"))return "Minimum viable, visible enemies required within the configured range and the action's supported radius.";
        if(p.ends_with("range"))return "Maximum enemy distance in metres. This cannot extend the actual radius of an area ability.";
    }
    if(p.rfind("defense.",0)==0)return "Configure "+name+" for the selected defensive action. The action still requires its binding, readiness and a detected incoming threat.";
    if(p.find("fade_start")!=std::string::npos)return "Distance in metres where opacity starts decreasing. Drawings reach zero opacity at the fade-end distance.";
    if(p.find("fade_end")!=std::string::npos)return "Distance in metres where fading drawings become invisible. Keep it above the fade-start distance.";
    if(p.find("_visible")!=std::string::npos)return "Appearance for players confirmed visible by the current visibility checks.";
    if(p.find("_hidden")!=std::string::npos)return "Appearance for players that are not confirmed visible. Unknown visibility is not treated as a clear shot.";
    if(p.find("_width")!=std::string::npos)return "Line thickness in overlay pixels for this visual element.";
    if(p.find("_position")!=std::string::npos)return "Choose the side of the player box for this label. Labels can also be dragged in the ESP builder.";
    if(p.rfind("esp.",0)==0)return "Control "+name+" for the currently selected player category. Appearance changes update the shared preview and live overlay.";
    if(p.rfind("minions.",0)==0)return "Control "+name+" for minions only. These settings are independent of player visuals.";
    if(p.rfind("ui.",0)==0)return "Change "+name+" in the menu. Changes apply immediately. Automatic saving follows Settings outside Preview; use Save explicitly in Preview.";
    if(p.rfind("dbg.",0)==0)return "Draw or record "+name+" from actual reader/action state. Missing live data is shown as unavailable.";
    return "Adjust "+name+" for this profile. Changes apply immediately. Automatic saving follows Settings outside Preview; use Save explicitly in Preview.";
}
}
