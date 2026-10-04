# ============================================================
#  Project Backup Tool  (backup.ps1)
#  ASCII-only on purpose: Windows PowerShell 5.1 mis-parses
#  UTF-8 files without a BOM. Chinese docs live in README.txt.
#  Double-click Scripts\backup.bat to run this.
# ============================================================

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $Root

function Say($text, $color = "Gray") { Write-Host $text -ForegroundColor $color }
function Line { Say ("-" * 64) "DarkGray" }

Line
Say "  PROJECT BACKUP" "Cyan"
Say ("  Folder : " + $Root) "DarkGray"
Say ("  Time   : " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss")) "DarkGray"
Line
Write-Host ""

# ---- 0. environment checks ----
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Say "  [FAIL] git not found. Please install Git for Windows." "Red"
    exit 1
}
if (-not (Test-Path -LiteralPath (Join-Path $Root ".git"))) {
    Say "  [FAIL] This folder is not a git repository." "Red"
    exit 1
}

# ---- 1. any changes? ----
$changes = git status --porcelain
if ([string]::IsNullOrWhiteSpace($changes)) {
    Say "  No new changes. Nothing to back up." "Green"
    Say "  (Did you save inside the UE editor? Unsaved work is not seen by git.)" "DarkGray"
    Write-Host ""
    exit 0
}

$changeList = @($changes -split "`n" | Where-Object { $_.Trim() -ne "" })
$newCount = @($changeList | Where-Object { $_ -match "^\?\?" }).Count
$modCount = @($changeList | Where-Object { $_ -match "^ ?M" }).Count
$delCount = @($changeList | Where-Object { $_ -match "^ ?D" }).Count

Say "  Changes detected:" "Yellow"
Say ("    new " + $newCount + "   modified " + $modCount + "   deleted " + $delCount) "Gray"
Write-Host ""

if ($changeList.Count -gt 3000) {
    Say "  [WARN] Very many files changed (" + $changeList.Count + ")." "Yellow"
    Say "         If this was an accident (e.g. imported a whole pack), press Ctrl+C now." "Yellow"
    Write-Host ""
    $ans = Read-Host "  Continue? (Y/N)"
    if ($ans -notmatch "^[Yy]") { Say "  Cancelled. Nothing was changed." "DarkGray"; exit 0 }
    Write-Host ""
}

# ---- 2. ask for a note ----
Say "  Describe this change in one line (Enter = automatic note):" "Cyan"
$note = Read-Host "  > "
if ([string]::IsNullOrWhiteSpace($note)) { $note = "periodic backup" }
$stamp = Get-Date -Format "yyyy-MM-dd HH:mm"
$msg = "backup $stamp - $note"
Write-Host ""

# ---- 3. stage ----
Say "  [1/4] Staging changes..." "Gray"
git add -A
if ($LASTEXITCODE -ne 0) { Say "  [FAIL] git add failed. Aborted, project untouched." "Red"; exit 1 }

# ---- 4. commit ----
Say "  [2/4] Creating save point..." "Gray"
git commit -m $msg --quiet
if ($LASTEXITCODE -ne 0) {
    Say "  [FAIL] git commit failed. Aborted." "Red"
    Say "  Tip: run Scripts\rollback.bat to return to an earlier save point." "DarkGray"
    exit 1
}
$hash = (git rev-parse --short HEAD).Trim()
Say ("        save point = " + $hash) "Green"

# ---- 5. push ----
Say "  [3/4] Uploading to GitHub (large files may take a while)..." "Gray"
git push origin main 2>&1 | ForEach-Object { Say ("        " + $_) "DarkGray" }

Write-Host ""
Line
if ($LASTEXITCODE -eq 0) {
    Say "  [4/4] Upload OK." "Green"
    Write-Host ""
    Say "  BACKUP COMPLETE" "Green"
    Say ("  Save point: " + $hash) "Gray"
} else {
    Say "  Local save OK, but upload to GitHub FAILED." "Yellow"
    Say ("  Local save point: " + $hash) "Gray"
    Write-Host ""
    Say "  Common causes:" "Yellow"
    Say "    1) Network issue  -> just run backup.bat again later." "DarkGray"
    Say "    2) Login expired  -> run this in PowerShell, then log in via browser:" "DarkGray"
    Say "       git credential-manager github login --username LadyStardust59 --browser" "White"
    Say "    3) Remote changed -> tell the AI assistant, do not force push." "DarkGray"
    Write-Host ""
    Say "  IMPORTANT: your work is already saved on this computer. Nothing is lost." "Green"
}
Line
Write-Host ""
