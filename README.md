# fstd
A dictionary engine powered by Finite State Transducers, a data structure that enables us to search in dictionaries with many ways, including regex, suggesting based on edit distance, other than just prefix search powered by trie data structure used by traditional dictionaries.  Fstd dictionary provides two format file: `fstdx` and `fstdd` , which is similar to the popular dictionary format: `mdx` and `mdd`. A [fstdtools](https://github.com/MouJieQin/fstdtools) that can convert mdx/mdd to fstdx/fstdd.

## Features

* Multiple threads are supported to offer  high-performance search and dictionary compilation.
* Regex search.
* Fuzzy search: suggest candidates sorted by the similarity calculated by edit distance to input keyword.
* Prefix distance search: provide candidates sorted by the distance calculated by common prefix and prior suffix. By providing prior suffix, it can top the candidates that has a longer common prefix with input keyword and has a prior suffix. It's useful to help us find the simple form of an verb in a language whose verb has some constant suffix, such as Japanese ("する", "う", "く", "ぐ", "す",  "つ", "ぬ", "ぶ",  "む", "る", "い") or Korean ("하다", "다"). However, it's not always effective in all cases.
* Prefix search(predictive), a way as the same as traditional dictionaries can provide.
* Esay to convert from mdx/mdd to fstdx/fstdd.
* Data related to dictionaries is compressed to reduce disk usage.
* Low memory usage and high-performance search, as the FST made by compiling entry words of a dictionary is loaded into memory to support high-performance search. FST is a data structure providing a better compression rate than trie, because it can not only compress common prefix but also common suffix.
* Python support by `import fstd`

## Install

It has been tested on Macos and Linux platform now. On Windows platform, there might be a bug while launching a thread pool. It's not convenient to fix it because I don't have a windows machine. Pull a request to me if you can fix it.

### Python

```shell
pip install fstd
```

It's recommanded to install a python command line tool [fstdtools](https://github.com/MouJieQin/fstdtools)  as it offers a safer and more friendly usage than the CLI compiled from c plus plus.

```shell
pip install fstdtools
fstdtools --version
```

### Install from source code

1. Install dependence

```shell
brew install cmake googletest indicators spdlog fmt zstd pcre2 nlohmann-json cli11
```

2. Compile and install

```shell
git clone https://github.com/MouJieQin/fstd.git
cd fstd
mkdir build && cd build
cmake ..
make install -j8
fstd -h
```

3. Run test

```shell
make check
```

## Usage

### Python

To run the demo.py, you should have a directory named dictionaries with a structure like this, fstdx files are required.

```
dictionaries
├── dict1
│   ├── dict1.css
│   ├── dict1.fstdd
│   ├── dict1.fstdx
│   ├── dict1.js
│   └── dict1.png
├── dict2
│   ├── dict2.css
│   ├── dict2.fstdd
│   ├── dict2.fstdx
│   ├── dict2.js
│   └── dict2.png
└── dict3
    ├── dict1.css
    ├── dict1.fstdd
    ├── dict1.fstdx
    ├── dict1.js
    └── dict1.png
```

```python
# demo.py
import os
import sys
from pathlib import Path
import fstd


class FstdSearcher:
    def __init__(self, dicts_path: str):
        fstd.set_log_level(4)
        self._all_dict_names: list[str] = []
        self._fstd_searcher: fstd.FstdxSearcher = fstd.FstdxSearcher()
        self._prior_suf = ["する", "う", "く", "ぐ", "す", "つ", "ぬ", "ぶ", "む", "る", "い", "하다", "다"]
        self._fstd_searcher.insert_prior_suffix(self._prior_suf)
        self._load_dicts(dicts_path)

    def _load_dicts(self, dicts_path: str) -> None:
        """
        Load the dictionaries from the path.
        :param dicts_path: the path to the dictionaries
        """
        for dict_path in os.listdir(dicts_path):
            if os.path.isdir(os.path.join(dicts_path, dict_path)):
                for file in os.listdir(os.path.join(dicts_path, dict_path)):
                    if file.endswith(".fstdx"):
                        dict_name = Path(dict_path).name
                        self._all_dict_names.append(dict_name)
                        self._fstd_searcher.insert_if_not_exists(dict_name, os.path.join(dicts_path, dict_path, file))
                        print(f"Load dictionary {dict_name} from {os.path.join(dicts_path, dict_path, file)}")

    def lookup(
        self,
        keyword: str,
        dict_names: list[str],
    ) -> dict[str, list[str]]:
        """
        Lookup the keyword in the dictionaries.
        :param keyword: the keyword to lookup
        :param dict_names: the dictionaries to lookup
        :return: the results of the lookup,a dictionary of dict_name -> result
        """
        results = {}
        dict_names = dict_names or self._all_dict_names
        for dict_name in dict_names:
            res = self._fstd_searcher.exact_match_search(keyword, dict_name)
            if res:
                result = []
                self._hand_link_word(result, res, dict_name, [keyword])
                results[dict_name] = result
        return results

    def _hand_link_word(
        self,
        result: list[str],
        cur_result: list[str],
        dict_name: str,
        words_show: list[str],
    ):
        """
        Process the redirect words in the result.
        :param result: the result to append the words
        :param cur_result: the current result to process
        :param dict_name: the dictionary name to process
        :param words_show: the words to show
        """
        for i in range(len(cur_result)):
            item = cur_result[i]
            if "@@@LINK=" not in item:
                result.append(item)
            else:
                redirect_word = item.split("@@@LINK=")[1].strip()
                if redirect_word not in words_show:
                    words_show.append(redirect_word)
                    res_redirect = self._fstd_searcher.exact_match_search(redirect_word, dict_name)
                    if res_redirect:
                        self._hand_link_word(
                            result, res_redirect, dict_name, words_show
                        )

    def keyword_options_search(
        self,
        keyword: str,
        search_method: str,
        dict_names: list[str]
    ):
        """
        Search the keyword in the dictionaries using the search method.
        :param keyword: the keyword to search
        :param search_method: the search method to use
        :param dict_names: the dictionaries to search
        :return: the results of the search,a dictionary of dict_name -> result
        """
        use_dicts = dict_names or self._all_dict_names
        if search_method == "prefix_search":
            return self._fstd_searcher.predictive_search(keyword, use_dicts)

        elif search_method == "suggest":
            return self._fstd_searcher.suggest(keyword, use_dicts)

        elif search_method == "regex_search":
            return self._fstd_searcher.regex_search(keyword, use_dicts)

        elif search_method == "prefix_distance_search":
            return self._fstd_searcher.prefix_distance_search(keyword, use_dicts, 3)

        else:
            print(f"Invalid search method: {search_method}", file=sys.stderr)
            return []


def print_options_result(method: str, results: list[str]) -> None:
    """
    Print the results of the options search.
    :param results: the results of the options search
    """
    print(f"\nResults of {method}: ----------")
    for result in results:
        print(result)


def main():
    fstd_searcher = FstdSearcher("./dictionaries")
    print("start search....")
    results = fstd_searcher.keyword_options_search("suggst", "suggest", ["dict1","dict2"])
    print_options_result("suggest", results)
    results = fstd_searcher.keyword_options_search("振り返ってるのは", "prefix_distance_search", [])
    print_options_result("prefix_distance_search", results)
    results = fstd_searcher.keyword_options_search("dict.*n.*y$", "regex_search", [])
    if results[1]:
        print(f"Regex error: {results[1]}")
    else:
        print_options_result("regex_search", results[0])

    results = fstd_searcher.lookup("do", [])
    print("\nlookup:-------")
    for dict_name, result in results.items():
        print(f"{dict_name}: --------")
        for item in result:
            print("---------")
            print(item)


if __name__ == "__main__":
    main()

```



#### API Reference

```python
class FstdxSearcher:
    """
    Multi-dictionary search manager, load multiple fstdx and batch search
    """

    @overload
    def __init__(self, worker_num: int = 0) -> None:
        """
        Initialize the searcher with worker_num.
        :param worker_num: the number of threads to use for search
        :default worker_num is 0, automatically use all the current threads
        """
        ...

    @overload
    def __init__(self, meta_json_path: str, worker_num: int = 0) -> None:
        """
        Initialize the searcher with meta_json_path and worker_num.
        :param meta_json_path: the path to the meta json file
        :param worker_num: the number of threads to use for search
        :default worker_num is 0, automatically use all the current threads
        """
        ...

    def compile_fstdx(*args, **kwargs) -> None:
        ...

    def __bool__(self) -> bool:
        """Check if the searcher is valid."""
        ...

    def is_valid(self) -> bool:
        """
        Check if the searcher is valid.
        :return: True if the searcher is valid, False otherwise
        """
        ...

    def extract(self, name: str, file_path: str, dst_dir: str = "") -> bool:
        """
        Extract file_path from the fstdd files found in the same directory as the fstdx file.
        :param name: the name of the dictionary
        :param file_path: the path(key) to the file to extract
        :param dst_dir: the destination directory to extract the files, if empty, will extract to the default directory
        :return: True if the extraction is successful, False otherwise
        """
        ...

    def contains(self, word: str, names: Sequence[str]) -> bool:
        """
        Check if the word is in the dictionaries.
        :param word: the word to check
        :param names: the names of dictionaries to check
        :return: True if the word is in the dictionaries, False otherwise
        """
        ...

    @overload
    def exact_match_search(self, word: str, name: str) -> List[str]:
        """
        Search the word in the dictionary with name.
        :param word: the word to search
        :param name: the name of the dictionary to search
        :return: the results of the search
        """
        ...

    @overload
    def exact_match_search(self, word: str, names: Sequence[str]) -> dict[str, list[str]]:
        """
        Search the word in the dictionaries with names.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :return: the results of the search
        """
        ...

    def exact_match_search(*args, **kwargs) -> List[str]:
        ...

    def common_prefix_search(self, word: str, names: Sequence[str]) -> List[str]:
        """
        Search the longest common prefix of the word in the dictionaries.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :return: the length of the longest common prefix in the dictionaries.
        """
        ...

    def longest_prefix_len(self, word: str, names: Sequence[str]) -> int:
        """
        Search the longest common prefix of the word in the dictionaries.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :return: the results of the search
        """
        ...

    def edit_distance_search(self, word: str, names: Sequence[str], edit_distance: int = 1) -> List[str]:
        """
        Search the word in the dictionaries with edit distance.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :param edit_distance: the maximum edit distance
        :return: the results of the search
        """
        ...

    def predictive_search(self, word: str, names: Sequence[str]) -> List[str]:
        """
        Perform predictive search for the word in the dictionaries.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :return: the results of the search
        """
        ...

    def suggest(self, word: str, names: Sequence[str]) -> List[str]:
        """
        Provide suggestions for the word in the dictionaries.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :return: the suggested words
        """
        ...

    def prefix_distance_search(self, word: str, names: Sequence[str], max_distance: int = 1) -> List[str]:
        """
        Search the word in the dictionaries with prefix distance.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :param max_distance: the maximum prefix distance
        :return: the results of the search
        """
        ...

    def regex_search(self, pattern: str, names: Sequence[str]) -> Tuple[List[str], str]:
        """
        Search the word in the dictionaries with regex.
        :param pattern: the regex pattern to search
        :param names: the names of dictionaries to search
        :return: the words that match the regex pattern in the dictionary in tuple[0], the error message if any in tuple[1]
        """
        ...

    def insert_prior_suffix(self, sufs: Sequence[str]) -> None:
        """
        Insert prior suffixes for the dictionaries.
        :param sufs: the prior suffixes to insert
        """
        ...

    def insert_if_not_exists(self, name: str, fstdx_path: str) -> None:
        """
        Insert the fstdx file if it does not exist.
        :param name: the name of the dictionary
        :param fstdx_path: the path to the fstdx file
        :return: True if the insertion is successful, False otherwise
        """
        ...

    def insert(self, name: str, fstdx_path: str) -> bool:
        """
        Insert the fstdx file.
        :param name: the name of the dictionary
        :param fstdx_path: the path to the fstdx file
        :return: True if the insertion is successful, False otherwise
        """
        ...

    def save_to_disk(self, meta_json_path: str) -> bool:
        """
        Save the meta json to disk.
        :param meta_json_path: the path to save the meta json
        :return: True if the save is successful, False otherwise
        """
        ...
```



### C plus plus

1. Compile fstdx/fstdd

```shell
# go to the project directory
cd fstd
# use default configure to compile a fstdx
fstd write -f tests/dict/dict.txt -o dict.fstdx
# use default configure to compile a fstdd. Note: fstdd normally include resource data, such as pictures and audios, but we use the project lib directory for test.
fstd write -f lib -o dict.fstdd
```
The raw content of tests/dict/dict.txt :
1. The first entry requires no preceding delimiter: write the entry word (key) directly, followed by its corresponding definition (value). Definitions can span multiple lines, but entry words must stay on a single line.
2. Starting from the second entry, each entry word (key) must be preceded by the delimiter `</>`. Entry words must still be written on one line, while definitions can span multiple lines.
3. The end of each complete entry (entry word + corresponding definition) must be marked with the delimiter `</>` as a closing tag.


````
Ab
The definition of Ab
</>
Ababdeh
The definition of Ababdeh
</>
Abby
The definition of Abby
</>
...
````

2. Search in a fstdx

```shell
# show meta data
> fstd search -m dict.fstdx
{
   "Compressionlevel": 5,
   "Creationdate": "2026-06-28",
   "Description": "",
   "Encoding": "UTF-8",
   "Format": "Html",
   "Keycasesensitive": false,
   "Left2Right": true,
   "Record": 23603,
   "Stripkey": true,
   "Stylesheet": "",
   "Title": "",
   "Version": "0.1.0"
}

# exact match search
> fstd search 'Ababdeh'  dict.fstdx
------------------------------
The definition of Ababdeh
------------------------------

# regex search
> fstd search -r 'di.*na.*y' dict.fstdx
dictionary
disciplinability
disproportionality
disproportionably

# suggest search
> fstd search -g 'dicioa' dict.fstdx
dictionary -> 0.544
dilo -> 0.438889
diol -> 0.438889
radicicola -> 0.42
decoat -> 0.4
disc -> 0.303704
io -> 0.185185
coca -> 0.175926

# fuzzy search
> fstd search -e 5 'dicionary' dict.fstdx
congiary
dictionary
donary
missionary
ordinary

# enumerate all entry words in a dictionary
> fstd search -u dict.fstdx
Ab
Aberia
Abelite
Abietineae
Ababdeh
Abby
Acanthodidae
Acarus
...
```

3. Search in multiple fstdx

It's just an example of showing how to search in multiple fstdx dictionaries. The project does not provide the following fstdx files.

```shell
# Prefix distance search in multiple fstdx dicionaries
>  fstd search -P 2 振り返ってみます -f lj.fstdx -f dcq.fstdx -f hgy.fstdx
振り返る
振り
振り乱す
振り仰ぐ
振り出す
振り切る
振り合う
...

> fstd search -P 2 알아보겠습 -f lj.fstdx -f dcq.fstdx -f hgy.fstdx
알아보다
알아보
알아보-
알아내다
알아듣다
알아먹다
알아주다
알아채다
알다
...

```

4. Extract a file from a fstdd

``` shell
# list all keys of a fstdd
> fstd search -u dict.fstdd
include/fstd/common.h
include/fstd/fstdd_compressor.h
include/fstd/fstdd_reader.h
include/fstd/fstdd_writer.h
...

# extract include/fstd/common.h from the fstdd to the data directory
> fstd extract -k include/fstd/common.h -o data dict.fstdd
```


### API reference

```   c++
namespace fstd {

class FstdxSearcher {

public:
   FstdxSearcher(size_t worker_num = 0);

   FstdxSearcher(const std::string &meta_json_path, size_t worker_num = 0);

   operator bool() const;

   bool extract(const std::string &name, const std::string &file_path,
               const std::string &dst_dir) const;

   bool extract(const std::string &name, const std::string &file_path) const;

   bool contains(std::string_view word,
                  const std::vector<std::string> &names) const;

   std::vector<std::string> search(std::string_view word,
                                    const std::string &name) const;

   std::unordered_map<std::string, std::vector<std::string>>
   search(std::string_view word, const std::vector<std::string> &names) const;

   std::vector<std::string>
   common_prefix_search(std::string_view word,
                        const std::vector<std::string> &names) const;

   size_t
   longest_prefix_len(std::string_view word,
                                 const std::vector<std::string> &names) const;

   std::vector<std::string>
   edit_distance_search(std::string_view word,
                        const std::vector<std::string> &names,
                        size_t edit_distance = 1) const;

   std::vector<std::string>
   predictive_search(std::string_view word,
                     const std::vector<std::string> &names) const;

   std::vector<std::string> suggest(std::string_view word,
                                    const std::vector<std::string> &names) const;

   std::vector<std::string>
   prefix_distance_search(std::string_view word,
                           const std::vector<std::string> &names,
                           size_t max_distance) const;

   std::pair<std::vector<std::string>, std::string>
   regex_search(std::string_view pattern,
               const std::vector<std::string> &names) const;

   void insert_prior_suffix(const std::vector<std::string> &sufs);

   void insert_if_not_exists(const std::string &name,
                              const std::string &fstdx_path);

   bool insert(const std::string &name, const std::string &fstdx_path);

   bool save_to_disk(const std::string &meta_json_path);
}
}
```

## Reference

Based on [cpp-fstlib](https://github.com/yhirose/cpp-fstlib.git) by [yhirose](https://github.com/yhirose).

