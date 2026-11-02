@echo off
setlocal
cd /d "%~dp0"
if errorlevel 1 (echo [X] Cannot enter the build directory & exit /b 1)
set "ACE_BUILD_SCRIPT=%~f0"
if "%~1"=="--ace-build-guarded" if defined ACE_BUILD_LOCK_OWNER if "%~2"=="%ACE_BUILD_LOCK_OWNER%" goto :guarded_build
call :run_build_guard Run
set "ACE_BUILD_EXIT=%ERRORLEVEL%"
pause
exit /b %ACE_BUILD_EXIT%

:guarded_build
title ACE build
echo ==================================================
echo  ACE - one-click build (v1.0-beta, DMA-only)
echo ==================================================

call :run_build_guard Begin
if errorlevel 1 goto :fail

if not exist ace_a.txt (echo [X] ace_a.txt missing & goto :fail)
if not exist ace_b.txt (echo [X] ace_b.txt missing & goto :fail)

echo [1/7] Validating and staging source files...
call :run_build_guard Extract
if errorlevel 1 goto :fail

echo [2/7] Checking locked compiler...
set "GCC=tools\mingw64\bin\g++.exe"
call :run_build_guard Dependencies
if errorlevel 1 goto :fail

set BUILD_EXE=loader.build.tmp.exe
if exist "%BUILD_EXE%" del /q "%BUILD_EXE%"
if exist "%BUILD_EXE%" goto :fail
if exist dist\loader.exe del /q dist\loader.exe
powershell -NoProfile -ExecutionPolicy Bypass -File src\build_stamp.ps1 -ProjectRoot "%CD%"
if errorlevel 1 goto :fail
call :run_build_guard Prepare
if errorlevel 1 goto :fail
echo [5/7] Compiling loader.exe (1-3 min, silence normal)...
"%GCC%" -std=c++20 -O2 -s -static -mwindows -Isrc -Ideps -Ideps\imgui -Ideps\imgui\backends src\*.cpp deps\imgui\imgui.cpp deps\imgui\imgui_draw.cpp deps\imgui\imgui_tables.cpp deps\imgui\imgui_widgets.cpp deps\imgui\backends\imgui_impl_dx11.cpp deps\imgui\backends\imgui_impl_win32.cpp -ld3d11 -ldxgi -ld3dcompiler_47 -lole32 -luuid -lws2_32 -lgdi32 -luser32 -ldwmapi -lpsapi -lwindowscodecs -lshell32 -lcabinet -lcrypt32 -lwinmm -o "%BUILD_EXE%"
if errorlevel 1 goto :fail
if not exist "%BUILD_EXE%" goto :fail
call :run_build_guard Compiled
if errorlevel 1 goto :fail
copy /y "%BUILD_EXE%" loader.exe >nul 2>&1
if errorlevel 1 echo       Development copy was not refreshed; final verification will identify the current product artifact.

echo [6/7] Packaging dist folder...
if exist dist rmdir /s /q dist
mkdir dist 2>nul
copy /y "%BUILD_EXE%" dist\loader.exe >nul
if errorlevel 1 goto :fail
del /q "%BUILD_EXE%" >nul 2>&1
for %%D in (vmm.dll leechcore.dll ftd3xx.dll) do (
    if not exist "%%D" (echo [X] Required runtime %%D missing & goto :fail)
    copy /y "%%D" "dist\%%D" >nul
    if errorlevel 1 goto :fail
    if not exist "dist\%%D" (echo [X] Required packaged runtime %%D missing & goto :fail)
)
if not exist assets\collision\dl_midtown.acecol (echo [X] Midtown collision asset missing & goto :fail)
if not exist assets\collision\hero_testing.acecol (echo [X] Hero Testing collision asset missing & goto :fail)
xcopy /e /i /y assets\collision dist\assets\collision >nul
if errorlevel 1 goto :fail
if not exist assets\radar\maps.json (echo [X] Radar map metadata missing & goto :fail)
if not exist assets\radar\dl_midtown.png (echo [X] Midtown radar image missing & goto :fail)
xcopy /e /i /y assets\radar dist\assets\radar >nul
if errorlevel 1 goto :fail
copy /y src\build-info.json dist\build-info.json >nul
if errorlevel 1 goto :fail
mkdir dist\configs 2>nul
rem New installations use registered defaults; never distribute developer profiles.

echo [7/7] Verifying...
if not exist dist\loader.exe (echo [X] packaging failed & goto :fail)
call :run_build_guard Verify
if errorlevel 1 goto :fail

echo.
echo ==================================================
echo  BUILD OK - verified product is dist\loader.exe
echo  Development-copy status and build identity are reported above.
echo ==================================================
exit /b 0

:fail
echo.
call :run_build_guard Failed
echo BUILD FAILED - this attempt did not produce a verified product.
echo Do not treat an existing loader.exe or old build-info.json as success.
echo Copy the diagnostics above and .ace-build-result.json when available.
exit /b 1

:run_build_guard
set "ACE_BUILD_GUARD_MODE=%~1"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $raw=[IO.File]::ReadAllText($env:ACE_BUILD_SCRIPT); $rx='(?ms)^:ACE_BUILD_GUARD_BEGIN\r?\n(.*?)^:ACE_BUILD_GUARD_END\r?$'; $parts=[regex]::Matches($raw,$rx); if($parts.Count -ne 1){ Write-Error 'Build guard section is missing or ambiguous'; exit 1 }; & ([scriptblock]::Create($parts[0].Groups[1].Value)) -ProjectRoot ([IO.Path]::GetDirectoryName($env:ACE_BUILD_SCRIPT)) -Mode $env:ACE_BUILD_GUARD_MODE"
exit /b %ERRORLEVEL%

