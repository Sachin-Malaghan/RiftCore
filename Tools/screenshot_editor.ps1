# Launches the editor, waits for it to load, saves a screenshot of the
# primary screen and closes the editor again. Used to check the UI.
#
#   powershell -ExecutionPolicy Bypass -File Tools\screenshot_editor.ps1 [-Out shot.png] [-Wait 8]
param(
    [string]$Out  = "Build\editor_screenshot.png",
    [int]   $Wait = 8,
    [string]$Exe  = "Build\bin\RiftCoreEditor.exe",
    [string]$EditorArgs = ""
)

$root = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $root $Exe
$outPath = Join-Path $root $Out
if (-not (Test-Path $exePath)) { Write-Error "Editor not built: $exePath"; exit 1 }

if ($EditorArgs) { $p = Start-Process -FilePath $exePath -WorkingDirectory $root -PassThru -ArgumentList $EditorArgs }
else             { $p = Start-Process -FilePath $exePath -WorkingDirectory $root -PassThru }
Start-Sleep -Seconds $Wait

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$b   = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g   = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
$bmp.Save($outPath)
$g.Dispose()
$bmp.Dispose()

if ($p.HasExited) {
    Write-Output "Editor exited early with code $($p.ExitCode)"
    exit 1
}
$p.CloseMainWindow() | Out-Null
if (-not $p.WaitForExit(4000)) { Stop-Process -Id $p.Id -Force }
Write-Output "Saved $outPath"
