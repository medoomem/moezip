# moezip.pyi

def compress(text: str) -> bytes:
    """Compresses a UTF-8 string into moezip binary bytes."""
    ...

def decompress(packed_bytes: bytes) -> str:
    """Decompresses moezip binary bytes back into a UTF-8 string."""
    ...