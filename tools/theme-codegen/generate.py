#!/usr/bin/env python3
"""Generate src/core/theme/theme_catalog_generated.inc.

Development-time tool only; the build never runs Python. The generated file is
committed so contributors do not need the reference repositories.

Sources:
  * MarkText v0.23.0 theme CSS + Prism CSS (MIT)
  * Microsoft VS Code 1.144.0 built-in theme JSON/JSONC (MIT)
  * GitHub Primer dark palettes (hand-authored below)

Usage (Git Bash / PowerShell):
  python -I tools/theme-codegen/generate.py \
      --marktext-root F:/3rd_source/tools/editor/markdown/marktext/src/v0.23.0/marktext \
      --vscode-root F:/3rd_source/tools/editor/vscode/src/1.144.0/vscode
  python -I tools/theme-codegen/generate.py ... --check
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Color helpers
# ---------------------------------------------------------------------------

NAMED_COLORS = {
    "black": (0, 0, 0, 255),
    "white": (255, 255, 255, 255),
    "slategray": (112, 128, 144, 255),
}


def parse_color(text):
    if text is None:
        return None
    text = text.strip().lower()
    if text in NAMED_COLORS:
        return NAMED_COLORS[text]
    if text.startswith("#"):
        value = text[1:]
        if len(value) in (3, 4):
            value = "".join(c * 2 for c in value)
        if len(value) == 6:
            return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16), 255)
        if len(value) == 8:
            return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16),
                    int(value[6:8], 16))
    match = re.match(
        r"rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)",
        text,
    )
    if match:
        r, g, b = (int(float(match.group(i))) for i in (1, 2, 3))
        a = 255
        if match.group(4) is not None:
            a = round(float(match.group(4)) * 255)
        return (r, g, b, max(0, min(255, a)))
    return None


def over(fg, bg):
    """Flatten fg over bg; returns an opaque color."""
    if fg is None:
        return bg
    alpha = fg[3] / 255.0
    return (
        round(fg[0] * alpha + bg[0] * (1 - alpha)),
        round(fg[1] * alpha + bg[1] * (1 - alpha)),
        round(fg[2] * alpha + bg[2] * (1 - alpha)),
        255,
    )


def opaque(color):
    return (color[0], color[1], color[2], 255)


def with_alpha(color, alpha):
    return (color[0], color[1], color[2], max(0, min(255, alpha)))


def blend(a, b, amount):
    """C++ Blend(): amount=0 -> a, amount=1 -> b."""
    return (
        round(a[0] * (1 - amount) + b[0] * amount),
        round(a[1] * (1 - amount) + b[1] * amount),
        round(a[2] * (1 - amount) + b[2] * amount),
        255,
    )


def luminance(color):
    def channel(value):
        c = value / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    return 0.2126 * channel(color[0]) + 0.7152 * channel(color[1]) + 0.0722 * channel(color[2])


def contrast(a, b):
    la, lb = luminance(a), luminance(b)
    lighter, darker = max(la, lb), min(la, lb)
    return (lighter + 0.05) / (darker + 0.05)


# ---------------------------------------------------------------------------
# MarkText parsing
# ---------------------------------------------------------------------------

CSS_VAR_RE = re.compile(r"--([A-Za-z0-9_-]+)\s*:\s*([^;]+);")


def parse_css_vars(path):
    text = Path(path).read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return {match.group(1): match.group(2).strip() for match in CSS_VAR_RE.finditer(text)}


def resolve_var(value, variables, seen=None):
    if value is None:
        return None
    value = value.strip()
    seen = seen or set()
    match = re.fullmatch(r"var\(\s*--([A-Za-z0-9_-]+)\s*\)", value)
    if match:
        name = match.group(1)
        if name in seen:
            return None
        seen.add(name)
        return resolve_var(variables.get(name), variables, seen)
    return value


def css_color(variables, name, fallback=None):
    resolved = resolve_var(variables.get(name), variables)
    color = parse_color(resolved)
    return color if color is not None else fallback


PRISM_SELECTORS = {
    "comment": [".token.comment", ".token.prolog", ".token.doctype", ".token.cdata"],
    "punctuation": [".token.punctuation"],
    "tag": [".token.tag", ".token.selector"],
    "property": [".token.property", ".token.constant", ".token.symbol", ".token.deleted"],
    "booleanNumber": [".token.boolean", ".token.number"],
    "string": [".token.string", ".token.char", ".token.builtin", ".token.inserted"],
    "oper": [".token.operator", ".token.entity", ".token.url"],
    "function": [".token.function", ".token.class-name", ".token.atrule"],
    "keyword": [".token.keyword"],
    "regex": [".token.regex", ".token.important", ".token.variable"],
}

RULE_RE = re.compile(r"([^{}]+)\{([^{}]*)\}")


def parse_prism(path):
    text = Path(path).read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    colors = {}
    for match in RULE_RE.finditer(text):
        selector_text, body = match.group(1), match.group(2)
        color_match = re.search(r"color\s*:\s*([^;]+);", body)
        if not color_match:
            continue
        color = parse_color(color_match.group(1))
        if color is None:
            continue
        for selector in selector_text.split(","):
            selector = re.sub(r"\s+", " ", selector.strip())
            if selector and selector not in colors:
                colors[selector] = color
    return colors


def prism_tokens(path, background, fallback):
    colors = parse_prism(path)
    tokens = {}
    for group, selectors in PRISM_SELECTORS.items():
        color = None
        for selector in selectors:
            if selector in colors:
                color = colors[selector]
                break
        tokens[group] = over(color, background) if color is not None else fallback
    return tokens


def marktext_spec(theme, marktext_root):
    if theme["source"] == "base":
        variables = parse_css_vars(marktext_root / "packages/desktop/src/renderer/src/assets/styles/index.css")
        prism_path = marktext_root / "packages/muya/src/assets/styles/prismjs/light.theme.css"
    else:
        name = theme["source"]
        variables = parse_css_vars(
            marktext_root / "packages/desktop/src/renderer/src/assets/themes" / f"{name}.theme.css"
        )
        prism_path = (
            marktext_root
            / "packages/desktop/src/renderer/src/assets/themes/prismjs"
            / f"{name}.theme.css"
        )
        if not prism_path.exists() and theme["dark"]:
            prism_path = (
                marktext_root
                / "packages/desktop/src/renderer/src/assets/themes/prismjs/dark.theme.css"
            )

    page_bg = css_color(variables, "editorBgColor", (255, 255, 255, 255))
    text = over(css_color(variables, "editorColor")
                or css_color(variables, "editorColor80"), page_bg)
    accent = over(css_color(variables, "themeColor", text), page_bg)
    code_bg = over(css_color(variables, "codeBlockBgColor", page_bg), page_bg)
    code_text = over(css_color(variables, "codeBlockColor", text), code_bg)

    def color(name, fallback):
        return over(css_color(variables, name, fallback), page_bg)

    tokens = prism_tokens(prism_path, code_bg, code_text)

    return {
        "pageBg": page_bg,
        "editorColor80": text,
        "editorColor50": css_color(variables, "editorColor50") or with_alpha(text, 128),
        "editorColor10": css_color(variables, "editorColor10") or with_alpha(text, 26),
        "editorColor04": css_color(variables, "editorColor04") or with_alpha(text, 10),
        "accent": accent,
        "danger": color("deleteColor", (255, 105, 105, 255)),
        "codeBlockBg": code_bg,
        "codeBlockColor": code_text,
        "tableBorder": color("tableBorderColor", blend(page_bg, text, 0.15)),
        "hr": color("hrColor", blend(page_bg, text, 0.15)),
        "link": color("linkColor", accent),
        "strong": color("strongColor", text),
        "em": color("emColor", text),
        "listMarker": color("listMarkerColor", text),
        "headings": [color(f"h{i}Color", text) for i in range(1, 7)],
        "blockquoteText": color("blockquoteTextColor", blend(page_bg, text, 0.35)),
        "blockquoteBorder": color("blockquoteBorderColor", accent),
        "sideBarBg": over(css_color(variables, "sideBarBgColor", page_bg), page_bg),
        "sideBarItemHover": over(
            css_color(variables, "sideBarItemHoverBgColor", with_alpha(text, 12)), page_bg
        ),
        "selectionColor": css_color(variables, "selectionColor") or with_alpha(accent, 64),
        "findMatchBg": css_color(variables, "highlightColor") or with_alpha(accent, 96),
        "findCurrentBg": with_alpha(accent, 150),
        "findText": text,
        "tokens": tokens,
    }


# ---------------------------------------------------------------------------
# VS Code parsing
# ---------------------------------------------------------------------------


def strip_jsonc(text):
    """Remove // and /* */ comments and trailing commas, respecting strings."""
    out = []
    i = 0
    n = len(text)
    in_string = False
    escaped = False
    while i < n:
        c = text[i]
        if in_string:
            out.append(c)
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == '"':
                in_string = False
            i += 1
            continue
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] not in "\r\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(c)
        i += 1
    return re.sub(r",(\s*[}\]])", r"\1", "".join(out))


