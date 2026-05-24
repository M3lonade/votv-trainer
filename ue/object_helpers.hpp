#pragma once

#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_structs.hpp"

#include <string>
#include <vector>

namespace votv::ue {

std::string name_to_string(const SDK::FName& name);
std::vector<std::string> fname_array_to_strings(const SDK::TArray<SDK::FName>& names, int limit = 5000);
SDK::FTransform make_spawn_transform(const SDK::FTransform& player_transform, float forward_offset);

} // namespace votv::ue
