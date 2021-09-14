/*
 * Copyright (C) 2020-2021 trickerer <https://github.com/trickerer>
 */

#include "ScriptMgr.h"
#include "Config.h"
#include "Log.h"
#include "SpellAuraEffects.h"

//version
static constexpr uint32 TRAUMAS_REVISION = 65;

//config
bool   _traumasEnabled;
bool   _traumasEnableDirect;
bool   _traumasEnablePeriodic;
bool   _traumasCritsOnly;
float  _traumasChanceMultCreature;
float  _traumasChanceMultPlayer;
float  _traumasChanceMultCrit;
bool   _traumasEnableCreatures;
bool   _traumasEnableRankNormal;
bool   _traumasEnableRankElite;
bool   _traumasEnableRankRare;
bool   _traumasEnableRankRareElite;
bool   _traumasEnableRankBoss;
bool   _traumasEnablePlayers;
bool   _traumasEnablePlayerControlled;
bool   _traumasEnableInPVP;
uint32 _traumasHpPctThreshold;
uint32 _traumasTriggerHpPctThreshold;
bool   _traumasEnableCure;
float  _traumasCureChanceMult;
uint32 _traumasDurationOverride;

//defs
static constexpr uint32 SPELLFAMILY_TRAUMA = SPELLFAMILY_UNK1;
static constexpr uint32 SPELLFAMILY_FLAG0_TRAUMA = 0x40000000;

enum TraumaTypes : uint8
{
    TRAUMA_EYE                          = 1,
    TRAUMA_LIMB                         = 2,
    TRAUMA_BODY                         = 3,
    TRAUMA_HEAD                         = 4,
    TRAUMA_INTERNAL                     = 5
};

static constexpr uint32 TRAUMA_NONE = 0;
static constexpr uint32 MAX_TRAUMAS = 5;
static constexpr size_t TRAUMA_MAP_SIZE = size_t(CREATURE_TYPE_GAS_CLOUD);

enum TraumaBaseSpells
{
    SPELL_TRAUMA_BASE_EYE               = 455001,
    SPELL_TRAUMA_BASE_LIMB              = 455002,
    SPELL_TRAUMA_BASE_BODY              = 455003,
    SPELL_TRAUMA_BASE_HEAD              = 455004,
    SPELL_TRAUMA_BASE_INTERNAL          = 455005
};

static constexpr uint32 SPELL_TRAUMA_BASE[MAX_TRAUMAS] =
{
    SPELL_TRAUMA_BASE_EYE,
    SPELL_TRAUMA_BASE_LIMB,
    SPELL_TRAUMA_BASE_BODY,
    SPELL_TRAUMA_BASE_HEAD,
    SPELL_TRAUMA_BASE_INTERNAL
};

namespace trauma_traits
{
template <TraumaTypes, CreatureType> struct Is_viable      { enum { value = true  }; };

#define NOT_VIABLE(t,c) template <>  struct Is_viable<t,c> { enum { value = false }; };
#define NOT_VIABLE_ALL(c) NOT_VIABLE(TRAUMA_EYE,c) NOT_VIABLE(TRAUMA_LIMB,c) NOT_VIABLE(TRAUMA_BODY,c) NOT_VIABLE(TRAUMA_HEAD,c) NOT_VIABLE(TRAUMA_INTERNAL,c)

/*Giant*/      NOT_VIABLE(TRAUMA_BODY, CREATURE_TYPE_GIANT) NOT_VIABLE(TRAUMA_INTERNAL, CREATURE_TYPE_GIANT)
/*Undead*/     NOT_VIABLE(TRAUMA_EYE, CREATURE_TYPE_UNDEAD) NOT_VIABLE(TRAUMA_BODY, CREATURE_TYPE_UNDEAD)
/*Undead*/     NOT_VIABLE(TRAUMA_INTERNAL, CREATURE_TYPE_UNDEAD)
/*Mechanical*/ NOT_VIABLE(TRAUMA_EYE, CREATURE_TYPE_MECHANICAL) NOT_VIABLE(TRAUMA_BODY, CREATURE_TYPE_MECHANICAL)
/*NSpecified*/ NOT_VIABLE_ALL(CREATURE_TYPE_NOT_SPECIFIED)
/*Totem*/      NOT_VIABLE_ALL(CREATURE_TYPE_TOTEM)
/*Gas Cloud*/  NOT_VIABLE_ALL(CREATURE_TYPE_GAS_CLOUD)

template <TraumaTypes t, CreatureType c>
constexpr bool Is_viable_v = Is_viable<t, c>::value;
}

