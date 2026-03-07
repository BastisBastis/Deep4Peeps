
#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/eq_constants.h"
#include "../common/eq_packet_structs.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/strings.h"
#include "../common/fastmath.h"
#include "../common/misc_functions.h"
#include "../common/events/player_event_logs.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "water_map.h"
#include "queryserv.h"
#include "worldserver.h"
#include "zone.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern WorldServer worldserver;
extern QueryServ* QServ;
extern FastMath g_Math;

#ifdef _WINDOWS
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#endif

extern EntityList entity_list;
extern Zone* zone;



// returns true on hit
bool Mob::AvoidanceCheck(Mob* attacker, EQ::skills::SkillType skillinuse)
{
	Mob *defender = this;

	if (IsClient() && CastToClient()->IsSitting())
	{
		if (IsClient() && attacker->IsNPC())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillDefense, attacker, zone->skill_difficulty[EQ::skills::SkillDefense].difficulty[GetClass()]);
		return true;
	}

	int toHit = attacker->GetToHit(skillinuse);
	int avoidance = defender->GetAvoidance();
	int toHitPct = 0;
	int avoidancePct = 0;
	
	// Hit Chance percent modifier
	// Disciplines: Evasive, Precision, Deadeye, Trueshot, Charge
	// Buffs: the Eagle Eye line
	toHitPct = attacker->itembonuses.HitChanceEffect[skillinuse] +
		attacker->spellbonuses.HitChanceEffect[skillinuse] +
		attacker->aabonuses.HitChanceEffect[skillinuse] +
		attacker->itembonuses.HitChanceEffect[EQ::skills::HIGHEST_SKILL + 1] +
		attacker->spellbonuses.HitChanceEffect[EQ::skills::HIGHEST_SKILL + 1] +
		attacker->aabonuses.HitChanceEffect[EQ::skills::HIGHEST_SKILL + 1];

	// Avoidance chance percent modifier
	// Disciplines: Evasive, Precision, Voiddance, Fortitude
	avoidancePct = defender->spellbonuses.AvoidMeleeChanceEffect + defender->itembonuses.AvoidMeleeChanceEffect;

	toHit = toHit * (100 + toHitPct) / 100;
	avoidance = avoidance * (100 + avoidancePct) / 100;

	Log(Logs::Detail, Logs::Attack, "AvoidanceCheck: %s attacked by %s;  Avoidance: %i (pct: %i)  To-Hit: %i (pct: %i)", 
		defender->GetName(), attacker->GetName(), avoidance, avoidancePct, toHit, toHitPct);

	double hitChance;
	toHit += 10;
	avoidance += 10;

	if (toHit * 1.21 > avoidance)
	{
		hitChance = 1.0 - avoidance / (toHit * 1.21 * 2.0);
	}
	else
	{
		hitChance = toHit * 1.21 / (avoidance * 2.0);
	}

	if (IsClient() && attacker->IsNPC())
		CastToClient()->CheckIncreaseSkill(EQ::skills::SkillDefense, attacker, zone->skill_difficulty[EQ::skills::SkillDefense].difficulty[GetClass()]);

	if (zone->random.Real(0.0, 1.0) < hitChance)
	{
		Log(Logs::Detail, Logs::Attack, "Hit;  Hit chance was %0.1f%%", hitChance*100);
		return true;
	}

	Log(Logs::Detail, Logs::Attack, "Miss;  Hit chance was %0.1f%%", hitChance * 100);
	return false;
}

bool Mob::AvoidDamage(Mob* attacker, int32 &damage, bool isRangedAttack)
{
	/* solar: called when a mob is attacked, does the checks to see if it's a hit
	* and does other mitigation checks. 'this' is the mob being attacked.
	*
	* special return values:
	* -1 - block
	* -2 - parry
	* -3 - riposte
	* -4 - dodge
	*
	*/
	Mob *defender = this;

	bool InFront = attacker->InFrontMob(this, attacker->GetX(), attacker->GetY());

	// block
	if (GetSkill(EQ::skills::SkillBlock))
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillBlock, attacker, zone->skill_difficulty[EQ::skills::SkillBlock].difficulty[GetClass()]);

			// check auto discs ... I guess aa/items too :P
		if (spellbonuses.IncreaseBlockChance == 10000 || aabonuses.IncreaseBlockChance == 10000 ||
			itembonuses.IncreaseBlockChance == 10000) {
			damage = DMG_BLOCK;
			return true;
		}
		int chance = GetSkill(EQ::skills::SkillBlock) + 100;
		chance += (chance * (aabonuses.IncreaseBlockChance + spellbonuses.IncreaseBlockChance + itembonuses.IncreaseBlockChance)) / 100;
		chance /= 25;

		if (zone->random.Roll(chance)) {
			damage = DMG_BLOCK;
			return true;
		}
	}

	// parry
	if (GetSkill(EQ::skills::SkillParry) && InFront && !isRangedAttack)
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillParry, attacker, zone->skill_difficulty[EQ::skills::SkillParry].difficulty[GetClass()]);

		// check auto discs ... I guess aa/items too :P
		if (spellbonuses.ParryChance == 10000 || aabonuses.ParryChance == 10000 || itembonuses.ParryChance == 10000) {
			damage = DMG_PARRY;
			return true;
		}
		int chance = GetSkill(EQ::skills::SkillParry) + 100;
		chance += (chance * (aabonuses.ParryChance + spellbonuses.ParryChance + itembonuses.ParryChance)) / 100;
		chance /= 50;		// this is 45 in modern EQ.  Old EQ logs parsed to a lower parry rate, so raising this

		if (zone->random.Roll(chance)) {
			damage = DMG_PARRY;
			return true;
		}
	}

	// riposte
	if (!isRangedAttack && GetSkill(EQ::skills::SkillRiposte) && InFront)
	{
		bool cannotRiposte = false;

		if (IsClient())
		{
			EQ::ItemInstance* weapon = nullptr;
			weapon = CastToClient()->GetInv().GetItem(EQ::invslot::slotPrimary);

			if (weapon != nullptr && !weapon->IsWeapon())
			{
				cannotRiposte = true;
			}
			else
			{
				CastToClient()->CheckIncreaseSkill(EQ::skills::SkillRiposte, attacker, zone->skill_difficulty[EQ::skills::SkillRiposte].difficulty[GetClass()]);
			}
		}

		// riposting ripostes is possible, but client attacks become unripable while under a rip disc
		if (attacker->IsEnraged() ||
			(attacker->IsClient() && (attacker->aabonuses.RiposteChance + attacker->spellbonuses.RiposteChance + attacker->itembonuses.RiposteChance) >= 10000)
		)
			cannotRiposte = true;

		if (!cannotRiposte)
		{
			if (IsEnraged() || spellbonuses.RiposteChance == 10000 || aabonuses.RiposteChance == 10000 || itembonuses.RiposteChance == 10000)
			{
				damage = DMG_RIPOSTE;
				return true;
			}

			int chance = GetSkill(EQ::skills::SkillRiposte) + 100;
			chance += (chance * (aabonuses.RiposteChance + spellbonuses.RiposteChance + itembonuses.RiposteChance)) / 100;
			chance /= 55;		// this is 50 in modern EQ.  Old EQ logs parsed to a lower rate, so raising this

			if (chance > 0 && zone->random.Roll(chance)) // could be <0 from offhand stuff
			{
				// March 19 2002 patch made pets not take non-enrage ripostes from NPCs.  it said 'more likely to avoid' but player comments say zero and logs confirm
				if (IsNPC() && attacker->IsPet())
				{
					damage = DMG_MISS;  // converting ripostes to misses.  don't know what Sony did but erring on conservative
					return true;
				}

				damage = DMG_RIPOSTE;
				return true;
			}
		}
	}

	// dodge
	if (GetSkill(EQ::skills::SkillDodge) && InFront)
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(EQ::skills::SkillDodge, attacker, zone->skill_difficulty[EQ::skills::SkillDodge].difficulty[GetClass()]);

		// check auto discs ... I guess aa/items too :P
		if (spellbonuses.DodgeChance == 10000 || aabonuses.DodgeChance == 10000 || itembonuses.DodgeChance == 10000) {
			damage = DMG_DODGE;
			return true;
		}
		int chance = GetSkill(EQ::skills::SkillDodge) + 100;
		chance += (chance * (aabonuses.DodgeChance + spellbonuses.DodgeChance + itembonuses.DodgeChance)) / 100;
		chance /= 45;

		if (zone->random.Roll(chance)) {
			damage = DMG_DODGE;
			return true;
		}
	}

	return false;
}

int Mob::RollD20(double offense, double mitigation)
{
	int atkRoll = zone->random.Roll0(offense + 5);
	int defRoll = zone->random.Roll0(mitigation + 5);

	int avg = (offense + mitigation + 10) / 2;
	int index = std::max(0, (atkRoll - defRoll) + (avg / 2));
	index = (index * 20) / avg;
	index = std::max(0, index);
	index = std::min(19, index);

	return index + 1;
}

// Our database uses a min hit and max hit system, instead of Sony's DB + baseDmg * 0.1-2.0
// This calcs a DB (which is minHit - DI) from min and max hits
int NPC::GetDamageBonus()
{
	if (min_dmg > max_dmg)
		return min_dmg;

	int di1k = ((max_dmg - min_dmg) * 1000) / 19;		// multiply damage interval by 1000 to avoid using floats
	di1k = (di1k + 50) / 100 * 100;						// round DI to nearest tenth of a point
	int db = max_dmg * 1000 - di1k * 20;

	return db / 1000;
}


