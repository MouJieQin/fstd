# fstd/__init__.pyi
"""
Python type stub for fstd._native C++ extension binding
"""
from typing import List, Tuple, Optional, Union, Sequence, Any

# ------------------------------ Top-level functions ------------------------------


def get_version() -> str:
    """Get fstd library version"""
    ...


def set_log_level(log_level: int) -> None:
    """
    Set the log level for fstd library.
    log_level: 0-6, 0 is trace, 1 is debug, 2 is info, 3 is warn, 4 is error, 5 is critical, 6 is off
    """
    ...

# ------------------------------ FstddReader ------------------------------


class FstddReader:
    """
    Reader for fstdd archive format
    """

    def __init__(self, output_file: str) -> None:
        """
        Initialize the reader with output_file.
        :param output_file: the path to the fstdd file
        """
        ...

    def __bool__(self) -> bool:
        """Check if the fstdd file is valid."""
        ...

    def is_valid(self) -> bool:
        """
        Check if the fstdd file is valid.
        :return: True if the fstdd file is valid, False otherwise
        """
        ...

    def get_meta(self) -> str:
        """
        Get the meta of the fstdd file.
        :return: the meta json string
        """
        ...

    def get_header(self) -> str:
        """
        Get the header of the fstdd file.
        :return: the header json string
        """
        ...

    def contains(self, word: str) -> bool:
        """
        Check if the word is in the fstdd file.
        :param word: the word to check
        :return: True if the word is in the fstdd file, False otherwise
        """
        ...

    def extract_all_key(self) -> List[str]:
        """
        Extract all keys from the fstdd file.
        :return: a vector of keys
        """
        ...

    def extract(self, key: str, dst_dir: str = "data") -> bool:
        """
        Extract the file with key to dst_dir.
        :param key: the key to extract
        :param dst_dir: the path to the destination directory, default is data
        :return: True if the extraction is successful, False otherwise
        """
        ...

    def extract_all(self, dst_dir: str) -> bool:
        """
        Extract all files in the fstdd file to dst_dir.
        :param dst_dir: the path to the destination directory
        :return: True if the extraction is successful, False otherwise
        """
        ...

# ------------------------------ FstddWriter ------------------------------


class FstddWriter:
    """
    Writer to compile fstdd archive bundles
    """

    def __init__(self) -> None:
        ...

    def compile_fstdd(
        self,
        data_path: str,
        output_file: str,
        meta_json_str: str,
        block_size_kb: int,
        compress_level: int,
        worker_num: int,
        opt_verbose: bool
    ) -> bool:
        """
        Compile the fstd file from data path.
        :param data_path: the path to the data file or directory
        :param output_file: the path to the output fstd file
        :param meta_json_str: the meta json string
        :param block_size_kb: the block size in kb
        :param compress_level: the compress level [0, 22]
        :param worker_num: the number of threads to use for compile
        :param opt_verbose: whether to print verbose info
        :return: True if the compilation is successful, False otherwise
        """
        ...

    def push_file_stream(self, file_path: str, data: bytes) -> bool:
        """
        Push a file stream to the writer.
        :param file_path: the path(key) to the file to push
        :param data: the data of the file to push
        :return: True if the push is successful, False otherwise
        """
        ...

    def compile_fstdd(
        self,
        file_stream_num: int,
        output_file: str,
        meta_json_str: str,
        block_size_kb: int,
        compress_level: int,
        worker_num: int,
        opt_verbose: bool
    ) -> bool:
        """
        Compile the fstd file from pushed file streams.
        :param file_stream_num: the number of file streams to compile
        :param output_file: the path to the output fstd file
        :param meta_json_str: the meta json string
        :param block_size_kb: the block size in kb
        :param compress_level: the compress level [0, 22]
        :param worker_num: the number of threads to use for compile
        :param opt_verbose: whether to print verbose info
        :return: True if the compilation is successful, False otherwise
        """
        ...

# ------------------------------ FstdxReader ------------------------------


