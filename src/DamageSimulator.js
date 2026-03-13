import { Random } from "./RandomNumberGenerator.js"

const rng = new Random()

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
  
  const getToHit = (offenseSkillLevel, weaponSkillLevel) => {
  	var accuracy = 0;
	var toHit = 7 + offenseSkillLevel + weaponSkillLevel;

    var itemBonus = 0
    var spellBonus = 0
    var aaBonusAccuracy = 0
    var itembonusHitChance = 0

    accuracy = itemBonus +
        spellBonus +
        aaBonusAccuracy +
        itembonusHitChance; //Item Mod 'Accuracy'


	


	toHit += accuracy;
	return toHit;
  }
  
  const getAvoidance= (targetLevel, targetBonusAGI = 0, bonusAvoidance = 0)=>
{
	
	var level = targetLevel
	var avoidance = level * 9 + 5
	

	if (level <= 50 && avoidance > 400)
		avoidance = 400;
	else if (avoidance > 460)
		avoidance = 460;
	

	// this is how Live does it for PCs and NPCs.  AK might have (likely) been different.  Can't know how AK did it.
	// but the difference is so small nobody would notice
	
	if (avoidance < 1)
		avoidance = 1;
	

	return avoidance;
}
  
  
  
  const avoidanceCheck = (offenseSkillLevel, weaponSkillLevel, targetLevel)=> {
	

	var toHit = getToHit(offenseSkillLevel, weaponSkillLevel)
	var avoidance = getAvoidance(targetLevel)
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
	//console.log(`Hit chance: ${hitChance}`)


	if (rng.real(0.0, 1.0) < hitChance)
	{
		//console.log("hit")
		return true;
	}

	//console.log("miss")
	return false;
}
  
  const getDamageBonus = (primary, charClass, level, weaponType, delay) =>{
    
    if (level < 28 || !isDmgBonusClass(charClass)) {
    	//console.log("no damage bonus", level, primary, !isDmgBonusClass())
		return 0
	}

	var bonus = 1 + (level - 28) / 3
	

	if ( weaponType == weaponTypes.TWO_HAND )
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
  
  const getOffense = (weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength, level, charClass, wornAtk) =>
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
	var itemBonus = wornAtk

	var offense = weaponSkillLevel + spellBonus + itemBonus + (statBonus >= 75 ? ((2 * statBonus - 150) / 3) : 0);
	if (offense < 1)
		offense = 1;

	if (charClass == classes.RANGER && level > 54)
	{
		offense = offense + level * 4 - 216;
	}
	
	return offense;
}

const rollD20 = (offense, mitigation) =>
{
	var atkRoll = rng.roll0(offense + 5);
	var defRoll = rng.roll0(mitigation + 5);

	var avg = (offense + mitigation + 10) / 2;
	var index = Math.max(0, (atkRoll - defRoll) + (avg / 2));
	index = (index * 20) / avg;
	index = Math.max(0, index);
	index = Math.min(19, index);

	return index + 1;
}

const getMitigation = (ac, level) =>
{
	var mit;

	
    if (level < 15)
    {
        mit = level * 3;

        if (level < 3)
            mit += 2;
    }
    else
    {
        if (false) //zone->GetZoneExpansion() == PlanesEQ)
            mit = 200;
        else
            mit = level * 41 / 10 - 15;
    }

    if (mit > 200)
        mit = 200;

    if (mit == 200 && ac > 200)
        mit = ac;


	//mit += (4 * itembonuses.AC / 3) + (spellbonuses.AC / 4); Don't think this is worth simulating
	if (mit < 1)
		mit = 1;

    //console.log(`mitigation: ${mit}`)

	return mit;
}
  
  const calcMeleeDamage = (baseDamage, weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength, level, charClass, targetAC, targetLevel, wornAtk) =>
{

	// ranged physical damage does half that of melee
	if (weaponType == weaponTypes.ARCHERY)
		baseDamage /= 2;

	var offense = getOffense(weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength, level, charClass, wornAtk)
    //console.log(`in calcMeleeDamage offense ${offense}`)
	// mitigation roll
	var roll = rollD20(offense, getMitigation(targetAC, targetLevel));
    //console.log(`in calcMeleeDamage roll ${roll}`)

	var minHit = baseDamage
    //console.log(`in calcMeleeDamage minhit ${minHit}`)
	

	var damage = (roll * baseDamage + 5) / 10;
	if (damage < minHit) damage = minHit;
	if (damage < 1)
		damage = 1;

	var roll2 = rollDamageMultiplier(offenseSkillLevel, weaponType, charClass, level);
    damage = damage * roll2 / 100;

    if (level >= 55 && damage > 1 && weaponType != weaponTypes.ARCHERY && isDmgBonusClass())
        damage++;

	return damage;
}