// returns the client's weapon (or hand to hand) damage
// slot parameter should be one of: SlotPrimary, SlotSecondary, SlotRange, SlotAmmo
// does not check for immunities
// calling this with SlotRange will also add the arrow damage
int Client::GetBaseDamage(Mob *defender, int slot, bool throwing_dmg)
{
	if (slot != EQ::invslot::slotSecondary && slot != EQ::invslot::slotRange && slot != EQ::invslot::slotAmmo)
		slot = EQ::invslot::slotPrimary;

	int dmg = 0;

	EQ::ItemInstance* weaponInst = GetInv().GetItem(slot);
	const EQ::ItemData* weapon = nullptr;
	if (weaponInst)
		weapon = weaponInst->GetItem();

	if (weapon)
	{
		// cheaters or GMs doing stuff
		if (weapon->ReqLevel > GetLevel())
			return dmg;

		if (!weaponInst->IsEquipable(GetBaseRace(), GetClass()))
			return dmg;

		if (GetLevel() < weapon->RecLevel)
			dmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon->RecLevel, weapon->Damage);
		else
			dmg = weapon->Damage;

		if (weapon->ElemDmgAmt && !defender->GetSpecialAbility(SpecialAbility::MagicImmunity))
		{
			int eledmg = 0;

			if (GetLevel() < weapon->RecLevel)
				eledmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon->RecLevel, weapon->ElemDmgAmt);
			else
				eledmg = weapon->ElemDmgAmt;

			if (eledmg)
			{
				eledmg = CalcEleWeaponResist(eledmg, weapon->ElemDmgType, defender);
				dmg += eledmg;
			}
		}

		if (weapon->BaneDmgBody == defender->GetBodyType() || weapon->BaneDmgRace == defender->GetRace())
		{
			if (GetLevel() < weapon->RecLevel)
				dmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon->RecLevel, weapon->BaneDmgAmt);
			else
				dmg += weapon->BaneDmgAmt;
		}

		if (slot == EQ::invslot::slotRange && GetInv().GetItem(EQ::invslot::slotAmmo) && !throwing_dmg)
		{
			dmg += GetBaseDamage(defender, EQ::invslot::slotAmmo);
		}
	}
	else if (slot == EQ::invslot::slotPrimary || slot == EQ::invslot::slotSecondary)
		dmg = GetHandToHandDamage();

	return dmg;
}

// For both Clients and NPCs.  Sony calls client weapon damage and innate NPC damage 'base damage'
// NPC base damage is essentially DI * 10.  Special skills have their own base damage
// All melee, archery and throwing damage should pass through here
// For reference, NPC damage is: minHit = DB + DI*1; maxHit = DB + DI*20.  Clients do the same, but get a multiplier after it
int Mob::CalcMeleeDamage(Mob* defender, int baseDamage, EQ::skills::SkillType skill)
{
	if (!defender || !baseDamage)
		return 0;

	// ranged physical damage does half that of melee
	if ((skill == EQ::skills::SkillArchery || skill == EQ::skills::SkillThrowing) && baseDamage > 1)
		baseDamage /= 2;

	int offense = GetOffense(skill);

	// mitigation roll
	int roll = RollD20(offense, defender->GetMitigation());

	if (defender->IsClient() && defender->CastToClient()->IsSitting())
		roll = 20;

	// SE_MinDamageModifier[186] for disciplines: Fellstrike, Innerflame, Duelist, Bestial Rage
	// min hit becomes 4 x weapon damage + 1 x damage bonus
	int minHit = baseDamage * GetMeleeMinDamageMod_SE(skill) / 100;

	// SE_DamageModifier[185] for disciplines: Aggressive, Ashenhand, Bestial Rage, Defensive, Duelist,
	//                                         Fellstrike, Innerflame, Silentfist, Thunderkick
	baseDamage += baseDamage * GetMeleeDamageMod_SE(skill) / 100;

	// SE_MeleeMitigation[168] for disciplines: Defensive (-50), Stonestance & Protective Spirit (-90)
	//											Aggressive (+50)
	baseDamage += baseDamage * defender->GetSpellBonuses().MeleeMitigationEffect / 100;

	if (defender->IsClient() && IsPet() && GetOwner()->IsClient()) {
		// pets do reduced damage to clients in pvp
		baseDamage /= 2;
	}

	int damage = (roll * baseDamage + 5) / 10;
	if (damage < minHit) damage = minHit;
	if (damage < 1)
		damage = 1;

	if (IsClient())
		CastToClient()->RollDamageMultiplier(offense, damage, skill);

	return damage;
}

// the output of this function is precise and is based on the code from:
// https://forums.daybreakgames.com/eq/index.php?threads/progression-monks-we-have-work-to-do.229581/
uint32 Client::RollDamageMultiplier(uint32 offense, int& damage, EQ::skills::SkillType skill)
{
	int rollChance = 51;
	int maxExtra = 210;
	int minusFactor = 105;

	if (GetClass() == Class::Monk && level >= 65)
	{
		rollChance = 83;
		maxExtra = 300;
		minusFactor = 50;
	}
	else if (level >= 65 || (GetClass() == Class::Monk && level >= 63))
	{
		rollChance = 81;
		maxExtra = 295;
		minusFactor = 55;
	}
	else if (level >= 63 || (GetClass() == Class::Monk && level >= 60))
	{
		rollChance = 79;
		maxExtra = 290;
		minusFactor = 60;
	}
	else if (level >= 60 || (GetClass() == Class::Monk && level >= 56))
	{
		rollChance = 77;
		maxExtra = 285;
		minusFactor = 65;
	}
	else if (level >= 56)
	{
		rollChance = 72;
		maxExtra = 265;
		minusFactor = 70;
	}
	else if (level >= 51 || GetClass() == Class::Monk)
	{
		rollChance = 65;
		maxExtra = 245;
		minusFactor = 80;
	}

	int baseBonus = (static_cast<int>(offense) - minusFactor) / 2;
	if (baseBonus < 10)
		baseBonus = 10;

	if (zone->random.Roll(rollChance))
	{
		uint32 roll;

		roll = zone->random.Int(0, baseBonus) + 100;
		if (roll > maxExtra)
			roll = maxExtra;

		damage = damage * roll / 100;

		if (level >= 55 && damage > 1 && skill != EQ::skills::SkillArchery && IsWarriorClass())
			damage++;

		return roll;
	}
	else
	{
		return 100;
	}
}

// decompile shows that weapon elemental damage resist uses a special function
// credit to demonstar
int Mob::CalcEleWeaponResist(int weaponDamage, int resistType, Mob *target)
{
	int resistValue = 0;

	switch (resistType)
	{
	case RESIST_FIRE:
		resistValue = target->GetFR();
		break;
	case RESIST_COLD:
		resistValue = target->GetCR();
		break;
	case RESIST_MAGIC:
		resistValue = target->GetMR();
		break;
	case RESIST_DISEASE:
		resistValue = target->GetDR();
		break;
	case RESIST_POISON:
		resistValue = target->GetPR();
		break;
	}

	if (resistValue > 200)
		return 0;

	int roll = zone->random.Int(1, 201) - resistValue;
	if (roll < 1)
		return 0;
	if (roll <= 99)
		return weaponDamage * roll / 100;
	else
		return weaponDamage;
}



