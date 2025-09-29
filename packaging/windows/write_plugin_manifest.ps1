[CmdletBinding()]
param(
    [Parameter()]
    [string]$PluginRoot = $(if ($env:KLOGG_WORKSPACE) { Join-Path $env:KLOGG_WORKSPACE 'release/totalcmd' } else { '' }),

    [Parameter()]
    [string]$Version = $env:KLOGG_VERSION,

    [Parameter()]
    [string]$PluginFile,

    [Parameter()]
    [string]$PluginType
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

$PluginFile = & $sanitize $PluginFile
$PluginType = & $sanitize $PluginType

$manifestPath = Join-Path $PluginRoot 'pluginst.inf'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "Manifest file '$manifestPath' does not exist."
}

$content = [System.IO.File]::ReadAllText($manifestPath, [System.Text.Encoding]::ASCII)
if (-not $content) {
    $content = ''
}

$options = [System.Text.RegularExpressions.RegexOptions]::Multiline

function Set-ManifestEntry {
    param(
        [string]$currentContent,
        [string]$entryName,
        [string]$entryValue
    )

    $pattern = "^(" + [System.Text.RegularExpressions.Regex]::Escape($entryName) + "=).*$"
    $updated = [System.Text.RegularExpressions.Regex]::Replace(
        $currentContent,
        $pattern,
        "`$1$entryValue",
        $options
    )

    if ($updated -ne $currentContent) {
        return $updated
    }

    $suffixNewLine = ''
    if ($currentContent -and -not $currentContent.EndsWith("`n")) {
        $suffixNewLine = "`n"
    }

    return $currentContent + $suffixNewLine + "$entryName=$entryValue`n"
}

$newContent = Set-ManifestEntry -currentContent $content -entryName 'version' -entryValue $Version

if ($PluginFile) {
    $newContent = Set-ManifestEntry -currentContent $newContent -entryName 'file' -entryValue $PluginFile
}

if ($PluginType) {
    $newContent = Set-ManifestEntry -currentContent $newContent -entryName 'type' -entryValue $PluginType
}

if (-not $newContent.EndsWith("`n")) {
    $newContent += "`n"
}

[System.IO.File]::WriteAllText(
    $manifestPath,
    $newContent,
    [System.Text.Encoding]::ASCII
)