// the output of this function is precise and is based on the code from:
// https://forums.daybreakgames.com/eq/index.php?threads/progression-monks-we-have-work-to-do.229581/
const rollDamageMultiplier = (offense, skill, charClass, level) =>
{
	var rollChance = 51;
	var maxExtra = 210;
	var minusFactor = 105;

	if (charClass == classes.MONK && level >= 65)
	{
		rollChance = 83;
		maxExtra = 300;
		minusFactor = 50;
	}
	else if (level >= 65 || (charClass == classes.MONK && level >= 63))
	{
		rollChance = 81;
		maxExtra = 295;
		minusFactor = 55;
	}
	else if (level >= 63 || (charClass == classes.MONK && level >= 60))
	{
		rollChance = 79;
		maxExtra = 290;
		minusFactor = 60;
	}
	else if (level >= 60 || (charClass == classes.MONK && level >= 56))
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
	else if (level >= 51 || charClass == classes.MONK)
	{
		rollChance = 65;
		maxExtra = 245;
		minusFactor = 80;
	}

	var baseBonus = (offense - minusFactor) / 2;
	if (baseBonus < 10)
		baseBonus = 10;

	if (rng.roll(rollChance))
	{
		var roll;

		roll = rng.int(0, baseBonus) + 100;
		if (roll > maxExtra)
			roll = maxExtra;

		

		return roll;
	}
	else
	{
		return 100;
	}
}
  
const getDoubleAttackChance = (doubleAttackSkillLevel, level) => {
    var chance = doubleAttackSkillLevel

	if (chance > 0)
	{
		chance += level
	}

    /*
	// Bestial Frenzy/Harmonious Attack AA - grants double attacks for classes that otherwise do not get the skill
	if (aabonuses.GiveDoubleAttack)
		chance += aabonuses.GiveDoubleAttack * 5;

	// Knight's Advantage and Ferocity AAs; Double attack rate is 67% at 245 skill with Ferocity, so this is the only way it can be
	if (aabonuses.DoubleAttackChance)
		chance += chance * aabonuses.DoubleAttackChance / 100;
    */

	return chance
}