//note: throughout this method, setting `damage` to a negative is a way to
//stop the attack calculations
bool Client::Attack(Mob* other, int hand, int damagePct)
{
	if (!other) {
		SetTarget(nullptr);
		Log(Logs::General, Logs::Error, "A null Mob object was passed to Client::Attack() for evaluation!");
		return false;
	}

	if (hand != EQ::invslot::slotSecondary)
		hand = EQ::invslot::slotPrimary;

	Log(Logs::Detail, Logs::Combat, "Attacking %s with hand %d", other?other->GetName():"(nullptr)", hand);

	

	EQ::ItemInstance* weapon = GetInv().GetItem(hand);

	Log(Logs::Detail, Logs::Combat, "Attacking with weapon: %s (%d)", weapon->GetItem()->Name, weapon->GetID());
	} else {
		Log(Logs::Detail, Logs::Combat, "Attacking without a weapon.");
	}



	// Now figure out damage
	int damage = 1;
	uint8 mylevel = GetLevel();
	int baseDamage = GetBaseDamage(other, hand);

	// anti-twink damage caps.  Taken from decompiles
	if (mylevel < 10)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 9)
				baseDamage = 9;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 6)
				baseDamage = 6;
			break;
		default:
			if (baseDamage > 10)
				baseDamage = 10;
		}
	}
	else if (mylevel < 20)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 12)
				baseDamage = 12;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 10)
				baseDamage = 10;
			break;
		default:
			if (baseDamage > 14)
				baseDamage = 14;
		}
	}
	else if (mylevel < 30)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 20)
				baseDamage = 20;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 12)
				baseDamage = 12;
			break;
		default:
			if (baseDamage > 30)
				baseDamage = 30;
		}
	}
	else if (mylevel < 40)
	{
		switch (GetClass())
		{
		case Class::Druid:
		case Class::Cleric:
		case Class::Shaman:
			if (baseDamage > 26)
				baseDamage = 26;
			break;
		case Class::Wizard:
		case Class::Magician:
		case Class::Necromancer:
		case Class::Enchanter:
			if (baseDamage > 18)
				baseDamage = 18;
			break;
		default:
			if (baseDamage > 60)
				baseDamage = 60;
		}
	}
	
	int damageBonus = 0;
	if (hand == EQ::invslot::slotPrimary)
		damageBonus = GetDamageBonus();
	int hate = baseDamage + damageBonus;

	if (other->IsImmuneToMelee(this, hand))
	{
		damage = DMG_INVUL;
	}
	else
	{
		// check avoidance skills
		other->AvoidDamage(this, damage);

		if (damage < 0 && (damage == DMG_DODGE || damage == DMG_PARRY || damage == DMG_RIPOSTE || damage == DMG_BLOCK)
			&& aabonuses.StrikeThrough && zone->random.Roll(aabonuses.StrikeThrough))
		{
			damage = 1;		// Warrior Tactical Mastery AA
		}

		//riposte
		if (damage == DMG_RIPOSTE)
		{
			DoRiposte(other);
			if (IsDead()) return false;
		}

		if (damage > 0)
		{
			// swing not avoided by skills; do avoidance AC check
			if (!other->AvoidanceCheck(this, skillinuse))
			{
				Log(Logs::Detail, Logs::Combat, "Attack missed. Damage set to 0.");
				damage = DMG_MISS;
			}
		}

		CheckIncreaseSkill(skillinuse, other, zone->skill_difficulty[skillinuse].difficulty[GetClass()]);
		CheckIncreaseSkill(EQ::skills::SkillOffense, other, zone->skill_difficulty[EQ::skills::SkillOffense].difficulty[GetClass()]);

		if (damage > 0)
		{
			//try a finishing blow.. if successful end the attack
			if(TryFinishingBlow(other, skillinuse, damageBonus))
				return (true);

			damage = damageBonus + CalcMeleeDamage(other, baseDamage, skillinuse);

			if (damagePct <= 0)
				damagePct = 100;
			damage = damage * damagePct / 100;

			other->TryShielderDamage(this, damage, skillinuse);		// warrior /shield
			TryCriticalHit(other, skillinuse, damage, baseDamage, damageBonus);

			Log(Logs::Detail, Logs::Combat, "Damage calculated to %d (str %d, skill %d, DMG %d, lv %d)",
				damage, GetSTR(), GetSkill(skillinuse), baseDamage, mylevel);
		}
	}

	// Hate Generation is on a per swing basis, regardless of a hit, miss, or block, its always the same.
	// If we are this far, this means we are atleast making a swing.
	other->AddToHateList(this, hate);

	///////////////////////////////////////////////////////////
	////// Send Attack Damage
	///////////////////////////////////////////////////////////
	other->Damage(this, damage, SPELL_UNKNOWN, skillinuse);

	if (IsDead()) return false;

	MeleeLifeTap(damage);

	// old rogue poison from apply poison skill.  guaranteed procs first hit then fades
	if (poison_spell_id && damage > 0 && hand == EQ::invslot::slotPrimary && skillinuse == EQ::skills::Skill1HPiercing)
	{
		ExecWeaponProc(weapon, poison_spell_id, other);
		poison_spell_id = 0;
	}

	CommonBreakInvisNoSneak();

	if (damage > 0)
		return true;

	else
		return false;
}



void Client::Damage(Mob* other, int32 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable, int8 buffslot, bool iBuffTic)
{
	if(dead || IsCorpse())
		return;

	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	if(spell_id!=0 && spell_id != SPELL_UNKNOWN && other && damage > 0)
	{
		if(other->IsNPC() && !other->IsPet())
		{
			float npcspellscale = other->CastToNPC()->GetSpellScale();
			damage = ((float)damage * npcspellscale) / (float)100;
		}
	}

	// Reduce PVP damage. Don't do PvP mitigation if the caster is damaging themself or it's from a DS.
	bool FromDamageShield = (attack_skill == EQ::skills::SkillAbjuration);
	if (!FromDamageShield && other && other->IsClient() && damage > 0)
	{
		if (spell_id != SPELL_UNKNOWN)
		{
			/*
			int ruleDmg = RuleI(Combat, PvPSpellDmgPct);
			if (ruleDmg < 1)
				ruleDmg = 62;

			// lower level spells are less reduced than higher level spells
			// this scales PvP damage from 91% at level 1 to 62% at level 50
			PvPMitigation = 91 - spells[spell_id].classes[other->GetClass()-1] * 58 / 100;

			if (PvPMitigation < ruleDmg)
				PvPMitigation = ruleDmg;
			*/

			// this spell mitigation part is from a client decompile
			if (IsValidSpell(spell_id) && (spells[spell_id].goodEffect == 0 || IsLichSpell(spell_id)))
			{
				int caster_class = other->GetClass();
				float mitigation;

				if (caster_class == Class::Bard || caster_class == Class::Paladin || caster_class == Class::Ranger || caster_class == Class::ShadowKnight)
				{
					if (spell_id != SPELL_IMP_HARM_TOUCH && spell_id != SPELL_HARM_TOUCH)
					{
						mitigation = 0.80000001f;
					}
					else
					{
						mitigation = 0.68000001f;
					}
				}
				else
				{
					int spell_level = 39;
					if (caster_class <= Class::PLAYER_CLASS_COUNT)
					{
						spell_level = spells[spell_id].classes[caster_class - 1];
					}
					if (spell_level < 14)
					{
						mitigation = 0.88f;
					}
					else if (spell_level < 24)
					{
						mitigation = 0.77999997f;
					}
					else if (spell_level < 39)
					{
						mitigation = 0.68000001f;
					}
					else
					{
						mitigation = 0.63f;
					}
				}
				
				// Reintroduce spell dampening for self, but only for lich spells at the correct value.
				if (other == this)
				{
					mitigation = 1.0f;
					if (RuleB(Quarm, LichDamageMitigation) && IsLichSpell(spell_id))
					{
						mitigation = 0.68000001f;
						Log(Logs::Detail, Logs::Spells, "%s is getting %.2f lich mitigation for %s in slot %d. Was %d damage", GetName(), mitigation, GetSpellName(spell_id), buffslot, damage);
					}
				}
				
				damage = (int32)((double)damage * mitigation);
				if (damage < 1)
				{
					damage = 1;
				}
			}
		}
		else if (attack_skill == EQ::skills::SkillArchery)
		{
			if (RuleI(Combat, PvPArcheryDmgPct) > 0)
			{
				int PvPMitigation = RuleI(Combat, PvPArcheryDmgPct);
				damage = (damage * PvPMitigation) / 100;
			}
		}
		else  // melee PvP mitigation
		{
			if (RuleI(Combat, PvPMeleeDmgPct) > 0)
			{
				int PvPMitigation = RuleI(Combat, PvPMeleeDmgPct);
				damage = (damage * PvPMitigation) / 100;
			}
		}
	}

	if (other && other->IsClient())
	{
		if ((float)damage > (float)GetMaxHP() * (float)RuleR(Quarm, MaxPVPDamagePercent))
			damage = (int)(float)((float)GetMaxHP() * (float)RuleR(Quarm, MaxPVPDamagePercent));
	}

	// 3 second flee stun check.  NPCs can stun players who are running away from them.  10% chance on hit
	if (spell_id == SPELL_UNKNOWN && damage > 0 && other && other->IsNPC() && !other->IsPet() && other->GetLevel() > 4 && EQ::skills::IsMeleeWeaponSkill(attack_skill))
	{
		if (animation > 0 && zone->random.Roll(10) && other->BehindMob(this, other->GetX(), other->GetY()))
		{
			int stun_resist = aabonuses.StunResist;

			if (!stun_resist || zone->random.Roll(100 - stun_resist))
			{
				if (CombatRange(other, 0.0f, false, true)) {
					Log(Logs::Detail, Logs::Combat, "3 second flee stun sucessful");
					Stun(3000, other);
				}
			}
			else
			{
				Log(Logs::Detail, Logs::Combat, "Stun Resisted. %d chance.", stun_resist);
				Message_StringID(Chat::DefaultText, StringID::AVOID_STUN);
			}
		}
	}


	if(!ClientFinishedLoading())
		damage = DMG_INVUL;

	//do a majority of the work...
	CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);
}






int Client::GetDamageBonus()
{
	if (GetLevel() < 28 || !IsWarriorClass())
		return 0;

	int delay = 1;
	int bonus = 1 + (GetLevel() - 28) / 3;

	EQ::ItemInstance* weaponInst = GetInv().GetItem(EQ::invslot::slotPrimary);
	const EQ::ItemData* weapon = nullptr;
	if (weaponInst)
		weapon = weaponInst->GetItem();

	if (!weapon)
		delay = GetHandToHandDelay();
	else
		delay = weapon->Delay;

	if ( weapon && (weapon->ItemType == EQ::item::ItemType2HSlash || weapon->ItemType == EQ::item::ItemType2HBlunt || weapon->ItemType == EQ::item::ItemType2HPiercing) )
	{
		if (delay <= 27)
			return bonus + 1;

		if (level > 29)
		{
			int level_bonus = (level - 30) / 5 + 1;
			if (level > 50)
			{
				level_bonus++;
				int level_bonus2 = level - 50;
				if (level > 67)
					level_bonus2 += 5;
				else if (level > 59)
					level_bonus2 += 4;
				else if (level > 58)
					level_bonus2 += 3;
				else if (level > 56)
					level_bonus2 += 2;
				else if (level > 54)
					level_bonus2++;
				level_bonus += level_bonus2 * delay / 40;
			}
			bonus += level_bonus;
		}
		if (delay >= 40)
		{
			int delay_bonus = (delay - 40) / 3 + 1;
			if (delay >= 45)
				delay_bonus += 2;
			else if (delay >= 43)
				delay_bonus++;
			bonus += delay_bonus;
		}
		return bonus;
	}
	return bonus;
}

