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

echo [2/7] Checking compiler...
set GCC=tools\mingw64\bin\g++.exe
if not exist "%GCC%" for /f "delims=" %%D in ('dir /b /ad tools ^| findstr /i mingw') do set GCC=tools\%%D\bin\g++.exe
if exist "%GCC%" (
    echo       OK.
) else (
    if not exist tools mkdir tools
    echo       downloading compiler - ONE TIME ONLY...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; $out=@(); try{ $rels=Invoke-RestMethod -Uri 'https://api.github.com/repos/brechtsanders/winlibs_mingw/releases?per_page=100' }catch{ exit 1 }; foreach($r in $rels){ foreach($x in @($r.assets)){ if($x.name -like 'winlibs-x86_64-posix-seh-*.zip'){ $out += $x } } }; if($out.Count -eq 0){ exit 1 }; $a = $out | Sort-Object -Property size | Select-Object -First 1; Write-Host ('downloading '+$a.name); Invoke-WebRequest -Uri $a.browser_download_url -OutFile 'tools\compiler.zip'; exit 0" || goto :fail
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path 'tools\compiler.zip' -DestinationPath 'tools' -Force; exit 0" || goto :fail
    set GCC=tools\mingw64\bin\g++.exe
    if not exist "%GCC%" for /f "delims=" %%D in ('dir /b /ad tools ^| findstr /i mingw') do set GCC=tools\%%D\bin\g++.exe
    if not exist "%GCC%" (echo [X] compiler not found & goto :fail)
)

