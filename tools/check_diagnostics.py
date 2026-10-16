#!/usr/bin/env python3
"""Verify policy diagnostic names, locations, severity, and process exit status."""
import argparse
from collections import Counter
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXPECT = re.compile(r"// expect(?P<error>-error)?: (?P<checks>[a-zA-Z0-9_,.\- ]+)\s*$")
DIAGNOSTIC = re.compile(r"^(.+?):(\d+):\d+: (warning|error|fatal error): .*? \[([^\]]+)\]$", re.M)
LOCAL_INCLUDE = re.compile(r'^\s*#include "([^"]+)"', re.M)


def expected_diagnostics(source, seen=None):
    seen = set() if seen is None else seen
    source = source.resolve()
    if source in seen:
        return Counter()
    seen.add(source)
    content = source.read_text()
    expected = Counter()
    for line, text in enumerate(content.splitlines(), 1):
        if match := EXPECT.search(text):
            severity = "error" if match["error"] else "warning"
            for check in match["checks"].split(","):
                expected[(str(source), line, check.strip(), severity)] += 1
    for include in LOCAL_INCLUDE.findall(content):
        expected.update(expected_diagnostics(source.parent / include, seen))
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-tidy", default="clang-tidy")
    parser.add_argument("--suite", choices=("custom", "integrated", "modules"), required=True)
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--module-compiler", default="clang++")
    args = parser.parse_args()
    if not args.plugin.is_file():
        parser.error(f"the {args.suite} suite requires an existing --plugin")
    sources = sorted((ROOT / "tests" / args.suite).glob("*.cpp"))
    module_flags = []
    if args.suite == "modules":
        module_dir = args.plugin.resolve().parent / "test-modules"
        module_dir.mkdir(exist_ok=True)
        interface = ROOT / "tests/modules/narrow_api.cppm"
        subprocess.run([args.module_compiler, "-std=c++2c", "--precompile", str(interface),
                        "-o", str(module_dir / "narrow_api.pcm")], check=True, cwd=ROOT)
        sources = [interface, *sources]
        module_flags = [f"-fprebuilt-module-path={module_dir}"]
    if not sources:
        parser.error("no fixtures found")
    failures = 0
    for source in sources:
        command = [args.clang_tidy, str(source), "--quiet", f"--load={args.plugin.resolve()}"]
        # The integrated suite exercises discovery of the root profile.
        if args.suite in ("custom", "modules"):
            command += ['--config={Checks: "-*,narrow-*", InheritParentConfig: false, HeaderFilterRegex: ".*"}']
        command += ["--", "-std=c++2c", "-fexceptions", "-frtti",
                    "-Wno-vla-extension", "-Wno-gnu-statement-expression", "-Wno-gnu-label-as-value"]
        for flags in re.findall(r"^// compile-flags: (.+)$", source.read_text(), re.M):
            command += shlex.split(flags)
        command += module_flags
        result = subprocess.run(command, capture_output=True, text=True, cwd=ROOT)
        output = result.stdout + result.stderr
        actual = Counter()
        errors = []
        parsed_error_lines = 0
        for path, line, severity, checks in DIAGNOSTIC.findall(output):
            parsed_error_lines += severity != "warning"
            for check in checks.split(","):
                check = check.strip()
                if check in {"warnings-as-errors", "-warnings-as-errors"}:
                    continue
                actual[(str(Path(path).resolve()), int(line), check, severity)] += 1
                if severity != "warning" and check.startswith("clang-diagnostic-"):
                    errors.append(check)
        raw_errors = len(re.findall(r"^.+?:\d+:\d+: (?:fatal )?error:", output, re.M))
        if raw_errors != parsed_error_lines or re.search(r"^(?:fatal )?error:", output, re.M):
            errors.append("unclassified error")
        expected = expected_diagnostics(source)
        missing, unexpected = expected - actual, actual - expected
        # A compiler failure is never accepted as evidence of policy enforcement.
        expected_status = 1 if any(key[3] == "error" for key in expected) else 0
        if result.returncode != expected_status or errors or missing or unexpected:
            failures += 1
            print(f"FAIL {source.relative_to(ROOT)}")
            for label, diagnostics in (("missing", missing), ("unexpected", unexpected)):
                for (path, line, check, severity), count in diagnostics.items():
                    print(f"  {label}: {path}:{line}: {severity}: {check} (x{count})")
            if result.returncode != expected_status:
                print(f"  exit status: expected {expected_status}, got {result.returncode}")
            print(output)
        else:
            print(f"PASS {source.relative_to(ROOT)} ({sum(actual.values())} expected diagnostics)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
