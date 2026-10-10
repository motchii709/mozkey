# data/dictionary_manual

This directory contains word entries to be added to the main dictionary.

The data here are used for proactive fixes before the main dictionary is
updated.

## TSV files (e.g. places.tsv, words.tsv)

Entries are added to the main dictionary with the following adjustments:

*   The POS (e.g., 名詞) is converted to a POS ID (e.g., 1843).
*   The cost is set to the median cost of all words sharing the same POS.

These adjustments are performed by
[dictionary/gen_aux_dictionary.py](https://github.com/google/mozc/blob/master/src/dictionary/gen_aux_dictionary.py).

If the same entries already exist in the main dictionary, the entries in this
directory are ignored. For more control, you may want to use
`aux_dictionary.tsv` and `dictionary_filter.tsv`.

*   https://github.com/google/mozc/blob/master/src/data/dictionary_oss/aux_dictionary.tsv
*   https://github.com/google/mozc/blob/master/src/data/dictionary_oss/dictionary_filter.tsv

## english_words.tsv

English words with their canonical spelling, so that a word typed in romaji can
be converted to its half-width form (`github` -> `GitHub`, `nodejs` ->
`Node.js`). The key is the ASCII spelling the user types and the value is the
canonical spelling that is displayed, so the two differ only by case and
punctuation. These entries are looked up by that ASCII spelling, unlike the
kana-keyed TSVs above.

The file is generated, not hand-edited:

```powershell
python tools/dictionary/generate_english_words.py --romaji-filter none --max-len 6
```

*   Source: [dwyl/english-words](https://github.com/dwyl/english-words),
    `words_alpha.txt`, Unlicense (public domain). The list is the Moby Word
    Lists by Grady Ward, Project Gutenberg eBook 3201, which is public domain in
    the USA. See the external source table in
    `src/data/dictionary_koyasi/README.md` for the URL, checksum and retrieval
    date.
*   These entries are meant to be consumed as *prediction candidates*, not as an
    automatic replacement: an English candidate is offered next to the kana
    conversion, so the kana reading stays available and a spelling that the
    romaji table can also consume (`code` -> こで) may be kept. The deny list
    documented in `src/composer/composer.cc` (`mac`, `ci`, `youtube`) is still
    dropped, because those spellings are far more likely to be meant as
    Japanese. `--romaji-filter full` restores the strict collision filter, and
    every dropped word is listed in `dist/dictionary/english_words-dropped.txt`.
*   The size bound is a length window, 3 to 6 letters by default, which keeps
    the list near 55,000 entries. Both bounds are parameters of the script.
*   The brand and technical names that need canonical casing (`GitHub`,
    `Node.js`, `PostgreSQL`, ...) are a separate hand-curated list in the
    script. They win over the generated words with the same key.
*   `python tools/dictionary/generate_english_words.py --check --romaji-filter
    none --max-len 6` fails when the committed file is not what the script
    produces.

## domain.txt

This file uses the same format as the main dictionary and is used as part of it.

We recommend using the TSV files instead, as the POS IDs and cost values
typically change with each dictionary update.