int Client::GetHandToHandDamage()
{
	static uint8 mnk_dmg[] = { 99,
		4, 4, 4, 4, 5, 5, 5, 5, 5, 6,           // 1-10
		6, 6, 6, 6, 7, 7, 7, 7, 7, 8,           // 11-20
		8, 8, 8, 8, 9, 9, 9, 9, 9, 10,          // 21-30
		10, 10, 10, 10, 11, 11, 11, 11, 11, 12, // 31-40
		12, 12, 12, 12, 13, 13, 13, 13, 13, 14, // 41-50
		14, 14, 14, 14, 14, 14, 14, 14, 14, 14, // 51-60
		14, 14 };                                // 61-62
	static uint8 bst_dmg[] = { 99,
		4, 4, 4, 4, 4, 5, 5, 5, 5, 5,        // 1-10
		5, 6, 6, 6, 6, 6, 6, 7, 7, 7,        // 11-20
		7, 7, 7, 8, 8, 8, 8, 8, 8, 9,        // 21-30
		9, 9, 9, 9, 9, 10, 10, 10, 10, 10,   // 31-40
		10, 11, 11, 11, 11, 11, 11, 12, 12 }; // 41-49

	if (GetClass() == Class::Monk)
	{
		if (IsClient() && CastToClient()->GetItemIDAt(12) == 10652 && GetLevel() > 50)		// Celestial Fists, monk epic
			return 9;
		if (level > 62)
			return 15;
		return mnk_dmg[level];
	}
	else if (GetClass() == Class::Beastlord)
	{
		if (level > 49)
			return 13;
		return bst_dmg[level];
	}
	return 2;
}

int Client::GetHandToHandDelay()
{
	int delay = 35;
	static uint8 mnk_hum_delay[] = { 99,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 1-10
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 11-20
		35, 35, 35, 35, 35, 35, 35, 34, 34, 34, // 21-30
		34, 33, 33, 33, 33, 32, 32, 32, 32, 31, // 31-40
		31, 31, 31, 30, 30, 30, 30, 29, 29, 29, // 41-50
		29, 28, 28, 28, 28, 27, 27, 27, 27, 26, // 51-60
		24, 22 };								// 61-62
	static uint8 mnk_iks_delay[] = { 99,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 1-10
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 11-20
		35, 35, 35, 35, 35, 35, 35, 35, 35, 34, // 21-30
		34, 34, 34, 34, 34, 33, 33, 33, 33, 33, // 31-40
		33, 32, 32, 32, 32, 32, 32, 31, 31, 31, // 41-50
		31, 31, 31, 30, 30, 30, 30, 30, 30, 29, // 51-60
		25, 23 };								// 61-62
	static uint8 bst_delay[] = { 99,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 1-10
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, // 11-20
		35, 35, 35, 35, 35, 35, 35, 35, 34, 34, // 21-30
		34, 34, 34, 33, 33, 33, 33, 33, 32, 32, // 31-40
		32, 32, 32, 31, 31, 31, 31, 31, 30, 30, // 41-50
		30, 30, 30, 29, 29, 29, 29, 29, 28, 28, // 51-60
		28, 28, 28 };							// 61-63

	if (GetClass() == Class::Monk)
	{
		if (GetItemIDAt(12) == 10652 && GetLevel() > 50)		// Celestial Fists, monk epic
			return 16;
		
		if (GetLevel() > 62)
			return GetRace() == Race::Iksar ? 21 : 20;

		return GetRace() == Race::Iksar ? mnk_iks_delay[level] : mnk_hum_delay[level];
	}
	else if (GetClass() == Class::Beastlord)
	{
		if (GetLevel() > 63)
			return 27;
		return bst_delay[level];
	}
	return 35;
}


int32 Mob::AffectMagicalDamage(int32 damage, uint16 spell_id, const bool iBuffTic, Mob* attacker)
{
	if(damage <= 0)
		return damage;

	bool DisableSpellRune = false;
	int32 slot = -1;

	// If this is a DoT, use DoT Shielding...
	if(iBuffTic) {
		damage -= (damage * itembonuses.DoTShielding / 100);
	}

	// This must be a DD then so lets apply Spell Shielding and runes.
	else
	{
		// Reduce damage by the Spell Shielding first so that the runes don't take the raw damage.
		damage -= (damage * itembonuses.SpellShield / 100);

		// Do runes now.
		if (spellbonuses.MitigateSpellRune[0] && !DisableSpellRune){
			slot = spellbonuses.MitigateSpellRune[1];
			if(slot >= 0)
			{
				int damage_to_reduce = damage * spellbonuses.MitigateSpellRune[0] / 100;

				if (spellbonuses.MitigateSpellRune[2] && (damage_to_reduce > spellbonuses.MitigateSpellRune[2]))
					damage_to_reduce = spellbonuses.MitigateSpellRune[2];

				if(spellbonuses.MitigateSpellRune[3] && (damage_to_reduce >= buffs[slot].magic_rune))
				{
					Log(Logs::Detail, Logs::Spells, "Mob::ReduceDamage SE_MitigateSpellDamage %d damage negated, %d"
						" damage remaining, fading buff.", damage_to_reduce, buffs[slot].magic_rune);
					damage -= buffs[slot].magic_rune;
					BuffFadeBySlot(slot);
				}
				else
				{
					Log(Logs::Detail, Logs::Spells, "Mob::ReduceDamage SE_MitigateMeleeDamage %d damage negated, %d"
						" damage remaining.", damage_to_reduce, buffs[slot].magic_rune);

					if (spellbonuses.MitigateSpellRune[3])
						buffs[slot].magic_rune = (buffs[slot].magic_rune - damage_to_reduce);

					damage -= damage_to_reduce;
				}
			}
		}

		if(damage < 1)
			return 0;

		//Regular runes absorb spell damage (except dots) - Confirmed on live.
		if (spellbonuses.MeleeRune[0] && spellbonuses.MeleeRune[1] >= 0)
			damage = RuneAbsorb(damage, SE_Rune);

		if (spellbonuses.AbsorbMagicAtt[0] && spellbonuses.AbsorbMagicAtt[1] >= 0)
			damage = RuneAbsorb(damage, SE_AbsorbMagicAtt);
	}
	return damage;
}

bool Mob::HasProcs() const
{
	for (int i = 0; i < MAX_PROCS; i++)
		if (SpellProcs[i].spellID != SPELL_UNKNOWN)
			return true;
	return false;
}

// returns either chance in %, or the effective skill level which is essentially chance% * 5
// combat rolls should use the latter for accuracy.  Sony rolls against 500
int Mob::GetDoubleAttackChance(bool returnEffectiveSkill)
{
	int chance = GetSkill(EQ::skills::SkillDoubleAttack);

	if (chance > 0)
	{
		if (IsClient())
			chance += GetLevel();
		else if (GetLevel() > 35)
			chance += GetLevel();
	}

	// Bestial Frenzy/Harmonious Attack AA - grants double attacks for classes that otherwise do not get the skill
	if (aabonuses.GiveDoubleAttack)
		chance += aabonuses.GiveDoubleAttack * 5;

	// Knight's Advantage and Ferocity AAs; Double attack rate is 67% at 245 skill with Ferocity, so this is the only way it can be
	if (aabonuses.DoubleAttackChance)
		chance += chance * aabonuses.DoubleAttackChance / 100;

	if (returnEffectiveSkill)
		return chance;
	else
		return chance / 5;
}

bool Mob::CheckDoubleAttack()
{
	if (GetDoubleAttackChance(true) > zone->random.Int(0, 499))			// 1% per 5 skill
		return true;

	return false;
}

// returns either chance in %, or the effective skill level which is essentially chance% * 3.75
// combat rolls should use the latter for accuracy.  Sony rolls against 375
int Mob::GetDualWieldChance(bool returnEffectiveSkill)
{
	int chance = GetSkill(EQ::skills::SkillDualWield); // 245 or 252
	if (chance > 0)
	{
		if (IsClient())
			chance += GetLevel();
		else if (GetLevel() > 35)
			chance += GetLevel();
	}

	chance += aabonuses.Ambidexterity; // 32

	// SE_DualWieldChance[176] - Deftdance and Kinesthetics Disciplines
	chance += spellbonuses.DualWieldChance;

	if (returnEffectiveSkill)
		return chance;
	else
		return chance * 100 / 375;
}

bool Mob::CheckDualWield()
{
	if (GetDualWieldChance(true) > zone->random.Int(0, 374))			// 1% per 3.75 skill
		return true;

	return false;
}

