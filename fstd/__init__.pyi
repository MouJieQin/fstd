# fstd/__init__.pyi
"""
Python type stub for fstd._native C++ extension binding
"""
from typing import overload, List, Tuple, Sequence

# ------------------------------ Top-level functions ------------------------------


def get_version() -> str:
    """Get fstd library version number."""
    ...


def set_log_level(log_level: int) -> None:
    """Set the log level for fstd library.

    Args:
        log_level: Log level value, range 0-6. 0=trace, 1=debug, 2=info,
            3=warn, 4=error, 5=critical, 6=off.
    """
    ...

# ------------------------------ FstddReader ------------------------------


class FstddReader:
    """Reader for fstdd archive format."""

    def __init__(self, fstdd_file: str) -> None:
        """Initialize the reader with an fstdd file.

        Args:
            fstdd_file: Path to the fstdd file.
        """
        ...

    def __bool__(self) -> bool:
        """Check if the fstdd reader is valid."""
        ...

    def is_valid(self) -> bool:
        """Check if the fstdd reader is valid.

        Returns:
            bool: True if the reader is valid, False otherwise.
        """
        ...

    def get_meta(self) -> str:
        """Get the meta of the fstdd file.

        Returns:
            str: Meta info as a JSON string.
        """
        ...

    def get_header(self) -> str:
        """Get the header of the fstdd file.

        Returns:
            str: Header info as a JSON string.
        """
        ...

    def contains(self, key_path: str) -> bool:
        """Check whether a key_path exists in the fstdd file.

        Args:
            key_path: The key path to check.

        Returns:
            bool: True if the key exists, False otherwise.
        """
        ...

    def extract_all_key(self) -> List[str]:
        """Extract all keys from the fstdd file.

        Returns:
            list[str]: All keys stored in the fstdd archive.
        """
        ...

    def extract(self, key_path: str, dst_dir: str = "data") -> bool:
        """Extract a single file by key to the destination directory.

        Args:
            key_path: The key of the file to extract.
            dst_dir: Destination directory path. Defaults to ``"data"``.

        Returns:
            bool: True if extraction succeeds, False otherwise.
        """
        ...

    def extract_all(self, dst_dir: str = "data") -> bool:
        """Extract all files in the fstdd file to dst_dir.

        Args:
            dst_dir: Destination directory path.

        Returns:
            bool: True if extraction succeeds, False otherwise.
        """
        ...

# ------------------------------ FstddWriter ------------------------------