def load_jsonc(path):
    return json.loads(strip_jsonc(Path(path).read_text(encoding="utf-8-sig")))


def resolve_vscode(path, seen=None):
    seen = seen or set()
    path = Path(path).resolve()
    if path in seen:
        raise RuntimeError(f"include cycle at {path}")
    seen.add(path)
    data = load_jsonc(path)
    colors = {}
    token_colors = []
    include = data.get("include")
    if include:
        includes = include if isinstance(include, list) else [include]
        for entry in includes:
            base = resolve_vscode(path.parent / entry, seen)
            colors.update(base["colors"])
            token_colors.extend(base["tokenColors"])
    colors.update(data.get("colors", {}))
    inherited = data.get("tokenColors")
    if isinstance(inherited, list):
        token_colors.extend(inherited)
    return {"colors": colors, "tokenColors": token_colors}


def vscode_color(colors, key, fallback=None):
    value = colors.get(key)
    return parse_color(value) if value else fallback


def scope_matches(scope, target):
    scope = scope.strip()
    if not scope:
        return 0
    if scope == target:
        return 1000 + len(target)
    if scope.startswith(target + "."):
        return 900 + len(target)
    if target.startswith(scope + "."):
        return 800 + len(scope)
    if target.split(".")[-1] in scope.split("."):
        return 700
    return 0