void Mob::CommonDamage(Mob* attacker, int32 &damage, const uint16 spell_id, const EQ::skills::SkillType skill_used, bool &avoidable, const int8 buffslot, const bool iBuffTic) 
{

	if (!IsSelfConversionSpell(spell_id) && !iBuffTic)
	{
		CommonBreakInvisible(true);

		bool miss = damage == DMG_MISS;
		bool skip_self = false;
		if (!hidden && !improved_hidden && sneaking && miss)
			skip_self = true;

		CommonBreakSneakHide(skip_self);

	}

	// This method is called with skill_used=ABJURE for Damage Shield damage.
	bool FromDamageShield = (skill_used == EQ::skills::SkillAbjuration);

	Log(Logs::Detail, Logs::Combat, "Applying damage %d done by %s with skill %d and spell %d, avoidable? %s, is %sa buff tic in slot %d",
		damage, attacker?attacker->GetName():"NOBODY", skill_used, spell_id, avoidable?"yes":"no", iBuffTic?"":"not ", buffslot);

	if ((GetInvul() || DivineAura()) && spell_id != SPELL_CAZIC_TOUCH) {
		Log(Logs::Detail, Logs::Combat, "Avoiding %d damage due to invulnerability.", damage);
		damage = DMG_INVUL;
	}

	if( spell_id != SPELL_UNKNOWN || attacker == nullptr )
		avoidable = false;

	if (attacker) 
	{
		if (attacker->IsClient())
		{
			// Damage shield damage shouldn't count towards who gets EXP
			if (!attacker->CastToClient()->IsFeigned() && !FromDamageShield)
			{
				AddToHateList(attacker, 0, damage, false, iBuffTic, false);
			}
		}
		else
		{
			AddToHateList(attacker, 0, damage, false, iBuffTic, false);
		}
	}

	bool died = false;
	if(damage > 0) {
		//if there is some damage being done and theres an attacker involved
		if(attacker) {
			if(spell_id == SPELL_LEECH_TOUCH && attacker->IsClient() && attacker->CastToClient()->CheckAAEffect(aaEffectLeechTouch)){
				attacker->CastToClient()->DisableAAEffect(aaEffectLeechTouch);
			}

			// if spell is lifetap add hp to the caster
			// TODO - investigate casting the reversed tap spell on the caster instead of doing this, so we can send proper tap_amount in action packet.  instant spells too not just buffs maybe.
			if (spell_id != SPELL_UNKNOWN && IsLifetapSpell(spell_id) && !IsBuffSpell(spell_id))
			{
				int healed = attacker->GetActSpellHealing(spell_id, damage, this);
				Log(Logs::General, Logs::Spells, "Applying lifetap heal of %d to %s", healed, attacker->GetName());
				attacker->HealDamage(healed);
			}

			if(IsNPC() && !zone->IsIdling())
			{
				int32 adj_damage = GetHP() - damage < 0 ? GetHP() : damage;

				total_damage += adj_damage;


				// NPC DS damage is just added to npc_damage.
				if (FromDamageShield && (attacker->IsClient() || attacker->IsPlayerOwned()))
					ds_damage += adj_damage;

				if (!FromDamageShield && attacker->IsClient())
				{
					player_damage += adj_damage;
				}

				Client* ultimate_owner = attacker->IsClient() ? attacker->CastToClient() : nullptr;

				if (attacker->IsPlayerOwned() && ultimate_owner == nullptr)
				{
					if (attacker->HasOwner() && attacker->GetUltimateOwner()->IsClient())
					{
						ultimate_owner = attacker->GetUltimateOwner()->CastToClient();
					}
					if ((attacker->IsNPC() && attacker->CastToNPC()->GetSwarmInfo() && attacker->CastToNPC()->GetSwarmInfo()->GetOwner() && attacker->CastToNPC()->GetSwarmInfo()->GetOwner()->IsClient()))
					{
						ultimate_owner = attacker->CastToNPC()->GetSwarmInfo()->GetOwner()->CastToClient();
					}
				}

				if (ultimate_owner)
				{
					bool is_raid_solo_fte_credit = ultimate_owner->GetRaid() ? ultimate_owner->GetRaid()->GetID() == CastToNPC()->solo_raid_fte : false;
					bool is_group_solo_fte_credit = ultimate_owner->GetGroup() ? ultimate_owner->GetGroup()->GetID() == CastToNPC()->solo_group_fte : false;
					bool is_solo_fte_credit = ultimate_owner->CharacterID() == CastToNPC()->solo_fte_charid ? true : false;
					if (is_solo_fte_credit || is_raid_solo_fte_credit || is_group_solo_fte_credit)
					{
						ssf_player_damage += adj_damage;
						if (FromDamageShield)
						{
							ssf_ds_damage += adj_damage;
						}
					}
				}

				if (attacker->IsDireCharmed())
					dire_pet_damage += adj_damage;

				if (attacker->IsNPC() && (!attacker->IsPet() || (attacker->GetOwner() && attacker->GetOwner()->IsNPC())))
					npc_damage += adj_damage;

				if (IsValidSpell(spell_id) && spells[spell_id].targettype == ST_AECaster)
					pbaoe_damage += adj_damage;
			}

			// only apply DS if physical damage (no spell damage) damage shield calls this function with spell_id set, so its unavoidable
			// We want to check damage shield after the round's damage packets are sent out, otherwise to the client we will be processing
			// the damage shield before we process the hit that caused it. 
			// solar: 2022-05-24 moved this up before rune contrary to above comment to make WotW work
			if (spell_id == SPELL_UNKNOWN && skill_used != EQ::skills::SkillArchery && skill_used != EQ::skills::SkillThrowing) {
				DamageShield(attacker);
			}
		}	//end `if there is some damage being done and theres an attacker person involved`

		//see if any runes want to reduce this damage. DS should bypass runes.
		if (!FromDamageShield)
		{
			if (spell_id == SPELL_UNKNOWN) {
				damage = ReduceDamage(damage);
				Log(Logs::Detail, Logs::Combat, "Melee Damage reduced to %d", damage);
			}
			else {
				int32 origdmg = damage;
				damage = AffectMagicalDamage(damage, spell_id, iBuffTic, attacker);
				if (origdmg != damage && attacker && attacker->IsClient()) {
					if (attacker->CastToClient()->GetFilter(FilterDamageShields) != FilterHide)
						attacker->Message(Chat::Yellow, "The Spellshield absorbed %d of %d points of damage", origdmg - std::max(damage, 0), origdmg);
				}
			}
		}

		//final damage has been determined.
		int old_hp_ratio = (int)GetHPRatio();
		if (damage > 0)
		{
			SetHP(GetHP() - damage);
			if (IsClient())
				CastToClient()->CalcAGI(); // AGI depends on current hp (near death)
		}

		// Don't let them go unconscious due to a DOT, otherwise the client may not complete its death routine properly.
		if(HasDied() || (iBuffTic && GetHP() <= 0))
		{
			bool IsSaved = false;

			if(TryDivineSave()) 
			{
				IsSaved = true;
            }

			if(!IsSaved) 
			{
				if (IsNPC())
					died = !CastToNPC()->GetDepop();
				else if (IsClient())
					died = CastToClient()->CheckDeath();

				if (died)
				{
					if (IsClient())
						CastToClient()->m_pp.cur_hp = -100;
					SetHP(-100);
				}
			}
		}
		else
		{
			if(GetHPRatio() < 16.0f)
				TryDeathSave();
		}

		if (!died)
		{
			TrySpinStunBreak();
		}

		//fade mez if we are mezzed
		if (IsMezzed() && attacker) {
			Log(Logs::Detail, Logs::Combat, "Breaking mez due to attack.");
			BuffFadeByEffect(SE_Mez);
			if(IsNPC())
				entity_list.MessageClose(this, true, RuleI(Range, SpellMessages), Chat::SpellWornOff, "%s is no longer mezzed. (%s - %s)", GetCleanName(), attacker->GetCleanName(), spell_id != SPELL_UNKNOWN ? GetSpellName(spell_id) : "melee" );
		}

		if(spell_id != SPELL_UNKNOWN && !iBuffTic) {
			//see if root will break
			if (IsRooted() && !FromDamageShield)  // neotoyko: only spells cancel root
				TryRootFadeByDamage(buffslot, attacker);
		}
		else if(spell_id == SPELL_UNKNOWN)
		{
			//increment chances of interrupting
			if(damage > 0 && IsCasting()) { //shouldnt interrupt on regular spell damage
				attacked_count++;
				Log(Logs::Detail, Logs::Combat, "Melee attack while casting. Attack count %d", attacked_count);
			}
		}

		//send an HP update if we are hurt
		if(GetHP() < GetMaxHP())
		{
			// Don't send a HP update for melee damage unless we've damaged ourself.
			bool hideme = attacker && attacker->IsClient() && attacker->CastToClient()->GMHideMe();
			bool sendtoself = true;
			if (spell_id == SPELL_UNKNOWN && !iBuffTic && attacker != this && !hideme)
			{
				sendtoself = false;
			}

			if (IsNPC()) 
			{
				int cur_hp_ratio = (int)GetHPRatio();
				if (cur_hp_ratio != old_hp_ratio)
					SendHPUpdate(true, sendtoself);
			}
			// Let regen handle buff tics unless this tic killed us.
			else if (!iBuffTic || died)
			{
				SendHPUpdate(true, sendtoself);
			}

			if (!died && IsNPC())
			{
				CheckFlee();
				CheckEnrage();
			}
		}
	}	//end `if damage was done`

	// hundreds of spells have the skill id set to tiger claw in the spell data
	// without this, lots of stuff will 'strike' instead of give a proper spell damage message
	uint8 skill_id = skill_used;
	if (skill_used == EQ::skills::SkillTigerClaw && spell_id > 0 && ((attacker && attacker->GetClass() != Class::Monk) || spell_id != SPELL_UNKNOWN))
	{
		skill_id = EQ::skills::SkillEvocation;
	}

	//buff ticks (DOTs) do not send damage, instead they just call SendHPUpdate()
	if (!iBuffTic) 
	{ 
		// Aggro pet. Pet won't add the attacker to their hatelist until after its master checks if it is dead or not. 
		// DOTs should only aggro pets on the inital cast, and that is handled in SpellOnTarget() DS should also not aggro a pet.
		if (!died && !FromDamageShield)
		{
			AggroPet(attacker);
		}

		if(!died)
			GenerateDamagePackets(attacker, FromDamageShield, damage, spell_id, skill_id, false);
	}
	else
	{ /* Else it is a buff tic - DoT DMG messages were implemented on live in LoY era */
		if (RuleB(Spells, ShowDotDmgMessages) && IsValidSpell(spell_id) && damage > 0 && attacker)
		{
			if (damage > 0 && spell_id != SPELL_UNKNOWN)
			{
				if (attacker && attacker->IsClient() && attacker != this)
				{
					attacker->Message_StringID(Chat::SpellWornOff, StringID::YOUR_HIT_DOT, GetCleanName(), itoa(damage), spells[spell_id].name);
				}
			}
		}
	}

	if (died)
	{
		EQ::skills::SkillType attack_skill = static_cast<EQ::skills::SkillType>(skill_id);
		Death(attacker, damage, spell_id, attack_skill, 0, iBuffTic);

		/* After this point, "this" is still a valid object, but its entityID is 0.*/

		if (attacker && attacker->IsNPC())
		{
			uint32 emoteid = attacker->GetEmoteID();
			if (emoteid != 0)
				attacker->CastToNPC()->DoNPCEmote(EQ::constants::EmoteEventTypes::Killed, emoteid, this);
		}
	}
}




