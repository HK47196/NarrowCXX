#!/usr/bin/env python3
"""Validate and display the source-linked coverage inventory."""
import argparse
from collections import Counter
import fnmatch
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def profile_checks(custom):
    profile = (ROOT / ".clang-tidy").read_text()
    patterns = re.search(r"Checks: >-\n(.*?)(?=^\S)", profile, re.M | re.S)[1]
    patterns = [item.strip() for item in patterns.split(",")]
    enabled = set()
    for pattern in patterns:
        if pattern == "-*":
            enabled.clear()
        elif pattern == "narrow-*":
            enabled.update(custom)
        elif pattern.startswith("-"):
            enabled = {check for check in enabled if not fnmatch.fnmatch(check, pattern[1:])}
        else:
            enabled.add(pattern)
    return enabled


def error_checks(enabled):
    profile = (ROOT / ".clang-tidy").read_text()
    patterns = re.search(r"WarningsAsErrors: >-\n(.*?)(?=^\S)", profile, re.M | re.S)[1]
    errors = set()
    for pattern in patterns.split(","):
        pattern = pattern.strip()
        negative = pattern.startswith("-")
        matches = {name for name in enabled if fnmatch.fnmatchcase(name, pattern[1:] if negative else pattern)}
        errors = errors - matches if negative else errors | matches
    return errors


