#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_element_ref_parse.h>

#include <small_utils/utils_string.h>

#include <cassert>

namespace be
{
    struct KeyParser
    {
        const char* key_;
        outcome::result<void> (*parse_)(TorrentMetainfo& metainfo, ElementRef& element);
        bool parsed_;
        ElementPosition* position_;
    };

    template<unsigned N>
    static outcome::result<void> InvokeParser(TorrentMetainfo& metainfo
        , KeyParser (&parsers)[N]
        , std::string_view key
        , ElementRef& value)
    {
        for (KeyParser& state : parsers)
        {
            if ((state.parsed_) || (key != state.key_))
            {
                continue;
            }
            state.parsed_ = true;
            auto result = state.parse_(metainfo, value);
            if (state.position_)
            {
                *state.position_ = value.position();
            }
            return result;
        }
        return outcome::failure(ParseTorrentErrorc::Impl_NoKeyFound);
    }

    static outcome::result<void> ParseAnnounce(TorrentMetainfo& metainfo, ElementRef& announce)
    {
        if (StringRef* str = announce.as_string())
        {
            if (!str->empty())
            {
                metainfo.tracker_url_utf8_.assign(
                    AsConstData(*str), str->size());
                return outcome::success();
            }
            return outcome::failure(ParseTorrentErrorc::EmptyAnnounce);
        }
        return outcome::failure(ParseTorrentErrorc::InvalidAnnounceType);
    }

    static outcome::result<void> ParseInfo_Name(TorrentMetainfo& metainfo, ElementRef& name)
    {
        if (StringRef* str = name.as_string())
        {
            // May be empty. It's just "suggested".
            if (!str->empty())
            {
                metainfo.info_.suggested_name_utf8_.assign(
                    AsConstData(*str), str->size());
            }
            return outcome::success();
        }
        return outcome::failure(ParseTorrentErrorc::InvalidInfoNameType);
    }

    static outcome::result<void> ParseInfo_PieceLength(TorrentMetainfo& metainfo, ElementRef& piece_length)
    {
        IntegerRef* n = piece_length.as_integer();
        if (!n)
        {
            return outcome::failure(ParseTorrentErrorc::InvalidInfoPieceLengthType);
        }

        std::uint64_t length_bytes = 0;
        if (!ParseLength(*n, length_bytes))
        {
            return outcome::failure(ParseTorrentErrorc::InvalidInfoPieceLengthValue);
        }
        metainfo.info_.piece_length_bytes_ = length_bytes;
        return outcome::success();
    }

    static outcome::result<void> ParseInfo_Pieces(TorrentMetainfo& metainfo, ElementRef& pieces)
    {
        const std::size_t k_SHA1_length = 20;

        StringRef* hashes = pieces.as_string();
        if (!hashes)
        {
            return outcome::failure(ParseTorrentErrorc::InvalidInfoPiecesType);
        }
        const std::size_t size = hashes->size();
        if (size == 0)
        {
            return outcome::failure(ParseTorrentErrorc::EmptyInfoPieces);
        }
        if ((size % k_SHA1_length) != 0)
        {
            return outcome::failure(ParseTorrentErrorc::InvalidInfoPiecesLength20);
        }
        metainfo.info_.pieces_SHA1_.resize(hashes->size());
        std::memcpy(&metainfo.info_.pieces_SHA1_[0]
            , AsConstData(*hashes), hashes->size());
        metainfo.info_.pieces_SHA1_.shrink_to_fit();
        return outcome::success();
    }

