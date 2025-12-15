$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$srcGen  = Join-Path $repoRoot 'winamp_plugin\gen_music4all.dll'
$srcML   = Join-Path $repoRoot 'winamp_plugin\ml_music4all.dll'
$srcPy   = Join-Path $repoRoot 'music4all.py'
$srcPyLegacy = Join-Path $repoRoot 'instapod.py'

foreach ($p in @($srcGen,$srcML,$srcPy,$srcPyLegacy)) {
  if (-not (Test-Path $p)) { throw "No existe: $p" }
}

function Get-WinampPluginsDir {
  $candidates = @(
    'C:\Program Files (x86)\Winamp\Plugins',
    'C:\Program Files\Winamp\Plugins'
  )
  foreach ($c in $candidates) { if (Test-Path $c) { return $c } }

  $uninstallKeys = @(
    'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
    'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall'
  )

  foreach ($key in $uninstallKeys) {
    if (-not (Test-Path $key)) { continue }

    $app = Get-ChildItem $key -ErrorAction SilentlyContinue |
      ForEach-Object { Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue } |
      Where-Object { $_.DisplayName -match 'Winamp' } |
      Select-Object -First 1

    if ($app -and $app.InstallLocation) {
      $plugins = Join-Path $app.InstallLocation 'Plugins'
      if (Test-Path $plugins) { return $plugins }
    }
  }

  throw 'No pude detectar la carpeta Plugins de Winamp.'
}

$dest = Get-WinampPluginsDir

Copy-Item -Force $srcGen (Join-Path $dest 'gen_music4all.dll')
Copy-Item -Force $srcML  (Join-Path $dest 'ml_music4all.dll')
Copy-Item -Force $srcPy  (Join-Path $dest 'music4all.py')
Copy-Item -Force $srcPyLegacy  (Join-Path $dest 'instapod.py')

Write-Host "Copiado OK a: $dest"