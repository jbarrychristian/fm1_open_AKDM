# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Refresh sdk/manifest.json after intentionally editing a listed SDK file
# (for example sdk/Update.html.in or sdk/I18n.js). The build refuses to run
# while any listed file differs from its recorded SHA-256. Only hashes of
# files already listed are updated; nothing is added or removed.
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$path = Join-Path $root 'sdk/manifest.json'
$text = [IO.File]::ReadAllText($path)
$manifest = $text | ConvertFrom-Json
$changed = 0
foreach ($file in $manifest.files.PSObject.Properties) {
    $full = Join-Path $root $file.Name
    if (!(Test-Path -LiteralPath $full)) { throw "Listed SDK file is missing: $($file.Name)" }
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $full).Hash.ToLowerInvariant()
    if ($hash -ne $file.Value) {
        $text = $text.Replace($file.Value, $hash)
        Write-Host "Updated $($file.Name)"
        ++$changed
    }
}
if ($changed) { [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding $false)) }
Write-Host "$changed manifest entr$(if ($changed -eq 1) { 'y' } else { 'ies' }) updated."
