# data/dictionary_oss

## aux_dictionary.tsv

AUX dictionary is a mechanism to add word entries to
`data/dictionary_oss/dictionary0x.txt` without running the internal training
pipeline.

> [!NOTE]
>
> We recommend to update TSV files in
> [data/dictionary_manual](https://github.com/google/mozc/blob/master/src/data/dictionary_manual/) as an
> easier way. Please consider using `data/dictionary_manual/` at first.

### Format of aux_dictionary.tsv

| key      | value    | base_key | base_value | cost_offset |
| :------: | :------: | :------: | :--------: |:-----------:|
| あるぱか | アルパカ | かぴばら | カピバラ | -1          |

* aux_dictionary.tsv is a list of additional words to be added to Mozc's word dictionary.
* key and value (i.e. アルパカ) are fields for the new word.
* base_key and base_value (i.e. カピバラ) are fields of the reference word already in the dictionary.
* cost_offset is the difference from the base cost. (i.e. -1 makes the new word prioritized than the base word).
* other fields of the new word (e.g. lid, rid , cost) are copied from the reference word.

## dictionary_filter.tsv

The entries in this file are used to filter out the entries in the dictionary.

### Format of dictionary_filter.tsv

| key      | value    |
| :------: | :------: |
| あるぱか | 或るパカ |

* `key` and `value` are the regexp patterns of words to be removed.
* Each line is extended to the following regexp pattern:
  + `'^{key}\t\\d+\t\\d+\t\\d+\t{value}(\t.*)?\n$'`

## english_words.txt

English words that a user types in romaji, with their canonical half-width
spelling (`github` -> `GitHub`, `nodejs` -> `Node.js`). Unlike the kana-keyed
files above, the key is the ASCII spelling itself, and the value differs from it
only by case and punctuation.

This file is **not** an aux dictionary and must not be moved to
`data/dictionary_manual/`: that route runs through `gen_aux_dictionary.py`, which
rewrites the cost to the **median cost of the given POS**, and for a POS like
固有名詞 that cost is low enough for an English entry to rank beside kana and
change the default conversion result. The six-column form used here instead goes
straight to `TextDictionaryLoader`, whose `ENGLISH` label reassigns lid/rid to
the isolated word POS (特殊,短縮よみ, see `src/data/rules/special_pos.def`). On top
of the low cost (8000, the cost of the ASCII entries already in `domain.txt`),
that POS adds a large per-transition cost, so these entries sort below ordinary
kana predictions and never carry `Attribute::REALTIME_TOP`. An English word can
therefore never take a kana reading away from the user, which is what makes it
safe to list spellings whose romaji reading is also a Japanese word (`code`
reads as こで): the kana conversion stays available and stays first.

The ranking claim above is a runtime property of the POS transition cost; the
data file only fixes lid/rid/cost so that it holds.

### Format

Each line is `key <tab> lid <tab> rid <tab> cost <tab> value <tab> ENGLISH`.
`TextDictionaryLoader::ParseTSV` requires five columns and accepts the optional
sixth label; columns 2 and 3 only have to be integers, so 0 is used and only the
cost is meaningful. Keys are lowercase ASCII letters (plus digits for names such
as `UTF-8`, typed as `utf8`), unique and sorted ascending. The loader understands
neither comments nor blank lines, so the file contains neither.

### Source and license

[dwyl/english-words](https://github.com/dwyl/english-words), data file
`words_alpha.txt`:

* raw URL
  <https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt>
* sha256 `3ed0c94610d8bcf7c11bbb49c56aa49c7234d32b66824df91f554169e572da48`
* license Unlicense (public domain dedication). The upstream `LICENSE.md` is the
  verbatim Unlicense text: "This is free and unencumbered software released into
  the public domain. ... For more information, please refer to
  <https://unlicense.org>"
* lineage the list is the Moby Word Lists by Grady Ward (Project Gutenberg eBook
  3201, "Public domain in the USA"). Upstream `word_list_moby_credits.txt`
  records that credit, and upstream `README.md` notes the words also appeared in
  an infochimps "simple english words" dataset. Both strands are public domain.
* retrieved 2026-10-10 (UTC)

The source file is not committed; the script downloads it on demand and checks
the sha256 above. `src/data/dictionary_koyasi/README.md` carries the same
provenance in its external source table.

### Regenerating

The file is generated, not hand-edited, and a plain run reproduces the committed
file exactly:

```powershell
python tools/dictionary/generate_english_words.py            # write
python tools/dictionary/generate_english_words.py --check    # verify
```