float Mob::GetProcChance(uint16 hand)
{
	double chance = 0.0;
	double weapon_speed = GetWeaponSpeedbyHand(hand);
	weapon_speed /= 100.0;

	double dex = GetDEX();
	if (dex > 255.0)
		dex = 255.0;		// proc chance caps at 255
	
	/* Kind of ugly, but results are very accurate
	   Proc chance is a linear function based on dexterity
	   0.0004166667 == base proc chance at 1 delay with 0 dex (~0.25 PPM for main hand)
	   1.1437908496732e-5 == chance increase per point of dex at 1 delay
	   Result is 0.25 PPM at 0 dex, 2 PPM at 255 dex
	*/
	chance = static_cast<double>((0.0004166667 + 1.1437908496732e-5 * dex) * weapon_speed);

	if (hand == EQ::invslot::slotSecondary)
	{
		chance *= 50.0 / static_cast<double>(GetDualWieldChance());
	}								
	
	Log(Logs::Detail, Logs::Combat, "Proc base chance percent: %.3f;  weapon delay: %.2f;  Est PPM: %0.2f", 
		static_cast<float>(chance*100.0), static_cast<float>(weapon_speed), static_cast<float>(chance * (600.0/weapon_speed)));
	return static_cast<float>(chance);
}

bool Mob::TryProcs(Mob *target, uint16 hand)
{
	if(!target) {
		SetTarget(nullptr);
		Log(Logs::General, Logs::Error, "A null Mob object was passed to Mob::TryWeaponProc for evaluation!");
		return false;
	}

	if (!IsAttackAllowed(target)) {
		Log(Logs::Detail, Logs::Combat, "Preventing procing off of unattackable things.");
		return false;
	}

	if (DivineAura() || target->HasDied())
	{
		return false;
	}

	EQ::ItemInstance* heldInst = nullptr;
	const EQ::ItemData* heldStruct = nullptr;

	if (IsNPC())
	{
		if (hand == EQ::invslot::slotPrimary)
			heldStruct = database.GetItem(GetEquipment(EQ::textures::weaponPrimary));
		else if (hand == EQ::invslot::slotSecondary)
			heldStruct = database.GetItem(GetEquipment(EQ::textures::weaponSecondary));
	}
	else if (IsClient())
	{
		heldInst = CastToClient()->GetInv().GetItem(hand);
		if (heldInst)
			heldStruct = heldInst->GetItem();

		if (heldInst && !heldInst->IsWeapon())
			return false;							// abort; holding a non-weapon
	}

	if (heldStruct && TryWeaponProc(heldInst, heldStruct, target, hand))
	{
		return true;	// procs from buffs do not fire if weapon proc succeeds
	}
	else if (hand == EQ::invslot::slotPrimary)	// only do buff procs on primary attacks
	{		
		return TrySpellProc(heldInst, heldStruct, target, hand);
	}

	return false;
}

bool Mob::TryWeaponProc(const EQ::ItemInstance *inst, const EQ::ItemData *weapon, Mob *on, uint16 hand)
{
	if (!weapon)
		return false;

	if (DivineAura())
	{
		return false;
	}

	if (weapon->Proc.Type == EQ::item::ItemEffectCombatProc && weapon->Proc.Effect)
	{
		float ProcChance = GetProcChance(hand);
		float WPC = ProcChance * (100.0f + static_cast<float>(weapon->ProcRate)) / 100.0f;

		if (zone->random.Roll(WPC))
		{
			if (weapon->Proc.Level > GetLevel())
			{
				Log(Logs::Detail, Logs::Combat,
						"Tried to proc (%s), but our level (%d) is lower than required (%d)",
						weapon->Name, GetLevel(), weapon->Proc.Level);

				if (IsPet())
				{
					Mob *own = GetOwner();
					if (own)
						own->Message_StringID(Chat::Red, StringID::PROC_PETTOOLOW);
				}
				else
				{
					Message_StringID(Chat::Red, StringID::PROC_TOOLOW);
				}
			}
			else
			{
				Log(Logs::Detail, Logs::Combat,
					"Attacking weapon (%s) successfully procing spell %s (%.3f percent chance; weapon proc mod is %i percent)",
					weapon->Name, spells[weapon->Proc.Effect].name, WPC * 100.0f, weapon->ProcRate);

				return ExecWeaponProc(inst, weapon->Proc.Effect, on);
			}
		}
	}
	return false;
}

bool Mob::TrySpellProc(const EQ::ItemInstance *inst, const EQ::ItemData *weapon, Mob *on, uint16 hand)
{
	if (DivineAura() || !on || on->HasDied())
	{
		return false;
	}

	for (uint32 i = 0; i < MAX_PROCS; i++)
	{
		// Spell procs (buffs)
		if (SpellProcs[i].spellID != SPELL_UNKNOWN && !SpellProcs[i].poison)
		{
			float ProcChance = GetProcChance(hand);
			float chance = ProcChance * (static_cast<float>(SpellProcs[i].chance) / 100.0f);
			if (zone->random.Roll(chance))
			{
				Log(Logs::Detail, Logs::Combat,
						"Spell buff proc %d procing spell %s (%.3f percent chance)",
						i, spells[SpellProcs[i].spellID].name, chance * 100.0f);
				ExecWeaponProc(nullptr, SpellProcs[i].spellID, on);
				return true;		// only one proc per round
			}
			else
			{
				Log(Logs::Detail, Logs::Combat,
						"Spell buff proc %d failed to proc %s (%.3f percent chance)",
						i, spells[SpellProcs[i].spellID].name, chance*100.0f);
			}
		}
	}

	return false;
}

bool NPC::TryInnateProc(Mob* target)
{
	// can't innate proc on runed targets
	if (innateProcSpellId == SPELL_UNKNOWN || target->GetSpellBonuses().MeleeRune[0])
	{
		return false;
	}

	if (zone->random.Roll(innateProcChance))
	{
		Log(Logs::Detail, Logs::Combat,
			"NPC innate proc success: spell %d (%d percent chance)",
			innateProcSpellId, innateProcChance);

		int16 resist_diff = spells[innateProcSpellId].ResistDiff;
		
		// Sleeper proc.  it seemed to have hit clients only and was unresistable
		if (innateProcSpellId == SPELL_DRAGON_CHARM && GetRace() == Race::PrismaticDragon)
		{
			if (!target->IsClient())
				return false;

			resist_diff = -1000;
		}

		SpellFinished(innateProcSpellId, target, EQ::spells::CastingSlot::Item, 0, -1, resist_diff, true);
		return true;
	}
	return false;
}