TOKEN_SCOPES = {
    "comment": ["comment"],
    "punctuation": ["punctuation"],
    "tag": ["entity.name.tag", "meta.tag"],
    "property": ["variable.other.property", "support.type.property-name", "entity.other.attribute-name"],
    "booleanNumber": ["constant.numeric", "constant.language.boolean"],
    "string": ["string"],
    "oper": ["keyword.operator"],
    "function": ["entity.name.function", "support.function"],
    "keyword": ["keyword", "storage.type", "storage.modifier"],
    "regex": ["string.regexp", "variable"],
}


def vscode_tokens(rules, background, fallback):
    tokens = {}
    for group, targets in TOKEN_SCOPES.items():
        best_score = 0
        best_color = None
        for rule in rules:
            settings = rule.get("settings") or {}
            color = parse_color(settings.get("foreground"))
            if color is None:
                continue
            scopes = rule.get("scope", "")
            if isinstance(scopes, str):
                scopes = [part.strip() for part in scopes.split(",")]
            for scope in scopes:
                for target in targets:
                    score = scope_matches(scope, target)
                    if score >= best_score:
                        best_score = score
                        best_color = color
        tokens[group] = over(best_color, background) if best_color is not None else fallback
    return tokens


def vscode_spec(theme, vscode_root):
    data = resolve_vscode(vscode_root / theme["source"])
    colors = data["colors"]
    is_dark = theme["dark"]
    is_hc = theme.get("hc", False)
    if is_hc:
        focus_default = "#F38518" if is_dark else "#006BBD"
        link_default = "#21A6FF" if is_dark else "#0F4A85"
        error_default = "#F48771" if is_dark else "#B5200D"
    else:
        focus_default = "#007FD4" if is_dark else "#0090F1"
        link_default = "#3794FF" if is_dark else "#006AB1"
        error_default = "#F48771" if is_dark else "#A1260D"
    defaults = {
        "editor.background": "#1E1E1E" if is_dark else "#FFFFFF",
        "editor.foreground": "#CCCCCC" if is_dark else "#333333",
        "focusBorder": focus_default,
        "errorForeground": error_default,
    }
    page_bg = vscode_color(colors, "editor.background") or parse_color(defaults["editor.background"])
    text = over(
        vscode_color(colors, "editor.foreground") or parse_color(defaults["editor.foreground"]),
        page_bg,
    )
    accent = over(
        vscode_color(colors, "focusBorder")
        or vscode_color(colors, "button.background")
        or vscode_color(colors, "textLink.foreground")
        or parse_color(defaults["focusBorder"]),
        page_bg,
    )
    code_bg = over(
        vscode_color(colors, "textCodeBlock.background")
        or vscode_color(colors, "editorWidget.background")
        or blend(page_bg, text, 0.06),
        page_bg,
    )
    code_text = over(
        vscode_color(colors, "textPreformat.foreground") or text, code_bg
    )
    border = over(
        vscode_color(colors, "textSeparator.foreground")
        or vscode_color(colors, "panel.border")
        or vscode_color(colors, "editorWidget.border")
        or blend(page_bg, text, 0.18),
        page_bg,
    )
    link = over(
        vscode_color(colors, "textLink.foreground") or parse_color(link_default), page_bg
    )
    side_bg = over(
        vscode_color(colors, "sideBar.background")
        or vscode_color(colors, "activityBar.background")
        or vscode_color(colors, "titleBar.activeBackground")
        or vscode_color(colors, "editorGroupHeader.tabsBackground")
        or blend(page_bg, text, 0.04),
        page_bg,
    )
    selection = (
        vscode_color(colors, "editor.selectionBackground")
        or vscode_color(colors, "selection.background")
        or with_alpha(accent, 64)
    )
    find_match = (
        vscode_color(colors, "editor.findMatchHighlightBackground")
        or vscode_color(colors, "editor.findMatchBackground")
        or (255, 224, 130, 255)
    )
    find_current = (
        vscode_color(colors, "editor.findMatchBackground")
        or (255, 183, 77, 255)
    )

    return {
        "pageBg": page_bg,
        "editorColor80": text,
        "editorColor50": with_alpha(text, 128),
        "editorColor10": with_alpha(text, 26),
        "editorColor04": with_alpha(text, 10),
        "accent": accent,
        "danger": over(
            vscode_color(colors, "errorForeground")
            or parse_color(defaults["errorForeground"]),
            page_bg,
        ),
        "codeBlockBg": code_bg,
        "codeBlockColor": code_text,
        "tableBorder": border,
        "hr": border,
        "link": link,
        "strong": text,
        "em": text,
        "listMarker": over(
            vscode_color(colors, "editorLineNumber.foreground") or accent, page_bg
        ),
        "headings": [text] * 6,
        "blockquoteText": blend(text, page_bg, 0.25),
        "blockquoteBorder": over(
            vscode_color(colors, "textBlockQuote.border") or accent, page_bg
        ),
        "sideBarBg": side_bg,
        "sideBarItemHover": over(
            vscode_color(colors, "list.hoverBackground")
            or vscode_color(colors, "list.inactiveSelectionBackground")
            or blend(side_bg, text, 0.08),
            page_bg,
        ),
        "selectionColor": selection,
        "findMatchBg": find_match,
        "findCurrentBg": find_current,
        "findText": over(
            vscode_color(colors, "editor.findMatchForeground") or text, page_bg
        ),
        "tokens": vscode_tokens(data["tokenColors"], code_bg, code_text),
    }