echo [3/7] Checking UI library...
if not exist deps mkdir deps
if not exist deps\imgui mkdir deps\imgui
if not exist deps\imgui\backends mkdir deps\imgui\backends
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $u='https://raw.githubusercontent.com/ocornut/imgui/v1.90.4/'; try{ foreach($x in @('imgui.cpp','imgui.h','imgui_internal.h','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp','imconfig.h','imstb_rectpack.h','imstb_textedit.h','imstb_truetype.h')){ if(-not (Test-Path ('deps\imgui\'+$x))){ Invoke-WebRequest -Uri ($u+$x) -OutFile ('deps\imgui\'+$x) } }; foreach($x in @('imgui_impl_dx11.cpp','imgui_impl_dx11.h','imgui_impl_win32.cpp','imgui_impl_win32.h')){ if(-not (Test-Path ('deps\imgui\backends\'+$x))){ Invoke-WebRequest -Uri ($u+'backends/'+$x) -OutFile ('deps\imgui\backends\'+$x) } }; if(-not (Test-Path 'deps\json.hpp')){ Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp' -OutFile 'deps\json.hpp' }; Write-Host '       UI files OK'; exit 0 }catch{ exit 1 }" || goto :fail
for %%H in (imconfig.h imstb_rectpack.h imstb_textedit.h imstb_truetype.h imgui.cpp imgui.h imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp) do ( if not exist "deps\imgui\%%H" (echo [X] deps\imgui\%%H missing & goto :fail) )

echo [4/7] Checking DMA engine...
if exist vmm.dll (
    echo       OK.
) else (
    echo       downloading DMA engine - ONE TIME ONLY...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; try{ $rel=Invoke-RestMethod -Uri 'https://api.github.com/repos/ufrisk/MemProcFS/releases/latest' }catch{ exit 1 }; $files=@(); $rest=@(); foreach($x in @($rel.assets)){ if($x.name -like '*.zip'){ if($x.name -like '*files*'){ $files += $x } else { $rest += $x } } }; $cand = @($files | Sort-Object -Property size) + @($rest | Sort-Object -Property size); $done=$false; foreach($a in $cand){ try{ Invoke-WebRequest -Uri $a.browser_download_url -OutFile 'vmm_pack.zip' }catch{ continue }; try{ if(Test-Path 'vmm_unpack'){ Remove-Item -Recurse -Force 'vmm_unpack' }; Expand-Archive -Path vmm_pack.zip -DestinationPath vmm_unpack -Force }catch{ continue }; $v=Get-ChildItem -Path vmm_unpack -Recurse -Filter vmm.dll | Select-Object -First 1; if($v){ Copy-Item $v.FullName vmm.dll -Force; foreach($n in @('leechcore.dll','ftd3xx.dll')){ $f=Get-ChildItem -Path vmm_unpack -Recurse -Filter $n | Select-Object -First 1; if($f){ Copy-Item $f.FullName $n -Force } }; $done=$true; break } }; if(-not $done){ exit 1 }; exit 0" || goto :fail
)

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
if exist vmm.dll copy /y vmm.dll dist\vmm.dll >nul
if exist leechcore.dll copy /y leechcore.dll dist\leechcore.dll >nul
if exist ftd3xx.dll copy /y ftd3xx.dll dist\ftd3xx.dll >nul
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
    [ValidateSet('Run','Begin','Validate','Extract','Prepare','Compiled','Verify','Failed')][string]$Mode='Validate'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$utf8=New-Object Text.UTF8Encoding($false,$true)
$root=[IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\','/')
$resultPath=Join-Path $root '.ace-build-result.json'
$receiptPath=Join-Path $root '.ace-extraction.json'

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
    }elseif($Mode -eq 'Prepare'){
        $receipt=Read-Extraction;$hashes=Snapshot-Source $receipt
        $meta=Read-Json (Join-Path (Child 'src') 'build-info.json')
        Require ($meta.source_sha256 -ceq (Source-Digest $receipt)) 'Generated metadata does not identify extracted source/build inputs'
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='compiling';source_extracted=$true;product_compiled=$false;verified_product=$false;source_count=$receipt.count;source_tree_sha256=$hashes;build_id=$meta.build_id;utc=[DateTime]::UtcNow.ToString('o')})
    }elseif($Mode -eq 'Compiled'){
        $receipt=Read-Extraction
        $prepared=Read-Json $resultPath
        Require ($prepared.state -eq 'compiling') 'No current pre-compile source snapshot'
        Verify-SourceSnapshot $prepared.source_tree_sha256
        $product=Child 'loader.build.tmp.exe';Regular $product
        Require ((Get-Item -LiteralPath $product).Length -gt 0) 'Compiler output is empty'
        Require (Has-BuildId $product $prepared.build_id) 'Compiler output does not contain this stamped build ID; refusing stale executable'
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='compiled';source_extracted=$true;product_compiled=$true;verified_product=$false;source_count=$receipt.count;source_tree_sha256=$prepared.source_tree_sha256;build_id=$prepared.build_id;product_sha256=(Hash-File $product);utc=[DateTime]::UtcNow.ToString('o')})
    }elseif($Mode -eq 'Verify'){
        $receipt=Read-Extraction;$compiled=Read-Json $resultPath
        Require ($compiled.state -eq 'compiled' -and $compiled.product_compiled -eq $true) 'No compiler-completion receipt for this attempt'
        Verify-SourceSnapshot $compiled.source_tree_sha256
        $dist=Child 'dist';Regular $dist $true
        $product=Join-Path $dist 'loader.exe';Regular $product
        Require ((Hash-File $product) -ceq $compiled.product_sha256) 'Packaged executable differs from compiler output'
        foreach($dll in @('vmm.dll','leechcore.dll','ftd3xx.dll')){
            $input=Child $dll
            if(Test-Path -LiteralPath $input){Regular $input;$copy=Join-Path $dist $dll;Regular $copy;Require ((Hash-File $input) -ceq (Hash-File $copy)) ('Runtime DLL copy differs: '+$dll)}
        }
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
        Write-AtomicJson $resultPath ([ordered]@{schema=1;state='passed';source_extracted=$true;product_compiled=$true;verified_product=$true;source_count=$receipt.count;build_id=$meta.build_id;product_sha256=$compiled.product_sha256;development_copy_current=$devUpdated;verified_product_path=$product;utc=[DateTime]::UtcNow.ToString('o')})
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