// minBase == 0 will skip the minimum base damage check
void Mob::TryCriticalHit(Mob *defender, uint16 skill, int32 &damage, int32 minBase, int32 damageBonus)
{
	if (damage < 1)
		return;
	if (damageBonus > damage)		// damage should include the bonus already, but calcs need the non-bonus portion
		damageBonus = damage;

	float critChance = 0.0f;
	bool isBerserk = false;
	bool undeadTarget = false;

	//1: Try Slay Undead
	if (defender && (defender->GetBodyType() == BodyType::Undead ||
		defender->GetBodyType() == BodyType::SummonedUndead || defender->GetBodyType() == BodyType::Vampire))
	{
		undeadTarget = true;

		// these were added together in a december 2004 patch.  before then it was probably this, but not 100% sure
		int32 SlayRateBonus = std::max(aabonuses.SlayUndead[0], spellbonuses.SlayUndead[0]);

		if (SlayRateBonus)
		{
			float slayChance = static_cast<float>(SlayRateBonus) / 10000.0f;
			if (zone->random.Roll(slayChance))
			{
				int32 slayDmgBonus = std::max(aabonuses.SlayUndead[1], spellbonuses.SlayUndead[1]);
				damage = ((damage - damageBonus + 6) * slayDmgBonus) / 100 + damageBonus;

				int minSlay = (minBase + 5) * slayDmgBonus / 100 + damageBonus;
				if (damage < minSlay)
					damage = minSlay;

				if (GetGender() == Gender::Female) // female
					entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, StringID::FEMALE_SLAYUNDEAD,
						GetCleanName(), itoa(damage));
				else // males and neuter I guess
					entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, StringID::MALE_SLAYUNDEAD,
						GetCleanName(), itoa(damage));
				return;
			}
		}
	}

	//2: Try Melee Critical
	if (IsClient())
	{
		// Combat Fury and Fury of the Ages AAs
		int critChanceMult = aabonuses.CriticalHitChance;

		critChance += RuleI(Combat, ClientBaseCritChance);
		float overCap = 0.0f;
		if (GetDEX() > 255)
			overCap = static_cast<float>(GetDEX() - 255) / 400.0f;

		// not used in anything, but leaving for custom servers I guess
		if (spellbonuses.BerserkSPA || itembonuses.BerserkSPA || aabonuses.BerserkSPA)
			isBerserk = true;

		if (GetClass() == Class::Warrior && GetLevel() >= 12)
		{
			if (IsBerserk())
				isBerserk = true;

			critChance += 0.5f + static_cast<float>(std::min(GetDEX(), 255)) / 90.0f + overCap;
		}
		else if (skill == EQ::skills::SkillArchery && GetClass() == Class::Ranger && GetLevel() > 16)
		{
			critChance += 1.35f + static_cast<float>(std::min(GetDEX(), 255)) / 34.0f + overCap * 2;
		}
		else if (GetClass() != Class::Warrior && critChanceMult)
		{
			critChance += 0.275f + static_cast<float>(std::min(GetDEX(), 255)) / 150.0f + overCap;
		}

		if (critChanceMult)
			critChance += critChance * static_cast<float>(critChanceMult) / 100.0f;

		// this is cleaner hardcoded due to the way bonuses work and holyforge crit rate is a max()
		uint8 activeDisc = CastToClient()->GetActiveDisc();

		if (activeDisc == disc_defensive)
			critChance = 0.0f;
		else if (activeDisc == disc_mightystrike)
			critChance = 100.0f;
		else if (activeDisc == disc_holyforge && undeadTarget && critChance < (spellbonuses.CriticalHitChance / 100.0f))
			critChance = static_cast<float>(spellbonuses.CriticalHitChance) / 100.0f;
	}

	int deadlyChance = 0;
	int deadlyMod = 0;

	if (skill == EQ::skills::SkillThrowing && GetClass() == Class::Rogue && GetSkill(EQ::skills::SkillThrowing) >= 65) {
		critChance += RuleI(Combat, RogueCritThrowingChance);
		deadlyChance = RuleI(Combat, RogueDeadlyStrikeChance);
		deadlyMod = RuleI(Combat, RogueDeadlyStrikeMod);
	}

	if (critChance > 0)
	{
		critChance /= 100.0f;

		if (zone->random.Roll(critChance))
		{
			int32 critMod = 17;
			bool crip_success = false;
			int32 cripplingBlowChance = spellbonuses.CrippBlowChance;		// Holyforge Discipline
			int32 minDamage = 0;

			if (cripplingBlowChance || isBerserk)
			{
				if (isBerserk || (cripplingBlowChance && zone->random.Roll(cripplingBlowChance)))
				{
					critMod = 29;
					crip_success = true;
				}
			}

			if (minBase)
				minDamage = (minBase * critMod + 5) / 10 + 8 + damageBonus;

			damage = ((damage - damageBonus) * critMod + 5) / 10 + 8 + damageBonus;
			if (crip_success)
			{
				damage += 2;
				minDamage += 2;
			}
			if (minBase && minDamage > damage)
				damage = minDamage;

			bool deadlySuccess = false;
			if (deadlyChance && zone->random.Roll(static_cast<float>(deadlyChance) / 100.0f))
			{
				if (BehindMob(defender, GetX(), GetY()))
				{
					damage *= deadlyMod;
					deadlySuccess = true;
				}
			}

			// sanity check; 1 damage crits = an error somewhere
			if (damage > 1000000 || damage < 0)
				damage = 1;

			if (crip_success)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, StringID::CRIPPLING_BLOW,
						GetCleanName(), itoa(damage));
				// Crippling blows also have a chance to stun
				//Kayen: Crippling Blow would cause a chance to interrupt for npcs < 55, with a staggers message.
				if (defender != nullptr && defender->GetLevel() <= 55 && !defender->GetSpecialAbility(StringID::IMMUNE_STUN) && zone->random.Roll(85))
				{
					defender->Emote("staggers.");
					defender->Stun(0, this);
				}
			}
			else if (deadlySuccess)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, StringID::DEADLY_STRIKE,
						GetCleanName(), itoa(damage));
			}
			else
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
						Chat::MeleeCrit, FilterMeleeCrits, StringID::CRITICAL_HIT,
						GetCleanName(), itoa(damage));
			}
		}
	}

	// Discs
	if (defender && IsClient() && CastToClient()->HasInstantDisc(skill))
	{
		if (damage > 0)
		{
			if (skill == EQ::skills::SkillFlyingKick)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
					Chat::MeleeCrit, FilterMeleeCrits, StringID::THUNDEROUS_KICK,
					GetName(), itoa(damage));
			}
			else if (skill == EQ::skills::SkillEagleStrike)
			{
				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
					Chat::MeleeCrit, FilterMeleeCrits, StringID::ASHEN_CRIT,
					GetName(), defender->GetCleanName());
			}
			else if (skill == EQ::skills::SkillDragonPunch)
			{
				uint32 stringid = StringID::SILENT_FIST_CRIT;
				if (GetRace() == Race::Iksar)
				{
					stringid = StringID::SILENT_FIST_TAIL;
				}

				entity_list.FilteredMessageClose_StringID(this, false, RuleI(Range, CombatSpecials),
					Chat::MeleeCrit, FilterMeleeCrits, stringid,
					GetName(), defender->GetCleanName());
			}
		}

		if (damage != DMG_MISS)
		{
			CastToClient()->FadeDisc();
		}
	}
}



void Mob::DoRiposte(Mob* defender) {
	Log(Logs::Detail, Logs::Combat, "Preforming a riposte");

	if (!defender || GetSpecialAbility(SpecialAbility::RiposteImmunity))
		return;

	if (defender->IsClient())
	{
		if (defender->CastToClient()->IsUnconscious() || defender->IsStunned() || defender->CastToClient()->IsSitting()
			|| defender->GetAppearance() == eaDead || defender->GetAppearance() == eaCrouching
		)
			return;
	}

	defender->Attack(this, EQ::invslot::slotPrimary);
	if (HasDied()) return;

	int32 DoubleRipChance = defender->aabonuses.GiveDoubleRiposte[0] +
							defender->spellbonuses.GiveDoubleRiposte[0] +
							defender->itembonuses.GiveDoubleRiposte[0];

	DoubleRipChance		 =  defender->aabonuses.DoubleRiposte +
							defender->spellbonuses.DoubleRiposte +
							defender->itembonuses.DoubleRiposte;

	//Live AA - Double Riposte
	if(DoubleRipChance && zone->random.Roll(DoubleRipChance)) {
		Log(Logs::Detail, Logs::Combat, "Preforming a double riposed (%d percent chance)", DoubleRipChance);
		defender->Attack(this, EQ::invslot::slotPrimary);
		if (HasDied()) return;
	}

	//Double Riposte effect, allows for a chance to do RIPOSTE with a skill specfic special attack (ie Return Kick).
	//Coded narrowly: Limit to one per client. Limit AA only. [1 = Skill Attack Chance, 2 = Skill]

	DoubleRipChance = defender->aabonuses.GiveDoubleRiposte[1];

	if(DoubleRipChance && zone->random.Roll(DoubleRipChance)) {
	Log(Logs::Detail, Logs::Combat, "Preforming a return SPECIAL ATTACK (%d percent chance)", DoubleRipChance);

		if (defender->GetClass() == Class::Monk)
			defender->DoMonkSpecialAttack(this, defender->aabonuses.GiveDoubleRiposte[2], true);
	}
}



/* Dev quotes:
 * Old formula
 *	 Final delay = (Original Delay / (haste mod *.01f)) + ((Hundred Hands / 100) * Original Delay)
 * New formula
 *	 Final delay = (Original Delay / (haste mod *.01f)) + ((Hundred Hands / 1000) * (Original Delay / (haste mod *.01f))
 * Base Delay	  20			  25			  30			  37
 * Haste		   2.25			2.25			2.25			2.25
 * HHE (old)	  -17			 -17			 -17			 -17
 * Final Delay	 5.488888889	 6.861111111	 8.233333333	 10.15444444
 *
 * Base Delay	  20			  25			  30			  37
 * Haste		   2.25			2.25			2.25			2.25
 * HHE (new)	  -383			-383			-383			-383
 * Final Delay	 5.484444444	 6.855555556	 8.226666667	 10.14622222
 *
 * Difference	 -0.004444444   -0.005555556   -0.006666667   -0.008222222
 *
 * These times are in 10th of a second
 */

void Mob::SetAttackTimer(bool trigger)
{
	attack_timer.SetDuration(3000, true);
	if (trigger) attack_timer.Trigger();
}

