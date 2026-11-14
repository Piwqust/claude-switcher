// A per-user default for ordinary Claude Code launches. Desktop remains the
// owner of its refresh tokens; only its current access token is handed to Code.
#pragma once
#include "core.h"

namespace CodeAuth {
    std::wstring Selected();
    void Use(const Profile& profile);
    void Restore();
    void Sync();
}
