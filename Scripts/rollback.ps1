# ============================================================
#  Project Rollback Tool  (rollback.ps1)
#  ASCII-only on purpose (see backup.ps1 header).
#  Safety: the current state is saved as a rescue commit first,
#  so you can always come back after rolling back.
# ============================================================

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $Root

function Say($text, $color = "Gray") { Write-Host $text -ForegroundColor $color }
function Line { Say ("-" * 64) "DarkGray" }

Line
Say "  PROJECT ROLLBACK" "Cyan"
Line
Write-Host ""

if (-not (Test-Path -LiteralPath (Join-Path $Root ".git"))) {
    Say "  [FAIL] Not a git repository." "Red"; exit 1
}

# ---- 1. rescue current state first ----
$dirty = git status --porcelain
$rescueHash = $null
if (-not [string]::IsNullOrWhiteSpace($dirty)) {
    Say "  Unsaved changes found. Saving them first (you can return later)..." "Yellow"
    git add -A
    git commit -m ("rescue " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss")) --quiet
    $rescueHash = (git rev-parse --short HEAD).Trim()
    Say ("        rescue save point = " + $rescueHash) "Green"
    Write-Host ""
}

# ---- 2. list recent save points ----
Say "  Recent save points (newest first):" "Cyan"
Write-Host ""
$log = git log --pretty=format:"%h|%ad|%s" --date=format:"%m-%d %H:%M" -15
$items = @()
$i = 0
foreach ($row in $log) {
    $p = $row -split "\|"
    $items += , @($p[0], $p[1], $p[2])
    $mark = ""
    if ($i -eq 0) { $mark = "   <== you are here" }
    $col = "Gray"
    if ($i -eq 0) { $col = "Green" }
    Say ("    [" + $i + "]  " + $p[0] + "  " + $p[1] + "  " + $p[2] + $mark) $col
    $i++
}
Write-Host ""
Line
Say "  Enter the number to roll back to (Enter = cancel):" "Yellow"
Say "  Example: 1 = go back to the previous save point." "DarkGray"
$sel = Read-Host "  > "
Write-Host ""

if ([string]::IsNullOrWhiteSpace($sel)) { Say "  Cancelled. Nothing was done." "DarkGray"; exit 0 }
if ($sel -notmatch "^\d+$") { Say "  [FAIL] Not a number. Cancelled." "Red"; exit 1 }
$idx = [int]$sel
if ($idx -ge $items.Count) { Say "  [FAIL] Number out of range. Cancelled." "Red"; exit 1 }

$target = $items[$idx]
Write-Host ""
Say ("  Target: " + $target[0] + "  " + $target[1] + "  " + $target[2]) "Yellow"
Say "  WARNING: rolling back overwrites the current files in your working folder." "Red"
Write-Host ""
$confirm = Read-Host "  Type YES to confirm"
if ($confirm -ne "YES") { Say "  Cancelled. Nothing was done." "DarkGray"; exit 0 }

# ---- 3. do it ----
Write-Host ""
Say "  Rolling back..." "Gray"
git reset --hard $target[0] --quiet
if ($LASTEXITCODE -ne 0) { Say "  [FAIL] rollback failed." "Red"; exit 1 }
Say ("  [DONE] Project is now at " + $target[0]) "Green"
Write-Host ""

if ($rescueHash) {
    Say ("  Your previous state was saved as " + $rescueHash + ". Run this tool again to return.") "Cyan"
} else {
    Say "  There were no unsaved changes before rollback." "DarkGray"
}
Write-Host ""
Line
Say "  Tip: if the UE editor is open, close and reopen the project afterwards." "Yellow"
Line
Write-Host ""
