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

$arch = $env:KLOGG_ARCH
$defaultIs64 = $false
if ($arch -and ($arch -match '64')) {
    $defaultIs64 = $true
}

if (-not $PluginFile) {
    $PluginFile = if ($defaultIs64) { 'klogg_lister.wlx64' } else { 'klogg_lister.wlx' }
}

if (-not $PluginType) {
    $PluginType = if ($defaultIs64) { 'wlx64' } else { 'wlx' }
}

if (-not $Version) {
    $Version = '0.0.0'
}

$PluginFile = $PluginFile.Trim()
$PluginType = $PluginType.Trim()
$Version = $Version.Trim()

$manifestPath = Join-Path $PluginRoot 'pluginst.inf'

$lines = @(
    '[plugininstall]',
    'version=' + $Version,
    'defaultdir=klogg_lister',
    'type=' + $PluginType,
    'file=' + $PluginFile,
    'name=Klogg Log Viewer',
    'description=Log viewer plugin for Total Commander',
    'defaultextension=LOG LOGX LOGS CEF CLF ELF W3C OUT ERR'
)

$content = ($lines -join "`n") + "`n"
[System.IO.File]::WriteAllText(
    $manifestPath,
    $content,
    [System.Text.Encoding]::ASCII
)