# ---------------------------------------------------------------------------
# GitHub Primer palettes (hand-authored)
# ---------------------------------------------------------------------------


def github_palette(dark, dimmed):
    if dimmed:
        return {
            "pageBg": (0x22, 0x27, 0x2E, 255),
            "editorColor80": (0xAD, 0xBA, 0xC7, 255),
            "editorColor50": (0x76, 0x83, 0x90, 255),
            "editorColor10": (0x44, 0x4C, 0x56, 255),
            "editorColor04": (0x2D, 0x33, 0x3B, 255),
            "accent": (0x53, 0x9B, 0xF5, 255),
            "danger": (0xE5, 0x53, 0x4B, 255),
            "codeBlockBg": (0x2D, 0x33, 0x3B, 255),
            "codeBlockColor": (0xAD, 0xBA, 0xC7, 255),
            "tableBorder": (0x44, 0x4C, 0x56, 255),
            "hr": (0x44, 0x4C, 0x56, 255),
            "link": (0x53, 0x9B, 0xF5, 255),
            "strong": (0xAD, 0xBA, 0xC7, 255),
            "em": (0xAD, 0xBA, 0xC7, 255),
            "listMarker": (0xAD, 0xBA, 0xC7, 255),
            "headings": [(0xAD, 0xBA, 0xC7, 255)] * 6,
            "blockquoteText": (0x76, 0x83, 0x90, 255),
            "blockquoteBorder": (0x44, 0x4C, 0x56, 255),
            "sideBarBg": (0x1C, 0x21, 0x28, 255),
            "sideBarItemHover": (0x37, 0x3E, 0x47, 255),
            "selectionColor": (0x44, 0x4C, 0x56, 255),
            "findMatchBg": (0xBB, 0x80, 0x09, 0x66),
            "findCurrentBg": (0xBB, 0x80, 0x09, 0xAA),
            "findText": (0xAD, 0xBA, 0xC7, 255),
            "tokens": {
                "comment": (0x76, 0x83, 0x90, 255),
                "punctuation": (0xAD, 0xBA, 0xC7, 255),
                "tag": (0x8D, 0xDB, 0x8C, 255),
                "property": (0x6C, 0xB6, 0xFF, 255),
                "booleanNumber": (0x6C, 0xB6, 0xFF, 255),
                "string": (0x96, 0xD0, 0xFF, 255),
                "oper": (0xF4, 0x70, 0x67, 255),
                "function": (0xDC, 0xBD, 0xFB, 255),
                "keyword": (0xF4, 0x70, 0x67, 255),
                "regex": (0x8D, 0xDB, 0x8C, 255),
            },
        }
    return {
        "pageBg": (0x0D, 0x11, 0x17, 255),
        "editorColor80": (0xE6, 0xED, 0xF3, 255),
        "editorColor50": (0x8B, 0x94, 0x9E, 255),
        "editorColor10": (0x30, 0x36, 0x3D, 255),
        "editorColor04": (0x16, 0x1B, 0x22, 255),
        "accent": (0x58, 0xA6, 0xFF, 255),
        "danger": (0xF8, 0x51, 0x49, 255),
        "codeBlockBg": (0x16, 0x1B, 0x22, 255),
        "codeBlockColor": (0xE6, 0xED, 0xF3, 255),
        "tableBorder": (0x30, 0x36, 0x3D, 255),
        "hr": (0x30, 0x36, 0x3D, 255),
        "link": (0x58, 0xA6, 0xFF, 255),
        "strong": (0xE6, 0xED, 0xF3, 255),
        "em": (0xE6, 0xED, 0xF3, 255),
        "listMarker": (0xE6, 0xED, 0xF3, 255),
        "headings": [(0xE6, 0xED, 0xF3, 255)] * 6,
        "blockquoteText": (0x8B, 0x94, 0x9E, 255),
        "blockquoteBorder": (0x30, 0x36, 0x3D, 255),
        "sideBarBg": (0x01, 0x04, 0x09, 255),
        "sideBarItemHover": (0x16, 0x1B, 0x22, 255),
        "selectionColor": (0x26, 0x4F, 0x78, 255),
        "findMatchBg": (0xBB, 0x80, 0x09, 0x66),
        "findCurrentBg": (0xBB, 0x80, 0x09, 0xAA),
        "findText": (0xE6, 0xED, 0xF3, 255),
        "tokens": {
            "comment": (0x8B, 0x94, 0x9E, 255),
            "punctuation": (0xE6, 0xED, 0xF3, 255),
            "tag": (0x7E, 0xE7, 0x87, 255),
            "property": (0x79, 0xC0, 0xFF, 255),
            "booleanNumber": (0x79, 0xC0, 0xFF, 255),
            "string": (0xA5, 0xD6, 0xFF, 255),
            "oper": (0xFF, 0x7B, 0x72, 255),
            "function": (0xD2, 0xA8, 0xFF, 255),
            "keyword": (0xFF, 0x7B, 0x72, 255),
            "regex": (0x7E, 0xE7, 0x87, 255),
        },
    }