class FstdxReader:
    """
    Read & search single fstdx dictionary index
    """

    def __init__(self, fstdx_path: str) -> None:
        """
        Initialize the reader with fstdx_path.
        :param fstdx_path: the path to the fstdx file
        """
        ...

    def __bool__(self) -> bool:
        """Check if the fstdx file is valid."""
        ...

    def is_valid(self) -> bool:
        """
        Check if the fstdx file is valid.
        :return: True if the fstdx file is valid, False otherwise
        """
        ...

    def get_meta(self) -> str:
        """
        Get the meta of the fstdx file.
        :return: the meta json string
        """
        ...

    def get_header(self) -> str:
        """
        Get the header of the fstdx file.
        :return: the header json string
        """
        ...

    def get_key_size(self) -> int:
        """
        Get the key size of the entry words.
        :return: the key size of the entry words
        """
        ...

    def get_fst_key_size(self) -> int:
        """
        Get the key size of the fst index. It's less or equal to the key size of the entry words because of duplicates.
        :return: the key size of the fst index
        """
        ...

    def extract_values(self) -> List[str]:
        """
        Extract all values of the dictionary.
        :return: the values of the dictionary
        """
        ...

    def contains(self, word: str) -> bool:
        """
        Check if the word is in the dictionary.
        :param word: the word to check
        :return: True if the word is in the dictionary, False otherwise
        """
        ...

    def exact_match_search(self, word: str) -> List[str]:
        """
        Search the exact match of the word in the dictionary.
        :param word: the word to search
        :return: the value of the word in the dictionary if the word is in the dictionary, otherwise an empty vector
        """
        ...

    def common_prefix_search(self, word: str) -> List[str]:
        """
        Search the common prefix of the word in the dictionary.
        :param word: the word to search
        :return: the common prefix of the word in the dictionary if the word is in the dictionary, otherwise an empty vector
        """
        ...

    def longest_prefix_len(self, word: str) -> int:
        """
        Get the longest prefix length of the word in the dictionary.
        :param word: the word to search
        :return: the longest prefix length of the word in the dictionary.
        """
        ...

    def predictive_search(self, word: str) -> List[str]:
        """
        Search the predictive of the word as a prefix in the dictionary.
        :param word: the word as a prefix to search
        :return: the predictive of the word as a prefix in the dictionary if the prefix is in the dictionary, otherwise an empty vector
        """
        ...

    def edit_distance_search(self, word: str, distance: int = 1) -> List[str]:
        """
        Search the edit distance of the word in the dictionary.
        :param word: the word to search
        :param distance: the edit distance to search
        :return: the words that have an edit distance less than equal to the distance from the word in the dictionary
        """
        ...

    def suggest(self, word: str) -> List[str]:
        """
        Suggest the word in the dictionary.
        :param word: the word to suggest
        :return: the words that are suggested according to the word in the dictionary
        """
        ...

    def regex_search(self, pattern: str, thread: int = 1) -> List[str]:
        """
        Search the regex of the word in the dictionary.
        :param pattern: the regex pattern to search
        :param thread: the number of threads to use
        :return: the words that match the regex pattern in the dictionary
        """
        ...

    def spellcheck_word(self, word: str, limit: int = 10) -> List[str]:
        """
        Spellcheck the word in the dictionary.
        :param word: the word to spellcheck
        :param limit: the number of suggestions to return
        :return: the spellchecked word in the dictionary
        """
        ...

    def enumerate_print(self) -> None:
        """Print the dictionary to the console."""
        ...

    def extract(self, output_file: str) -> bool:
        """
        Extract the raw text of the dictionary to the output file.
        :param output_file: the path to the output file
        :return: True if the dictionary is extracted, False otherwise
        """
        ...

    def extract_keys(self) -> List[str]:
        """
        Extract all keys of the dictionary.
        :return: the keys of the dictionary
        """
        ...

# ------------------------------ FstdxWriter ------------------------------