    static outcome::result<void> ParseInfo_Length(TorrentMetainfo& metainfo, ElementRef& length)
    {
        if (metainfo.info_.length_or_files_.index() != 0)
        {
            // 'files' already placed/parsed.
            // Only 'length' or 'files' can exist, not both.
            return outcome::failure(ParseTorrentErrorc::AmbiguousMultiOrSingleTorrent);
        }

        IntegerRef* n = length.as_integer();
        if (!n)
        {
            return outcome::failure(ParseTorrentErrorc::InvalidInfoLengthType);
        }
        std::uint64_t length_bytes = 0;
        if (!ParseLength(*n, length_bytes))
        {
            return outcome::failure(ParseTorrentErrorc::InvalidInfoLengthValue);
        }

        metainfo.info_.length_or_files_.emplace<1>(length_bytes);
        return outcome::success();
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

    static outcome::result<TorrentMetainfo::File> ParseInfo_FilesFile(ElementRef& length, ElementRef& path)
    {
        IntegerRef* n = length.as_integer();
        ListRef* path_parts = path.as_list();
        if (!n || !path_parts)
        {
            return outcome::failure(ParseTorrentErrorc::TODO);
        }
        if (path_parts->empty())
        {
            return outcome::failure(ParseTorrentErrorc::TODO);
        }

        std::uint64_t length_bytes = 0;
        if (!ParseLength(*n, length_bytes))
        {
            return outcome::failure(ParseTorrentErrorc::TODO);
        }

        // Validate that path is actually array of non-empty strings.
        std::size_t total_length = 0;
        for (const ElementRef& part : *path_parts)
        {
            const StringRef* name = part.as_string();
            if (!name || name->empty())
            {
                return outcome::failure(ParseTorrentErrorc::TODO);
            }
            total_length += name->length();
        }

        TorrentMetainfo::File file;
        file.length_bytes_ = length_bytes;
        file.path_utf8_ = JoinUTF8PartsToString(*path_parts, total_length);
        return outcome::success(std::move(file));
    }

    static outcome::result<void> ParseInfo_Files(TorrentMetainfo& metainfo, ElementRef& files_)
    {
        if (metainfo.info_.length_or_files_.index() != 0)
        {
            // 'length' already placed/parsed.
            // Only 'length' or 'files' can exist, not both.
            return outcome::failure(ParseTorrentErrorc::AmbiguousMultiOrSingleTorrent);
        }
        ListRef* list = files_.as_list();
        if (!list)
        {
            return outcome::failure(ParseTorrentErrorc::TODO);
        }

        auto parse_file = [](ElementRef& e)
            -> outcome::result<TorrentMetainfo::File>
        {
            DictionaryRef* data = e.as_dictionary();
            if (!data)
            {
                return outcome::failure(ParseTorrentErrorc::TODO);
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
                return outcome::failure(ParseTorrentErrorc::TODO);
            }
            return ParseInfo_FilesFile(*length, *path);
        };

        std::vector<TorrentMetainfo::File> files;
        for (ElementRef& e : *list)
        {
            OUTCOME_TRY(file, parse_file(e));
            files.push_back(std::move(file));
        }

        if (files.empty())
        {
            return outcome::failure(ParseTorrentErrorc::TODO);
        }

        files.shrink_to_fit();
        metainfo.info_.length_or_files_.emplace<2>(std::move(files));
        return outcome::success();
    }

    static outcome::result<void> ParseInfo(TorrentMetainfo& metainfo, ElementRef& info)
    {
        DictionaryRef* data = info.as_dictionary();
        if (!data)
        {
            return outcome::failure(ParseTorrentErrorc::InvalidInfoKeyType);
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
            auto result = InvokeParser(metainfo, k_parsers, name, element);
            if (result)
            {
                continue;
            }
            if (result.error() == ParseTorrentErrorc::Impl_NoKeyFound)
            {
                // That's fine. Some of the keys are optional.
                // Final object's invariant will be validated at the end.
                continue;
            }
            return result;
        }

        if ((metainfo.tracker_url_utf8_.size() > 0)
            && (metainfo.info_.piece_length_bytes_ > 0)
            // if exists, guarantees to be divisible by 20.
            && (metainfo.info_.pieces_SHA1_.size() > 0)
            // either 'length' or 'files' should be in place.
            && (metainfo.info_.length_or_files_.index() != 0))
        {
            return outcome::success();
        }

        return outcome::failure(ParseTorrentErrorc::InvalidInvariant);
    }

    outcome::result<TorrentFileInfo> ParseTorrentFileContent(std::string_view content)
    {
        OUTCOME_TRY(data, ParseDictionary(content));

        ElementPosition info_position;
        KeyParser k_parsers[] =
        {
            {"announce", &ParseAnnounce, false, nullptr},
            {"info",     &ParseInfo,     false, &info_position},
        };

        TorrentMetainfo metainfo;
        for (auto& [name, element] : data)
        {
            auto result = InvokeParser(metainfo, k_parsers, name, element);
            if (result)
            {
                continue;
            }
            if (result.error() == ParseTorrentErrorc::Impl_NoKeyFound)
            {
                continue;
            }
            return outcome::failure(result.error());
        }

        for (const auto& state : k_parsers)
        {
            if (!state.parsed_)
            {
                return outcome::failure(ParseTorrentErrorc::TODO);
            }
        }

        TorrentFileInfo info;
        info.metainfo_ = std::move(metainfo);
        info.info_position_ = info_position;
        return outcome::success(std::move(info));
    }
} // namespace be