const getDualWieldChance = (dualWieldSkillLevel, level) => {
    var chance = dualWieldSkillLevel
	if (chance > 0)
	{
		chance += level
	}

	//chance += aabonuses.Ambidexterity; // 32

	// SE_DualWieldChance[176] - Deftdance and Kinesthetics Disciplines
	//chance += spellbonuses.DualWieldChance;

	return chance
}


  const get_attack_result = (primary = true, charClass = classes.WARRIOR, level = 60, weaponType = weaponTypes.ONE_HAND, baseDamage = 10, delay = 30, weaponSkillLevel = 250, offenseSkillLevel = 250, dexterity = 255, strength = 255, targetAC = 800, targetLevel = 60, wornAtk = 0) => {
    var damage = 1
    
    //console.log(`Base Damage ${baseDamage}`)

    //skip damage caps - not worth it
    
    const damageBonus = getDamageBonus(primary, charClass, level, weaponType, delay)
    //console.log(`Damage bonus: ${damageBonus}`)
    
    //Assume attacking from behind, skip avoidDamage() (dodge etc)
    
    if (!avoidanceCheck(offenseSkillLevel, weaponSkillLevel, targetLevel)) {
    	//console.log("Failed avoidance check")
    	return 0
    }
    
    damage = damageBonus + calcMeleeDamage(baseDamage, weaponType, weaponSkillLevel, offenseSkillLevel, dexterity, strength, level, charClass, targetAC, targetLevel, wornAtk)
    
    
    
    //console.log("damage damagebonus", baseDamage, damageBonus)
    

    return Math.floor(damage)
  }

  export const simulate = ({
    charClass = classes.WARRIOR, 
    level = 60, 
    weaponType = weaponTypes.ONE_HAND, 
    baseDamage = 10, 
    delay = 30, 
    weaponSkillLevel = 250, 
    offhandWeaponSkillLevel = 250,
    offenseSkillLevel = 250, 
    dexterity = 255, 
    strength = 255, 
    targetAC = 800, 
    targetLevel = 60,
    simulation_duration = 1000, //seconds,
    dualWield = true,
    offhandWeaponType = weaponTypes.ONE_HAND,
    offhandBaseDamage = 15,
    offhandDelay = 10,
    doubleAttackSkillLevel = 250,
    dualWieldSkillLevel = 250, 
    haste = 40,
    wornAtk = 0
  } = {}) => {

    delay = delay / ((100+haste)/100)
    offhandDelay = offhandDelay / ((100+haste)/100)
    console.log(delay)

    var timer = 0
    var total_damage = 0
    var hits = []
    var dualWieldTimer = 0
    var numMainAttacks = 0
    var numDoubleAttacks = 0
    var numTripleAttacks = 0
    var numOffhandAttacks = 0
    while (timer < simulation_duration) {
        //try proc
        
        //Regular attack
        var damage = get_attack_result(true, charClass, level, weaponType, baseDamage, delay, weaponSkillLevel, offenseSkillLevel, dexterity, strength, targetAC, targetLevel, wornAtk)
        hits.push(damage)
        total_damage += damage
        numMainAttacks++

        //Try double attack
        if (getDoubleAttackChance(doubleAttackSkillLevel, level) > rng.int(0,499)) {
            var damage = get_attack_result(true, charClass, level, weaponType, baseDamage, delay, weaponSkillLevel, offenseSkillLevel, dexterity, strength, targetAC, targetLevel, wornAtk)
            hits.push(damage)
            total_damage += damage
            numDoubleAttacks++

            //try triple attack
            if ((charClass == classes.WARRIOR || charClass == classes.MONK) && level >= 60 && (rng.int(0,999) < 135)) {
                var damage = get_attack_result(true, charClass, level, offhandWeaponType, baseDamage, delay, weaponSkillLevel, offenseSkillLevel, dexterity, strength, targetAC, targetLevel, wornAtk)
                hits.push(damage)
                total_damage += damage
                numTripleAttacks++
            }

        }

        if (dualWield) {
         //Dual Wield
            dualWieldTimer += delay
            while (dualWieldTimer >= offhandDelay) {
                
                //CheckDualWield
                if (getDualWieldChance(dualWieldSkillLevel, level) > rng.int(0, 374)) {
                    numOffhandAttacks++
                    var damage = get_attack_result(false, charClass, level, weaponType, offhandBaseDamage, offhandDelay, offhandWeaponSkillLevel, offenseSkillLevel, dexterity, strength, targetAC, targetLevel, wornAtk)
                    hits.push(damage)
                    total_damage += damage
                    //try proc
                    //attack offhand
                }
                dualWieldTimer -= offhandDelay
            }
        }

       

        
        timer += delay/10
    }
    var dps = total_damage/simulation_duration
    return {
        dps: Math.floor(dps * 10) / 10,
        total_damage : total_damage,
        hits: hits,
        numDoubleAttacks,
        numMainAttacks,
        numTripleAttacks,
        numOffhandAttacks
    }

  }