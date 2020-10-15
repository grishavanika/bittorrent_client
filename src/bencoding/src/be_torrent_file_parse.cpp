#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <bencoding/utils_string.h>

#include <cassert>

namespace be
{
    struct KeyParser
    {
        const char* key_;
        bool (*parse_)(TorrentMetainfo& metainfo, ElementRef& element);
        bool found_;
        ElementPosition* position_;
    };

    inline const char* AsConstData(std::string_view str)
    {
        assert(!str.empty());
        return &(str[0]);
    }

    template<unsigned N>
    static bool InvokeParser(TorrentMetainfo& metainfo
        , KeyParser (&parsers)[N]
        , std::string_view key
        , ElementRef& value
        , bool& ok)
    {
        for (KeyParser& state : parsers)
        {
            if (state.found_)
            {
                continue;
            }
            if (key != state.key_)
            {
                continue;
            }
            state.found_ = true;
            ok = state.parse_(metainfo, value);
            if (state.position_)
            {
                *state.position_ = value.position();
            }
            return true;
        }
        return false;
    }

    static bool ParseAnnounce(TorrentMetainfo& metainfo, ElementRef& announce)
    {
        if (StringRef* str = announce.as_string())
        {
            if (!str->empty())
            {
                metainfo.tracker_url_utf8_.assign(
                    AsConstData(*str), str->size());
                return true;
            }
        }
        return false;
    }

    static bool ParseInfo_Name(TorrentMetainfo& metainfo, ElementRef& name)
    {
        if (StringRef* str = name.as_string())
        {
            // May be empty. It's just "suggested".
            if (!str->empty())
            {
                metainfo.info_.suggested_name_utf8_.assign(
                    AsConstData(*str), str->size());
            }
            return true;
        }
        return false;
    }

    static bool ParseInfo_PieceLength(TorrentMetainfo& metainfo, ElementRef& piece_length)
    {
        IntegerRef* n = piece_length.as_integer();
        if (!n)
        {
            return false;
        }

        std::uint64_t length_bytes = 0;
        if (!ParseLength(*n, length_bytes))
        {
            return false;
        }
        metainfo.info_.piece_length_bytes_ = length_bytes;
        return true;
    }

    static bool ParseInfo_Pieces(TorrentMetainfo& metainfo, ElementRef& pieces)
    {
        const std::size_t k_SHA1_length = 20;

        StringRef* hashes = pieces.as_string();
        if (!hashes)
        {
            return false;
        }
        const std::size_t size = hashes->size();
        if (size == 0)
        {
            return false;
        }
        if ((size % k_SHA1_length) != 0)
        {
            return false;
        }
        metainfo.info_.pieces_SHA1_.resize(hashes->size());
        std::memcpy(&metainfo.info_.pieces_SHA1_[0]
            , AsConstData(*hashes), hashes->size());
        metainfo.info_.pieces_SHA1_.shrink_to_fit();
        return true;
    }

    static bool ParseInfo_Length(TorrentMetainfo& metainfo, ElementRef& length)
    {
        if (metainfo.info_.length_or_files_.index() != 0)
        {
            // 'files' already placed/parsed.
            // Only 'length' or 'files' can exist, not both.
            return false;
        }

        IntegerRef* n = length.as_integer();
        if (!n)
        {
            return false;
        }
        std::uint64_t length_bytes = 0;
        if (!ParseLength(*n, length_bytes))
        {
            return false;
        }

        metainfo.info_.length_or_files_.emplace<1>(length_bytes);
        return true;
    }

    static std::string JoinUTF8PartsToString(const ListRef& parts, std::size_t total_length)
    {
        if (parts.empty())
        {
            return std::string();
        }
        const std::size_t count = parts.size();
        const std::size_t size = total_length
            + (count - 1);/*+1 for each slash / - dir separator*/

        std::string path;
        path.reserve(size);

        for (std::size_t i = 0; i < (count - 1); ++i)
        {
            const std::string_view part = *parts[i].as_string();
            path.append(&part[0], part.size());
            path.append("/");
        }

        const std::string_view last_part = *parts.back().as_string();
        path.append(&last_part[0], last_part.size());

        return path;
    }

