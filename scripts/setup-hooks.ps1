# Git Hooks Setup Script
# Installs pre-commit hooks for MacroMan AI Aimbot project
#
# Usage: .\scripts\setup-hooks.ps1

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  MacroMan Aimbot - Git Hooks Setup" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if we're in the git repository root
if (-not (Test-Path ".git")) {
    Write-Host "❌ Error: Not in a git repository root directory" -ForegroundColor Red
    Write-Host "   Please run this script from the repository root" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

# Verify .githooks directory exists
if (-not (Test-Path ".githooks")) {
    Write-Host "❌ Error: .githooks directory not found" -ForegroundColor Red
    Write-Host "   Expected location: .githooks/" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

# Create .git/hooks directory if it doesn't exist
$hookDir = ".git\hooks"
if (-not (Test-Path $hookDir)) {
    New-Item -ItemType Directory -Path $hookDir -Force | Out-Null
    Write-Host "✅ Created .git/hooks directory" -ForegroundColor Green
}

# Copy pre-commit hook
$sourceHook = ".githooks\pre-commit"
$destHook = "$hookDir\pre-commit"

if (-not (Test-Path $sourceHook)) {
    Write-Host "❌ Error: Source hook not found at $sourceHook" -ForegroundColor Red
    Write-Host ""
    exit 1
}

Copy-Item -Path $sourceHook -Destination $destHook -Force
Write-Host "✅ Installed pre-commit hook" -ForegroundColor Green

# Verify installation
if (Test-Path $destHook) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "✅ Git hooks installed successfully!" -ForegroundColor Green
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "📋 What happens now:" -ForegroundColor Cyan
    Write-Host "   • Every commit will run the full test suite" -ForegroundColor White
    Write-Host "   • Tests: unit + integration + benchmark smoke test" -ForegroundColor White
    Write-Host "   • Typical run time: 30-60 seconds" -ForegroundColor White
    Write-Host ""
    Write-Host "⚠️  Requirements:" -ForegroundColor Yellow
    Write-Host "   • Build the project first:" -ForegroundColor White
    Write-Host "     cmake -B build -S . -DCMAKE_BUILD_TYPE=Release" -ForegroundColor Gray
    Write-Host "     cmake --build build --config Release -j" -ForegroundColor Gray
    Write-Host ""
    Write-Host "🚨 Emergency bypass (NOT RECOMMENDED):" -ForegroundColor Yellow
    Write-Host "   git commit --no-verify -m `"message`"" -ForegroundColor Gray
    Write-Host ""
    Write-Host "📖 Documentation: .githooks/README.md" -ForegroundColor Cyan
    Write-Host ""
} else {
    Write-Host "❌ Failed to install hooks" -ForegroundColor Red
    Write-Host ""
    exit 1
}
