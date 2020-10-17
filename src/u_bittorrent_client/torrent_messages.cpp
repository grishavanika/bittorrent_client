#include "torrent_messages.h"
#include <small_utils/utils_bytes.h>
#include <small_utils/utils_string.h>

#include <cstring>
#include <cassert>

namespace be
{
    /*static*/ auto Message_Handshake::SerializeDefault(
        const SHA1Bytes& info_hash, const PeerId& client_id)
            -> Buffer
    {
        const ExtensionsBuffer reserved; // No extensions.

        Buffer buffer;
        BytesWriter::make(buffer.data_)
            .write(std::uint8_t(SizeNoNull(k_protocol)))
            .write_str_no_null(k_protocol)
            .write(reserved.data_)
            .write(info_hash.data_)
            .write(client_id.data_)
            .finalize();
        return buffer;
    }

    /*static*/ std::optional<Message_Handshake> Message_Handshake::Parse(const Buffer& buffer)
    {
        std::optional<Message_Handshake> m(std::in_place);
        const bool ok = BytesReader::make(buffer.data_)
            .read(m->protocol_length_)
            .consume(m->pstr_, m->protocol_length_/*how many*/)
            .read(m->reserved_.data_)
            .read(m->info_hash_.data_)
            .read(m->peer_id_.data_)
            .finalize();
        return (ok ? m : std::nullopt);
    }
}
