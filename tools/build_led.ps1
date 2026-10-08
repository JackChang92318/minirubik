param()
$ErrorActionPreference = 'Stop'
$directory = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $directory
$asmDirectory = Join-Path $root 'asm'
$source = Join-Path $asmDirectory 'rubik_solver_source.S'
foreach ($variant in @(@(1,'rubik_solver.s'), @(0,'rubik_solver_measure.s'))) {
    $output = Join-Path $asmDirectory $variant[1]
    & gcc -E -P -x assembler-with-cpp ('-DRENDER=' + $variant[0]) $source -o $output
    if ($LASTEXITCODE -ne 0) { throw "Could not generate $output" }
    $header = "# Generated from asm/rubik_solver_source.S; RENDER=$($variant[0]).`n# AI-assisted implementation. Edit the source and rebuild; see LED_README.md.`n"
    [IO.File]::WriteAllText($output, $header + [IO.File]::ReadAllText($output), [Text.UTF8Encoding]::new($false))
    Write-Host "Created $($variant[1]) (RENDER=$($variant[0]))"
}
