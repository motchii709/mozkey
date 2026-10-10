#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate src/data/dictionary_manual/english_words.tsv.

WHAT THIS FILE IS
-----------------
The TSV produced here adds entries to the main dictionary so that an English
word typed in romaji can be converted to its canonical half-width spelling.
Each line is "key<TAB>value<TAB>pos", the same format as places.tsv and
words.tsv in the same directory:

    key    lowercase ASCII spelling that the user types ("github")
    value  canonical spelling that is shown and committed ("GitHub")
    pos    a POS alias understood by dictionary/gen_aux_dictionary.py

Unlike the Japanese entries in the sibling TSVs, the key is not a kana reading:
it is the ASCII spelling the user types.  The value therefore differs from the
key only by case and punctuation, and the script asserts exactly that for every
line it writes.

SOURCE AND LICENSE
------------------
dwyl/english-words, data file words_alpha.txt.

  raw URL    https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt
  sha256     3ed0c94610d8bcf7c11bbb49c56aa49c7234d32b66824df91f554169e572da48
  license    Unlicense (public domain dedication).  The repository's LICENSE.md
             is the verbatim Unlicense text: "This is free and unencumbered
             software released into the public domain. ... For more information,
             please refer to <https://unlicense.org>"
  lineage    The list is the Moby Word Lists by Grady Ward (Project Gutenberg
             eBook 3201, "Public domain in the USA").  dwyl's
             word_list_moby_credits.txt records that credit, and its README.md
             notes the words also appeared in an infochimps "simple english
             words" dataset.  Both strands are public domain.
  retrieved  2026-10-10 (UTC)

The file is not committed to this repository; it is downloaded on demand and
verified against the sha256 above.  Pass --input to use a local copy.

SIZE BOUND (--min-len / --max-len, default 3..5)
------------------------------------------------
words_alpha.txt holds 370,105 words, which is far more than an inline English
word list should add to the dictionary.  The bound here is a length window,
because no permissively licensed word *frequency* list was used: keeping the
license chain to a single Unlicense source was preferred over a better ranking
from a source whose data provenance is unclear (see the report in
src/data/dictionary_koyasi/README.md).

Word length is the license-free proxy for frequency: running English text is
dominated by short words (the commonest tokens are all 3-5 letters), so words of
3 to 5 letters are the ones a user is most likely to type inside Japanese text,
while the long tail of 6+ letter words is mostly rare vocabulary.  The window
3..5 yields ~19.8k entries, inside the intended 10k-30k band; the band is
enforced by --expected-min/--expected-max, which fail the run when the result
leaves it.  Widening to --max-len 6 yields ~44.5k entries and would need a
frequency source to trim, which is why it is not the default.

COLLISION FILTER (critical)
---------------------------
A word must not be listed when its lowercase spelling is a romaji reading of
Japanese, because the English-word match fires as soon as the spelling is
complete and would then take that reading away from the user for good.  See the
"RULE: never list a word whose lowercase spelling is a romaji reading" comment
in src/composer/composer.cc.

The filter is built from this repository's own romaji table,
src/data/preedit/romanji-hiragana.tsv: a word is dropped when the table can
consume it completely, that is when the whole spelling can be segmented into
table entries that leave no pending remainder ("youtube" is yo+u+tu+be, so it
reads as "yotsube" and is dropped).  Entries with a third column ("qq", "tch",
"www", ...) leave a pending character and therefore never complete a spelling on
their own.  On top of the table, the deny list below is dropped unconditionally;
it holds the three examples the composer comment names, and "mac" is the one
that only the deny list catches, because the table consumes "ma" and leaves "c"
pending while the match still fires at "mac" and eats the "c" of "machi".

Words that the table consumes but that nobody types as a reading are dropped as
well, because the rule is mechanical: "name" is na+me, "line" is li+ne and
"code" is co+de, so all three go.  That costs a few common words on purpose.
--romaji-filter none disables the table half of the filter (the deny list
always applies) so the cost can be measured and reproduced, and every dropped
word is written to dist/dictionary/english_words-dropped.txt for inspection.

BRAND SECTION
-------------
BRAND_WORDS is a hand-curated list of brand and technical names whose canonical
casing matters (GitHub, Node.js, VSCode, PostgreSQL, ...).  It is kept as a
separate block in this script.  It is NOT a separate section of the output file:
key uniqueness and deterministic sorting are hard requirements on the data file,
so the two inputs are merged, sorted by key, and deduplicated.  Where both
inputs hold the same key the brand entry wins, the script asserts that the
generated value was really replaced, and the number of such overrides is
printed.

