# P2: Spell Checker

## Overview
`spell` loads a dictionary (one word per line) and scans files, directories, or stdin. 
For every token that qualifies as a **word** but **is not in the dictionary**, it prints:
```
file:line:col word
```
If checking **only one explicit file** or **stdin**, the filename may be omitted:
```
line:col word
```

## Command Line
```
spell [-s suffix] dictionary [file|dir]*
```
- `dictionary` (required): path to dictionary file.
- `-s suffix` (optional): limit directory scans to files with this suffix (default: `.txt`).
- If no files or directories are provided, the program reads from **stdin**.
- When a directory is provided: recurse, **skip entries whose names start with `.`**, and include **only** files ending with the suffix.
- When a file is explicitly provided: process it **regardless** of suffix.

## Word Rules
- Tokenization: split on **whitespace** (contiguous non-whitespace bytes = one token).
- Ignore tokens that contain **no letters** (covers all-digits and all-symbols tokens).
- Strip **leading** openers only: `("'[{`
- Strip **trailing** symbols (common punctuation) — see `spell.c` for the set.
- **Never** ignore or strip **mid-word** punctuation (e.g., `foo-bar` stays one token).
- Positions: `line` and `col` are **1-based**. The printed `col` is the **first character of the actual word after leading-openers are stripped.**

## Dictionary & Capitalization
- Dictionary loaded with `read()`; one entry per line; duplicates allowed.
- Lookups use a lowercase key.
- If a lowercase key exists and **any** of its dictionary forms are **all lowercase letters**, **any case** in the text is accepted.
- Otherwise (no all-lowercase form exists), the text must **exactly** match one of the dictionary’s original (case-sensitive) forms.

## Exit Status
- **SUCCESS** (0): every input file was opened successfully **and** no misspellings were printed.
- **FAILURE** (1): otherwise (any open failure **or** any misspellings).

## Build
```bash
make
```
This produces `./spell`.

## Usage Examples
```bash
# Single file (filename omitted on output)
./spell tests/dicts/small.txt tests/inputs/good.txt

# Multiple files (filenames included)
./spell tests/dicts/small.txt tests/inputs/bad.txt tests/inputs/caps.txt

# Directory recursion (suffix default .txt)
./spell tests/dicts/small.txt tests/tree

# Stdin
echo "Badd wordz" | ./spell tests/dicts/small.txt
```

## Project Structure
```
P2/
├─ Makefile
├─ README.md
├─ AUTHOR
└─ src/
   ├─ main.c        # CLI, recursion, orchestration
   ├─ spell.c       # tokenizer + checker using read()
   ├─ spell.h
   ├─ dict.c        # dictionary load + lookup (capitalization rule)
   └─ dict.h
tests/
  ├─ dicts/
  ├─ inputs/
  ├─ expected/
  └─ tree/          # directory traversal test (with subdir and hidden files)
```

## Testing
Run:
```bash
make test
```
This script builds `spell`, runs a few representative cases, and diffs the output with files in `tests/expected/`.

## Notes on Compliance
- All file reading uses `read()` (no `fgets`/`getline`). 
- We track 1-based `line`/`col`; columns reflect the first character **after** leading-openers are stripped.
- Explicit files ignore the suffix rule; directories obey it and skip names starting with `.`.
- Output format and exit semantics match the write-up.