struct TraumaViability
{
    const TraumaTypes t_type;
    const bool viable;
};

typedef std::pair<const CreatureType, const TraumaViability> TraumaViabilityPair;
typedef std::array<TraumaViabilityPair, MAX_TRAUMAS> TraumaViabilityContainer;
typedef std::array<TraumaViabilityContainer, TRAUMA_MAP_SIZE> TraumaViabilityMap;
#define Tpair(t,c) TraumaViabilityPair{c, TraumaViability{t, trauma_traits::Is_viable_v<t, c>}}
#define Tpairs(c) Tpair(TRAUMA_EYE,c), Tpair(TRAUMA_LIMB,c), Tpair(TRAUMA_BODY,c), Tpair(TRAUMA_HEAD,c), Tpair(TRAUMA_INTERNAL,c)

static constexpr TraumaViabilityMap TraumasMap = {
    Tpairs(CREATURE_TYPE_BEAST),
    Tpairs(CREATURE_TYPE_DRAGONKIN),
    Tpairs(CREATURE_TYPE_DEMON),
    Tpairs(CREATURE_TYPE_ELEMENTAL),
    Tpairs(CREATURE_TYPE_GIANT),
    Tpairs(CREATURE_TYPE_UNDEAD),
    Tpairs(CREATURE_TYPE_HUMANOID),
    Tpairs(CREATURE_TYPE_CRITTER),
    Tpairs(CREATURE_TYPE_MECHANICAL),
    Tpairs(CREATURE_TYPE_NOT_SPECIFIED),
    Tpairs(CREATURE_TYPE_TOTEM),
    Tpairs(CREATURE_TYPE_NON_COMBAT_PET),
    Tpairs(CREATURE_TYPE_GAS_CLOUD)
};

//scripts
class traumas_config : public WorldScript
{
public:
    traumas_config() : WorldScript("traumas_config") { }

    void OnConfigLoad(bool reload) override
    {
        _InitTraumasSystem(reload);
    }

private:
    static void _InitTraumasSystem(bool reload)
    {
        if (!reload)
            TC_LOG_INFO("server.loading", "Loading Traumas system...");

        _LoadConfig();

        TC_LOG_INFO("server.loading", ">> Traumas config %s.", reload ? "re-loaded" : "loaded");

        if (_traumasEnabled)
            TC_LOG_INFO("server.loading", ">> Traumas system enabled (rev %u)", TRAUMAS_REVISION);
    }

    static void _LoadConfig()
    {
        _traumasEnabled                = sConfigMgr->GetBoolDefault("Trauma.Enable", true);

        _traumasEnableDirect           = sConfigMgr->GetBoolDefault("Trauma.Damage.Direct", true);
        _traumasEnablePeriodic         = sConfigMgr->GetBoolDefault("Trauma.Damage.Periodic", true);

        _traumasEnableCreatures        = sConfigMgr->GetBoolDefault("Trauma.Target.Creature", true);
        _traumasEnableRankNormal       = sConfigMgr->GetBoolDefault("Trauma.Target.Creature.Rank.Normal", true);
        _traumasEnableRankElite        = sConfigMgr->GetBoolDefault("Trauma.Target.Creature.Rank.Elite", true);
        _traumasEnableRankRare         = sConfigMgr->GetBoolDefault("Trauma.Target.Creature.Rank.Rare", true);
        _traumasEnableRankRareElite    = sConfigMgr->GetBoolDefault("Trauma.Target.Creature.Rank.RareElite", true);
        _traumasEnableRankBoss         = sConfigMgr->GetBoolDefault("Trauma.Target.Creature.Rank.Boss", false);

        _traumasEnablePlayers          = sConfigMgr->GetBoolDefault("Trauma.Target.Player", true);
        _traumasEnablePlayerControlled = sConfigMgr->GetBoolDefault("Trauma.Target.Player.Minions", true);
        _traumasEnableInPVP            = sConfigMgr->GetBoolDefault("Trauma.Target.Player.PVP", false);

        _traumasHpPctThreshold         = sConfigMgr->GetIntDefault("Trauma.HealthPctThreshold", 0);
        _traumasTriggerHpPctThreshold  = sConfigMgr->GetIntDefault("Trauma.TriggerHealthPctThreshold", 0);
        _traumasCritsOnly              = sConfigMgr->GetBoolDefault("Trauma.CritsOnly", true);
        _traumasChanceMultCreature     = sConfigMgr->GetFloatDefault("Trauma.ChanceMultiplier.Creature", 1.0f);
        _traumasChanceMultPlayer       = sConfigMgr->GetFloatDefault("Trauma.ChanceMultiplier.Player", 1.0f);
        _traumasChanceMultCrit         = sConfigMgr->GetFloatDefault("Trauma.ChanceMultiplier.Crit", 1.0f);

        _traumasEnableCure             = sConfigMgr->GetBoolDefault("Trauma.Cure.Enable", true);
        _traumasCureChanceMult         = sConfigMgr->GetFloatDefault("Trauma.Cure.ChanceMultiplier", 1.0f);

        _traumasDurationOverride       = sConfigMgr->GetIntDefault("Trauma.DurationOverride", 0);
    }
};