:ACE_BUILD_GUARD_BEGIN
param(
    [Parameter(Mandatory=$true)][string]$ProjectRoot,
    [ValidateSet('Run','Begin','Validate','Extract','Dependencies','Runtime','Prepare','Compiled','Verify','Failed')][string]$Mode='Validate'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$utf8=New-Object Text.UTF8Encoding($false,$true)
$root=[IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\','/')
$resultPath=Join-Path $root '.ace-build-result.json'
$receiptPath=Join-Path $root '.ace-extraction.json'
$runtimeNames=@('vmm.dll','leechcore.dll','ftd3xx.dll')
$dependencyLockJson=@'
{"baseline":"Existing local dependency bytes observed 2026-11-02","compiler":{"archive_path":"tools/compiler.zip","assets_page":"https://github.com/brechtsanders/winlibs_mingw/releases/expanded_assets/14.0.0-snapshot20231217posix-11.0.1-msvcrt-r1","bytes":197015246,"executable_path":"tools/mingw64/bin/g++.exe","executable_sha256":"8a4a2f65a6b6e7ae70ab4b68ca4238188dbea382f1d24f4ae6c83ee8bee33016","installed_directory":"tools/mingw64","release_page":"https://github.com/brechtsanders/winlibs_mingw/releases/tag/14.0.0-snapshot20231217posix-11.0.1-msvcrt-r1","sha256":"462d67f17fd99a7c749056712a3afff27a0f794b256af9f69d15ac8418630d56","upstream_digest_correlated":false,"url":"https://github.com/brechtsanders/winlibs_mingw/releases/download/14.0.0-snapshot20231217posix-11.0.1-msvcrt-r1/winlibs-x86_64-posix-seh-gcc-14.0.0-snapshot20231217-mingw-w64msvcrt-11.0.1-r1.zip","version":"GCC 14.0.0-snapshot20231217 / MinGW-w64 MSVCRT 11.0.1-r1"},"provenance":{"compiler":"Versioned asset is listed on the official WinLibs GitHub release. Expected archive SHA256 was measured locally; upstream checksum text could not be retrieved by the web tool.","limitations":"Content lock and fail-closed acquisition. No new archive was downloaded or executed. This does not prove full bit-for-bit build reproducibility, upstream signatures, hardware operation, or that prior tests used these exact bytes.","runtime":"Official MemProcFS v5.19 expanded-assets metadata advertises the same SHA256 as the retained local archive.","source_files":"Versioned URLs are already used by the current build; expected individual SHA256 values were measured from current local bytes."},"runtime":{"archive_path":"vmm_pack.zip","assets_page":"https://github.com/ufrisk/MemProcFS/releases/expanded_assets/v5.19","bytes":6737710,"files":[{"bytes":2628552,"file_version":"5.19.0.251","path":"vmm.dll","sha256":"1bfa5e308f18b61d38f9471aad5d2801410cae608e3e274242e7e788838a12df"},{"bytes":180680,"file_version":"2.23.4.104","path":"leechcore.dll","sha256":"87b69a036772e58d6d9297982f6029d02ea0dbaf98c91f366e3fbb28f6045673"},{"bytes":513928,"file_version":"1.3.0.10","path":"ftd3xx.dll","sha256":"3c0fc158fd4aa604c526d58ed8274ae830d53e69e01210cc5b5fa54ec5a9b7c0"}],"release_page":"https://github.com/ufrisk/MemProcFS/releases/tag/v5.19","sha256":"5dc42673f35371e309b2c3f5c302556489897451a90905061035b630239d3ca9","upstream_digest_correlated":true,"url":"https://github.com/ufrisk/MemProcFS/releases/download/v5.19/MemProcFS_files_and_binaries_v5.19.0-win_x64-20261014.zip","version":"MemProcFS 5.19.0 / LeechCore 2.23.4.104 / FTD3XX 1.3.0.10"},"schema":1,"source_files":[{"bytes":919975,"path":"deps/json.hpp","sha256":"9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6","url":"https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp"},{"bytes":803634,"path":"deps/imgui/imgui.cpp","sha256":"594831847b5e0c95d15554c61c47a9e204d9ce87deed0b504e9c661a31bdb28c","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imgui.cpp"},{"bytes":328809,"path":"deps/imgui/imgui.h","sha256":"8470a6ba7841f0f2ddd50e137005079b27d6f61021c5372a72a628e6d8d27f08","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imgui.h"},{"bytes":262687,"path":"deps/imgui/imgui_internal.h","sha256":"8304c957f0452f01d23ea951f39e9a94ddb61cd63f26f2ce8c6e5dc44d199d90","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imgui_internal.h"},{"bytes":221080,"path":"deps/imgui/imgui_draw.cpp","sha256":"8f4867550086b3bd8444f232098b85a526c27dafb4df7ade772bc73cf8c217ec","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imgui_draw.cpp"},{"bytes":237948,"path":"deps/imgui/imgui_tables.cpp","sha256":"f2ed29a8eb6ef19883a80bf1727120ed2962c0a28e584d4cf70f34f8d061f567","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imgui_tables.cpp"},{"bytes":443296,"path":"deps/imgui/imgui_widgets.cpp","sha256":"9d8a13aae4a217966cdbfc784bec4b9b2abe9bc2290a8ddffd2c4d117b2917bb","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imgui_widgets.cpp"},{"bytes":10507,"path":"deps/imgui/imconfig.h","sha256":"a253e82c1960f8aa7594a51a8ac606a6235738c923f12d67a7742e88bf07ed63","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imconfig.h"},{"bytes":20344,"path":"deps/imgui/imstb_rectpack.h","sha256":"2efa3d5f7d003c19743b15155dad9f46d9f9fc783a18893d703b431d3c990972","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imstb_rectpack.h"},{"bytes":55207,"path":"deps/imgui/imstb_textedit.h","sha256":"6cb9de88e714f42bdbdc25ba190f01e346a2b6eba727c2044acccc7e70966c70","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imstb_textedit.h"},{"bytes":199484,"path":"deps/imgui/imstb_truetype.h","sha256":"5b7693f174b0458c7bec5522c8d6be4fe606d3f5da5082351959c14e59269b53","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/imstb_truetype.h"},{"bytes":29334,"path":"deps/imgui/backends/imgui_impl_dx11.cpp","sha256":"c568d1637e5c0b53f74ebc5ad527050e39183da53b95bb215139b138ff80be57","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/backends/imgui_impl_dx11.cpp"},{"bytes":1538,"path":"deps/imgui/backends/imgui_impl_dx11.h","sha256":"f9d3d41f7db6d9e8f380fdd05b62c6ddaec6c5813ffa879e541816f49a50c1b8","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/backends/imgui_impl_dx11.h"},{"bytes":46165,"path":"deps/imgui/backends/imgui_impl_win32.cpp","sha256":"677ed71e7d7d44cf0693ecd6a480366121885e7c1ee19ae3bdd7d6a5ea230a2f","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/backends/imgui_impl_win32.cpp"},{"bytes":3433,"path":"deps/imgui/backends/imgui_impl_win32.h","sha256":"02e1762148172d480cfdef1dab08ee7255ffade2a445379fdd005a301bf28214","url":"https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/backends/imgui_impl_win32.h"}]}
'@
$dependencyLock=$dependencyLockJson|ConvertFrom-Json

function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Hash-Bytes([byte[]]$Bytes){$h=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($h.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant()}finally{$h.Dispose()}}
function Hash-File([string]$Path){
    Regular $Path
    $before=Get-Item -LiteralPath $Path
    $length=$before.Length;$written=$before.LastWriteTimeUtc.Ticks
    $stream=[IO.File]::OpenRead($Path);$hasher=[Security.Cryptography.SHA256]::Create()
    try{$digest=([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$hasher.Dispose()}
    $after=Get-Item -LiteralPath $Path
    Require ($length -eq $after.Length -and $written -eq $after.LastWriteTimeUtc.Ticks) ('File changed while hashing: '+$Path)
    return $digest
}
function Child([string]$Name){
    Require (-not [IO.Path]::IsPathRooted($Name) -and $Name -notmatch '[:\\/]') ('Unsafe generated child name: '+$Name)
    $path=[IO.Path]::GetFullPath((Join-Path $root $Name))
    Require ([IO.Path]::GetDirectoryName($path) -ieq $root) ('Generated path escaped project root: '+$Name)
    return $path
}
function Regular([string]$Path,[bool]$Directory=$false){
    $full=[IO.Path]::GetFullPath($Path)
    Require ($full -ieq $root -or $full.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) ('Input escaped project root: '+$Path)
    Require (Test-Path -LiteralPath $Path) ('Missing path: '+$Path)
    $item=Get-Item -LiteralPath $Path -Force
    Require (-not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) ('Linked/reparse path refused: '+$Path)
    Require ($item.PSIsContainer -eq $Directory) ('Unexpected path type: '+$Path)
    $parent=[IO.Path]::GetDirectoryName($full)
    while($full -ine $root -and $parent -ine $root){
        $ancestor=Get-Item -LiteralPath $parent -Force
        Require ($ancestor.PSIsContainer -and -not ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint)) ('Linked ancestor refused: '+$parent)
        $parent=[IO.Path]::GetDirectoryName($parent)
    }
}
function Check-Root{
    Regular $root $true
    $ancestor=(Get-Item -LiteralPath $root -Force).Parent
    while($null -ne $ancestor){
        Require (-not ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint)) ('Linked project ancestor refused: '+$ancestor.FullName)
        $ancestor=$ancestor.Parent
    }
    foreach($name in @('src','dist','vmm_unpack')){
        $path=Child $name
        if(Test-Path -LiteralPath $path){Regular $path $true}
    }
    foreach($receipt in @($resultPath,$receiptPath)){if(Test-Path -LiteralPath $receipt){Regular $receipt}}
}
function Write-AtomicJson([string]$Path,$Value){
    Require ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Path)) -ieq $root) 'Receipt must stay in project root'
    if(Test-Path -LiteralPath $Path){Regular $Path}
    $temporary=$Path+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    $bytes=$utf8.GetBytes(($Value|ConvertTo-Json -Depth 10)+"`n")
    $stream=New-Object IO.FileStream($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
    if(Test-Path -LiteralPath $Path){
        $previous=$Path+'.'+[Guid]::NewGuid().ToString('N')+'.previous.tmp'
        [IO.File]::Replace($temporary,$Path,$previous)
        Regular $previous;[IO.File]::Delete($previous)
    }else{[IO.File]::Move($temporary,$Path)}
}
function Read-Json([string]$Path){Regular $Path;$length=(Get-Item -LiteralPath $Path).Length;Require ($length -gt 0 -and $length -le 2097152) 'Empty/oversized build receipt';return ([IO.File]::ReadAllText($Path,$utf8)|ConvertFrom-Json)}
function Has-BuildId([string]$Path,[string]$BuildId){
    Require ($BuildId -cmatch '^[A-Za-z0-9_-]{1,128}$') 'Invalid stamped build identity'
    Regular $Path;$stream=[IO.File]::OpenRead($Path);$buffer=New-Object byte[] 65536;$tail=''
    try{
        while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){
            $text=$tail+[Text.Encoding]::ASCII.GetString($buffer,0,$count)
            if($text.Contains($BuildId)){return $true}
            $keep=[Math]::Min($BuildId.Length-1,$text.Length);$tail=$text.Substring($text.Length-$keep)
        }
        return $false
    }finally{$stream.Dispose()}
}
function Safe-SourceName([string]$Name){
    if($Name -notmatch '^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$' -or $Name.EndsWith('.')){return $false}
    $stem=$Name.Split('.')[0]
    return $stem -notmatch '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$'
}
function Parse-Bundles{
    $files=New-Object 'Collections.Generic.List[object]'
    $seen=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $inputs=[ordered]@{}
    foreach($bundle in @('Ace_a.txt','Ace_b.txt')){
        $path=Child $bundle;Regular $path
        $info=Get-Item -LiteralPath $path
        Require ($info.Length -gt 0 -and $info.Length -le 134217728) ('Empty/oversized bundle: '+$bundle)
        $raw=[IO.File]::ReadAllBytes($path);$inputs[$bundle]=Hash-Bytes $raw
        Require ($raw.Length -gt 0 -and $raw.Length -le 134217728) ('Bundle changed size or exceeds limit: '+$bundle)
        $text=$utf8.GetString($raw)
        if($text.Length -gt 0 -and $text[0] -eq [char]0xfeff){$text=$text.Substring(1)}
        $text=$text.Replace("`r`n","`n")
        Require (-not $text.Contains("`r") -and -not $text.Contains([char]0)) ('Invalid newline/NUL in '+$bundle)
        $name=$null;$start=0;$body=New-Object Text.StringBuilder
        $lines=$text.Split([char]10)
        for($index=0;$index -lt $lines.Length;$index++){
            $line=$lines[$index];$number=$index+1
            if($line -cmatch '^:::FILE (.+)$'){
                Require ($null -eq $name) ($bundle+':'+$number+': nested FILE before closing '+$name+' opened at line '+$start)
                $candidate=$Matches[1]
                Require (Safe-SourceName $candidate) ($bundle+':'+$number+': unsafe/invalid source filename')
                Require ($candidate -ine 'generated_build_info.h' -and $candidate -ine 'build-info.json') ($bundle+':'+$number+': generated build output must not be a bundled source block')
                Require ($seen.Add($candidate)) ($bundle+':'+$number+': duplicate/case-colliding filename '+$candidate)
                $name=$candidate;$start=$number;$null=$body.Clear()
                continue
            }
            if($line -ceq ':::ENDFILE'){
                Require ($null -ne $name) ($bundle+':'+$number+': ENDFILE without FILE')
                $bytes=$utf8.GetBytes($body.ToString())
                Require ($bytes.Length -le 67108864) ($bundle+':'+$start+': source block exceeds64MiB')
                $files.Add([pscustomobject]@{name=$name;bytes=$bytes;sha256=(Hash-Bytes $bytes);bundle=$bundle;line=$start})
                Require ($files.Count -le 1024) 'Too many source blocks (limit1024)'
                $name=$null;continue
            }
            Require (-not $line.TrimStart().StartsWith(':::')) ($bundle+':'+$number+': malformed/misplaced marker; use whole-line :::FILE name / :::ENDFILE at column1')
            if($null -eq $name){Require ([string]::IsNullOrWhiteSpace($line)) ($bundle+':'+$number+': unexpected text outside a source block')}
            else{$null=$body.Append($line);$null=$body.Append("`n")}
        }
        Require ($null -eq $name) ($bundle+':'+$start+': unterminated FILE '+$name)
    }
    Require ($files.Count -gt 0) 'No complete source blocks found'
    foreach($required in @('main.cpp','build_stamp.ps1','build_info.h')){
        Require ($seen.Contains($required)) ('Missing required build entry: '+$required)
        $entry=@($files|Where-Object {$_.name -ieq $required})[0]
        Require ($entry.bytes.Length -gt 0) ('Required build entry is empty: '+$required)
    }
    $script=Child 'build.bat';Regular $script;$inputs['build.bat']=Hash-File $script
    return [pscustomobject]@{files=$files;inputs=$inputs}
}
function Verify-Inputs($Receipt){
    Require (@($Receipt.inputs.PSObject.Properties).Count -eq 3) 'Extraction input set must contain the two bundles and build.bat'
    foreach($required in @('Ace_a.txt','Ace_b.txt','build.bat')){Require ($null -ne $Receipt.inputs.PSObject.Properties[$required]) ('Missing captured build input: '+$required)}
    foreach($property in $Receipt.inputs.PSObject.Properties){
        $path=Child $property.Name;Regular $path
        Require ((Hash-File $path) -ceq $property.Value) ('Build input changed after extraction: '+$property.Name)
    }
}
function Read-Extraction{
    $receipt=Read-Json $receiptPath
    Require ($receipt.schema -eq 1 -and $receipt.root -ieq $root -and $receipt.state -eq 'extracted') 'Invalid extraction receipt'
    Verify-Inputs $receipt
    return $receipt
}
function Source-Digest($Receipt){
    $lines=@();foreach($name in @('Ace_a.txt','Ace_b.txt','build.bat')){$lines+=($name+':'+$Receipt.inputs.$name)}
    return Hash-Bytes ($utf8.GetBytes(($lines -join "`n")))
}
function Snapshot-Source($Receipt){
    $source=Child 'src';Regular $source $true
    foreach($property in $Receipt.files.PSObject.Properties){
        $path=Join-Path $source $property.Name;Regular $path
        Require ((Hash-File $path) -ceq $property.Value) ('Extracted source changed: '+$property.Name)
    }
    $items=@(Get-ChildItem -LiteralPath $source -Force)
    Require (@($items|Where-Object {$_.PSIsContainer}).Count -eq 0) 'Unexpected directory in extracted source'
    foreach($generated in @('generated_build_info.h','build-info.json')){Regular (Join-Path $source $generated)}
    Require ($items.Count -eq $Receipt.count+2) 'Unexpected/missing source file (only generated_build_info.h and build-info.json may be added)'
    foreach($item in $items){Require ($null -ne $Receipt.files.PSObject.Properties[$item.Name] -or $item.Name -ieq 'generated_build_info.h' -or $item.Name -ieq 'build-info.json') ('Unexpected source file: '+$item.Name)}
    $hashes=[ordered]@{}
    foreach($item in $items){Regular $item.FullName;$hashes[$item.Name]=Hash-File $item.FullName}
    return $hashes
}
function Verify-SourceSnapshot($Snapshot){
    $source=Child 'src';Regular $source $true
    $items=@(Get-ChildItem -LiteralPath $source -Force)
    Require ($items.Count -eq @($Snapshot.PSObject.Properties).Count) 'Source set changed while compiling'
    foreach($property in $Snapshot.PSObject.Properties){
        $path=Join-Path $source $property.Name;Regular $path
        Require ((Hash-File $path) -ceq $property.Value) ('Source changed while compiling: '+$property.Name)
    }
}