    static std::optional<TorrentMetainfo::File> ParseInfo_FilesFile(ElementRef& length, ElementRef& path)
    {
        IntegerRef* n = length.as_integer();
        ListRef* path_parts = path.as_list();
        if (!n || !path_parts)
        {
            return std::nullopt;
        }
        if (path_parts->empty())
        {
            return std::nullopt;
        }

        std::uint64_t length_bytes = 0;
        if (!ParseLength(*n, length_bytes))
        {
            return std::nullopt;
        }

        // Validate that path is actually array of non-empty strings.
        std::size_t total_length = 0;
        for (const ElementRef& part : *path_parts)
        {
            const StringRef* name = part.as_string();
            if (!name || name->empty())
            {
                return std::nullopt;
            }
            total_length += name->length();
        }

        TorrentMetainfo::File file;
        file.length_bytes_ = length_bytes;
        file.path_utf8_ = JoinUTF8PartsToString(*path_parts, total_length);
        return std::optional<TorrentMetainfo::File>(std::move(file));
    }

    static bool ParseInfo_Files(TorrentMetainfo& metainfo, ElementRef& files_)
    {
        if (metainfo.info_.length_or_files_.index() != 0)
        {
            // 'length' already placed/parsed.
            // Only 'length' or 'files' can exist, not both.
            return false;
        }
        ListRef* list = files_.as_list();
        if (!list)
        {
            return false;
        }

        auto parse_file = [](ElementRef& e) -> std::optional<TorrentMetainfo::File>
        {
            DictionaryRef* data = e.as_dictionary();
            if (!data)
            {
                return std::nullopt;
            }

            ElementRef* length = nullptr;
            ElementRef* path = nullptr;
            for (auto& [name, element] : *data)
            {
                if ((name == "length") && !length)
                {
                    length = &element;
                }
                else if ((name == "path") && !path)
                {
                    path = &element;
                }
            }
            if (!length || !path)
            {
                return std::nullopt;
            }
            return ParseInfo_FilesFile(*length, *path);
        };

        std::vector<TorrentMetainfo::File> files;
        for (ElementRef& e : *list)
        {
            if (std::optional<TorrentMetainfo::File> file = parse_file(e))
            {
                files.push_back(std::move(*file));
            }
            else
            {
                return false;
            }
        }

        if (files.empty())
        {
            return false;
        }

        files.shrink_to_fit();
        metainfo.info_.length_or_files_.emplace<2>(std::move(files));
        return true;
    }

    static bool ParseInfo(TorrentMetainfo& metainfo, ElementRef& info)
    {
        DictionaryRef* data = info.as_dictionary();
        if (!data)
        {
            return false;
        }

        KeyParser k_parsers[] =
        {
            {"name",         &ParseInfo_Name,        false, nullptr},
            {"piece length", &ParseInfo_PieceLength, false, nullptr},
            {"pieces",       &ParseInfo_Pieces,      false, nullptr},
            {"length",       &ParseInfo_Length,      false, nullptr},
            {"files",        &ParseInfo_Files,       false, nullptr},
        };

        for (auto& [name, element] : *data)
        {
            bool ok = false;
            if (InvokeParser(metainfo, k_parsers, name, element, ok)
                && !ok)
            {
                return false;
            }
        }

        if ((metainfo.tracker_url_utf8_.size() > 0)
            && (metainfo.info_.piece_length_bytes_ > 0)
            // if exists, guarantees to be divisible by 20.
            && (metainfo.info_.pieces_SHA1_.size() > 0)
            // either 'length' or 'files' should be in place.
            && (metainfo.info_.length_or_files_.index() != 0))
        {
            return true;
        }

        return true;
    }

    std::optional<TorrentFileInfo> ParseTorrentFileContent(std::string_view content)
    {
        Parsed<DictionaryRef> data = ParseDictionary(content);
        if (!data.has_value())
        {
            return std::nullopt;
        }

        ElementPosition info_position;
        KeyParser k_parsers[] =
        {
            {"announce", &ParseAnnounce, false, nullptr},
            {"info",     &ParseInfo,     false, &info_position},
        };

        TorrentMetainfo metainfo;
        for (auto& [name, element] : data.value())
        {
            bool ok = false;
            if (InvokeParser(metainfo, k_parsers, name, element, ok)
                && !ok)
            {
                return std::nullopt;
            }
        }

        for (const auto& state : k_parsers)
        {
            if (!state.found_)
            {
                return std::nullopt;
            }
        }

        TorrentFileInfo info;
        info.torrent_file_ = std::move(metainfo);
        info.info_position_ = info_position;
        return std::optional<TorrentFileInfo>(std::move(info));
    }
} // namespace be
