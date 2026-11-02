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
$script:linkedSuites = 0
$script:executedSuites = 0
$syntaxOnlyCount = 0
function Invoke-Check([string]$Name, [string[]]$Sources, [string[]]$Libraries) {
    Write-Host "Compiling and linking $Name..."
    $target = Join-Path $testWork ($Name + '.exe')
    & $compiler @common @Sources @Libraries '-o' $target
    if ($LASTEXITCODE -ne 0) { throw "$Name compilation failed ($LASTEXITCODE)." }
    $script:linkedSuites++
    if ($CompileOnly) { return }
    Write-Host "Executing $Name..."
    & $target
    if ($LASTEXITCODE -ne 0) { throw "$Name failed ($LASTEXITCODE)." }
    $script:executedSuites++
}
Push-Location $testWork
try {
    Invoke-Check 'vmm_contract_check' @('vmm_contract_check.cpp') @()
    Write-Host 'Source-only fixtures receive compiler checks; their runtime assert(...) bodies are not executed.'
    foreach ($sourceOnlyCheck in @('memory_batch_checks.cpp','decision_frame_checks.cpp','map_retry_policy_checks.cpp','output_queue_policy_checks.cpp','aim_visibility_checks.cpp','settings_snapshot_contract_checks.cpp','engine_map_reader_checks.cpp','map_asset_registry_checks.cpp','profile_scope_checks.cpp','ability_read_demand_checks.cpp','scan_incremental_checks.cpp','scatter_sdk_checks.cpp','map_geometry_checks.cpp','health_state_checks.cpp','entity_health_checks.cpp','deadzone_checks.cpp','motion_policy_checks.cpp','visibility_cache_checks.cpp','collision_any_checks.cpp','pe_header_checks.cpp','ballistics_demand_checks.cpp','esp_render_checks.cpp','consumer_demand_checks.cpp','action_pause_checks.cpp','launch_request_checks.cpp','cooperative_capture_checks.cpp','world_style_checks.cpp','native_joint_integration_checks.cpp','reader_scatter_contract_checks.cpp','discovery_revision_checks.cpp','projectile_checks.cpp','profile_lifecycle_checks.cpp','profile_selection_checks.cpp','menu_layout_checks.cpp')) {
        Write-Host ("Syntax-checking " + $sourceOnlyCheck + ' (static_assert checks evaluated; no runtime execution)...')
        & $compiler @common '-fsyntax-only' $sourceOnlyCheck
        if ($LASTEXITCODE -ne 0) { throw ("Syntax check failed: " + $sourceOnlyCheck) }
        $syntaxOnlyCount++
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
    if ($CompileOnly) {
        Write-Host "$script:linkedSuites regression suites compiled and linked; $syntaxOnlyCount source-only fixtures passed syntax/static_assert checks. No test executable was run."
    } else {
        Write-Host "$script:executedSuites linked regression executables completed successfully; $syntaxOnlyCount source-only fixtures passed syntax/static_assert checks. The source-only fixtures' runtime assertions were not executed."
    }
} finally { Pop-Location }
