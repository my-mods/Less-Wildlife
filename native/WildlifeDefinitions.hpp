#pragma once
#include <array>
#include <string_view>

namespace LessWildlife {
enum class Species : unsigned { None = 0, Boar = 1, Wolf = 2 };
struct WildlifeDefinition { std::wstring_view path; Species species; };
// Exact stock definitions. Quest, boss and summoned definitions are excluded;
// encounter conditions and scripted spawn rules are checked separately.
inline constexpr std::array wildlifeDefinitions{
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/Boar/NPCDef_Boar_Base.NPCDef_Boar_Base_C", Species::Boar},
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/Boar/NPCDef_Boar_NewAI.NPCDef_Boar_NewAI_C", Species::Boar},
    WildlifeDefinition{L"/Game/_Dawnwalker/NPC/BaseDefinitions/Enemies/NPCDef_Boar.NPCDef_Boar_C", Species::Boar},
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/Wolf/NPCDef_Wolf_Base.NPCDef_Wolf_Base_C", Species::Wolf},
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/Wolf/NewAI/NPCDef_Wolf_NewAI.NPCDef_Wolf_NewAI_C", Species::Wolf},
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/EnemyVariants/Wolf/NPCDef_Wolf_White.NPCDef_Wolf_White_C", Species::Wolf},
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/EnemyVariants/Wolf/NPCDef_Wolf_Brown.NPCDef_Wolf_Brown_C", Species::Wolf},
    WildlifeDefinition{L"/Game/_Dawnwalker/NPC/BaseDefinitions/Enemies/NPCDef_Wolf.NPCDef_Wolf_C", Species::Wolf},
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/AstralEnemies/AstralWolf/NPCDef_Astral_Wolf_Base.NPCDef_Astral_Wolf_Base_C", Species::Wolf},
    WildlifeDefinition{L"/Game/_Dawnwalker/Combat/Enemies/EnemyVariants/AstralWolf/NPCDef_Astral_Wolf_Dark.NPCDef_Astral_Wolf_Dark_C", Species::Wolf}
};
constexpr Species speciesOf(std::wstring_view path) {
    for (const auto& item : wildlifeDefinitions) if (item.path == path) return item.species;
    return Species::None;
}
constexpr unsigned replacementSettings(long long boars, long long wolves) {
    return (boars == 100 ? 1u : 0u) | (wolves == 100 ? 2u : 0u);
}
}
