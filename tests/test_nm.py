#!/usr/bin/env python3
"""Compare real ELF fixtures with GNU nm; exercise malformed input separately."""

import itertools
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get("FT_NM_BINARY", ROOT / "ft_nm")).resolve()
ENV = dict(os.environ, LC_ALL="C")
CHECKS = 0

C_SOURCE = r"""
extern int external;
extern int weak_object __attribute__((weak));
extern void weak_function(void) __attribute__((weak));
int common;
int initialized = 42;
static int local_data = 7;
static int local_bss;
const int readonly = 5;
__thread int tls;
__thread int tls_init = 4;
int weak_defined __attribute__((weak)) = 8;
void weak_defined_func(void) __attribute__((weak));
void weak_defined_func(void) {}
static void hidden_func(void) { local_bss++; }
int entry(void) {
    hidden_func();
    if (weak_function) weak_function();
    return external + local_data + weak_object;
}
"""

ASM_SOURCE = r"""
.file "types.s"
.section .debug_custom,"",@progbits
.globl debug_global
debug_global: .byte 0
.local debug_local
debug_local: .byte 0
.section .other,"",@progbits
.globl other_symbol
other_symbol: .byte 0
.section .sdata,"aw",@progbits
.globl small_data
small_data: .long 1
.section .sbss,"aw",@nobits
.globl small_bss
small_bss: .skip 4
.comm common_big,64,16
.globl abs_symbol
.set abs_symbol,123
.globl ifunc
.type ifunc,@gnu_indirect_function
.text
ifunc: ret
.weak weak_ifunc
.type weak_ifunc,@gnu_indirect_function
weak_ifunc: ret
.type ifunc_undef,@gnu_indirect_function
.globl ifunc_undef
.type weak_ifunc_undef,@gnu_indirect_function
.weak weak_ifunc_undef
.data
.quad ifunc_undef
.quad weak_ifunc_undef
.globl unique
.type unique,@gnu_unique_object
.data
unique: .byte 0
.type unique_undef,@gnu_unique_object
.quad unique_undef
.section .nonalloc_exec,"x",@progbits
local_nonalloc_exec: ret
.section .nonalloc_write,"w",@progbits
local_nonalloc_write: .byte 0
.section .nonalloc_nobits,"",@nobits
local_nonalloc_nobits: .skip 1
.section .note.GNU-stack,"",@progbits
.section .text.group_func,"axG",@progbits,group_signature,comdat
group_func: ret
"""


def run(args, cwd, check=False):
    result = subprocess.run([str(arg) for arg in args], cwd=cwd, env=ENV,
                            capture_output=True, timeout=10)
    if check and result.returncode:
        raise AssertionError(f"Command failed: {args}\n{result.stderr.decode()}")
    return result


def compare(args, cwd):
    global CHECKS
    reference = run(["nm", *args], cwd)
    actual = run([BINARY, *args], cwd)
    if actual.stdout != reference.stdout or actual.returncode != reference.returncode:
        import difflib
        diff = "".join(difflib.unified_diff(
            reference.stdout.decode().splitlines(True),
            actual.stdout.decode().splitlines(True), fromfile="nm", tofile="ft_nm"))
        raise AssertionError(f"Mismatch: {args}\n{diff}\n{actual.stderr.decode()}")
    CHECKS += 1


def safe_run(path, cwd, must_fail=False):
    global CHECKS
    actual = run([BINARY, path], cwd)
    assert actual.returncode in (0, 1), (path, actual.returncode, actual.stderr)
    assert not any(marker in actual.stderr for marker in (
        b"AddressSanitizer", b"LeakSanitizer", b"runtime error:")), actual.stderr
    if must_fail:
        assert actual.returncode == 1 and actual.stderr, path
    CHECKS += 1


