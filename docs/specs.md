ZPack File Format Specifications
================================
Version 1, 22nd November 2021

Overview
-------------------------
ZPack is a simple, general-purpose archive file format with lossless data compression.

Its design is intended to be as simple as possible, storing just enough data to serve as an archive,
while being easy to use and implement.

Supported compression methods: none, zstd and lz4.

The standard file extension for ZPack-formatted files is `.zpk`

Overall data structure
-------------------------
The archive is split into multiple blocks:
- Archive header
- File data
- Central directory record
- End of central directory record

These blocks must be laid out in this specific order. However, it is not guaranteed that all of the
blocks are right next to each other. Some blocks (such as the central directory record) might have a
specific offset that the reader is expected to read from.

Intended reading order:
1. Archive header
2. End of central directory record
3. Central directory record
4. Files

Archive header
-------------------------
The archive header contains basic information about the archive and can be used to identify a ZPack
file:

|   Field   |  Type  | Size |           Description           |
| --------- | ------ | ---- | ------------------------------- |
| Signature | uint32 | 4    | Header signature (0x0x5a504b15) |
|  Version  | uint16 | 2    | Archive version*                |

*: The archive version is also the version of the specifications.

File data
-------------------------
The file data block is required to be right next to the archive header and contains a signature to 
mark the start of it:

|   Field   |  Type  | Size |             Description             |
| --------- | ------ | ---- | ----------------------------------- |
| Signature | uint32 | 4    | Data start signature (0x0x5a504b14) |

The file data block contains an undetermined amount of data that may or may not correlate to the files
that are actually stored within the archive. It is not reliable to assume the total (compressed) size of files stored in the archive depending on the size of the block.

Central directory record
-------------------------
The central directory record contains a list of file entries that can be used to read the files
stored in the archive. It starts off with a header:

|   Field    |  Type  | Size |                  Description                    |
| ---------- | ------ | ---- | ----------------------------------------------- |
| Signature  | uint32 | 4    | CDR signature (0x0x5a504b13)                    |
| File count | uint64 | 8    | Number of file entries (n)                      |
| Block size | uint64 | 8    | Size of the entire block (excluding the header) |

After this, `n` count of file entries are written right next to each other:

|       Field        |  Type  | Size |                  Description                    |
| ------------------ | ------ | ---- | ----------------------------------------------- |
| Filename length    | uint16 | 2    | The filename's length (n)                       |
| Filename           | string | n    | UTF-8 formatted filename. May contain paths*    |
| Offset             | uint64 | 8    | Offset of the file's (compressed) data          |
| Compressed size    | uint64 | 8    | The file's compressed size                      |
| Uncompressed size  | uint64 | 8    | The file's uncompressed size                    |
| File hash          | uint64 | 8    | XXH3 hash of the original data                  |
| Compression method | uint8  | 1    | The compression method used**                   |

Since the actual size of each of file entry is undetermined (due to the filename field), the block 
size can be used to allocate a single memory block to read all of the file entries in one go.

The filename should not be null-terminated. It is done automatically by the reader.

*: There are absolutely no rules on what's allowed in the filename field. Malicious paths might be 
present and it is up to the implementer to address these issues. There are no standards on how to 
handle the paths.

However, it is recommended to use `/` as the path separator.

**: Supported compression methods:

| Value | Method |
| ----- | ------ |
| 0     | none   |
| 1     | zstd   |
| 2     | lz4    |

Note: The LZ4 frame format is used for LZ4 compression (lz4f).

End of central directory record
-------------------------
The end of central directory record is located right after the central directory record. It is also 
expected to be at the end of the file.

It is used to determine the offset of the central directory record.

|   Field    |  Type  | Size |              Description               |
| ---------- | ------ | ---- | -------------------------------------- |
| Signature  | uint32 | 4    | EOCDR signature (0x0x5a504b12)         |
| CDR offset | uint64 | 8    | Offset of the central directory record |