class script_traumas : public UnitScript
{
public:
    script_traumas() : UnitScript("script_traumas") { }

    void OnHeal(Unit* /*healer*/, Unit* reciever, uint32& gain) override
    {
        if (!_traumasEnabled)
            return;

        if (!_traumasEnableCure)
            return;

        float chance = reciever->IsFullHealth() ? 100.0f :
            (reciever->GetHealth() + gain >= reciever->GetMaxHealth()) ? 50.f : (gain * 100.f) / reciever->GetMaxHealth();
        chance *= _traumasCureChanceMult;

        if (!roll_chance_f(chance))
            return;

        //1 - eye
        if (reciever->HasAuraTypeWithFamilyFlags(SPELL_AURA_MOD_CRIT_PCT, SPELLFAMILY_TRAUMA, SPELLFAMILY_FLAG0_TRAUMA))
            reciever->RemoveAurasDueToSpell(SPELL_TRAUMA_BASE_EYE);
        //2 - limb
        else if (reciever->HasAuraTypeWithFamilyFlags(SPELL_AURA_MOD_MELEE_RANGED_HASTE, SPELLFAMILY_TRAUMA, SPELLFAMILY_FLAG0_TRAUMA))
            reciever->RemoveAurasDueToSpell(SPELL_TRAUMA_BASE_LIMB);
        //3 - body
        else if (reciever->HasAuraTypeWithFamilyFlags(SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT, SPELLFAMILY_TRAUMA, SPELLFAMILY_FLAG0_TRAUMA))
            reciever->RemoveAurasDueToSpell(SPELL_TRAUMA_BASE_BODY);
        //4 - head
        else if (reciever->HasAuraTypeWithFamilyFlags(SPELL_AURA_MOD_SPEED_SLOW_ALL, SPELLFAMILY_TRAUMA, SPELLFAMILY_FLAG0_TRAUMA))
            reciever->RemoveAurasDueToSpell(SPELL_TRAUMA_BASE_HEAD);
        //5 - innards
        else if (reciever->HasAuraTypeWithFamilyFlags(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELLFAMILY_TRAUMA, SPELLFAMILY_FLAG0_TRAUMA))
            reciever->RemoveAurasDueToSpell(SPELL_TRAUMA_BASE_INTERNAL);
    }

