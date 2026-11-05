#!/usr/bin/env python3
"""Memory access checks and allocation ownership/failure checks, in temp builds."""

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def command(args, env=None):
    result = subprocess.run([str(arg) for arg in args], cwd=ROOT, env=env)
    if result.returncode:
        raise SystemExit(result.returncode)


def main():
    sources = sorted((ROOT / "srcs").rglob("*.c"))
    compiler = ["gcc", "-Wall", "-Werror", "-Wextra", "-g", "-DBONUS",
                "-Iincludes", *sources]
    with tempfile.TemporaryDirectory(prefix="ft-nm-memory-") as temporary:
        directory = Path(temporary)
        sanitized = directory / "sanitized"
        command([*compiler, "-O1", "-fsanitize=address,undefined",
                 "-fno-omit-frame-pointer", "-o", sanitized])
        env = dict(os.environ, FT_NM_BINARY=str(sanitized),
                   ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1")
        command(["python3", "tests/test_nm.py"], env)
        tracked = directory / "tracked"
        command([*compiler, "tests/alloc_tracker.c", "-Wl,--wrap=malloc",
                 "-Wl,--wrap=realloc", "-Wl,--wrap=free", "-o", tracked])
        env = dict(os.environ, FT_NM_BINARY=str(tracked))
        command(["python3", "tests/test_nm.py"], env)
        source = directory / "fixture.c"
        source.write_text("int data = 42; int fixture(void) { return data; }\n")
        obj = directory / "fixture.o"
        archive = directory / "fixture.a"
        command(["gcc", "-g", "-c", source, "-o", obj])
        command(["ar", "rcs", archive, obj])
        checks = 0
        for arguments in [[obj], [obj, obj], [archive], ["--help"], ["-Z"],
                          [directory / "missing", obj]]:
            for fail_at in range(1, 25):
                env = dict(os.environ, NM_ALLOC_FAIL_AT=str(fail_at))
                result = subprocess.run([str(tracked), *map(str, arguments)],
                                        env=env, capture_output=True, timeout=10)
                assert result.returncode in (0, 1, 2), result.stderr
                assert b"allocation tracker:" not in result.stderr, result.stderr
                if arguments == [obj] and fail_at <= 5:
                    assert result.returncode == 1 and result.stderr, fail_at
                checks += 1
        print(f"OK: {checks} allocation failure scenarios; no outstanding allocations")


if __name__ == "__main__":
    main()