# ---------------------------------------------------------------------------
# Theme metadata
# ---------------------------------------------------------------------------

# id, display name, dark, author, family, source kind, source path/name
THEMES = [
    # --- MarkText light ---
    ("cadmium-light", "Cadmium Light", False, "Ran Luo（MarkText）", "Green", "marktext", "base"),
    ("ayu-light", "Ayu Light", False, "Dempfi（Ayu）", "Blue", "marktext", "ayu-light"),
    ("catppuccin-latte", "Catppuccin Latte", False, "Catppuccin", "Blue", "marktext", "catppuccin-latte"),
    ("everforest-light", "Everforest Light", False, "Sainnhe（Everforest）", "Green", "marktext", "everforest-light"),
    ("graphite", "Graphite Light", False, "Ran Luo（MarkText）", "Blue", "marktext", "graphite"),
    ("gruvbox-light", "Gruvbox Light", False, "Pavel Pertsev（morhetz）", "Cyan", "marktext", "gruvbox-light"),
    ("rose-pine-dawn", "Rosé Pine Dawn", False, "Rosé Pine", "Purple", "marktext", "rose-pine-dawn"),
    ("tokyo-night-light", "Tokyo Night Light", False, "Enkia（Tokyo Night）", "Blue", "marktext", "tokyo-night-light"),
    # --- MarkText dark ---
    ("ayu-dark", "Ayu Dark", True, "Dempfi（Ayu）", "Cyan", "marktext", "ayu-dark"),
    ("ayu-mirage", "Ayu Mirage", True, "Dempfi（Ayu）", "Warm", "marktext", "ayu-mirage"),
    ("catppuccin-mocha", "Catppuccin Mocha", True, "Catppuccin", "Blue", "marktext", "catppuccin-mocha"),
    ("cyberdream", "Cyberdream", True, "Scott McKendry", "Multicolor", "marktext", "cyberdream"),
    ("cadmium-dark", "Cadmium Dark", True, "Ran Luo（MarkText）", "Blue", "marktext", "dark"),
    ("everforest-dark", "Everforest Dark", True, "Sainnhe（Everforest）", "Green", "marktext", "everforest-dark"),
    ("gruvbox-dark", "Gruvbox Dark", True, "Pavel Pertsev（morhetz）", "Cyan", "marktext", "gruvbox-dark"),
    ("horizon-dark", "Horizon Dark", True, "Jonathan Olaleye", "RedPink", "marktext", "horizon-dark"),
    ("kanagawa", "Kanagawa", True, "Rebelot（Kanagawa）", "Blue", "marktext", "kanagawa"),
    ("material-dark", "Material Dark", True, "Mattia Astorino（equinusocio）", "Warm", "marktext", "material-dark"),
    ("monokai-pro", "Monokai Pro", True, "Monokai（Wimer Hazenberg）", "Warm", "marktext", "monokai-pro"),
    ("nightfox", "Nightfox", True, "EdenEast", "Blue", "marktext", "nightfox"),
    ("oxocarbon-dark", "Oxocarbon Dark", True, "Nyoom Engineering（IBM Carbon）", "Blue", "marktext", "oxocarbon-dark"),
    ("palenight", "Palenight", True, "Mattia Astorino（equinusocio）", "Blue", "marktext", "palenight"),
    ("rose-pine", "Rosé Pine", True, "Rosé Pine", "Purple", "marktext", "rose-pine"),
    ("rose-pine-moon", "Rosé Pine Moon", True, "Rosé Pine", "Purple", "marktext", "rose-pine-moon"),
    ("solarized-dark", "Solarized Dark", True, "Ethan Schoonover", "Blue", "marktext", "solarized-dark"),
    ("synthwave-84", "Synthwave '84", True, "Robb Owen", "Multicolor", "marktext", "synthwave-84"),
    ("tokyo-night", "Tokyo Night", True, "Enkia（Tokyo Night）", "Blue", "marktext", "tokyo-night"),
    ("tokyo-night-storm", "Tokyo Night Storm", True, "Enkia（Tokyo Night）", "Blue", "marktext", "tokyo-night-storm"),
    # --- VS Code ---
    ("vscode-2026-dark", "VS Code Dark 2026", True, "Microsoft", "Cyan", "vscode", "extensions/theme-defaults/themes/2026-dark.json"),
    ("vscode-2026-light", "VS Code Light 2026", False, "Microsoft", "Blue", "vscode", "extensions/theme-defaults/themes/2026-light.json"),
    ("vscode-dark-plus", "VS Code Dark+", True, "Microsoft", "Blue", "vscode", "extensions/theme-defaults/themes/dark_plus.json"),
    ("vscode-light-plus", "VS Code Light+", False, "Microsoft", "Blue", "vscode", "extensions/theme-defaults/themes/light_plus.json"),
    ("vscode-dark-modern", "VS Code Dark Modern", True, "Microsoft", "Blue", "vscode", "extensions/theme-defaults/themes/dark_modern.json"),
    ("vscode-light-modern", "VS Code Light Modern", False, "Microsoft", "Blue", "vscode", "extensions/theme-defaults/themes/light_modern.json"),
    ("vscode-dark-vs", "VS Code Dark (Visual Studio)", True, "Microsoft", "Neutral", "vscode", "extensions/theme-defaults/themes/dark_vs.json"),
    ("vscode-light-vs", "VS Code Light (Visual Studio)", False, "Microsoft", "Neutral", "vscode", "extensions/theme-defaults/themes/light_vs.json"),
    ("vscode-high-contrast-black", "VS Code Dark High Contrast", True, "Microsoft", "Neutral", "vscode", "extensions/theme-defaults/themes/hc_black.json"),
    ("vscode-high-contrast-light", "VS Code Light High Contrast", False, "Microsoft", "Neutral", "vscode", "extensions/theme-defaults/themes/hc_light.json"),
    ("vscode-abyss", "Abyss", True, "Ofer Reichman", "Blue", "vscode", "extensions/theme-abyss/themes/abyss-color-theme.json"),
    ("vscode-kimbie-dark", "Kimbie Dark", True, "Jan T. Sott", "RedPink", "vscode", "extensions/theme-kimbie-dark/themes/kimbie-dark-color-theme.json"),
    ("vscode-monokai", "VS Code Monokai", True, "Monokai（Wimer Hazenberg）", "RedPink", "vscode", "extensions/theme-monokai/themes/monokai-color-theme.json"),
    ("vscode-monokai-dimmed", "VS Code Monokai Dimmed", True, "Monokai（Wimer Hazenberg）", "RedPink", "vscode", "extensions/theme-monokai-dimmed/themes/dimmed-monokai-color-theme.json"),
    ("vscode-quiet-light", "Quiet Light", False, "Ian Beck（One Crayon）", "Purple", "vscode", "extensions/theme-quietlight/themes/quietlight-color-theme.json"),
    ("vscode-red", "Red", True, "1ClickComputing", "RedPink", "vscode", "extensions/theme-red/themes/Red-color-theme.json"),
    ("vscode-tomorrow-night-blue", "Tomorrow Night Blue", True, "Chris Kempson", "Blue", "vscode", "extensions/theme-tomorrow-night-blue/themes/tomorrow-night-blue-color-theme.json"),
    # --- GitHub Primer ---
    ("github-dark", "GitHub Dark", True, "GitHub", "Blue", "github", None),
    ("github-dark-dimmed", "GitHub Dark Dimmed", True, "GitHub", "Blue", "github-dimmed", None),
]


