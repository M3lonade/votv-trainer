#include "ue/object_helpers.hpp"

#include <algorithm>

namespace votv::ue {

std::string name_to_string(const SDK::FName& name)
{
    if (name.IsNone()) {
        return "None";
    }

    return name.ToString();
}

std::vector<std::string> fname_array_to_strings(const SDK::TArray<SDK::FName>& names, int limit)
{
    std::vector<std::string> result;
    const int count = std::min(names.Num(), limit);
    result.reserve(count);

    for (int i = 0; i < count; ++i) {
        result.push_back(name_to_string(names[i]));
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

SDK::FTransform make_spawn_transform(const SDK::FTransform& player_transform, float forward_offset)
{
    SDK::FTransform transform = player_transform;
    transform.Translation.X += forward_offset;
    transform.Translation.Z += 50.0f;
    transform.Scale3D = SDK::FVector(1.0f, 1.0f, 1.0f);
    return transform;
}

} // namespace votv::ue
