# moezip.pyi

def init(vocab_path: str = "", router_path: str = "", quiet: bool = True) -> None:
    """Initializes or re-loads moezip engine assets quietly."""
    ...

def compress(text: str) -> bytes:
    """Compresses a UTF-8 string into moezip binary bytes."""
    ...

def decompress(packed_bytes: bytes) -> str:
    """Decompresses moezip binary bytes back into a UTF-8 string."""
    ...

def train(corpus_or_path: str, output_filepath: str = "router_stateless.json", quiet: bool = False) -> None:
    """Trains the router transition matrix on a raw text string or corpus file path."""
    ...