class FstdxWriter:
    """
    Compile single fstdx dictionary index from raw text / key-value list
    """

    def __init__(self) -> None:
        ...

    def compile_fstdx(
        self,
        input_file: str,
        output_file: str,
        meta_json_str: str,
        block_size_kb: int,
        compress_level: int,
        zstd_dict_size_kb: int,
        worker_num: int,
        opt_sorted: bool,
        opt_verbose: bool
    ) -> bool:
        """
        Compile the fstdx file.
        :param input_file: the path to the input fstdx file
        :param output_file: the path to the output fstdx file
        :param meta_json_str: the meta json string
        :param block_size_kb: the block size in kb
        :param compress_level: the compress level [0, 22]
        :param zstd_dict_size_kb: the zstd dict size in kb
        :param worker_num: the number of threads to use for compile
        :param opt_sorted: whether to sort the values
        :param opt_verbose: whether to print verbose info
        :return: True if the compilation is successful, False otherwise
        """
        ...

    def compile_fstdx(
        self,
        output_file: str,
        keys: Sequence[str],
        values: Sequence[str],
        meta_json_str: str,
        block_size_kb: int,
        compress_level: int,
        zstd_dict_size_kb: int,
        worker_num: int,
        opt_sorted: bool,
        opt_verbose: bool
    ) -> bool:
        """
        Compile the fstdx file.
        :param output_file: the path to the output fstdx file
        :param keys: the keys to compile
        :param values: the values to compile
        :param meta_json_str: the meta json string
        :param block_size_kb: the block size in kb
        :param compress_level: the compress level [0, 22]
        :param zstd_dict_size_kb: the zstd dict size in kb
        :param worker_num: the number of threads to use for compile
        :param opt_sorted: whether to sort the values
        :param opt_verbose: whether to print verbose info
        :return: True if the compilation is successful, False otherwise
        """
        ...

# ------------------------------ FstdxSearcher ------------------------------


class FstdxSearcher:
    """
    Multi-dictionary search manager, load multiple fstdx and batch search
    """

    def __init__(self, worker_num: int = 0) -> None:
        """
        Initialize the searcher with worker_num.
        :param worker_num: the number of threads to use for search
        :default worker_num is 0, automatically use all the current threads
        """
        ...

    def __init__(self, meta_json_path: str, worker_num: int = 0) -> None:
        """
        Initialize the searcher with meta_json_path and worker_num.
        :param meta_json_path: the path to the meta json file
        :param worker_num: the number of threads to use for search
        :default worker_num is 0, automatically use all the current threads
        """
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
        Extract the fstdx file.
        :param name: the name of the dictionary
        :param file_path: the path(key) to the file to extract
        :param dst_dir: the destination directory to extract the files, if empty, will extract to the default directory
        :return: True if the extraction is successful, False otherwise
        """
        ...

    def contains(self, word: str, names: Union[str, Sequence[str]]) -> bool:
        """
        Check if the word is in the dictionary.
        :param word: the word to check
        :param names: the names of dictionaries to check
        :return: True if the word is in the dictionaries, False otherwise
        """
        ...

    def exact_match_search(self, word: str, name: str) -> List[str]:
        """
        Search the word in the dictionary.
        :param word: the word to search
        :param name: the name of the dictionary to search
        :return: the results of the search
        """
        ...

    def exact_match_search(self, word: str, names: Sequence[str]) -> List[str]:
        """
        Search the word in the dictionaries.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :return: the results of the search
        """
        ...

    def common_prefix_search(self, word: str, names: Sequence[str]) -> List[str]:
        """
        Search the common prefix of the word in the dictionaries.
        :param word: the word to search
        :param names: the names of dictionaries to search
        :return: the results of the search
        """
        ...

    def longest_common_prefix_search(self, word: str, names: Sequence[str]) -> List[str]:
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

    def regex_search(self, pattern: str, names: Sequence[str]) -> List[str]:
        """
        Search the word in the dictionaries with regex.
        :param pattern: the regex pattern to search
        :param names: the names of dictionaries to search
        :return: the results of the search
        """
        ...

    def insert_prior_suffix(self, sufs: Sequence[str]) -> None:
        """
        Insert prior suffixes for the dictionaries.
        :param sufs: the prior suffixes to insert
        """
        ...

    def insert_if_not_exists(self, name: str, fstdx_path: str) -> bool:
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


# Export public API
__all__ = [
    "get_version",
    "set_log_level",
    "FstddReader",
    "FstddWriter",
    "FstdxReader",
    "FstdxWriter",
    "FstdxSearcher",
]
