"""Minimal YAML subset loader: nested maps, integers, and strings."""


def _is_int(val):
    if val[:1] == "-" and len(val) > 1:
        return val[1:].isdigit()
    return val.isdigit()


def load_yaml(text):
    root = {}
    stack = [(-1, root)]
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip(" "))
        body = line.strip()
        if ":" not in body:
            raise ValueError("bad yaml")
        key, _, val = body.partition(":")
        key = key.strip()
        val = val.strip()
        while len(stack) > 1 and indent <= stack[-1][0]:
            stack.pop()
        parent = stack[-1][1]
        if val == "":
            child = {}
            parent[key] = child
            stack.append((indent, child))
        elif _is_int(val):
            parent[key] = int(val)
        else:
            parent[key] = val
    return root


def load_file(path):
    with open(path, "r", encoding="utf-8") as handle:
        return load_yaml(handle.read())


def get(tree, path):
    cur = tree
    for part in path.split("."):
        if not isinstance(cur, dict) or part not in cur:
            raise KeyError(path)
        cur = cur[part]
    return cur


def get_int(tree, path):
    val = get(tree, path)
    if not isinstance(val, int):
        raise TypeError(path)
    return val


def get_str(tree, path):
    val = get(tree, path)
    if not isinstance(val, str):
        raise TypeError(path)
    return val
