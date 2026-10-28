"""Indent-sensitive YAML subset reader for limits.yaml.

The file is also valid YAML. This reader exists so the C++ loader can
follow the same rules without a third-party dependency.
"""

from __future__ import annotations


def _strip_comment(line: str) -> str:
    in_single = False
    in_double = False
    for index, char in enumerate(line):
        if char == "'" and not in_double:
            in_single = not in_single
        elif char == '"' and not in_single:
            in_double = not in_double
        elif char == "#" and not in_single and not in_double:
            return line[:index]
    return line


def parse_scalar(token: str):
    token = token.strip()
    if len(token) >= 2 and token[0] == token[-1] and token[0] in ("'", '"'):
        return token[1:-1]
    lowered = token.lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    if lowered in ("null", "~"):
        return None
    if token.startswith("[") and token.endswith("]"):
        inner = token[1:-1].strip()
        if not inner:
            return []
        return [parse_scalar(part.strip()) for part in inner.split(",")]
    try:
        if any(mark in token for mark in (".", "e", "E")):
            return float(token)
        return int(token)
    except ValueError:
        return token


def _split_key(text: str) -> tuple[str, str]:
    colon = text.find(":")
    if colon < 0:
        raise ValueError("expected key: value, got " + text)
    return text[:colon].strip(), text[colon + 1 :].strip()


def _prepare(text: str) -> list[tuple[int, str]]:
    lines: list[tuple[int, str]] = []
    for raw in text.splitlines():
        cleaned = _strip_comment(raw).rstrip()
        if not cleaned.strip():
            continue
        if "\t" in cleaned[: len(cleaned) - len(cleaned.lstrip(" \t"))]:
            raise ValueError("tabs are not allowed in limits yaml")
        indent = len(cleaned) - len(cleaned.lstrip(" "))
        lines.append((indent, cleaned.strip()))
    return lines


def parse_yaml(text: str):
    lines = _prepare(text)
    if not lines:
        return None
    value, index = _parse_block(lines, 0)
    if index != len(lines):
        raise ValueError("trailing yaml near " + lines[index][1])
    return value


def _parse_block(lines: list[tuple[int, str]], index: int):
    if index >= len(lines):
        return {}, index
    indent, text = lines[index]
    if text.startswith("- "):
        return _parse_seq(lines, index, indent)
    return _parse_map(lines, index, indent)


def _parse_map(lines, index: int, indent: int):
    result = {}
    total = len(lines)
    while index < total and lines[index][0] == indent and not lines[index][1].startswith("- "):
        key, rest = _split_key(lines[index][1])
        index += 1
        if rest == "":
            if index < total and lines[index][0] > indent:
                child, index = _parse_block(lines, index)
            else:
                child = {}
            result[key] = child
        else:
            result[key] = parse_scalar(rest)
    return result, index


def _parse_seq(lines, index: int, indent: int):
    items = []
    total = len(lines)
    while index < total and lines[index][0] == indent and lines[index][1].startswith("- "):
        content = lines[index][1][2:].strip()
        index += 1
        if content == "":
            if index < total and lines[index][0] > indent:
                child, index = _parse_block(lines, index)
            else:
                child = None
            items.append(child)
            continue
        if ":" in content and not content.startswith(("'", '"', "[")):
            key, rest = _split_key(content)
            item = {}
            if rest == "":
                if index < total and lines[index][0] > indent:
                    child, index = _parse_block(lines, index)
                else:
                    child = {}
                item[key] = child
            else:
                item[key] = parse_scalar(rest)
            item_indent = None
            while index < total and not lines[index][1].startswith("- "):
                if item_indent is None:
                    if lines[index][0] <= indent:
                        break
                    item_indent = lines[index][0]
                if lines[index][0] != item_indent:
                    break
                key2, rest2 = _split_key(lines[index][1])
                index += 1
                if rest2 == "":
                    if index < total and lines[index][0] > item_indent:
                        child, index = _parse_block(lines, index)
                    else:
                        child = {}
                    item[key2] = child
                else:
                    item[key2] = parse_scalar(rest2)
            items.append(item)
            continue
        items.append(parse_scalar(content))
    return items, index
