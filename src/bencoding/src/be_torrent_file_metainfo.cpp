#include <bencoding/be_torrent_file_metainfo.h>
#include <bencoding/be_element_ref_decoder.h>
#include <bencoding/utils_string.h>

namespace be
{
    namespace
    {
        struct KeyParser
        {
            const char* key_;
            bool (*parse_)(TorrentFile& metainfo, ElementRef& ref);
            bool found_;
        };

        template<unsigned N>
        bool InvokeParser(TorrentFile& metainfo, KeyParser (&parsers)[N]
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
                if (key == state.key_)
                {
                    state.found_ = true;
                    ok = state.parse_(metainfo, value);
                    return true;
                }
            }
            return false;
        }

        bool ParseAnnounce(TorrentFile& metainfo, ElementRef& announce_ref)
        {
            if (StringRef* str = announce_ref.as_string())
            {
                if (str->size() != 0)
                {
                    metainfo.tracker_url_utf8_.assign(
                        &((*str)[0]), str->size());
                    return true;
                }
            }
            return false;
        }

        bool ParseInfo_Name(TorrentFile& metainfo, ElementRef& ref)
        {
            if (StringRef* str = ref.as_string())
            {
                // May be empty. It's just "suggested".
                if (str->empty())
                {
                    metainfo.info_.suggested_name_utf8_.clear();
                }
                else
                {
                    metainfo.info_.suggested_name_utf8_.assign(
                        &((*str)[0]), str->size());
                }
                return true;
            }
            return false;
        }

        bool ParseInfo_PieceLength(TorrentFile& metainfo, ElementRef& ref)
        {
            IntegerRef* integer = ref.as_integer();
            if (!integer)
            {
                return false;
            }
            std::uint64_t length_bytes = 0;
            if (!ParseLength(*integer, length_bytes))
            {
                return false;
            }

            metainfo.info_.piece_length_bytes_ = length_bytes;
            return true;
        }

        bool ParseInfo_Pieces(TorrentFile& metainfo, ElementRef& ref)
        {
            const std::size_t k_SHA1_length = 20;

            StringRef* hashes = ref.as_string();
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
                , &((*hashes)[0]), hashes->size());
            metainfo.info_.pieces_SHA1_.shrink_to_fit();
            return true;
        }

        bool ParseInfo_Length(TorrentFile& metainfo, ElementRef& ref)
        {
            if (metainfo.info_.length_or_files_.index() != 0)
            {
                // 'files' already placed/parsed.
                // Only 'length' or 'files' can exist, not both.
                return false;
            }

            IntegerRef* integer = ref.as_integer();
            if (!integer)
            {
                return false;
            }
            std::uint64_t length_bytes = 0;
            if (!ParseLength(*integer, length_bytes))
            {
                return false;
            }

            metainfo.info_.length_or_files_.emplace<1>(length_bytes);
            return true;
        }

        std::string JoinUTF8PartsToString(const ListRef& parts, std::size_t total_length)
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

        std::optional<TorrentFile::File> ParseInfo_FilesFile(ElementRef& length, ElementRef& path)
        {
            IntegerRef* integer = length.as_integer();
            ListRef* path_parts = path.as_list();
            if (!integer || !path_parts)
            {
                return std::nullopt;
            }
            if (path_parts->empty())
            {
                return std::nullopt;
            }

            std::uint64_t length_bytes = 0;
            if (!ParseLength(*integer, length_bytes))
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

            TorrentFile::File file;
            file.length_bytes_ = length_bytes;
            file.path_utf8_ = JoinUTF8PartsToString(*path_parts, total_length);
            return std::optional<TorrentFile::File>(std::move(file));
        }

        bool ParseInfo_Files(TorrentFile& metainfo, ElementRef& ref)
        {
            if (metainfo.info_.length_or_files_.index() != 0)
            {
                // 'length' already placed/parsed.
                // Only 'length' or 'files' can exist, not both.
                return false;
            }
            ListRef* list = ref.as_list();
            if (!list)
            {
                return false;
            }

            auto parse_file = [](ElementRef& e) -> std::optional<TorrentFile::File>
            {
                DictionaryRef* data = e.as_dictionary();
                if (!data)
                {
                    return std::nullopt;
                }

                ElementRef* length = nullptr;
                ElementRef* path = nullptr;
                for (auto& [name, ref] : *data)
                {
                    if (name == "length")
                    {
                        length = &ref;
                    }
                    else if (name == "path")
                    {
                        path = &ref;
                    }
                }
                if (!length || !path)
                {
                    return std::nullopt;
                }
                return ParseInfo_FilesFile(*length, *path);
            };

            std::vector<TorrentFile::File> files;
            for (ElementRef& e : *list)
            {
                std::optional<TorrentFile::File> file = parse_file(e);
                if (!file)
                {
                    return false;
                }
                files.push_back(std::move(*file));
            }

            if (files.empty())
            {
                return false;
            }

            files.shrink_to_fit();
            metainfo.info_.length_or_files_.emplace<2>(std::move(files));
            return true;
        }

        bool ParseInfo(TorrentFile& metainfo, ElementRef& info_ref)
        {
            DictionaryRef* data = info_ref.as_dictionary();
            if (!data)
            {
                return false;
            }

            KeyParser k_parsers[] =
            {
                {"name",         &ParseInfo_Name,        false},
                {"piece length", &ParseInfo_PieceLength, false},
                {"pieces",       &ParseInfo_Pieces,      false},
                {"length",       &ParseInfo_Length,      false},
                {"files",        &ParseInfo_Files,       false},
            };

            for (auto& [name, ref] : *data)
            {
                bool ok = false;
                if (InvokeParser(metainfo, k_parsers, name, ref, ok)
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
    } // namespace

    std::optional<TorrentFile> ParseTorrentFileContent(std::string_view content)
    {
        Decoded<DictionaryRef> data = DecodeDictionary(content);
        if (!data.has_value())
        {
            return std::nullopt;
        }

        KeyParser k_parsers[] =
        {
            {"announce", &ParseAnnounce, false},
            {"info",     &ParseInfo,     false},
        };

        TorrentFile metainfo;
        for (auto& [name, ref] : data.value())
        {
            bool ok = false;
            if (InvokeParser(metainfo, k_parsers, name, ref, ok)
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

        return std::optional<TorrentFile>(std::move(metainfo));
    }
} // namespace be
