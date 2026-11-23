import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "Source" / "Aetheria"


def has_non_ascii(text: str) -> bool:
    return any(ord(ch) > 127 for ch in text)


def ensure_utf8_bom_for_cyrillic(path: Path) -> bool:
    raw = path.read_bytes()
    text = raw.decode("utf-8-sig")
    if has_non_ascii(text) and not raw.startswith(b"\xef\xbb\xbf"):
        path.write_bytes(b"\xef\xbb\xbf" + text.encode("utf-8"))
        return True
    return False


def strip_comments_and_strings(code: str) -> str:
    # Remove block comments, line comments, and string literals for brace/paren checks
    pattern = re.compile(
        r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
        re.MULTILINE,
    )
    return pattern.sub(" ", code)


def check_balanced(text: str, open_ch: str, close_ch: str) -> bool:
    depth = 0
    for ch in text:
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth < 0:
                return False
    return depth == 0


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []
    bom_fixed: list[str] = []

    # 1. Validate .uproject JSON
    uproject = ROOT / "Aetheria.uproject"
    if not uproject.exists():
        errors.append("Missing Aetheria.uproject")
    else:
        try:
            data = json.loads(uproject.read_text(encoding="utf-8-sig"))
            if data.get("FileVersion") != 3 or not data.get("Modules"):
                errors.append("Invalid Aetheria.uproject schema")
        except Exception as exc:
            errors.append(f"Invalid Aetheria.uproject JSON: {exc}")

    # 2. Validate .Target.cs and .Build.cs
    for cs_rel in [
        "Source/Aetheria.Target.cs",
        "Source/AetheriaEditor.Target.cs",
        "Source/Aetheria/Aetheria.Build.cs",
    ]:
        if not (ROOT / cs_rel).exists():
            errors.append(f"Missing {cs_rel}")

    # 3. Collect all headers and cpp files
    headers = sorted((SRC_DIR / "Public").glob("*.h"))
    cpps = sorted((SRC_DIR / "Private").glob("*.cpp"))
    header_names = {h.name for h in headers}

    if not headers:
        errors.append("No headers found in Source/Aetheria/Public")
    if not cpps:
        errors.append("No cpp files found in Source/Aetheria/Private")

    # 4. Validate headers
    for h in headers:
        if ensure_utf8_bom_for_cyrillic(h):
            bom_fixed.append(str(h.relative_to(ROOT)))

        text = h.read_text(encoding="utf-8-sig")
        if "#pragma once" not in text:
            errors.append(f"{h.name}: missing #pragma once")

        has_uht = bool(re.search(r"\b(UCLASS|USTRUCT|UENUM|UINTERFACE)\s*\(", text))
        includes = re.findall(r'^\s*#include\s+"([^"]+)"', text, flags=re.MULTILINE)
        gen_name = f"{h.stem}.generated.h"
        if has_uht:
            if gen_name not in includes:
                errors.append(f"{h.name}: uses UHT macro but missing #include \"{gen_name}\"")
            elif includes[-1] != gen_name:
                errors.append(f"{h.name}: #include \"{gen_name}\" MUST be the last #include (found {includes[-1]})")

        stripped = strip_comments_and_strings(text)
        if not check_balanced(stripped, "{", "}"):
            errors.append(f"{h.name}: unbalanced curly braces {{ }}")
        if not check_balanced(stripped, "(", ")"):
            errors.append(f"{h.name}: unbalanced parentheses ( )")

        # Check matching cpp exists (except pure types header)
        if h.name != "AetheriaTypes.h":
            matching_cpp = SRC_DIR / "Private" / f"{h.stem}.cpp"
            if not matching_cpp.exists():
                errors.append(f"{h.name}: missing matching Private/{h.stem}.cpp")

        # Check replicated properties and RPC declarations against matching cpp
        rep_props = re.findall(
            r"UPROPERTY\s*\([^)]*\bReplicated(?:Using\s*=\s*\w+)?\b[^)]*\)\s*[\r\n]+\s*(?:[^\n;]+?)\s+(\w+)\s*(?:=[^;]+)?;",
            text,
        )
        rpc_decls = re.findall(
            r"UFUNCTION\s*\([^)]*\b(?:Server|Client|NetMulticast)\b[^)]*\)\s*[\r\n]+\s*(?:virtual\s+)?void\s+(\w+)\s*\(",
            text,
        )

        cpp_path = SRC_DIR / "Private" / f"{h.stem}.cpp"
        if cpp_path.exists():
            cpp_text = cpp_path.read_text(encoding="utf-8-sig")
            for prop in rep_props:
                if f"DOREPLIFETIME" not in cpp_text or not re.search(rf"DOREPLIFETIME\w*\s*\([^,]+,\s*{prop}\s*\)", cpp_text):
                    errors.append(f"{h.name} / {cpp_path.name}: Replicated property '{prop}' missing DOREPLIFETIME registration")
            for rpc in rpc_decls:
                if f"{rpc}_Implementation" not in cpp_text:
                    errors.append(f"{h.name} / {cpp_path.name}: RPC '{rpc}' missing '{rpc}_Implementation'")

    # 5. Validate all .cpp files
    for c in cpps:
        if ensure_utf8_bom_for_cyrillic(c):
            bom_fixed.append(str(c.relative_to(ROOT)))

        text = c.read_text(encoding="utf-8-sig")
        stripped = strip_comments_and_strings(text)
        if not check_balanced(stripped, "{", "}"):
            errors.append(f"{c.name}: unbalanced curly braces {{ }}")
        if not check_balanced(stripped, "(", ")"):
            errors.append(f"{c.name}: unbalanced parentheses ( )")

        # Verify local Aetheria includes exist
        includes = re.findall(r'^\s*#include\s+"([^"]+)"', text, flags=re.MULTILINE)
        for inc in includes:
            if inc.startswith("Aetheria") and inc.endswith(".h") and inc not in header_names:
                errors.append(f"{c.name}: includes missing local header \"{inc}\"")

    # 6. Verify Config files point to valid GameMode & Entry map
    engine_ini = (ROOT / "Config" / "DefaultEngine.ini").read_text(encoding="utf-8-sig")
    if "/Script/Aetheria.AetheriaGameMode" not in engine_ini:
        errors.append("DefaultEngine.ini missing GlobalDefaultGameMode=/Script/Aetheria.AetheriaGameMode")
    if "/Engine/Maps/Entry" not in engine_ini:
        warnings.append("DefaultEngine.ini does not use /Engine/Maps/Entry")

    print(f"Checked {len(headers)} headers and {len(cpps)} cpp files.")
    if bom_fixed:
        print(f"Added UTF-8 BOM for MSVC Cyrillic safety on {len(bom_fixed)} file(s): {', '.join(bom_fixed)}")
    if warnings:
        for w in warnings:
            print(f"[WARN] {w}")
    if errors:
        for e in errors:
            print(f"[ERROR] {e}")
        return 1

    print("SUCCESS: 0 UE5 C++/UHT/config errors found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