class FstddWriter:
    """Writer to compile fstdd archive bundles."""

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
        """Compile an fstdd file from a data file or directory.

        Args:
            data_path: Path to the source data file or directory.
            output_file: Path to the output fstdd file.
            meta_json_str: Meta information as a JSON string.
            block_size_kb: Block size in KB.
            compress_level: Compression level, range [0, 22].
            worker_num: Number of worker threads used for compilation.
            opt_verbose: Whether to print verbose progress info.

        Returns:
            int: 0 on success, non-zero error code otherwise.
        """
        ...

    def push_file_stream(self, file_path: str, data: bytes) -> bool:
        """Push a file stream into the writer.

        Args:
            file_path: Key path of the file to push.
            data: Raw bytes content of the file.

        Returns:
            bool: True if the push succeeds, False otherwise.
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
        """Compile an fstdd file from previously pushed file streams.

        Args:
            file_stream_num: Number of pushed file streams to compile.
            output_file: Path to the output fstdd file.
            meta_json_str: Meta information as a JSON string.
            block_size_kb: Block size in KB.
            compress_level: Compression level, range [0, 22].
            worker_num: Number of worker threads used for compilation.
            opt_verbose: Whether to print verbose progress info.

        Returns:
            bool: True if compilation succeeds, False otherwise.
        """
        ...

    def compile_fstdd(*args, **kwargs) -> bool:
        ...

# ------------------------------ FstdxReader ------------------------------


class FstdxReader:
    """Read & search single fstdx dictionary index."""

    def __init__(self, fstdx_path: str) -> None:
        """Initialize the reader with an fstdx dictionary file.

        Args:
            fstdx_path: Path to the fstdx file.
        """
        ...

    def __bool__(self) -> bool:
        """Check if the fstdx reader is valid."""
        ...

    def is_valid(self) -> bool:
        """Check if the fstdx file is valid.

        Returns:
            bool: True if the file is valid, False otherwise.
        """
        ...

    def get_meta(self) -> str:
        """Get the meta of the fstdx file.

        Returns:
            str: Meta info as a JSON string.
        """
        ...

    def get_header(self) -> str:
        """Get the header of the fstdx file.

        Returns:
            str: Header info as a JSON string.
        """
        ...

    def get_key_size(self) -> int:
        """Get the total key count of entry words.

        Returns:
            int: Number of entry word keys.
        """
        ...

    def get_fst_key_size(self) -> int:
        """Get the key count of the FST index.

        The FST key count is less than or equal to the entry key size due to
        duplicate entries sharing the same key.

        Returns:
            int: Number of unique keys in the FST index.
        """
        ...

    def extract_values(self) -> List[str]:
        """Extract all values of the dictionary.

        Returns:
            list[str]: All entry values in the dictionary.
        """
        ...

    def contains(self, word: str) -> bool:
        """Check whether a word exists in the dictionary.

        Args:
            word: The word to check.

        Returns:
            bool: True if the word exists, False otherwise.
        """
        ...

    def exact_match_search(self, word: str) -> List[str]:
        """Perform an exact match search for the given word.

        Args:
            word: The word to search.

        Returns:
            list[str]: Matching entry values; empty if the word is not found.
        """
        ...

    def common_prefix_search(self, word: str) -> List[str]:
        """Perform a common prefix search for the given word.

        Args:
            word: The word whose prefixes are searched.

        Returns:
            list[str]: Words in the dictionary that are prefixes of the input word.
        """
        ...

    def longest_prefix_len(self, word: str) -> int:
        """Get the length of the longest matching prefix in the dictionary.

        Args:
            word: The word to search.

        Returns:
            int: Length of the longest common prefix found.
        """
        ...

    def predictive_search(self, word: str) -> List[str]:
        """Perform a predictive (prefix) search.

        Returns all words in the dictionary that start with the given prefix.

        Args:
            word: The prefix to search.

        Returns:
            list[str]: Words starting with the given prefix.
        """
        ...

    def edit_distance_search(self, word: str, distance: int = 1) -> List[str]:
        """Perform an edit distance (fuzzy) search.

        Finds all words within the given Levenshtein distance from the input.

        Args:
            word: The word to search.
            distance: Maximum allowed edit distance. Defaults to 1.

        Returns:
            list[str]: Words whose edit distance is less than or equal to
                ``distance``.
        """
        ...

    def suggest(self, word: str) -> List[Tuple[float, str]]:
        """Get spelling suggestions for the given word.

        Args:
            word: The word to get suggestions for.

        Returns:
            list[tuple[float, str]]: Suggested words paired with their
                similarity scores, sorted by similarity.
        """
        ...

    def regex_search(self, pattern: str, thread: int = 1) -> Tuple[List[str], str]:
        """Perform a regular expression search on the dictionary.

        Args:
            pattern: The regex pattern to match.
            thread: Number of threads to use. Defaults to 1.

        Returns:
            tuple[list[str], str]: Matched words at index 0, error message
                (if any) at index 1.
        """
        ...

    def spellcheck_word(self, word: str, names: Sequence[str], limit: int = 10) -> List[str]:
        """Spell-check a word and return the best suggestions.

        Args:
            word: The word to spell-check.
            names: List of dictionary names to search.
            limit: Maximum number of suggestions to return. Defaults to 10.

        Returns:
            list[tuple[float, str]]: Suggested words with similarity scores.
        """
        ...

    def enumerate_print(self) -> None:
        """Print the entire dictionary to the console.

        Returns:
            None.
        """
        ...

    def extract(self, output_file: str) -> bool:
        """Extract the raw text of the dictionary to a file.

        Args:
            output_file: Path to the output text file.

        Returns:
            bool: True if extraction succeeds, False otherwise.
        """
        ...

    def extract_keys(self) -> List[str]:
        """Extract all keys (headwords) of the dictionary.

        Returns:
            list[str]: All keys in the dictionary.
        """
        ...

# ------------------------------ FstdxWriter ------------------------------


class FstdxWriter:
    """Compile single fstdx dictionary index from raw text / key-value list."""

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
        """Compile an fstdx file from a plain text input file.

        Args:
            input_file: Path to the source dictionary text file.
            output_file: Path to the output fstdx file.
            meta_json_str: Meta information as a JSON string.
            block_size_kb: Block size in KB.
            compress_level: Zstd compression level, range [0, 22].
            zstd_dict_size_kb: Zstd dictionary size in KB.
            worker_num: Number of worker threads used for compilation.
            opt_sorted: Whether to sort values by key.
            opt_verbose: Whether to print verbose progress info.

        Returns:
            bool: True if compilation succeeds, False otherwise.
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
        """Compile an fstdx file from in-memory key and value lists.

        Args:
            output_file: Path to the output fstdx file.
            keys: List of headword keys.
            values: List of entry values, parallel to ``keys``.
            meta_json_str: Meta information as a JSON string.
            block_size_kb: Block size in KB.
            compress_level: Zstd compression level, range [0, 22].
            zstd_dict_size_kb: Zstd dictionary size in KB.
            worker_num: Number of worker threads used for compilation.
            opt_sorted: Whether to sort values by key.
            opt_verbose: Whether to print verbose progress info.

        Returns:
            bool: True if compilation succeeds, False otherwise.
        """
        ...

    def compile_fstdx(*args, **kwargs) -> bool:
        ...

