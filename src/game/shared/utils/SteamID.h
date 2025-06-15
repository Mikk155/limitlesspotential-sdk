#pragma once

struct SteamID
{
    static constexpr uint64_t steamOffset = 76561197960265728ULL;

    private:

        uint64_t Allocateuint64( const char* _sid )
        {
            return static_cast<uint64_t>( std::stoull( std::string( _sid ) ) );
        }

        std::string AllocateSteamString( const char* _sid )
        {
            uint64_t authID = Allocateuint64(_sid) - steamOffset;

            uint32_t authFirst = authID % 2;

            uint32_t authSecond = static_cast<uint32_t>( authID / 2 );

            return fmt::format( "STEAM_0:{}:{}", std::to_string( authFirst ), std::to_string( authSecond ) );
        }

        std::string AllocateSteamProfile( const char* _sid )
        {
            return fmt::format( "https://steamcommunity.com/profiles/{}", _sid );
        }

    public:

        // Steam id in uint64 format as a string form
        const std::string sid;

        // Steam id in uint64 format
        const uint64_t steamID64;

        // Steam id in string format (STEAM_0:{}:{})
        const std::string steamID;

        // Steam profile url "https://steamcommunity.com/profiles/{steamID64}"
        const std::string steamProfile;

        // Player owner ID if > 0
        const int owner;

        SteamID( const char* _sid, int _owner = 0 ) :
            sid( std::move( _sid ) ),
            steamID64( Allocateuint64( _sid ) ),
            steamID( std::move( AllocateSteamString( _sid ) ) ),
            steamProfile( std::move( AllocateSteamProfile( _sid ) ) ),
            owner( _owner )
        {}

        // Compare the given Steam ID in uint64 or STEAM_0 formats
        bool Equals( std::string_view other ) const
        {
            // Compare uint64 in string form
            if( other == std::string_view( sid ) )
            {
                return true;
            }

            if( other == std::string_view( steamID ) )
            {
                return true;
            }

            return false;
        }
};

#ifdef CLIENT_DLL
inline std::shared_ptr<SteamID> ClientSteamIDPointer;

inline uint64_t ClientSteamIDUint64;

// Get the steamID on the client side. Note: the client must join a server first or this will be nullptr
inline SteamID* GetClientSteamID()
{
    if( !ClientSteamIDUint64 )
    {
        return nullptr;
    }

    if( !ClientSteamIDPointer )
    {
        ClientSteamIDPointer = std::make_shared<SteamID>( std::to_string( ClientSteamIDUint64 ).c_str() );
    }

    return ClientSteamIDPointer.get();
}
#endif