$dependencyLockSha256=Hash-Bytes ($utf8.GetBytes($dependencyLockJson))
Require ($dependencyLock.schema -eq 1 -and @($dependencyLock.source_files).Count -eq 15 -and @($dependencyLock.runtime.files).Count -eq 3) 'Invalid embedded dependency lock'

function Dependency-Path([string]$Relative){
    Require (-not [IO.Path]::IsPathRooted($Relative) -and $Relative -notmatch '[:\x00]') ('Unsafe dependency path: '+$Relative)
    $parts=$Relative.Replace('\','/').Split('/')
    foreach($part in $parts){Require ($part.Length -gt 0 -and $part -ne '.' -and $part -ne '..') ('Unsafe dependency path component: '+$Relative)}
    $path=[IO.Path]::GetFullPath((Join-Path $root ($parts -join '\')))
    Require ($path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) ('Dependency path escaped project root: '+$Relative)
    return $path
}
function Ensure-DependencyDirectory([string]$Path){
    $full=[IO.Path]::GetFullPath($Path)
    Require ($full -ieq $root -or $full.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) 'Dependency directory escaped project root'
    if(Test-Path -LiteralPath $full){Regular $full $true;return}
    Ensure-DependencyDirectory ([IO.Path]::GetDirectoryName($full))
    $null=[IO.Directory]::CreateDirectory($full);Regular $full $true
}
function Assert-LockedFile([string]$Path,$Spec){
    Regular $Path
    Require ([string]$Spec.sha256 -cmatch '^[0-9a-f]{64}$') ('Invalid locked digest: '+$Path)
    Require ((Get-Item -LiteralPath $Path).Length -eq [long]$Spec.bytes -and [long]$Spec.bytes -gt 0) ('Locked dependency length differs: '+$Path)
    $actual=Hash-File $Path
    Require ($actual -ceq [string]$Spec.sha256) ('Locked dependency SHA256 differs: '+$Path+'. Preserve/rename it and restore the approved version; the build will not overwrite a mismatched existing file.')
    return $actual
}
function Ensure-LockedDownload($Spec,[string]$Relative){
    $path=Dependency-Path $Relative
    Ensure-DependencyDirectory ([IO.Path]::GetDirectoryName($path))
    if(Test-Path -LiteralPath $path){$null=Assert-LockedFile $path $Spec;return $path}
    Require ([string]$Spec.url -cmatch '^https://') ('A locked HTTPS URL is required: '+$Relative)
    $temporary=Join-Path ([IO.Path]::GetDirectoryName($path)) ('.ace-download-'+[Guid]::NewGuid().ToString('N')+'.tmp')
    Require (-not (Test-Path -LiteralPath $temporary)) 'Fresh dependency download path already exists'
    try{
        Write-Host ('       Downloading locked dependency: '+$Relative)
        Invoke-WebRequest -Uri ([string]$Spec.url) -OutFile $temporary -UseBasicParsing -ErrorAction Stop
        $null=Assert-LockedFile $temporary $Spec
        Require (-not (Test-Path -LiteralPath $path)) ('Dependency destination appeared during download: '+$path)
        [IO.File]::Move($temporary,$path)
        $null=Assert-LockedFile $path $Spec
        return $path
    }finally{
        if(Test-Path -LiteralPath $temporary){Regular $temporary;[IO.File]::Delete($temporary)}
    }
}
function New-DependencyStage([string]$Kind){
    Require ($Kind -eq 'compiler' -or $Kind -eq 'runtime') 'Invalid dependency staging kind'
    $stage=Child ('.ace-dependency-'+$Kind+'-'+[Guid]::NewGuid().ToString('N'))
    Require (-not (Test-Path -LiteralPath $stage)) 'Fresh dependency stage already exists'
    $null=[IO.Directory]::CreateDirectory($stage);Regular $stage $true
    return $stage
}
function Remove-DependencyStage([string]$Stage){
    $full=[IO.Path]::GetFullPath($Stage)
    Require ([IO.Path]::GetDirectoryName($full) -ieq $root -and [IO.Path]::GetFileName($full) -cmatch '^\.ace-dependency-(compiler|runtime)-[0-9a-f]{32}$') 'Refusing unsafe dependency cleanup path'
    Regular $full $true
    foreach($item in @(Get-ChildItem -LiteralPath $full -Recurse -Force)){Regular $item.FullName ([bool]$item.PSIsContainer)}
    Remove-Item -LiteralPath $full -Recurse -Force -ErrorAction Stop
}
function Zip-Target([string]$Base,[string]$Name){
    Require (-not [IO.Path]::IsPathRooted($Name) -and $Name -notmatch '[:\x00\\]') ('Unsafe archive entry: '+$Name)
    $relative=$Name.TrimEnd('/')
    Require ($relative.Length -gt 0) 'Empty archive entry'
    foreach($part in $relative.Split('/')){Require ($part.Length -gt 0 -and $part -ne '.' -and $part -ne '..') ('Unsafe archive entry component: '+$Name)}
    $baseFull=[IO.Path]::GetFullPath($Base).TrimEnd('\')
    $target=[IO.Path]::GetFullPath((Join-Path $baseFull $relative.Replace('/','\')))
    Require ($target.StartsWith($baseFull+'\',[StringComparison]::OrdinalIgnoreCase)) ('Archive entry escaped staging directory: '+$Name)
    return $target
}
function Assert-ZipTargets([string]$Archive,[string]$Base,[bool]$Compiler=$false){
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=[IO.Compression.ZipFile]::OpenRead($Archive)
    try{
        $seen=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
        foreach($entry in $zip.Entries){
            $target=Zip-Target $Base $entry.FullName
            Require ($seen.Add($target)) ('Duplicate/case-colliding archive entry: '+$entry.FullName)
            if($Compiler){Require ($entry.FullName -ceq 'mingw64/' -or $entry.FullName.StartsWith('mingw64/',[StringComparison]::Ordinal)) 'Compiler archive has an unexpected top-level entry'}
        }
        Require ($zip.Entries.Count -gt 0) 'Dependency archive is empty'
    }finally{$zip.Dispose()}
}
function Hash-Stream([IO.Stream]$Stream){
    $hasher=[Security.Cryptography.SHA256]::Create()
    try{return ([BitConverter]::ToString($hasher.ComputeHash($Stream))).Replace('-','').ToLowerInvariant()}finally{$hasher.Dispose()}
}
function Verify-LockedToolchain([string]$Archive,[string]$Base){
    $null=Assert-LockedFile $Archive $dependencyLock.compiler
    Regular $Base $true
    Assert-ZipTargets $Archive $Base $true
    $toolchain=Join-Path $Base 'mingw64';Regular $toolchain $true
    $expected=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $zip=[IO.Compression.ZipFile]::OpenRead($Archive)
    try{
        foreach($entry in $zip.Entries){
            $target=Zip-Target $Base $entry.FullName
            if($entry.FullName.EndsWith('/')){Regular $target $true;continue}
            Regular $target
            Require ((Get-Item -LiteralPath $target).Length -eq $entry.Length) ('Installed toolchain length differs from locked archive: '+$entry.FullName)
            $stream=$entry.Open();try{$archiveHash=Hash-Stream $stream}finally{$stream.Dispose()}
            Require ((Hash-File $target) -ceq $archiveHash) ('Installed toolchain differs from locked archive: '+$entry.FullName)
            $null=$expected.Add($target)
        }
    }finally{$zip.Dispose()}
    $count=0
    foreach($item in @(Get-ChildItem -LiteralPath $toolchain -Recurse -Force)){
        Regular $item.FullName ([bool]$item.PSIsContainer)
        if(-not $item.PSIsContainer){$count++;Require ($expected.Contains($item.FullName)) ('Unexpected installed toolchain file: '+$item.FullName)}
    }
    Require ($count -eq $expected.Count) 'Installed toolchain inventory differs from locked archive'
    $null=Assert-LockedFile $Archive $dependencyLock.compiler
}
function Locked-RuntimeSpec([string]$Name){
    $runtimeSpecs=@($dependencyLock.runtime.files|Where-Object {$_.path -ceq $Name})
    Require ($runtimeSpecs.Count -eq 1) ('Required runtime member absent/ambiguous in dependency lock: '+$Name)
    return $runtimeSpecs[0]
}
function Install-LockedCompiler{
    $archive=Ensure-LockedDownload $dependencyLock.compiler $dependencyLock.compiler.archive_path
    $installed=Dependency-Path $dependencyLock.compiler.installed_directory
    if(-not (Test-Path -LiteralPath $installed)){
        $stage=New-DependencyStage 'compiler'
        try{
            # Check the fixed archive digest and every extraction destination before expansion.
            $null=Assert-LockedFile $archive $dependencyLock.compiler
            Assert-ZipTargets $archive $stage $true
            Expand-Archive -LiteralPath $archive -DestinationPath $stage -Force -ErrorAction Stop
            Verify-LockedToolchain $archive $stage
            Require (-not (Test-Path -LiteralPath $installed)) 'Compiler destination appeared while staging'
            [IO.Directory]::Move((Join-Path $stage 'mingw64'),$installed)
            Remove-DependencyStage $stage
        }catch{Write-Warning ('Compiler staging was not installed; retained when available at '+$stage);throw}
    }
    Regular $installed $true
    $compiler=Dependency-Path $dependencyLock.compiler.executable_path;Regular $compiler
    Require ((Hash-File $compiler) -ceq $dependencyLock.compiler.executable_sha256) 'Selected compiler executable differs from the dependency lock'
    Write-Host ('       Locked compiler selected: '+$dependencyLock.compiler.version)
}
function Install-LockedRuntime{
    $missing=$false
    foreach($dll in $runtimeNames){
        $path=Child $dll
        if(Test-Path -LiteralPath $path){$null=Assert-LockedFile $path (Locked-RuntimeSpec $dll)}else{$missing=$true}
    }
    if($missing){
        $archive=Ensure-LockedDownload $dependencyLock.runtime $dependencyLock.runtime.archive_path
        $stage=New-DependencyStage 'runtime'
        try{
            $null=Assert-LockedFile $archive $dependencyLock.runtime
            Assert-ZipTargets $archive $stage
            Expand-Archive -LiteralPath $archive -DestinationPath $stage -Force -ErrorAction Stop
            $payload=$null
            foreach($v in @(Get-ChildItem -LiteralPath $stage -Recurse -File -Filter 'vmm.dll')){
                $set=@{}
                try{foreach($dll in $runtimeNames){$path=Join-Path $v.DirectoryName $dll;$null=Assert-LockedFile $path (Locked-RuntimeSpec $dll);$set[$dll]=$path}}catch{continue}
                if($set.Count -eq $runtimeNames.Count){$payload=$set;break}
            }
            Require ($null -ne $payload) 'Locked runtime archive does not contain the complete approved DLL set in one directory'
            foreach($dll in $runtimeNames){
                $target=Child $dll
                if(-not (Test-Path -LiteralPath $target)){[IO.File]::Copy($payload[$dll],$target,$false)}
                $null=Assert-LockedFile $target (Locked-RuntimeSpec $dll)
            }
            Remove-DependencyStage $stage
        }catch{Write-Warning ('Runtime staging was not fully installed; retained when available at '+$stage);throw}
    }
    foreach($dll in $runtimeNames){$null=Assert-LockedFile (Child $dll) (Locked-RuntimeSpec $dll)}
    Write-Host '       Complete locked runtime DLL set verified.'
}
function Install-LockedDependencies{
    $ProgressPreference='SilentlyContinue'
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    Install-LockedCompiler
    Write-Host '[3/7] Checking locked UI library...'
    foreach($spec in $dependencyLock.source_files){$null=Ensure-LockedDownload $spec $spec.path}
    Write-Host '       All locked UI/JSON files verified.'
    Write-Host '[4/7] Checking locked DMA runtime...'
    Install-LockedRuntime
}
function Verify-LockedDependencies{
    $archive=Dependency-Path $dependencyLock.compiler.archive_path
    Write-Host 'Verifying complete installed toolchain against its locked archive...'
    Verify-LockedToolchain $archive ([IO.Path]::GetDirectoryName((Dependency-Path $dependencyLock.compiler.installed_directory)))
    foreach($spec in $dependencyLock.source_files){$null=Assert-LockedFile (Dependency-Path $spec.path) $spec}
    foreach($dll in $runtimeNames){$null=Assert-LockedFile (Child $dll) (Locked-RuntimeSpec $dll)}
}

function Snapshot-Runtime{
    $hashes=[ordered]@{}
    foreach($dll in $runtimeNames){
        $path=Child $dll;Regular $path
        Require ((Get-Item -LiteralPath $path).Length -gt 0) ('Required runtime DLL is empty: '+$dll)
        $hashes[$dll]=Assert-LockedFile $path (Locked-RuntimeSpec $dll)
    }
    return $hashes
}
function Verify-RuntimeSnapshot($Snapshot,[string]$Directory=$root){
    Require ($null -ne $Snapshot -and @($Snapshot.PSObject.Properties).Count -eq $runtimeNames.Count) 'Current build receipt must identify every required runtime DLL'
    Regular $Directory $true
    foreach($dll in $runtimeNames){
        Require ($null -ne $Snapshot.PSObject.Properties[$dll]) ('Runtime snapshot is missing: '+$dll)
        $expected=[string]$Snapshot.$dll
        Require ($expected -cmatch '^[0-9a-f]{64}$') ('Invalid runtime snapshot digest: '+$dll)
        $path=Join-Path $Directory $dll;Regular $path
        Require ((Get-Item -LiteralPath $path).Length -gt 0) ('Required runtime DLL is empty: '+$dll)
        Require ((Hash-File $path) -ceq $expected) ('Required runtime DLL changed or differs from prepared input: '+$path)
    }
}

try{
    Check-Root
    if($Mode -eq 'Run'){
        # The lock lives in this wrapper for the entire child batch. It releases
        # automatically when the wrapper exits; an old lock file is harmless.
        $lockPath=Child '.ace-build.lock'
        if(Test-Path -LiteralPath $lockPath){Regular $lockPath}
        try{$lock=New-Object IO.FileStream($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)}catch{throw 'Another build owns this folder, or the lock cannot be opened. No source/compiler work was started.'}
        $previousOwner=$env:ACE_BUILD_LOCK_OWNER
        try{
            $token=[Guid]::NewGuid().ToString('N');$env:ACE_BUILD_LOCK_OWNER=$token
            $script=Child 'build.bat';Regular $script
            $start=New-Object Diagnostics.ProcessStartInfo
            $start.FileName=$env:ComSpec;$start.WorkingDirectory=$root
            $start.Arguments='/d /s /c ""'+$script+'" --ace-build-guarded '+$token+' 2>&1"'
            $start.UseShellExecute=$false;$start.CreateNoWindow=$true
            $start.RedirectStandardOutput=$true
            $child=[Diagnostics.Process]::Start($start)
            try{
                # CMD merges stderr into this single pipe. Pump raw chunks live,
                # including fragments without newline; no two-pipe deadlock or
                # hidden pause prompt. The outer visible batch owns the pause.
                $output=[Console]::OpenStandardOutput();$chunk=New-Object byte[] 8192
                while(($read=$child.StandardOutput.BaseStream.Read($chunk,0,$chunk.Length)) -gt 0){$output.Write($chunk,0,$read);$output.Flush()}
                $child.WaitForExit();$childExit=$child.ExitCode
            }finally{$child.Dispose()}
        }finally{$env:ACE_BUILD_LOCK_OWNER=$previousOwner;$lock.Dispose()}
        exit $childExit
    }elseif($Mode -eq 'Begin'){
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='building';source_extracted=$false;product_compiled=$false;verified_product=$false;utc=[DateTime]::UtcNow.ToString('o')})
        Write-Host 'Build started. Existing executables are not evidence of this attempt.'
    }elseif($Mode -eq 'Validate' -or $Mode -eq 'Extract'){
        $parsed=Parse-Bundles
        if($Mode -eq 'Validate'){
            Write-Host ($parsed.files.Count.ToString()+' unique source blocks validated; no files written.')
        }else{
            $captured=[pscustomobject]@{inputs=[pscustomobject]$parsed.inputs}
            Verify-Inputs $captured
            # All bundles are parsed before any source mutation. Stage and both
            # rename destinations are checked as direct children of this root.
            $token=[Guid]::NewGuid().ToString('N')
            $stage=Child ('.ace-source-stage-'+$token)
            $backup=Child ('.ace-source-backup-'+$token)
            $source=Child 'src'
            Require (-not (Test-Path -LiteralPath $stage) -and -not (Test-Path -LiteralPath $backup)) 'Fresh extraction paths already exist'
            $null=[IO.Directory]::CreateDirectory($stage)
            foreach($file in $parsed.files){
                $target=Join-Path $stage $file.name
                [IO.File]::WriteAllBytes($target,$file.bytes)
                Require ((Hash-File $target) -ceq $file.sha256) ('Staged source hash mismatch: '+$file.name)
            }
            Require (@(Get-ChildItem -LiteralPath $stage -File -Force).Count -eq $parsed.files.Count) 'Staged source inventory mismatch'
            # Check before each move; no recursive deletion is performed.
            Verify-Inputs $captured
            Regular $stage $true
            if(Test-Path -LiteralPath $source){Regular $source $true;[IO.Directory]::Move($source,$backup)}
            try{[IO.Directory]::Move($stage,$source)}catch{
                if(-not (Test-Path -LiteralPath $source) -and (Test-Path -LiteralPath $backup)){[IO.Directory]::Move($backup,$source)}
                throw
            }
            $fileHashes=[ordered]@{};foreach($file in $parsed.files){$fileHashes[$file.name]=$file.sha256}
            Write-AtomicJson $receiptPath ([ordered]@{schema=1;state='extracted';root=$root;count=$parsed.files.Count;inputs=$parsed.inputs;files=$fileHashes;previous_source_backup=$(if(Test-Path -LiteralPath $backup){$backup}else{$null});utc=[DateTime]::UtcNow.ToString('o')})
            Write-Host ($parsed.files.Count.ToString()+' unique source blocks staged, hash-checked and extracted.')
            if(Test-Path -LiteralPath $backup){Write-Host ('Previous generated source retained at '+$backup)}
        }
    }elseif($Mode -eq 'Dependencies'){
        Install-LockedDependencies
    }elseif($Mode -eq 'Runtime'){
        $null=Snapshot-Runtime
        Write-Host 'Required runtime DLL set is present and nonempty.'
    }elseif($Mode -eq 'Prepare'){
        $receipt=Read-Extraction;$hashes=Snapshot-Source $receipt;$runtime=Snapshot-Runtime
        Verify-LockedDependencies
        $meta=Read-Json (Join-Path (Child 'src') 'build-info.json')
        Require ($meta.source_sha256 -ceq (Source-Digest $receipt)) 'Generated metadata does not identify extracted source/build inputs'
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='compiling';source_extracted=$true;product_compiled=$false;verified_product=$false;source_count=$receipt.count;source_tree_sha256=$hashes;runtime_sha256=$runtime;dependency_lock_sha256=$dependencyLockSha256;build_id=$meta.build_id;utc=[DateTime]::UtcNow.ToString('o')})
    }elseif($Mode -eq 'Compiled'){
        $receipt=Read-Extraction
        $prepared=Read-Json $resultPath
        Require ($prepared.state -eq 'compiling') 'No current pre-compile source snapshot'
        Verify-SourceSnapshot $prepared.source_tree_sha256
        Verify-RuntimeSnapshot $prepared.runtime_sha256
        Require ($prepared.dependency_lock_sha256 -ceq $dependencyLockSha256) 'Dependency lock changed after preparation'
        Verify-LockedDependencies
        $product=Child 'loader.build.tmp.exe';Regular $product
        Require ((Get-Item -LiteralPath $product).Length -gt 0) 'Compiler output is empty'
        Require (Has-BuildId $product $prepared.build_id) 'Compiler output does not contain this stamped build ID; refusing stale executable'
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='compiled';source_extracted=$true;product_compiled=$true;verified_product=$false;source_count=$receipt.count;source_tree_sha256=$prepared.source_tree_sha256;runtime_sha256=$prepared.runtime_sha256;dependency_lock_sha256=$dependencyLockSha256;build_id=$prepared.build_id;product_sha256=(Hash-File $product);utc=[DateTime]::UtcNow.ToString('o')})
    }elseif($Mode -eq 'Verify'){
        $receipt=Read-Extraction;$compiled=Read-Json $resultPath
        Require ($compiled.state -eq 'compiled' -and $compiled.product_compiled -eq $true) 'No compiler-completion receipt for this attempt'
        Verify-SourceSnapshot $compiled.source_tree_sha256
        $dist=Child 'dist';Regular $dist $true
        $product=Join-Path $dist 'loader.exe';Regular $product
        Require ((Hash-File $product) -ceq $compiled.product_sha256) 'Packaged executable differs from compiler output'
        Require ($compiled.dependency_lock_sha256 -ceq $dependencyLockSha256) 'Dependency lock differs from the compiled receipt'
        Verify-RuntimeSnapshot $compiled.runtime_sha256
        Verify-RuntimeSnapshot $compiled.runtime_sha256 $dist
        foreach($relative in @('assets/collision/dl_midtown.acecol','assets/collision/hero_testing.acecol','assets/radar/maps.json','assets/radar/dl_midtown.png')){
            $input=Join-Path $root $relative;$copy=Join-Path $dist $relative
            Regular $input;Regular $copy
            Require ((Hash-File $input) -ceq (Hash-File $copy)) ('Required asset copy differs: '+$relative)
        }
        $metaPath=Join-Path $dist 'build-info.json';Regular $metaPath
        Require ((Hash-File $metaPath) -ceq $compiled.source_tree_sha256.'build-info.json') 'Packaged metadata differs from prepared compiler input'
        $meta=Read-Json $metaPath
        Require ($meta.build_id -ceq $compiled.build_id) 'Packaged metadata differs from compiled build identity'
        $sourceDigest=Source-Digest $receipt
        Require ($meta.source_sha256 -ceq $sourceDigest) 'Packaged metadata does not identify extracted source/build inputs'
        $dev=Child 'loader.exe';$devUpdated=$false
        if(Test-Path -LiteralPath $dev){Regular $dev;$devUpdated=(Hash-File $dev) -ceq $compiled.product_sha256}
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='passed';source_extracted=$true;product_compiled=$true;verified_product=$true;source_count=$receipt.count;build_id=$meta.build_id;product_sha256=$compiled.product_sha256;runtime_sha256=$compiled.runtime_sha256;dependency_lock_sha256=$dependencyLockSha256;development_copy_current=$devUpdated;verified_product_path=$product;utc=[DateTime]::UtcNow.ToString('o')})
        Write-Host ('Verified product: '+$product+' | build '+$meta.build_id+' | SHA256 '+$compiled.product_sha256)
        if($devUpdated){Write-Host 'Development loader.exe matches this build.'}else{Write-Warning 'Root loader.exe was not refreshed and must not be treated as this build. A lock is only one possible cause; use the verified product artifact.'}
    }elseif($Mode -eq 'Failed'){
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='failed';verified_product=$false;utc=[DateTime]::UtcNow.ToString('o');note='This attempt failed. Existing executables or earlier metadata are not a successful build result.'})
    }
    exit 0
}catch{
    Write-Error ('ACE build guard: '+$_.Exception.Message) -ErrorAction Continue
    exit 1
}
:ACE_BUILD_GUARD_END
