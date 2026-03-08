<script>
  let count = 0
  
  const classes={
    "WARRIOR" : 0,
    "ROGUE" : 1,
    "MONK" : 2,
    "PALADIN" : 3,
    "SHADOW_KNIGHT": 4,
    "BARD" : 5,
    "RANGER" : 6,
    "BEASTLORD" : 7,
    "CLERIC" : 8,
    "DRUID" : 9,
    "SHAMAN" : 10,
    "MAGICIAN" : 11,
    "ENCHANTER" : 12,
    "NECROMANCER" : 13,
    "WIZARD" : 14
  }
  
  const weaponTypes = {
  	"ONE_HAND" : 0,
  	"TWO_HAND" : 1,
  	"ARCHERY" : 2
  }
  
  const isDmgBonusClass = (charClass) => {
    return [
      classes.WARRIOR,
      classes.ROGUE,
      classes.MONK,
      classes.PALADIN,
      classes.SHADOW_KNIGHT,
      classes.BARD,
      classes.BEASTLORD,
      classes.RANGER
    ].includes(charClass)
  }
  
  const getToHit = () => {
  	return 50
  }
  
  const getAvoidance = () =>{
  	return 50
  }
  
  const randf = (min, max) => { //TODO: Update to EQ RNG
  	return Math.random() * (max - min) + min
  }
  
  const randi = (min, max) => { //TODO: Update to EQ RNG
  	return Math.Floor(Math.random() * (max - min) )+ min
  }
  
  const avoidanceCheck = ()=> {
	

	var toHit = getToHit()
	var avoidance = getAvoidance()
	var toHitPct = 0
	var avoidancePct = 0
	
	/* ADD SUPPORT FOR MODIFIERS
	
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
	*/

	toHit = toHit * (100 + toHitPct) / 100;
	avoidance = avoidance * (100 + avoidancePct) / 100;
	
	

	var hitChance
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
	console.log(`Hit chance: ${hitChance}`)


	if (randf(0.0, 1.0) < hitChance)
	{
		console.log("hit")
		return true;
	}

	console.log("miss")
	return false;
}
  
  const getDamageBonus = (primary, charClass, level, weaponType, delay) =>{
    
    if (level < 28 || !isDmgBonusClass(charClass)) {
    	console.log("no damage bonus", level, primary, !isDmgBonusClass())
		return 0
	}

	var bonus = 1 + (level - 28) / 3
	

	if ( weaponType == weaponTypes.ONE_HAND )
	{
		if (delay <= 27)
			return bonus + 1

		if (level > 29)
		{
			var level_bonus = (level - 30) / 5 + 1
			if (level > 50)
			{
				level_bonus++;
				var level_bonus2 = level - 50;
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
			var delay_bonus = (delay - 40) / 3 + 1;
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
  
  const getOffense = (weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength, level, charClass) =>
{
	var statBonus

	if (weaponType == weaponType.ARCHERY)
	{
		statBonus = dexterity
	}
	else
	{
		statBonus = strength
	}
	
	var spellBonus = 0 // spellbonus.ATK
	var itemBonus = 0 // itemBonus

	var offense = weaponSkillLevel + spellBonuses + itemBonus + (statBonus >= 75 ? ((2 * statBonus - 150) / 3) : 0);
	if (offense < 1)
		offense = 1;

	if (charClass == classes.RANGER && level > 54)
	{
		offense = offense + level * 4 - 216;
	}
	
	return offense;
}
  
  const calcMeleeDamage => (baseDamage, weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength, level, charClass) =>
{

	// ranged physical damage does half that of melee
	if (weaponType == weaponTypes.ARCHERY)
		baseDamage /= 2;

	var offense = getOffense(weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength, level, charClass)

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
  


  const get_damage = (primary = true, charClass = classes.WARRIOR, level = 60, weaponType = weaponTypes.ONE_HAND, baseDamage = 10, delay = 30, weaponSkillLevel = 250, offenseSkillLevel = 250, dexterity = 255, strength = 255) => {
    var damage = 1
    
    console.log(`Base Damage ${baseDamage}`)

    //skip damage caps - not worth it
    
    const damageBonus = getDamageBonus(primary, charClass, level, weaponType, delay)
    console.log(`Damage bonus: ${damageBonus}`)
    
    //Assume attacking from behind, skip avoidDamage() (dodge etc)
    
    if (!avoidanceCheck()) {
    	console.log("Failed avoidance check")
    	return 0
    }
    
    damage = damageBonus + calcMeleeDamage(baseDamage, weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength)
    
    
    
    console.log("damage damagebonus", baseDamage, damageBonus)
    

    return baseDamage + damageBonus
  }

  var result = "Resultat: "
</script>

<h1>Hello Svelte</h1>

<button on:click={() => count++}>
  Klick {count}
</button>

<h2>{result}: {get_damage()}</h2>