void Client::SetAttackTimer(bool trigger)
{
	float haste_mod = GetHaste() * 0.01f;

	//default value for attack timer in case they have
	//an invalid weapon equipped:
	attack_timer.SetDuration(4000, true);

	Timer *TimerToUse = nullptr;
	const EQ::ItemData *PrimaryWeapon = nullptr;

	for (int i = EQ::invslot::slotRange; i <= EQ::invslot::slotSecondary; i++) {
		//pick a timer
		if (i == EQ::invslot::slotPrimary)
			TimerToUse = &attack_timer;
		else if (i == EQ::invslot::slotRange)
			TimerToUse = &ranged_timer;
		else if (i == EQ::invslot::slotSecondary)
			TimerToUse = &attack_dw_timer;
		else	//invalid slot (hands will always hit this)
			continue;

		const EQ::ItemData *ItemToUse = nullptr;

		//find our item
		EQ::ItemInstance *ci = GetInv().GetItem(i);
		if (ci)
			ItemToUse = ci->GetItem();

		//see if we have a valid weapon
		if (ItemToUse != nullptr) {
			//check type and damage/delay
			if (ItemToUse->ItemClass != EQ::item::ItemClassCommon
					|| ItemToUse->Damage == 0
					|| ItemToUse->Delay == 0) {
				//no weapon
				ItemToUse = nullptr;
			}
			// Check to see if skill is valid
			else if ((ItemToUse->ItemType > EQ::item::ItemTypeLargeThrowing) &&
					(ItemToUse->ItemType != EQ::item::ItemTypeMartial) &&
					(ItemToUse->ItemType != EQ::item::ItemType2HPiercing)) {
				//no weapon
				ItemToUse = nullptr;
			}
		}

		// Hundred Fists and Blinding Speed disciplines
		int hhe = itembonuses.HundredHands + spellbonuses.HundredHands;
		int speed = 0;
		int delay = 36;
		float quiver_haste = 0.0f;
		int min_delay = RuleI(Combat, MinHastedDelay);
		// for ranged attack, the average on a client will be min delay
		// but it can be lower on a single interval.  Add an additional delay
		// adjust for clients to prevent repeated skill not ready.
		int timer_corr = 0;

		//if we have no weapon..
		if (ItemToUse == nullptr)
		{
			if (GetClass() == Class::Monk || GetClass() == Class::Beastlord)
				delay = GetHandToHandDelay();
		}
		else
		{
			//we have a weapon, use its delay
			delay = ItemToUse->Delay;
		}

		speed = static_cast<int>(((delay / haste_mod) + ((hhe / 100.0f) * delay)) * 100);

		if (ItemToUse != nullptr && ItemToUse->ItemType == EQ::item::ItemTypeBow)
		{
			timer_corr = 100;
			quiver_haste = GetQuiverHaste(); // 0.15 Fleeting Quiver
			if (quiver_haste)
			{
				int bow_delay_reduction = quiver_haste * speed + 1;
				if (speed - bow_delay_reduction > 1000) // this is how sony did it, if your delay is too it doesn't cap at the limit, it just does nothing
				{
					speed -= bow_delay_reduction;
				}
			}
		}

		TimerToUse->SetDuration(std::max(min_delay, speed) - timer_corr, true);
		if (trigger) TimerToUse->Trigger();

		if (i == EQ::invslot::slotPrimary)
			PrimaryWeapon = ItemToUse;
	}

	if (!IsDualWielding())
		attack_dw_timer.Disable();
}



void NPC::DisplayAttackTimer(Client* sender)
{
	uint32 primary = 0;
	uint32 ranged = 0;
	uint32 dualwield = 0;
	float haste_mod = GetHaste() * 0.01f;
	int speed = 0;

	for (int i = EQ::invslot::slotRange; i <= EQ::invslot::slotSecondary; i++)
	{
		int16 delay = attack_delay;

		// NOTE: delay values under 401 are consdiered hundreds of milliseconds, so we multiply by 100 for those
		if (delay < 401)
			delay *= 100;

		//special offhand stuff
		if (i == EQ::invslot::slotSecondary)
		{
			if (!IsDualWielding()) 
			{
				continue;
			}
		}

		if (!IsPet())
		{
			//find our item
			EQ::ItemInstance* ItemToUseInst = database.CreateItem(database.GetItem(equipment[i]));
			if (ItemToUseInst)
			{
				uint8 weapon_delay = ItemToUseInst->GetItem()->Delay;
				if (ItemToUseInst->IsWeapon() && weapon_delay < attack_delay)
				{
					delay = weapon_delay * 100;
				}

				safe_delete(ItemToUseInst);
			}
		}

		speed = static_cast<int>(delay / haste_mod);

		if (i == EQ::invslot::slotPrimary)
		{
			primary = speed;
		}
		else if (i == EQ::invslot::slotSecondary)
		{
			dualwield = speed;
		}
		else if (i == EQ::invslot::slotRange)
		{
			ranged = speed;
		}
	}

	sender->Message(Chat::White, "Attack Delays: Main-hand: %d  Off-hand: %d  Ranged: %d", primary, dualwield, ranged);
}



// This is one half of the atk value displayed in clients
// This is accurate and based on a client decompile done by demonstar
int Client::GetOffense(EQ::skills::SkillType skill)
{
	int statBonus;

	if (skill == EQ::skills::SkillArchery || skill == EQ::skills::SkillThrowing)
	{
		statBonus = GetDEX();
	}
	else
	{
		statBonus = GetSTR();
	}

	int offense = GetSkill(skill) + spellbonuses.ATK + itembonuses.ATK + (statBonus >= 75 ? ((2 * statBonus - 150) / 3) : 0);
	if (offense < 1)
		offense = 1;

	if (GetClass() == Class::Ranger && GetLevel() > 54)
	{
		offense = offense + GetLevel() * 4 - 216;
	}
	
	return offense;
}

int Mob::GetOffenseByHand(int hand)
{
	const EQ::ItemData* weapon = nullptr;

	if (hand != EQ::invslot::slotSecondary)
		hand = EQ::invslot::slotPrimary;

	if (IsNPC())
	{
		uint32 handItem = CastToNPC()->GetEquipment(hand == EQ::invslot::slotSecondary ? EQ::textures::weaponSecondary : EQ::textures::weaponPrimary);
		if (handItem)
			weapon = database.GetItem(handItem);
	}
	else if (IsClient())
	{
		EQ::ItemInstance* weaponInst = CastToClient()->GetInv().GetItem(hand);
		if (weaponInst && weaponInst->IsType(EQ::item::ItemClassCommon))
			weapon = weaponInst->GetItem();
	}

	if (weapon)
	{
		return GetOffense(static_cast<EQ::skills::SkillType>(GetSkillByItemType(weapon->ItemType)));
	}
	else
	{
		return GetOffense(EQ::skills::SkillHandtoHand);
	}
}

int Mob::GetToHit(EQ::skills::SkillType skill)
{
	int accuracy = 0;
	int toHit = 7 + GetSkill(EQ::skills::SkillOffense) + GetSkill(skill);

	if (IsClient())
	{
		accuracy = itembonuses.Accuracy[EQ::skills::HIGHEST_SKILL + 1] +
			spellbonuses.Accuracy[EQ::skills::HIGHEST_SKILL + 1] +
			aabonuses.Accuracy[EQ::skills::HIGHEST_SKILL + 1] +
			aabonuses.Accuracy[skill] +
			itembonuses.HitChance; //Item Mod 'Accuracy'

		// taken from a client decompile (credit: demonstar)
		int drunkValue = CastToClient()->m_pp.intoxication / 2;
		if (drunkValue > 20)
		{
			int drunkReduction = 110 - drunkValue;
			if (drunkReduction > 100)
				drunkReduction = 100;
			toHit = toHit * drunkReduction / 100;
		}
		else if (GetClass() == Class::Warrior && CastToClient()->IsBerserk())
		{
			toHit += 2 * GetLevel() / 5;
		}

	}
	else
	{
		accuracy = CastToNPC()->GetAccuracyRating();	// database value
		if (GetLevel() < 3)
			accuracy += 2;		// level 1 and 2 NPCs parsed a few points higher than expected
	}

	toHit += accuracy;
	return toHit;
}

int Mob::GetToHitByHand(int hand)
{
	if (IsNPC())
		return GetToHit(EQ::skills::SkillHandtoHand);
	
	EQ::ItemInstance* weapon;

	if (hand == EQ::invslot::slotSecondary)
		weapon = CastToClient()->GetInv().GetItem(EQ::invslot::slotSecondary);
	else
		weapon = CastToClient()->GetInv().GetItem(EQ::invslot::slotPrimary);

	if (weapon && weapon->IsType(EQ::item::ItemClassCommon))
	{
		return GetToHit(static_cast<EQ::skills::SkillType>(GetSkillByItemType(weapon->GetItem()->ItemType)));
	}
	else
	{
		return GetToHit(EQ::skills::SkillHandtoHand);
	}
}

/*	This will ignore the database AC value for NPCs under level 52 or so and calculate a value instead.
	Low level NPC mitigation estimates parsed to highly predictable and uniform values, and the AC value
	is very sensitive to erroneous entries, which means entering the wrong value in the database will
	result in super strong or weak NPCs, so it seems wiser to hardcode it.

	Most NPCs level 50+ have ~200 mit AC.  Raid bosses have more. (anywhere from 200-1200)  This uses the
	database AC value if it's higher than 200 and the default calcs to 200.

	Note that the database AC values are the computed estimates from parsed logs, so it factors in AC from
	the defense skill+agility.  If NPC data is ever leaked in the future then Sony's AC values will likely
	be lower than what the AC values in our database are because of this, and this algorithm will need to
	be altered to add in AC from defense skill and agility.
*/
int Mob::GetMitigation()
{
	int mit;

	if (IsSummonedClientPet())
	{		
		mit = GetAC();
	}
	else
	{
		if (GetLevel() < 15)
		{
			mit = GetLevel() * 3;

			if (GetLevel() < 3)
				mit += 2;
		}
		else
		{
			if (zone->GetZoneExpansion() == PlanesEQ)
				mit = 200;
			else
				mit = GetLevel() * 41 / 10 - 15;
		}

		if (mit > 200)
			mit = 200;

		if (mit == 200 && GetAC() > 200)
			mit = GetAC();
	}

	mit += (4 * itembonuses.AC / 3) + (spellbonuses.AC / 4);
	if (mit < 1)
		mit = 1;

	return mit;
}



// These are fairly accurate estimates based on many parsed Live logs
// NPC defense skill and agility values are unknowable, so we estimate avoidance AC based on miss rates
int Mob::GetAvoidance()
{
	int level = GetLevel();
	int avoidance = level * 9 + 5;

	if (level <= 50 && avoidance > 400)
		avoidance = 400;
	else if (avoidance > 460)
		avoidance = 460;

	// this is how Live does it for PCs and NPCs.  AK might have (likely) been different.  Can't know how AK did it.
	// but the difference is so small nobody would notice
	avoidance += (spellbonuses.AGI + itembonuses.AGI) * 22 / 100;
	avoidance += bonusAvoidance;
	if (avoidance < 1)
		avoidance = 1;

	return avoidance;
}

