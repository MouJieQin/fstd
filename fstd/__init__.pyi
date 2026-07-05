# fstd/__init__.pyi
"""
Python type stub for fstd._native C++ extension binding
"""
from typing import overload, List, Tuple, Sequence

# ------------------------------ Top-level functions ------------------------------


def get_version() -> str:
    """Get fstd library version number"""
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

    def __init__(self, fstdd_file: str) -> None:
        """
        Initialize the reader with fstdd_file.
        :param fstdd_file: the path to the fstdd file
        """
        ...

    def __bool__(self) -> bool:
        """Check if the fstdd reader is valid."""
        ...

    def is_valid(self) -> bool:
        """
        Check if the fstdd reader is valid.
        :return: True if the fstdd reader is valid, False otherwise
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

    def contains(self, key_path: str) -> bool:
        """
        Check if the key_path is in the fstdd file.
        :param key_path: the key_path to check
        :return: True if the key_path is in the fstdd file, False otherwise
        """
        ...

    def extract_all_key(self) -> List[str]:
        """
        Extract all keys from the fstdd file.
        :return: a list of keys
        """
        ...

    def extract(self, key_path: str, dst_dir: str = "data") -> bool:
        """
        Extract the file with key_path to dst_dir.
        :param key_path: the key_path to extract
        :param dst_dir: the path to the destination directory, default is data
        :return: True if the extraction is successful, False otherwise
        """
        ...

    def extract_all(self, dst_dir: str = "data") -> bool:
        """
        Extract all files in the fstdd file to dst_dir.
        :param dst_dir: the path to the destination directory, default is data
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

    @overload
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
        :return: 0 if the compilation is successful, non-zero otherwise
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

    @overload
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
        :return: 0 if the compilation is successful, non-zero otherwise
        """
        ...

    def compile_fstdd(*args, **kwargs) -> bool:
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
        """Check if the fstdx reader is valid."""
        ...

    def is_valid(self) -> bool:
        """
        Check if the fstdx reader is valid.
        :return: True if the fstdx reader is valid, False otherwise
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

    def suggest(self, word: str) -> List[Tuple[float, str]]:
        """
        Suggest the word in the dictionary.
        :param word: the word to suggest
        :return: the words and its similarity, the words are suggested according to similarity in descending order
        """
        ...

    def regex_search(self, pattern: str, thread: int = 1) -> Tuple[List[str], str]:
        """
        Search the word in the dictionaries with regex.
        :param pattern: the regex pattern to search
        :param thread: the number of threads to use
        :return: the words that match the regex pattern in the dictionary in tuple[0], the error message if any in tuple[1]
        """
        ...

    def spellcheck_word(self, word: str, names: Sequence[str], limit: int = 10) -> List[str]:
        """
        Spellcheck the word in the dictionaries.
        :param word: the word to spellcheck
        :param names: the names of dictionaries to spellcheck
        :param limit: the number of suggestions to return
        :return: the spellchecked word and its similarity in the dictionary
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

    @overload
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

    @overload
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

    def compile_fstdx(*args, **kwargs) -> bool:
        ...
# ------------------------------ FstdxSearcher ------------------------------


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
