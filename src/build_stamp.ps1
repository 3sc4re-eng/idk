param([string]$ProjectRoot = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$taskUtc = [DateTime]::UtcNow
$taskZone = [TimeZoneInfo]::FindSystemTimeZoneById('GMT Standard Time')
$taskLondon = [TimeZoneInfo]::ConvertTimeFromUtc($taskUtc, $taskZone)
$taskZoneName = if ($taskZone.IsDaylightSavingTime($taskLondon)) { 'BST' } else { 'GMT' }
$taskHashes = @('Ace_a.txt','Ace_b.txt','build.bat') | ForEach-Object {
    $taskSourceName = $_
    $taskSourceHasher = [Security.Cryptography.SHA256]::Create()
    $taskSourceStream = [IO.File]::OpenRead((Join-Path $taskRoot $taskSourceName))
    try { $taskSourceDigest = [BitConverter]::ToString($taskSourceHasher.ComputeHash($taskSourceStream)).Replace('-','').ToLowerInvariant() }
    finally { $taskSourceStream.Dispose(); $taskSourceHasher.Dispose() }
    $taskSourceName + ':' + $taskSourceDigest
}
$taskHasher = [Security.Cryptography.SHA256]::Create()
try { $taskDigest = -join ($taskHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes(($taskHashes -join "`n"))) | ForEach-Object { $_.ToString('x2') }) }
finally { $taskHasher.Dispose() }
$taskId = $taskUtc.ToString("yyyyMMdd'T'HHmmss'Z'") + '-' + $taskDigest.Substring(0,10)
$taskDisplay = 'Built ' + $taskLondon.ToString('d MMM yyyy, HH:mm:ss',[Globalization.CultureInfo]::InvariantCulture) + ' ' + $taskZoneName
$taskIso = $taskUtc.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'")
$taskMetadata = [ordered]@{ build_id=$taskId; built_utc=$taskIso; display=$taskDisplay; source_sha256=$taskDigest; capabilities=[ordered]@{ auto_launch_menu_deadlock=$true; cooperative_capture=$true } }
$taskHeader = "#pragma once`nnamespace build_info{`n"
foreach ($taskEntry in @(@('id',$taskId),@('utc',$taskIso),@('display',$taskDisplay),@('source_sha256',$taskDigest))) {
    $taskHeader += 'inline constexpr char ' + $taskEntry[0] + '[]=' + (ConvertTo-Json -InputObject $taskEntry[1] -Compress) + ";`n"
}
$taskHeader += "}`n"
$taskEncoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $taskRoot 'src\generated_build_info.h'), $taskHeader, $taskEncoding)
[IO.File]::WriteAllText((Join-Path $taskRoot 'src\build-info.json'), ($taskMetadata | ConvertTo-Json), $taskEncoding)
Write-Host ('      ' + $taskDisplay + ' | ' + $taskId)