# ------------------------------ FstdxSearcher ------------------------------


class FstdxSearcher:
    """Multi-dictionary search manager, load multiple fstdx and batch search."""

    @overload
    def __init__(self, worker_num: int = 0) -> None:
        """Initialize the searcher with a worker thread count.

        Args:
            worker_num: Number of threads for parallel search. Defaults to 0,
                which auto-detects available CPU threads.
        """
        ...

    @overload
    def __init__(self, meta_json_path: str, worker_num: int = 0) -> None:
        """Initialize the searcher from a meta JSON file.

        Args:
            meta_json_path: Path to the meta JSON file describing loaded
                dictionaries.
            worker_num: Number of threads for parallel search. Defaults to 0,
                which auto-detects available CPU threads.
        """
        ...

    def compile_fstdx(*args, **kwargs) -> None:
        ...

    def __bool__(self) -> bool:
        """Check if the searcher is valid."""
        ...

    def is_valid(self) -> bool:
        """Check if the searcher is valid.

        Returns:
            bool: True if the searcher is valid, False otherwise.
        """
        ...

    def extract(self, name: str, file_path: str, dst_dir: str = "") -> bool:
        """Extract a file from the fstdd archive paired with an fstdx dictionary.

        The fstdd file is expected to reside in the same directory as the fstdx file.

        Args:
            name: Name of the dictionary.
            file_path: Key path of the file to extract inside the fstdd archive.
            dst_dir: Destination directory. If empty, uses the default directory.

        Returns:
            bool: True if extraction succeeds, False otherwise.
        """
        ...

    def contains(self, word: str, names: Sequence[str]) -> bool:
        """Check whether a word exists in the specified dictionaries.

        Args:
            word: The word to check.
            names: List of dictionary names to search.

        Returns:
            bool: True if the word is found in any of the dictionaries.
        """
        ...

    @overload
    def exact_match_search(self, word: str, name: str) -> List[str]:
        """Perform an exact match search on a single dictionary.

        Args:
            word: The word to search.
            name: Name of the dictionary to search.

        Returns:
            list[str]: Matching entry values.
        """
        ...

    @overload
    def exact_match_search(self, word: str, names: Sequence[str]) -> dict[str, list[str]]:
        """Perform an exact match search across multiple dictionaries.

        Args:
            word: The word to search.
            names: List of dictionary names to search.

        Returns:
            dict[str, list[str]]: Matching entry values from all specified
                dictionaries, keyed by dictionary name.
        """
        ...

    def exact_match_search(*args, **kwargs) -> List[str]:
        ...

    def common_prefix_search(self, word: str, names: Sequence[str]) -> List[str]:
        """Perform a common prefix search across multiple dictionaries.

        Args:
            word: The word whose prefixes are searched.
            names: List of dictionary names to search.

        Returns:
            list[str]: Words that are prefixes of the input word.
        """
        ...

    def longest_prefix_len(self, word: str, names: Sequence[str]) -> int:
        """Get the length of the longest matching prefix across dictionaries.

        Args:
            word: The word to search.
            names: List of dictionary names to search.

        Returns:
            int: Length of the longest common prefix found.
        """
        ...

    def edit_distance_search(self, word: str, names: Sequence[str], edit_distance: int = 1) -> List[str]:
        """Perform an edit distance search across multiple dictionaries.

        Args:
            word: The word to search.
            names: List of dictionary names to search.
            edit_distance: Maximum allowed edit distance. Defaults to 1.

        Returns:
            list[str]: Matching words within the edit distance threshold.
        """
        ...

    def predictive_search(self, word: str, names: Sequence[str]) -> List[str]:
        """Perform a predictive (prefix) search across multiple dictionaries.

        Args:
            word: The prefix to search.
            names: List of dictionary names to search.

        Returns:
            list[str]: Words starting with the given prefix.
        """
        ...

    def suggest(self, word: str, names: Sequence[str]) -> List[str]:
        """Get spelling suggestions across multiple dictionaries.

        Args:
            word: The word to get suggestions for.
            names: List of dictionary names to search.

        Returns:
            list[tuple[float, str]]: Suggested words with similarity scores.
        """
        ...

    def prefix_distance_search(self, word: str, names: Sequence[str], max_distance: int = 1) -> List[str]:
        """Perform a prefix-distance search across multiple dictionaries.

        Args:
            word: The word to search.
            names: List of dictionary names to search.
            max_distance: Maximum allowed prefix distance. Defaults to 1.

        Returns:
            list[str]: Matching words within the prefix distance threshold.
        """
        ...

    def regex_search(self, pattern: str, names: Sequence[str]) -> Tuple[List[str], str]:
        """Perform a regex search across multiple dictionaries.

        Args:
            pattern: The regex pattern to match.
            names: List of dictionary names to search.

        Returns:
            tuple[list[str], str]: Words matching the regex pattern and an
                error message (empty on success).
        """
        ...

    def insert_prior_suffix(self, sufs: Sequence[str]) -> None:
        """Insert prior suffix rules into the searcher.

        Args:
            sufs: List of prior suffixes to insert.

        Returns:
            None.
        """
        ...

    def insert_if_not_exists(self, name: str, fstdx_path: str) -> None:
        """Insert an fstdx dictionary only if it does not already exist.

        Args:
            name: Name of the dictionary.
            fstdx_path: Path to the fstdx file.

        Returns:
            None.
        """
        ...

    def insert(self, name: str, fstdx_path: str) -> bool:
        """Insert an fstdx dictionary into the searcher.

        Args:
            name: Name of the dictionary.
            fstdx_path: Path to the fstdx file.

        Returns:
            bool: True if insertion succeeds, False otherwise.
        """
        ...

    def save_to_disk(self, meta_json_path: str) -> bool:
        """Persist the current meta information to a JSON file on disk.

        Args:
            meta_json_path: Path where the meta JSON file will be saved.

        Returns:
            bool: True if saving succeeds, False otherwise.
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