def elf_variants(original, directory):
    """Valid extended section numbering and SHN_XINDEX; no toolchain dependency."""
    data = bytearray(original)
    shoff = struct.unpack_from("<Q", data, 40)[0]
    entsize, count, names = struct.unpack_from("<HHH", data, 58)
    struct.pack_into("<HH", data, 60, 0, 0xffff)
    struct.pack_into("<Q", data, shoff + 32, count)
    struct.pack_into("<I", data, shoff + 40, names)
    (directory / "extended.o").write_bytes(data)
    compare(["-a", "extended.o"], directory)

    data = bytearray(original)
    sections = [bytearray(data[shoff + i * entsize:shoff + (i + 1) * entsize])
                for i in range(count)]
    sym_index = next(i for i, s in enumerate(sections)
                     if struct.unpack_from("<I", s, 4)[0] == 2)
    symoff, symsize = struct.unpack_from("<QQ", sections[sym_index], 24)
    extended = bytearray()
    for offset in range(symoff, symoff + symsize, 24):
        index = struct.unpack_from("<H", data, offset + 6)[0]
        extended += struct.pack("<I", index if 0 < index < 0xff00 else 0)
        if 0 < index < 0xff00:
            struct.pack_into("<H", data, offset + 6, 0xffff)
    xoffset = len(data)
    data += extended
    section = bytearray(64)
    struct.pack_into("<I", section, 4, 18)  # SHT_SYMTAB_SHNDX
    struct.pack_into("<QQ", section, 24, xoffset, len(extended))
    struct.pack_into("<I", section, 40, sym_index)
    struct.pack_into("<QQ", section, 48, 4, 4)
    sections.append(section)
    struct.pack_into("<Q", data, 40, len(data))
    struct.pack_into("<H", data, 60, len(sections))
    data += b"".join(sections)
    (directory / "xindex.o").write_bytes(data)
    compare(["-a", "xindex.o"], directory)


def malformed(original, directory):
    is_64 = original[4] == 2
    word = "<Q" if is_64 else "<I"
    word_size = 8 if is_64 else 4
    shoff_field = 40 if is_64 else 32
    shsize_field = 58 if is_64 else 46
    shnames_field = 62 if is_64 else 50
    data_field = 24 if is_64 else 16
    link_field = 40 if is_64 else 24
    entry_field = 56 if is_64 else 36
    shoff = struct.unpack_from(word, original, shoff_field)[0]
    entsize, count = struct.unpack_from("<HH", original, shsize_field)
    symoff = None
    for i in range(count):
        off = shoff + i * entsize
        if struct.unpack_from("<I", original, off + 4)[0] == 2:
            symoff = off
            break
    mutations = [(4, b"\x00"), (5, b"\x00"), (6, b"\x00"),
                 (18, b"\x00\x00"), (shoff_field, b"\xff" * word_size),
                 (shsize_field, b"\x00\x00"), (shnames_field, b"\xfe\xff"),
                 (symoff + link_field, b"\xff" * 4),
                 (symoff + entry_field, b"\x00" * word_size)]
    symbol_data = struct.unpack_from(word, original, symoff + data_field)[0]
    mutations.append((symbol_data + (24 if is_64 else 16), b"\xff" * 4))
    for offset, value in mutations:
        data = bytearray(original)
        data[offset:offset + len(value)] = value
        (directory / "broken").write_bytes(data)
        safe_run("broken", directory, must_fail=True)
    # No terminator anywhere in either the symbol or section name table.
    sym_names = struct.unpack_from("<I", original, symoff + link_field)[0]
    section_names = struct.unpack_from("<H", original, shnames_field)[0]
    for index in [sym_names, section_names]:
        table = shoff + index * entsize
        offset, size = struct.unpack_from("<QQ" if is_64 else "<II",
                                         original, table + data_field)
        data = bytearray(original)
        data[offset:offset + size] = b"X" * size
        (directory / "broken").write_bytes(data)
        safe_run("broken", directory, must_fail=True)
    for size in [0, 1, 4, 16, 32, 51, 63, len(original) - 1]:
        (directory / "broken").write_bytes(original[:size])
        safe_run("broken", directory, must_fail=True)
    generator = random.Random(42)
    for _ in range(400):
        data = bytearray(original)
        for _ in range(generator.randrange(1, 12)):
            data[generator.randrange(len(data))] = generator.randrange(256)
        (directory / "broken").write_bytes(data)
        safe_run("broken", directory)
    for _ in range(100):
        (directory / "broken").write_bytes(generator.randbytes(generator.randrange(4096)))
        safe_run("broken", directory, must_fail=True)
    for data in [b"!<arch>\nX", b"!<arch>\n" + b" " * 60,
                 b"!<arch>\n" + b"/               " + b" " * 32
                 + b"9999999999`\n"]:
        (directory / "broken").write_bytes(data)
        safe_run("broken", directory, must_fail=True)