def inventory_markdown(inventory, source, enabled, errors):
    blocks = source["blocks"]
    represented = {rule["source_block"] for rule in inventory["rules"]}
    ids = {rule["id"] for rule in inventory["rules"]}
    context = inventory["context_blocks"]
    lines = []
    lines.append("# Narrow C++ enforcement inventory\n")
    lines.append(inventory["note"] + "\n")
    lines.append(f"Source: [Narrow C++](../policy/narrow-cpp.md), draft 0.1, sequence {source['sequence']}. "
                 "Exact source block IDs are stored in [rules.json](../policy/rules.json).\n")
    lines.append(f"All {len(blocks)} blocks have a disposition: {len(represented)} substantive blocks map to "
                 f"{len(ids)} grouped items; {len(context)} metadata, heading, status, or example "
                 "blocks are recorded as context in the JSON.\n")
    lines.append("`syntax` checks a concrete form; `review-sites` flags a use whose qualifying purpose needs "
                 "human judgment; `mixed` combines these; `none` has no direct enforcement. "
                 "`fixture-tested` verifies only the linked examples, not every named API or semantic clause. "
                 "`implementation-only` has no dedicated fixture for this item. `not-configured`, "
                 "`not-implemented`, `partial-toolchain`, and `review-required` identify remaining work.\n")
    lines.append("Routes: `custom-partial` is an implemented subset, `upstream-partial` is upstream assistance, "
                 "`custom-candidate` is proposed work, `build` belongs partly to toolchain integration, "
                 "and `review` requires design judgment. Check annotations distinguish fixture evidence, "
                 "enabled checks without item-specific evidence, and disabled candidates. Task callback "
                 "model configuration is separate from enabling exception-escape.\n")
    lines.append(f"The combined profile promotes {len(errors)} ban checks to errors. Other enabled checks "
                 "remain warnings. Severity is shown beside each check; local suppressions still require "
                 "a valid policy reason. A consuming build must run clang-tidy as a required step.\n")
    lines.append("| Item / requirement | Route / assessment | Verification / checks | Evidence | Remaining work |")
    lines.append("| --- | --- | --- | --- | --- |")
    for rule in inventory["rules"]:
        checks = [f"`{check}` ({'error' if check in errors else 'warning' if check in enabled else 'disabled'}; "
                  f"{'tested' if check in rule['evidence'] else 'enabled' if check in enabled else 'candidate'})"
                  for check in rule["checks"]]
        fixtures = sorted({path for paths in rule["evidence"].values() for path in paths})
        links = ", ".join(f"[{Path(path).name}](../{path})" for path in fixtures) or "—"
        cells = [f"**{rule['id']}**: {rule['rule']}",
                 f"{rule['route']} / {rule['assessment']}",
                 rule["verification"] + "<br>" + (", ".join(checks) or "—"), links, rule["limitation"]]
        lines.append("| " + " | ".join(cell.replace("|", "\\|") for cell in cells) + " |")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--markdown", action="store_true", help="Print the generated inventory")
    output.add_argument("--write", action="store_true", help="Regenerate the policy and inventory Markdown")
    parser.add_argument("--plugin", type=Path, help="Also validate the loaded combined check registry")
    parser.add_argument("--clang-tidy", default="clang-tidy")
    args = parser.parse_args()
    inventory = json.loads((ROOT / "policy/rules.json").read_text())
    source = json.loads((ROOT / "policy/source.json").read_text())
    blocks = {block["id"]: block for block in source["blocks"]}
    custom = set()
    for path in (ROOT / "checks").glob("*.cpp"):
        custom.update(re.findall(r'registerCheck<[^>]+>\(\s*"([^"]+)"', path.read_text()))
    enabled = profile_checks(custom)
    errors = error_checks(enabled)
    ids = set()
    routes = {"review", "upstream-partial", "custom-candidate", "custom-partial", "build"}
    for block in blocks.values():
        if hashlib.sha256(block["markdown"].encode()).hexdigest() != block["hash"]:
            raise ValueError(f"source hash mismatch: {block['id']}")
    if inventory["schema_version"] != 2:
        raise ValueError("unsupported inventory schema")
    context = set(inventory["context_blocks"])
    if not context <= blocks.keys():
        raise ValueError("context block missing from source")
    represented, referenced, tested = set(), set(), set()
    for rule in inventory["rules"]:
        if rule["id"] in ids or rule["source_block"] not in blocks or rule["route"] not in routes:
            raise ValueError(f"invalid inventory row: {rule['id']}")
        ids.add(rule["id"])
        represented.add(rule["source_block"])
        referenced.update(rule["checks"])
        if rule["assessment"] not in {"syntax", "review-sites", "mixed", "none"}:
            raise ValueError(f"invalid assessment: {rule['id']}")
        statuses = {"fixture-tested", "implementation-only", "not-configured", "not-implemented",
                    "review-required", "partial-toolchain"}
        if rule["verification"] not in statuses:
            raise ValueError(f"invalid verification: {rule['id']}")
        if bool(rule["evidence"]) != (rule["verification"] == "fixture-tested"):
            raise ValueError(f"verification/evidence mismatch: {rule['id']}")
        for check, fixtures in rule["evidence"].items():
            if check not in rule["checks"] or check not in enabled or not fixtures:
                raise ValueError(f"invalid evidence check: {rule['id']}: {check}")
            negative = False
            for fixture in fixtures:
                path = ROOT / fixture
                if not fixture.startswith("tests/") or not path.is_file() or not path.resolve().is_relative_to(ROOT):
                    raise ValueError(f"missing/invalid fixture: {fixture}")
                for marker in re.findall(r"// expect(?:-error)?: ([a-zA-Z0-9_,.\- ]+)\s*$", path.read_text(), re.M):
                    negative |= check in {item.strip() for item in marker.split(",")}
            if not negative:
                raise ValueError(f"no diagnostic fixture for {rule['id']}: {check}")
            tested.add(check)
    if represented & context or represented | context != blocks.keys():
        raise ValueError(f"source disposition mismatch: {blocks.keys() - represented - context}")
    if {name for name in referenced if name.startswith("narrow-")} != custom:
        raise ValueError("custom registry and inventory references disagree")
    if not enabled <= referenced:
        raise ValueError("enabled check missing from inventory")
    if missing := enabled - tested:
        raise ValueError(f"enabled checks lack diagnostic evidence: {sorted(missing)}")
    if args.plugin:
        result = subprocess.run([args.clang_tidy, f"--load={args.plugin.resolve()}",
                                 f"--config-file={ROOT / '.clang-tidy'}", "--list-checks"],
                                capture_output=True, text=True, check=True)
        loaded = set(re.findall(r"^\s+([a-z][a-z0-9-]+)\s*$", result.stdout, re.M))
        if loaded != enabled:
            raise ValueError(f"loaded profile mismatch: missing {enabled - loaded}; extra {loaded - enabled}")
    views = {
        ROOT / "policy/narrow-cpp.md": "\n\n".join(block["markdown"] for block in source["blocks"]) + "\n",
        ROOT / "docs/rule-inventory.md": inventory_markdown(inventory, source, enabled, errors),
    }
    if args.write:
        for path, content in views.items():
            path.write_text(content)
    elif not args.markdown:
        stale = [str(path.relative_to(ROOT)) for path, content in views.items()
                 if not path.is_file() or path.read_text() != content]
        if stale:
            raise ValueError(f"stale generated views: {', '.join(stale)}; run python3 tools/inventory.py --write")
    if args.markdown:
        print(views[ROOT / "docs/rule-inventory.md"], end="")
    else:
        print(f"Validated {len(blocks)} source blocks and {len(ids)} policy items.")
        for route, count in sorted(Counter(rule["route"] for rule in inventory["rules"]).items()):
            print(f"  {route}: {count}")
        for status, count in sorted(Counter(rule["verification"] for rule in inventory["rules"]).items()):
            print(f"  {status}: {count}")
        print(f"Profile: {len(custom)} custom + {len(enabled - custom)} upstream checks; "
              f"{len(tested)} checks have item-specific diagnostic fixtures.")


if __name__ == "__main__":
    main()
