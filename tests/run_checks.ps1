param([switch]$CompileOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$compiler = Join-Path $projectRoot 'tools\mingw64\bin\g++.exe'
$testWork = Join-Path $PSScriptRoot 'work'
New-Item -ItemType Directory -Path $testWork -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $testWork 'fixture') -Force | Out-Null
$overlaySource = [System.IO.File]::ReadAllText((Join-Path $projectRoot 'src\overlay.cpp'))
$widgetEnd = $overlaySource.IndexOf('static void threat_list')
if ($widgetEnd -lt 0) { throw 'Cannot find the current widget fixture boundary.' }
[System.IO.File]::WriteAllText((Join-Path $testWork 'fixture\overlay_widgets_fixture.h'), $overlaySource.Substring(0,$widgetEnd) + "`n}`n")
# The test translation units include each other; keep their exact checked-in copies together.
Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.cpp' | Copy-Item -Destination $testWork -Force
$common = @('-std=c++20','-O1','-static',"-I$projectRoot\src", "-I$projectRoot\deps", "-I$projectRoot\deps\imgui", "-I$projectRoot\deps\imgui\backends", "-I$projectRoot\vmm_unpack")
$imgui = @('imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp') | ForEach-Object { Join-Path $projectRoot "deps\imgui\$_" }
$modelLibraries = @('-ld3d11','-ldxgi','-ld3dcompiler_47','-lwindowscodecs','-lole32','-luuid','-lcabinet','-lws2_32','-lgdi32','-ldwmapi')
function Invoke-Check([string]$Name, [string[]]$Sources, [string[]]$Libraries) {
    Write-Host "Checking $Name..."
    $target = Join-Path $testWork ($Name + '.exe')
    & $compiler @common @Sources @Libraries '-o' $target
    if ($LASTEXITCODE -ne 0) { throw "$Name compilation failed ($LASTEXITCODE)." }
    if ($CompileOnly) { return }
    & $target
    if ($LASTEXITCODE -ne 0) { throw "$Name failed ($LASTEXITCODE)." }
}
Push-Location $testWork
try {
    Invoke-Check 'vmm_contract_check' @('vmm_contract_check.cpp') @()
    foreach ($sourceOnlyCheck in @('memory_batch_checks.cpp','aim_visibility_checks.cpp','settings_snapshot_contract_checks.cpp','engine_map_reader_checks.cpp','map_asset_registry_checks.cpp','profile_scope_checks.cpp','ability_read_demand_checks.cpp','scan_incremental_checks.cpp','scatter_sdk_checks.cpp','map_geometry_checks.cpp','health_state_checks.cpp','entity_health_checks.cpp','deadzone_checks.cpp','motion_policy_checks.cpp','visibility_cache_checks.cpp','collision_any_checks.cpp','pe_header_checks.cpp','ballistics_demand_checks.cpp','esp_render_checks.cpp','consumer_demand_checks.cpp','action_pause_checks.cpp','launch_request_checks.cpp','cooperative_capture_checks.cpp','world_style_checks.cpp','native_joint_integration_checks.cpp','reader_scatter_contract_checks.cpp','discovery_revision_checks.cpp','projectile_checks.cpp','profile_lifecycle_checks.cpp','profile_selection_checks.cpp','menu_layout_checks.cpp')) {
        Write-Host ("Syntax-checking " + $sourceOnlyCheck + '...')
        & $compiler @common '-fsyntax-only' $sourceOnlyCheck
        if ($LASTEXITCODE -ne 0) { throw ("Syntax check failed: " + $sourceOnlyCheck) }
    }
    Invoke-Check 'makcu_parser_checks' @('makcu_parser_checks.cpp') @()
    Invoke-Check 'process_discovery_checks' @('process_discovery_checks.cpp') @()
    Invoke-Check 'diagnostic_checks' @('diagnostic_checks.cpp',"$projectRoot\src\utils.cpp") @()
    $behaviorSources = @('behavior_checks.cpp') + (@('config.cpp','entities.cpp','dma.cpp','offsets.cpp','utils.cpp','game.cpp','live_backend.cpp','scan_incremental.cpp','collision.cpp') | ForEach-Object { Join-Path $projectRoot "src\$_" })
    Invoke-Check 'behavior_checks' $behaviorSources @('-lws2_32','-lpsapi','-lcabinet')
    $visualSources = @('visual_checks.cpp',"$projectRoot\src\config.cpp","$projectRoot\src\utils.cpp") + $imgui
    Invoke-Check 'visual_checks' $visualSources @('-lws2_32')
    Invoke-Check 'render_model_check' @('render_model_check.cpp',"$projectRoot\src\model_preview.cpp") $modelLibraries
    $uiSources = @('ui_model_checks.cpp',"$projectRoot\src\model_preview.cpp","$projectRoot\src\config.cpp","$projectRoot\src\utils.cpp","$projectRoot\src\game.cpp") + $imgui + @("$projectRoot\deps\imgui\backends\imgui_impl_dx11.cpp","$projectRoot\deps\imgui\backends\imgui_impl_win32.cpp")
    Invoke-Check 'ui_model_checks' $uiSources $modelLibraries
    $backendSources = @('backend_checks.cpp') + (@('live_backend.cpp','scan_incremental.cpp','autouse.cpp','defense.cpp','input.cpp','config.cpp','entities.cpp','dma.cpp','offsets.cpp','utils.cpp','game.cpp','collision.cpp') | ForEach-Object { Join-Path $projectRoot "src\$_" })
    Invoke-Check 'backend_checks' $backendSources @('-lws2_32','-lpsapi','-lcabinet')
    Invoke-Check 'prediction_checks' @('prediction_checks.cpp') @()
    Invoke-Check 'followup_checks' @('followup_checks.cpp',"$projectRoot\src\config.cpp","$projectRoot\src\utils.cpp") @('-lws2_32')
    if ($CompileOnly) { Write-Host 'All regression sources compiled. No test binaries were executed.' } else { Write-Host 'All regression checks passed. No game process, DMA card, or mouse-output device was opened.' }
} finally { Pop-Location }