def main():
    for tool in ["gcc", "nm", "ar", "objcopy"]:
        assert shutil.which(tool), f"Required tool: {tool}"
    with tempfile.TemporaryDirectory(prefix="ft-nm-tests-") as temporary:
        directory = Path(temporary)
        (directory / "fixture.c").write_text(C_SOURCE)
        (directory / "types.s").write_text(ASM_SOURCE)
        (directory / "main.c").write_text("int z = 7; int main(void) { return z; }\n")
        matrix = []
        for section_type in ["progbits", "nobits", "note"]:
            for size in range(4):
                for combo in itertools.combinations("awx", size):
                    flags = "".join(combo)
                    name = f"{section_type}_{flags or 'none'}"
                    matrix += [f'.section .matrix_{name},"{flags}",@{section_type}',
                               f"local_{name}: .skip 4", f".globl global_{name}",
                               f"global_{name}: .skip 4"]
        (directory / "matrix.s").write_text("\n".join(matrix) + "\n")
        run(["gcc", "-c", "matrix.s", "-o", "matrix.o"], directory, True)
        run(["gcc", "-fcommon", "-g", "-fPIC", "-c", "fixture.c", "-o", "64.o"], directory, True)
        run(["gcc", "-m32", "-fcommon", "-g", "-fPIC", "-c", "fixture.c", "-o", "32.o"], directory, True)
        run(["gcc", "-c", "types.s", "-o", "types.o"], directory, True)
        run(["gcc", "-shared", "64.o", "-o", "64.so"], directory, True)
        run(["gcc", "-m32", "-nostdlib", "-shared", "32.o", "-o", "32.so"], directory, True)
        run(["gcc", "-g", "main.c", "-o", "a.out"], directory, True)
        run(["gcc", "-no-pie", "main.c", "-o", "exec64"], directory, True)
        run(["gcc", "-m32", "-nostdlib", "-no-pie", "-Wl,-e,entry", "32.o", "-o", "exec32",
             "-Wl,--unresolved-symbols=ignore-all"], directory, True)
        run(["objcopy", "--strip-all", "a.out", "stripped"], directory, True)
        long_name = "archive_member_with_a_long_filename.o"
        shutil.copy(directory / "64.o", directory / long_name)
        shutil.copy(directory / "32.o", directory / "space name.o")
        shutil.copy(directory / "64.o", directory / "-file.o")
        run(["ar", "rcs", "fixture.a", "64.o", "32.o", long_name, "space name.o"], directory, True)
        run(["ar", "rcs", "empty.a"], directory, True)
        files = ["64.o", "32.o", "types.o", "64.so", "32.so", "a.out",
                 "exec64", "exec32", "stripped", "fixture.a", "empty.a", "matrix.o"]
        options = [""] + ["-" + "".join(combo) for size in range(1, 6)
                          for combo in itertools.combinations("agurp", size)]
        for file in files:
            for option in options:
                compare(([option] if option else []) + [file], directory)
        compare([], directory)
        compare(["--", "-file.o"], directory)
        compare(["64.o", "32.o", "fixture.a", "stripped"], directory)
        compare(["64.o", "-a", "32.o"], directory)
        compare(["64.o", "missing", "32.o"], directory)
        compare(["missing-one", "64.o", "missing-two"], directory)
        compare(["empty.a", "stripped"], directory)
        safe_run("missing", directory, must_fail=True)
        safe_run(".", directory, must_fail=True)
        os.mkfifo(directory / "fifo")
        safe_run("fifo", directory, must_fail=True)
        invalid = run([BINARY, "-Z"], directory)
        assert invalid.returncode == 1 and invalid.stderr
        original = (directory / "64.o").read_bytes()
        run(["objcopy", "--change-section-address", ".data=0x1000",
             "types.o", "address.o"], directory, True)
        compare(["-a", "address.o"], directory)
        elf_variants(original, directory)
        malformed(original, directory)
        malformed((directory / "32.o").read_bytes(), directory)
    print(f"OK: {CHECKS} checks (GNU nm comparisons, ELF32/64, bonus, malformed inputs)")


if __name__ == "__main__":
    main()
