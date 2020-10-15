#pragma once
#include <bencoding/be_element_ref.h>

#include <cstdint>
#include <cstring>
#include <string_view>
#include <string>
#include <variant>
#include <optional>

namespace be
{
    // https://www.bittorrent.org/beps/bep_0003.html
    // .torrent file.
    struct TorrentFile
    {
        struct File
        {
            std::uint64_t length_bytes_ = 0; // 'length'.
            std::string path_utf8_; // 'path'.
        };

        using LengthOrFiles = std::variant<std::monostate
            // The length of the file, in bytes (in single file case).
            , std::uint64_t // 'length'.
            // List of all files, with length information.
            , std::vector<File>>; // 'files'.

        struct Info
        {
            // Suggested name to save the file (in single file case)
            // or directory (multi-file case) as. It is purely advisory.
            std::string suggested_name_utf8_; // 'name'.

            // The number of bytes in each piece the file is split into.
            // 
            // Files are split into fixed-size pieces which are all the same length
            // (except for possibly the last one which may be truncated).
            //
            // Almost always a power of two, most commonly 2^18 = 256 K.
            std::uint64_t piece_length_bytes_ = 0; // 'piece length'.

            // String subdivided into strings of *length 20*,
            // each of which is the SHA1 hash of the piece at the corresponding index.
            std::vector<std::uint8_t> pieces_SHA1_; // 'pieces'.

            LengthOrFiles length_or_files_; // 'length' or 'files'.
        };

        std::string tracker_url_utf8_; // 'announce'.
        Info info_; // 'info'.
    };

    // Makes deep copy for the content.
    // #UUU: add error information.
    std::optional<TorrentFile> ParseTorrentFileContent(std::string_view content);

} // namespace be
