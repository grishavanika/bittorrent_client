#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <bencoding/be_parse_utils.h>

#include <cassert>

namespace be
{
    const std::size_t k_SHA1_length = 20;

    struct KeyParser
    {
        const char* key_;
        outcome::result<void> (*parse_)(TorrentMetainfo& metainfo, ElementRef& element);
        bool parsed_;
        ElementPosition* position_;
    };

    template<unsigned N>
    static outcome::result<void> InvokeParserOptionalKey(TorrentMetainfo& metainfo
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
        return outcome::success();
    }

    static outcome::result<void> ParseAnnounce(TorrentMetainfo& metainfo, ElementRef& announce)
    {
        OUTCOME_TRY(str, be::ElementRefAs<StringRef>(announce));
        if (!str->empty())
        {
            metainfo.tracker_url_utf8_.assign(
                AsConstData(*str), str->size());
            return outcome::success();
        }
        return outcome::failure(ParseErrorc::EmptyAnnounce);
    }

    static outcome::result<void> ParseInfo_Name(TorrentMetainfo& metainfo, ElementRef& name)
    {
        OUTCOME_TRY(str, be::ElementRefAs<StringRef>(name));
        // May be empty. It's just "suggested".
        if (!str->empty())
        {
            metainfo.info_.suggested_name_utf8_.assign(
                AsConstData(*str), str->size());
        }
        return outcome::success();
    }

    static outcome::result<void> ParseInfo_PieceLength(TorrentMetainfo& metainfo, ElementRef& piece_length)
    {
        OUTCOME_TRY(n, be::ElementRefAs<be::IntegerRef>(piece_length));
        OUTCOME_TRY(length_bytes, ParseAsUint64(*n));
        metainfo.info_.piece_length_bytes_ = length_bytes;
        return outcome::success();
    }

    static outcome::result<void> ParseInfo_Pieces(TorrentMetainfo& metainfo, ElementRef& pieces)
    {
        OUTCOME_TRY(hashes, be::ElementRefAs<be::StringRef>(pieces));
        const std::size_t size = hashes->size();
        if ((size == 0) || ((size % k_SHA1_length) != 0))
        {
            return outcome::failure(ParseErrorc::InvalidInfoPiecesLength20);
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
            return outcome::failure(ParseErrorc::AmbiguousMultiOrSingleTorrent);
        }

        OUTCOME_TRY(n, be::ElementRefAs<IntegerRef>(length));
        OUTCOME_TRY(length_bytes, ParseAsUint64(*n));
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
        OUTCOME_TRY(n, be::ElementRefAs<be::IntegerRef>(length));
        OUTCOME_TRY(path_parts, be::ElementRefAs<be::ListRef>(path));
        OUTCOME_TRY(length_bytes, ParseAsUint64(*n));

        // Validate that path is actually array of non-empty strings.
        std::size_t total_length = 0;
        for (const ElementRef& part : *path_parts)
        {
            OUTCOME_TRY(name, be::ElementRefAs<StringRef>(part));
            if (name->empty())
            {
                return outcome::failure(ParseErrorc::EmptyMultiFileName);
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
            return outcome::failure(ParseErrorc::AmbiguousMultiOrSingleTorrent);
        }
        OUTCOME_TRY(list, be::ElementRefAs<ListRef>(files_));

        auto parse_file = [](ElementRef& e)
            -> outcome::result<TorrentMetainfo::File>
        {
            OUTCOME_TRY(data, be::ElementRefAs<DictionaryRef>(e));
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
                return outcome::failure(ParseErrorc::MissingMultiFileProperty);
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
            return outcome::failure(ParseErrorc::EmptyMultiFile);
        }

        files.shrink_to_fit();
        metainfo.info_.length_or_files_.emplace<2>(std::move(files));
        return outcome::success();
    }

    static outcome::result<void> ParseInfo(TorrentMetainfo& metainfo, ElementRef& info)
    {
        OUTCOME_TRY(data, be::ElementRefAs<DictionaryRef>(info));

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
            OUTCOME_TRY(InvokeParserOptionalKey(metainfo, k_parsers, name, element));
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

        return outcome::failure(ParseErrorc::InvalidInvariant);
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
            OUTCOME_TRY(InvokeParserOptionalKey(metainfo, k_parsers, name, element));
        }

        for (const auto& state : k_parsers)
        {
            if (!state.parsed_)
            {
                return outcome::failure(ParseErrorc::MissingInfoProperty);
            }
        }

        TorrentFileInfo info;
        info.metainfo_ = std::move(metainfo);
        info.info_position_ = info_position;
        return outcome::success(std::move(info));
    }
} // namespace be
