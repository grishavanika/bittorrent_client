#include "client_errors.h"

namespace
{
    struct ClientErrorCategory : std::error_category
    {
        virtual const char* name() const noexcept override;
        virtual std::string message(int ev) const override;
    };

    const char* ClientErrorCategory::name() const noexcept
    {
        return "client";
    }

    std::string ClientErrorCategory::message(int ev) const
    {
        using E = ClientErrorc;
        switch (E(ev))
        {
        case E::Ok: return "<success>";
        case E::TODO: return "<todo>";
        }
        return "<unknown>";
    }
} // namespace

std::error_code make_error_code(ClientErrorc e)
{
    static const ClientErrorCategory domain;
    return {int(e), domain};
}