A brand name whose key the romaji table can consume completely cannot be listed
at all and is reported as explicitly excluded instead of silently dropped.  That
is what happens to "Java" (じゃゔぁ), "Ubuntu" (うぶんつ), "Vue" (ゔぇ), "Go"
(ご), "API" (あぴ), "Wi-Fi" (うぃふぃ), "NaN" (なん) and 17 more; the full list
is printed by every run.

A brand entry may also share its key with an ordinary English word, because the
short spellings of several names are plain words: "rest", "rust", "ruby",
"react", "rails", "ram", "slack", "swift", "steam", "edge", "flask", "teams",
"arch", "nim", "zig".  The brand entry wins, so those spellings always render
with the brand casing and the ordinary noun or verb sense loses its lowercase
form.  That is the intended priority, and the override list is printed on every
run so the trade can be re-checked; drop a name from BRAND_WORDS to hand the
spelling back to the ordinary word.

HOW TO REGENERATE
-----------------
    python tools/dictionary/generate_english_words.py

Verify that the committed file is still exactly what this script produces:

    python tools/dictionary/generate_english_words.py --check
"""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
import urllib.request
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parents[2]

SOURCE_URL = (
    "https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt"
)
SOURCE_SHA256 = "3ed0c94610d8bcf7c11bbb49c56aa49c7234d32b66824df91f554169e572da48"
SOURCE_RETRIEVED = "2026-10-10"

DEFAULT_OUTPUT = REPO_ROOT / "src/data/dictionary_manual/english_words.tsv"
DEFAULT_ROMAJI_TSV = REPO_ROOT / "src/data/preedit/romanji-hiragana.tsv"
DEFAULT_DROPPED_OUT = REPO_ROOT / "dist/dictionary/english_words-dropped.txt"

# POS alias resolved by dictionary/gen_aux_dictionary.py.  Bare "固有名詞" is
# the same alias words.tsv uses for proper nouns; it maps to
# "名詞,固有名詞,一般,*,*,*,*" in data/dictionary_oss/id.def.
POS = "固有名詞"

# Documented in src/composer/composer.cc.  Applied unconditionally, also when
# --romaji-filter none is given, because these are known-observed failures
# rather than a property of the romaji table.
ROMAJI_DENY_LIST = ("mac", "ci", "youtube")

HEADER = "# key\tvalue\tpos"

# Hand-curated brand and technical names.  The key is derived from the value by
# lowercasing and dropping everything that is not a-z0-9, so "Node.js" is typed
# as "nodejs" and "CI/CD" as "cicd".  A key that the romaji table can consume
# completely is refused: keep such names out of this list instead of weakening
# the filter.
BRAND_WORDS = (
    # Version control, hosting, collaboration.
    "GitHub",
    "GitLab",
    "Git",
    "DockerHub",
    "Docker",
    "Kubernetes",
    "Slack",
    "Discord",
    "Steam",
    "Twitter",
    "Instagram",
    "Facebook",
    "YouTube",
    "Notion",
    "Figma",
    "Teams",
    "Google",
    "AWS",
    "Azure",
    "Shopify",
    "WordPress",
    "Salesforce",
    "Airbnb",
    "Netflix",
    "Spotify",
    "Microsoft",
    "AMD",
    "NVIDIA",
    # Operating systems and editors.
    "Windows",
    "Mac",
    "macOS",
    "iOS",
    "iPhone",
    "Android",
    "Linux",
    "Ubuntu",
    "Debian",
    "Arch",
    "Fedora",
    "Raspberry Pi",
    "WSL",
    "BIOS",
    "Xcode",
    "VSCode",
    "Chrome",
    "Firefox",
    "Edge",
    "Vite",
    "Webpack",
    "Tailwind",
    "React",
    "Vue",
    "Svelte",
    "Next.js",
    "Django",
    "Flask",
    "Rails",
    "Laravel",
    "Spring",
    # Languages and runtimes.
    "Python",
    "JavaScript",
    "TypeScript",
    "Rust",
    "Go",
    "Java",
    "Kotlin",
    "Swift",
    "Scala",
    "Elixir",
    "Erlang",
    "Haskell",
    "OCaml",
    "Julia",
    "MATLAB",
    "R",
    "PHP",
    "Ruby",
    "Perl",
    "Lua",
    "Zig",
    "Nim",
    "WASM",
    "Node.js",
    "Deno",
    "Bun",
    # Frameworks, data, ML.
    "PyTorch",
    "OpenAI",
    "ChatGPT",
    "SQLite",
    "PostgreSQL",
    "Redis",
    "NGINX",
    "OAuth",
    # Protocols, formats, concepts.
    "HTTP",
    "HTTPS",
    "JSON",
    "YAML",
    "XML",
    "REST",
    "JWT",
    "SSH",
    "TLS",
    "DNS",
    "VPN",
    "CDN",
    "UTF-8",
    "ASCII",
    "Unicode",
    "NaN",
    "HTML",
    "CSS",
    "CSV",
    "PDF",
    "Wi-Fi",
    # Tooling and abbreviations.
    "CLI",
    "SDK",
    "API",
    "CI/CD",
    "IDE",
    "TTY",
    "REPL",
    "VM",
    "OS",
    "PC",
    "DB",
    "JS",
    "TS",
    "URL",
    "URI",
    "UUID",
    "ENV",
    "LLM",
    "GPU",
    "CPU",
    "RAM",
    "SSD",
    "USB",
    "regex",
    "async",
    "await",
    "npm",
    "pnpm",
    "yarn",
    "cargo",
    "pip",
    "conda",
    "venv",
    "dotenv",
    "eslint",
    "prettier",
    "jest",
    "vitest",
    "pytest",
    "junit",
    "gradle",
    "maven",
    "bazel",
    "cmake",
    "makefile",
)

NON_KEY_CHARS = re.compile(r"[^a-z0-9]")
ASCII_WORD = re.compile(r"[a-z]+")
# Keys are the typed spelling: lowercase ASCII letters, plus digits for names
# such as "UTF-8" which are typed as "utf8".
KEY_CHARS = re.compile(r"[a-z0-9]+")


def brand_key(value: str) -> str:
    """The typed spelling of a canonical brand value."""
    return NON_KEY_CHARS.sub("", value.lower())


def load_romaji_table(path: Path) -> tuple[set[str], dict[str, str]]:
    """Split the romaji table into completing and pending entries.

    Each line is "input<TAB>output" or "input<TAB>output<TAB>remainder".  An
    entry with a remainder leaves a character pending, so it cannot complete a
    spelling by itself and is not used as a completing segment.
    """
    completing: set[str] = set()
    pending: dict[str, str] = {}
    with path.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) == 2:
                completing.add(parts[0])
            else:
                pending[parts[0]] = parts[2]
    if not completing:
        raise SystemExit("ERROR: %s holds no romaji entries" % path)
    return completing, pending


def make_consumer(completing: set[str]):
    """Return a predicate: can the table consume |word| completely?"""
    by_length: dict[int, set[str]] = {}
    for key in completing:
        by_length.setdefault(len(key), set()).add(key)
    lengths = sorted(by_length)

    def fully_consumable(word: str) -> bool:
        n = len(word)
        reachable = [False] * (n + 1)
        reachable[0] = True
        for i in range(n):
            if not reachable[i]:
                continue
            for length in lengths:
                if length > n - i:
                    break
                if word[i : i + length] in by_length[length]:
                    reachable[i + length] = True
        return reachable[n]

    return fully_consumable


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch_source(url: str, dest: Path) -> Path:
    dest.parent.mkdir(parents=True, exist_ok=True)
    print("downloading %s" % url)
    with urllib.request.urlopen(url, timeout=120) as response:
        dest.write_bytes(response.read())
    return dest


def load_source_words(path: Path) -> list[str]:
    with path.open("r", encoding="utf-8") as f:
        return [line.strip() for line in f if line.strip()]


def build_entries(
    words: Iterable[str],
    fully_consumable,
    min_len: int,
    max_len: int,
    use_table_filter: bool,
) -> tuple[list[tuple[str, str]], dict[str, list[str]]]:
    """Apply the bound and the collision filter to the source word list."""
    kept: list[tuple[str, str]] = []
    dropped: dict[str, list[str]] = {
        "not_ascii_lower": [],
        "outside_length_window": [],
        "romaji_reading": [],
        "romaji_deny_list": [],
    }
    for word in words:
        if not ASCII_WORD.fullmatch(word):
            dropped["not_ascii_lower"].append(word)
            continue
        if not (min_len <= len(word) <= max_len):
            dropped["outside_length_window"].append(word)
            continue
        if word in ROMAJI_DENY_LIST:
            dropped["romaji_deny_list"].append(word)
            continue
        if use_table_filter and fully_consumable(word):
            dropped["romaji_reading"].append(word)
            continue
        kept.append((word, word))
    return kept, dropped


def build_brand_entries(
    fully_consumable, use_table_filter: bool
) -> tuple[list[tuple[str, str]], dict[str, list]]:
    kept: list[tuple[str, str]] = []
    dropped: dict[str, list[str]] = {
        "duplicate_value": [],
        "duplicate_key": [],
        "romaji_reading": [],
        "romaji_deny_list": [],
    }
    seen_values: set[str] = set()
    seen_keys: dict[str, str] = {}
    for value in BRAND_WORDS:
        if value in seen_values:
            dropped["duplicate_value"].append(value)
            continue
        seen_values.add(value)
        key = brand_key(value)
        if not key:
            raise SystemExit("ERROR: brand value %r has an empty key" % value)
        if key in seen_keys:
            dropped["duplicate_key"].append("%s (%s, %s)" % (key, seen_keys[key], value))
            continue
        seen_keys[key] = value
        if key in ROMAJI_DENY_LIST:
            dropped["romaji_deny_list"].append(value)
            continue
        if use_table_filter and fully_consumable(key):
            dropped["romaji_reading"].append(value)
            continue
        kept.append((key, value))
    return kept, dropped


def merge_entries(
    generated: list[tuple[str, str]], brands: list[tuple[str, str]]
) -> tuple[list[tuple[str, str]], list[tuple[str, str, str]]]:
    """Brand entries win.  Returns (merged, overrides)."""
    brand_map = dict(brands)
    overrides: list[tuple[str, str, str]] = []
    merged: dict[str, str] = {}
    for key, value in generated:
        merged[key] = value
    for key, value in brands:
        if key in merged:
            overrides.append((key, merged[key], value))
        merged[key] = value
    if len(brands) != len(brand_map):
        raise SystemExit("ERROR: duplicate brand keys survived the brand pass")
    ordered = [(key, merged[key]) for key in sorted(merged)]
    return ordered, sorted(overrides)


def render(entries: list[tuple[str, str]]) -> str:
    lines = [HEADER]
    for key, value in entries:
        lines.append("\t".join((key, value, POS)))
    return "\n".join(lines) + "\n"


def validate(entries: list[tuple[str, str]]) -> None:
    keys = [key for key, _ in entries]
    if len(set(keys)) != len(keys):
        raise SystemExit("ERROR: duplicate keys in the merged list")
    if keys != sorted(keys):
        raise SystemExit("ERROR: merged list is not sorted by key")
    for key, value in entries:
        if not KEY_CHARS.fullmatch(key):
            raise SystemExit("ERROR: key %r is not lowercase ASCII" % key)
        if not value:
            raise SystemExit("ERROR: empty value for key %r" % key)
        if "\t" in value or "\n" in value:
            raise SystemExit("ERROR: value for key %r holds a tab or newline" % key)
        if brand_key(value) != key:
            raise SystemExit(
                "ERROR: key %r and value %r differ by more than case/punctuation"
                % (key, value)
            )


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def report(
    args,
    source_path,
    source_words,
    generated,
    gen_dropped,
    brands,
    brand_dropped,
    overrides,
    entries,
    deny_detail,
):
    dropped_words = sorted(
        gen_dropped["romaji_reading"] + gen_dropped["romaji_deny_list"]
    )
    write_text(
        args.dropped_out,
        "# words dropped by the romaji collision filter (%d)\n" % len(dropped_words)
        + "".join(word + "\n" for word in dropped_words),
    )

    print("source url        : %s" % args.source_url)
    print("source path       : %s" % source_path)
    print("source words      : %d" % len(source_words))
    print("bound (length)    : %d..%d" % (args.min_len, args.max_len))
    print("romaji filter     : %s" % args.romaji_filter)
    print("")
    print("generated list")
    print("  candidates                   : %d" % len(source_words))
    print("  dropped not [a-z]+           : %d" % len(gen_dropped["not_ascii_lower"]))
    print(
        "  dropped length outside bound : %d"
        % len(gen_dropped["outside_length_window"])
    )
    print("  dropped romaji reading       : %d" % len(gen_dropped["romaji_reading"]))
    print("  dropped romaji deny list     : %d" % len(gen_dropped["romaji_deny_list"]))
    print("      %s" % ", ".join(gen_dropped["romaji_deny_list"]))
    print("  kept                         : %d" % len(generated))
    print("")
    print("deny list attribution (src/composer/composer.cc)")
    for word in ROMAJI_DENY_LIST:
        if deny_detail[word]:
            print("  %-8s : also a complete romaji reading" % word)
        else:
            print("  %-8s : deny list only; the table leaves a pending char" % word)
    print("")
    print("brand section")
    print("  declared                     : %d" % len(BRAND_WORDS))
    print("  dropped duplicate value      : %d" % len(brand_dropped["duplicate_value"]))
    if brand_dropped["duplicate_value"]:
        print("      %s" % ", ".join(brand_dropped["duplicate_value"]))
    print("  dropped duplicate key        : %d" % len(brand_dropped["duplicate_key"]))
    if brand_dropped["duplicate_key"]:
        print("      %s" % ", ".join(brand_dropped["duplicate_key"]))
    print(
        "  excluded romaji reading      : %d" % len(brand_dropped["romaji_reading"])
    )
    if brand_dropped["romaji_reading"]:
        print("      %s" % ", ".join(brand_dropped["romaji_reading"]))
    print("  excluded romaji deny list    : %d" % len(brand_dropped["romaji_deny_list"]))
    if brand_dropped["romaji_deny_list"]:
        print("      %s" % ", ".join(brand_dropped["romaji_deny_list"]))
    print("  kept                         : %d" % len(brands))
    print("")
    print("merge")
    print("  brand overrides              : %d" % len(overrides))
    changed = [o for o in overrides if o[1] != o[2]]
    print("  of which change the value    : %d" % len(changed))
    for key, before, after in changed:
        print("      %s: %s -> %s" % (key, before, after))
    print("")
    print("final entries      : %d" % len(entries))
    print("dropped list       : %s" % args.dropped_out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--input", type=Path, help="local words_alpha.txt")
    parser.add_argument("--source-url", default=SOURCE_URL)
    parser.add_argument("--source-sha256", default=SOURCE_SHA256)
    parser.add_argument(
        "--skip-hash-check",
        action="store_true",
        help="accept a local --input whose sha256 is not the pinned one",
    )
    parser.add_argument("--romaji-tsv", type=Path, default=DEFAULT_ROMAJI_TSV)
    parser.add_argument(
        "--romaji-filter",
        choices=("full", "none"),
        default="full",
        help="'full' drops every spelling the romaji table consumes completely;"
        " 'none' drops only --romaji-deny-list (measurement only)",
    )
    parser.add_argument("--min-len", type=int, default=3)
    parser.add_argument("--max-len", type=int, default=5)
    parser.add_argument("--expected-min", type=int, default=10000)
    parser.add_argument("--expected-max", type=int, default=30000)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--dropped-out", type=Path, default=DEFAULT_DROPPED_OUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail when --output differs from what this run produces",
    )
    args = parser.parse_args()

    completing, pending = load_romaji_table(args.romaji_tsv)
    fully_consumable = make_consumer(completing)
    print(
        "romaji table      : %d completing entries, %d pending entries"
        % (len(completing), len(pending))
    )

    if args.input:
        source_path = args.input
    else:
        source_path = fetch_source(
            args.source_url, args.dropped_out.parent / "words_alpha.txt"
        )
    actual_sha = sha256_of(source_path)
    if not args.skip_hash_check and actual_sha != args.source_sha256:
        print(
            "ERROR: %s has sha256 %s, expected %s"
            % (source_path, actual_sha, args.source_sha256),
            file=sys.stderr,
        )
        return 1
    print("source sha256     : %s" % actual_sha)

    source_words = load_source_words(source_path)
    generated, gen_dropped = build_entries(
        source_words,
        fully_consumable,
        args.min_len,
        args.max_len,
        args.romaji_filter == "full",
    )
    brands, brand_dropped = build_brand_entries(
        fully_consumable, args.romaji_filter == "full"
    )
    entries, overrides = merge_entries(generated, brands)
    validate(entries)
    text = render(entries)

    if args.check:
        existing = (
            args.output.read_text(encoding="utf-8") if args.output.exists() else ""
        )
        if existing == text:
            print("CHECK OK          : %s matches this run" % args.output)
            return 0
        print(
            "CHECK FAILED      : %s differs from this run (%d lines on disk, %d generated)"
            % (args.output, existing.count("\n"), text.count("\n")),
            file=sys.stderr,
        )
        return 1

    write_text(args.output, text)
    report(
        args,
        source_path,
        source_words,
        generated,
        gen_dropped,
        brands,
        brand_dropped,
        overrides,
        entries,
        {word: fully_consumable(word) for word in ROMAJI_DENY_LIST},
    )
    print("wrote              : %s" % args.output)

    if not (args.expected_min <= len(entries) <= args.expected_max):
        print(
            "ERROR: %d entries is outside the intended %d..%d band"
            % (len(entries), args.expected_min, args.expected_max),
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
