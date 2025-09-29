[CmdletBinding()]
param(
    [Parameter()]
    [string]$PluginRoot = $(if ($env:KLOGG_WORKSPACE) { Join-Path $env:KLOGG_WORKSPACE 'release/totalcmd' } else { '' }),

    [Parameter()]
    [string]$Version = $env:KLOGG_VERSION
)

Set-StrictMode -Version Latest

if (-not $PluginRoot) {
    throw 'PluginRoot must be provided either via parameter or KLOGG_WORKSPACE environment variable.'
}

if (-not (Test-Path -LiteralPath $PluginRoot)) {
    throw "Plugin root '$PluginRoot' does not exist."
}

if (-not $Version) {
    $Version = '0.0.0'
}

$sanitize = {
    param([string]$value)
    if ($null -eq $value) { return '' }
    return $value.Trim().Replace("`r", '').Replace("`n", '')
}

$Version = & $sanitize $Version
if (-not $Version) {
    $Version = '0.0.0'
}

$manifestPath = Join-Path $PluginRoot 'pluginst.inf'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "Manifest file '$manifestPath' does not exist."
}

$content = [System.IO.File]::ReadAllText($manifestPath, [System.Text.Encoding]::ASCII)
if (-not $content) {
    $content = ''
}

$pattern = '^(version=).*$'
if (-not [System.Text.RegularExpressions.Regex]::IsMatch($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)) {
    throw "Manifest file '$manifestPath' is missing a version entry."
}

$newContent = [System.Text.RegularExpressions.Regex]::Replace(
    $content,
    $pattern,
    "`$1$Version",
    [System.Text.RegularExpressions.RegexOptions]::Multiline
)

if (-not $newContent.EndsWith("`n")) {
    $newContent += "`n"
}

[System.IO.File]::WriteAllText(
    $manifestPath,
    $newContent,
    [System.Text.Encoding]::ASCII
)
