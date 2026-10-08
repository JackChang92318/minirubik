# Mini Rubik Solver

An optimal 2×2×2 Rubik's Cube solver using IDA* search.

The C solver uses permutation and orientation pattern databases (PDBs).
The RV32I assembly solver uses precomputed rank-transition tables to avoid
repeated ranking during the search. It provides an LED display build and
a renderer-free measurement build.

## Files

| Path | Purpose |
|---|---|
| `ida_solver.c` | C IDA* solver |
| `include/pdb_data.h` | Permutation PDB for the C solver |
| `include/pdb_data_ori.h` | Orientation PDB for the C solver |
| `asm/rubik_solver_source.S` | Shared assembly source |
| `asm/rubik_solver.s` | Ripes LED display build |
| `asm/rubik_solver_measure.s` | Renderer-free measurement build |
| `tools/build_led.ps1` | Generates both assembly builds |
| `builders/` | Host programs that generate the C PDB headers |
| `logs/` | Recorded measurements |
| `tests/rank-asm-results/` | Distance-11 verification results |

The original baseline programs remain in the repository for reference.

## Build and Run the C Solver

Run these commands from the repository root:

```sh
gcc -O2 -std=c99 -Wall -Wextra ida_solver.c -o ida_solver
./ida_solver 21345671111111
```

On Windows PowerShell:

```powershell
gcc -O2 -std=c99 -Wall -Wextra ida_solver.c -o ida_solver.exe
.\ida_solver.exe 21345671111111
```

The program prints the solution moves. An already-solved input prints
an empty line.

The existing `make` target builds the baseline programs, not `ida_solver.c`.

### Input Format

The input contains 14 digits: `PPPPPPPOOOOOOO`.

- `P`: digits 1–7, each appearing exactly once.
- `O`: digits 1–3, decoded as orientations 0–2.
- The sum of the decoded orientations must be divisible by three.

The fixed corner is omitted from the input.

Example solved state:

```text
12345671111111
```

### Solution Format

Moves use the faces `R`, `B`, and `D`.

| Suffix | Meaning |
|---|---|
| None | 90° clockwise |
| `'` | 90° counterclockwise |
| `2` | 180° |

Clockwise is viewed directly from outside the corresponding face.
Each token counts as one move in the half-turn metric.

## Run the RV32I Solver in Ripes

### Generate the Assembly Builds

The generated `.s` files are included in the repository.
After editing the shared source, regenerate them from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_led.ps1
```

This requires GCC in `PATH` and uses only its preprocessor.

Edit `input_state` in `asm/rubik_solver_source.S` to change the input.
Open the generated `.s` files in Ripes, rather than the `.S` source.
Direct edits to generated files will be replaced during regeneration.

### LED Display

1. Add **LED Matrix 0** in the Ripes I/O tab.
2. Set **Width = 35** and **Height = 25**.
3. Select **RV32_ISS** with the **RV32I** instruction set.
4. Open `asm/rubik_solver.s`, assemble, reset, and run.

The display shows the initial cube and replays the solution moves.

The cube net places U above F, L/F/R/B across the middle, and D below F.
Each facelet occupies 4×3 LEDs.

| Face | Color |
|---|---|
| U | White |
| F | Green |
| R | Red |
| B | Blue |
| L | Orange |
| D | Yellow |

### Instruction Measurement

Use `asm/rubik_solver_measure.s` with **RV32_ISS**.
This build excludes LED rendering and animation delays.

From PowerShell, replace the Ripes executable path below with your own:

```powershell
Start-Process -FilePath "C:\path\to\Ripes.exe" `
    -ArgumentList '--mode cli --src asm/rubik_solver_measure.s -t asm --proc RV32_ISS --iret' `
    -WorkingDirectory (Get-Location).Path `
    -WindowStyle Hidden -Wait `
    -RedirectStandardOutput ".\measurement.log" `
    -RedirectStandardError ".\measurement_error.log"

Get-Content .\measurement.log
```

The LED build's instruction count includes animation delays and should
not be used for the instruction-budget comparison.

## Results and Report

The recorded distance-11 verification checked all 2,644 states.

| Metric | Result |
|---|---:|
| Checked states | 2,644 |
| Failed gates | 0 |
| Worst retired instruction count | 21,486,458 |
| Instruction limit per state | 50,000,000 |
| Renderer-free static data | 114,849 bytes |
| LED build static data | 115,061 bytes |
| Static data limit | 131,072 bytes |

Verification evidence:

- [Summary](tests/rank-asm-results/summary.txt)
- [Per-state results](tests/rank-asm-results/results.csv)
- [Measurement configuration](tests/rank-asm-results/config.json)
- [C host verification log](logs/verify_host_full.log)
- [GCC measurement log](logs/gcc_ripes.log)

The distance-11 results identify the tested source snapshot by SHA256.
They were collected before the final assembly files were renamed and
organized into `asm/`.

The GCC log records a measurement build with a fixed input and replay
validation. The current C program uses command-line input.

See the [HackMD report](https://hackmd.io/@jackchang92318/B1Wvy1b9Mx)
for the design, optimization, measurements, pipeline walkthrough,
and AI-use disclosure.