# ---------------------------------------------------------------------------
# C++ emission
# ---------------------------------------------------------------------------


def cpp_color(color):
    r, g, b, a = color
    if a == 255:
        return f"Rgb({r}, {g}, {b})"
    return f"Rgba({r}, {g}, {b}, {a})"


def cpp_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def function_name(theme_id):
    parts = re.split(r"[^A-Za-z0-9]+", theme_id)
    return "Generated" + "".join(part[:1].upper() + part[1:] for part in parts if part)


def emit_spec(theme_id, name, dark, author, family, spec):
    lines = [
        f"ThemeDefinition {function_name(theme_id)}()",
        "{",
        "    ThemeSpec spec{};",
        f"    spec.id = {cpp_string(theme_id)};",
        f"    spec.name = {cpp_string(name)};",
        f"    spec.dark = {'true' if dark else 'false'};",
        f"    spec.author = {cpp_string(author)};",
        f"    spec.family = ThemeFamily::{family};",
    ]
    fields = [
        ("pageBg", "pageBg"),
        ("editorColor80", "editorColor80"),
        ("editorColor50", "editorColor50"),
        ("editorColor10", "editorColor10"),
        ("editorColor04", "editorColor04"),
        ("accent", "accent"),
        ("danger", "danger"),
        ("codeBlockBg", "codeBlockBg"),
        ("codeBlockColor", "codeBlockColor"),
        ("tableBorder", "tableBorder"),
        ("hr", "hr"),
        ("link", "link"),
        ("strong", "strong"),
        ("em", "em"),
        ("listMarker", "listMarker"),
    ]
    for field, key in fields:
        lines.append(f"    spec.{field} = {cpp_color(spec[key])};")
    for index in range(6):
        lines.append(f"    spec.headings[{index}] = {cpp_color(spec['headings'][index])};")
    for field in ["blockquoteText", "blockquoteBorder", "sideBarBg", "sideBarItemHover",
                  "selectionColor", "findMatchBg", "findCurrentBg", "findText"]:
        lines.append(f"    spec.{field} = {cpp_color(spec[field])};")
    for token in ["comment", "punctuation", "tag", "property", "booleanNumber", "string",
                  "oper", "function", "keyword", "regex"]:
        lines.append(f"    spec.tokens.{token} = {cpp_color(spec['tokens'][token])};")
    lines += [
        "    return BuildTheme(spec);",
        "}",
        "",
    ]
    return "\n".join(lines)