    void OnDamageEx(Unit* attacker, Unit* victim, uint32& damage, bool crit, bool direct, uint32 /*schoolMask*/) override
    {               /*may be NULL*/
        if (!_traumasEnabled)
            return;

        if (!attacker || !damage || !victim->IsAlive() || damage >= victim->GetHealth())
            return;

        if (direct ? !_traumasEnableDirect : !_traumasEnablePeriodic)
            return;
        if (_traumasHpPctThreshold && (victim->GetHealth() - damage) / victim->GetMaxHealth() > _traumasHpPctThreshold)
            return;
        if (_traumasTriggerHpPctThreshold && victim->GetMaxHealth() &&
            damage < uint32(CalculatePct(float(victim->GetMaxHealth()), float(_traumasTriggerHpPctThreshold))))
            return;
        if (victim->IsControlledByPlayer())
        {
            if (victim->GetTypeId() == TYPEID_PLAYER)
            {
                if (!_traumasEnablePlayers)
                    return;
            }
            else if (!_traumasEnablePlayerControlled)
                return;
            if (!_traumasEnableInPVP && attacker && attacker->IsControlledByPlayer())
                return;
        }
        else// if (victim->GetTypeId() == TYPEID_UNIT)
        {
            if (!_traumasEnableCreatures)
                return;

            uint32 rank = victim->ToCreature()->GetCreatureTemplate()->rank;
            if (!_traumasEnableRankNormal && rank == CREATURE_ELITE_NORMAL)
                return;
            if (!_traumasEnableRankElite && rank == CREATURE_ELITE_ELITE)
                return;
            if (!_traumasEnableRankRare && rank == CREATURE_ELITE_RARE)
                return;
            if (!_traumasEnableRankRareElite && rank == CREATURE_ELITE_RAREELITE)
                return;
            if (!_traumasEnableRankBoss && rank == CREATURE_ELITE_WORLDBOSS)
                return;
        }
        if (_traumasCritsOnly && !crit)
            return;

        //chance is (2x hp pct damage dealt)
        float chance = (damage * 200.f) / victim->GetMaxHealth();
        chance *= victim->IsControlledByPlayer() ? _traumasChanceMultPlayer : _traumasChanceMultCreature;
        if (crit)
            chance *= _traumasChanceMultCrit;

        uint32 defense = victim->GetDefenseSkillValue();
        if (defense > victim->GetMaxSkillValueForLevel())
        {
            static const float defenseBonusCap = 140.f;
            AddPct(chance, ((defense - victim->GetMaxSkillValueForLevel()) * -100.f) / defenseBonusCap);
            chance = std::max<float>(chance, 0.0f);
        }

        if (!roll_chance_f(chance))
            return;

        uint8 traumaType = _GetViableTraumaType(victim->GetCreatureType());
        if (traumaType == TRAUMA_NONE)
        {
            //TC_LOG_ERROR("scripts", "No viable trauma type for cre type %u", victim->GetCreatureType());
            return;
        }

        uint32 traumaSpellId = SPELL_TRAUMA_BASE[traumaType - 1];

        CastSpellExtraArgs args(true);
        args.SetOriginalCaster(attacker->GetGUID());
        victim->CastSpell(victim, traumaSpellId, args);

        if (_traumasDurationOverride)
        {
            if (Aura* trauma = victim->GetAura(traumaSpellId, attacker->GetGUID()))
            {
                trauma->SetDuration(int32(_traumasDurationOverride));
                trauma->SetMaxDuration(int32(_traumasDurationOverride));
            }
        }
    }

private:

    template<typename T = TraumaTypes, typename...Ts>
    inline static constexpr std::enable_if_t<std::disjunction_v<std::is_same<T,Ts>...>, std::array<T, sizeof...(Ts)>>
        make_arr(Ts... ts) noexcept { return { ts... }; }

    static uint8 _GetViableTraumaType(uint32 creatureType)
    {
        TraumaTypes ts[MAX_TRAUMAS]{};
        size_t count = 0;
        for (auto const& p : TraumasMap[creatureType])
            if (p.second.viable)
                ts[count++] = p.second.t_type;

        switch (count)
        {
            case 1: return ts[0];
            case 2: return Trinity::Containers::SelectRandomContainerElement(make_arr(ts[0], ts[1]));
            case 3: return Trinity::Containers::SelectRandomContainerElement(make_arr(ts[0], ts[1], ts[2]));
            case 4: return Trinity::Containers::SelectRandomContainerElement(make_arr(ts[0], ts[1], ts[2], ts[3]));
            case 5: return Trinity::Containers::SelectRandomContainerElement(make_arr(ts[0], ts[1], ts[2], ts[3], ts[4]));
            default:
                break;
        }

        return uint8(TRAUMA_NONE);
    }
};

void AddSC_traumas()
{
    new traumas_config();
    new script_traumas();
}
