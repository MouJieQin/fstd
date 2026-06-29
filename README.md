# fstd
A dictionary engine powered by Finite State Transducers, a data structure that enables us to search in dictionaries with many ways, including regex, suggesting based on edit distance, other than just prefix search powered by trie data structure used by traditional dictionaries.  Fstd dictionary provides two format file: `fstdx` and `fstdd` , which is similar to the popular dictionary format: `mdx` and `mdd`. A [converter](https://github.com/MouJieQin/mdict-fstd) from mdx/mdd to fstdx/fstdd.

## Features

* Multiple threads are supported to offer  high-performance search and dictionary compilation.
* Regex search.
* Fuzzy search: suggest candidates sorted by the similarity calculated by edit distance to input keyword.
* Prefix distance search: provide candidates sorted by the distance calculated by common prefix and prior suffix. By providing prior suffix, it can top the candidates that has a longer common prefix with input keyword and has a prior suffix. It's useful to help us find the simple form of the verb in a language whose verb has some constant suffix, such as Japanese ("する", "う", "く", "ぐ", "す",  "つ", "ぬ", "ぶ",  "む", "る", "い") or Korean ("하다", "다"). However, it's not always effective in all cases.
* Prefix search(predictive), a way as the same as traditional dictionaries can provide.
* Esay to convert from mdx/mdd to fstdx/fstdd.
* Data related to dictionaries is compressed to reduce disk usage.
* Low memory usage and high-performance search, as the FST made by compiling entry words of a dictionary is loaded into memory to support high-performance search. FST is a data structure providing a better compression rate than trie, because it can not only compress common prefix but also common suffix.

## Install

It has only been tested to compile and install in Macos x86 platform now.

1. Install dependence

   ```shell
   brew install cmake googletest indicators spdlog fmt zstd pcre2 nlohmann-json
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

1. Compile fstdx/fstdd

   ```shell
   cd fstd
   # use default configure to compile a fstdx
   fstd write -f tests/dict/dict.txt -o dict.fstdx
   # use default configure to compile a fstdd. Note: fstdd normally include resource data, such as pictures and audios, but we use the project lib directory for test.
   fstd write -f lib -o dict.fstdd
   ```

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
  
   > It's just an example of showing how to search in multiple fstdx dictionaries. The project does not provide the following fstdx files.
   
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
   
    
    ​     

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
     longest_common_prefix_search(std::string_view word,
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