def generate(marktext_root, vscode_root):
    blocks = [
        "// AUTO-GENERATED by tools/theme-codegen/generate.py — do not edit by hand.",
        "// Sources: MarkText v0.23.0 (MIT), Microsoft VS Code 1.144.0 (MIT),",
        "//          GitHub Primer palettes (hand-authored from GitHub's design system).",
        "// clang-format off",
        "",
    ]
    for theme_id, name, dark, author, family, kind, source in THEMES:
        if kind == "marktext":
            spec = marktext_spec({"source": source, "dark": dark}, marktext_root)
        elif kind == "vscode":
            spec = vscode_spec(
                {"source": source, "dark": dark, "hc": "high-contrast" in theme_id},
                vscode_root,
            )
        elif kind == "github":
            spec = github_palette(True, False)
        elif kind == "github-dimmed":
            spec = github_palette(True, True)
        else:
            raise ValueError(f"unknown source kind {kind}")
        blocks.append(emit_spec(theme_id, name, dark, author, family, spec))

    blocks.append("void AppendGeneratedThemes(std::vector<ThemeDefinition>& themes)")
    blocks.append("{")
    for theme_id, *_ in THEMES:
        blocks.append(f"    themes.push_back({function_name(theme_id)}());")
    blocks.append("}")
    blocks.append("")
    blocks.append("// clang-format on")
    blocks.append("")
    return "\n".join(blocks)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--marktext-root",
        default=os.environ.get(
            "AWM_MARKTEXT_ROOT",
            r"F:/3rd_source/tools/editor/markdown/marktext/src/v0.23.0/marktext",
        ),
    )
    parser.add_argument(
        "--vscode-root",
        default=os.environ.get(
            "AWM_VSCODE_ROOT",
            r"F:/3rd_source/tools/editor/vscode/src/1.144.0/vscode",
        ),
    )
    parser.add_argument(
        "--output",
        default=str(
            Path(__file__).resolve().parents[2]
            / "src/core/theme/theme_catalog_generated.inc"
        ),
    )
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    marktext_root = Path(args.marktext_root)
    vscode_root = Path(args.vscode_root)
    if not marktext_root.is_dir():
        sys.exit(f"MarkText root not found: {marktext_root}")
    if not vscode_root.is_dir():
        sys.exit(f"VS Code root not found: {vscode_root}")

    output = Path(args.output)
    text = generate(marktext_root, vscode_root)
    if args.check:
        existing = output.read_text(encoding="utf-8") if output.exists() else ""
        if existing != text:
            sys.exit(f"Generated file is out of date: {output}")
        print(f"{output} is up to date")
        return
    output.write_text(text, encoding="utf-8")
    print(f"Wrote {output} ({len(THEMES)} themes)")


if __name__ == "__main__":
    main()
