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

$ensureEntry = {
    param([string]$pattern, [string]$valueDescription)
    if (-not [System.Text.RegularExpressions.Regex]::IsMatch($content, $pattern, $options)) {
        throw "Manifest file '$manifestPath' is missing a $valueDescription entry."
    }
}

$newContent = $content

$versionPattern = '^(version=).*$'
& $ensureEntry $versionPattern 'version'
$newContent = [System.Text.RegularExpressions.Regex]::Replace(
    $newContent,
    $versionPattern,
    "`$1$Version",
    $options
)

if ($PluginFile) {
    $filePattern = '^(file=).*$'
    & $ensureEntry $filePattern 'file'
    $newContent = [System.Text.RegularExpressions.Regex]::Replace(
        $newContent,
        $filePattern,
        "`$1$PluginFile",
        $options
    )
}

if ($PluginType) {
    $typePattern = '^(type=).*$'
    & $ensureEntry $typePattern 'type'
    $newContent = [System.Text.RegularExpressions.Regex]::Replace(
        $newContent,
        $typePattern,
        "`$1$PluginType",
        $options
    )
}

if (-not $newContent.EndsWith("`n")) {
    $newContent += "`n"
}

[System.IO.File]::WriteAllText(
    $manifestPath,
    $newContent,
    [System.Text.Encoding]::ASCII
)
