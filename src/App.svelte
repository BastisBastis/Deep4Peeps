<script>
  import {simulate} from "./DamageSimulator.js"
  var result_string = "Not started yet"
  var hits_string = ""

  const classes={
    "Warrior" : 0,
    "Rogue" : 1,
    "Monk" : 2,
    "Paladin" : 3,
    "Shadow Knight" : 4,
    "Bard" : 5,
    "Ranger" : 6,
    "Beastlord" : 7,
    "Cleric" : 8,
    "Druid" : 9,
    "Shaman" : 10,
    "Magician" : 11,
    "Enchanter" : 12,
    "Necromancer" : 13,
    "Wizard" : 14
  }
  

  const weaponTypes = {
    "One handed weapon" : 0,
    "Two handed weapon" : 1,
    "Archery" : 2
  };

  let charClass = classes.WARRIOR;
  let level = 60;
  let weaponType = weaponTypes.ONE_HAND;
  let baseDamage = 10;
  let delay = 30;
  let weaponSkillLevel = 250;
  let offenseSkillLevel = 250;
  let dexterity = 255;
  let strength = 255;
  let targetAC = 800;
  let targetLevel = 60;
  let simulation_duration = 1000;
  let dualWield = true;
  let offhandWeaponType = weaponTypes.ONE_HAND;
  let offhandBaseDamage = 10
  let offhandDelay = 30
  let offhandWeaponSkillLevel = 250


  const start_sim = () => {
    var result = simulate({
      charClass,
      level,
      weaponType,
      baseDamage,
      delay,
      weaponSkillLevel,
      offenseSkillLevel,
      dexterity,
      strength,
      targetAC,
      targetLevel,
      simulation_duration,
      offhandBaseDamage,
      offhandWeaponSkillLevel,
      offhandWeaponType,
      offhandDelay
    })
    result_string = `Result: ${result.dps} dps - MH: ${result.numMainAttacks} - DA: ${result.numDoubleAttacks} - TA: ${result.numTripleAttacks} - OH: ${result.numOffhandAttacks}`
    hits_string = ""
    for (let i in result.hits) {
      if (i > 0)
        hits_string += "\n"
      if (result.hits[i] == 0)
        hits_string += "miss"
      else
        hits_string += result.hits[i]
    }
  }
  
  

  
</script>

<h1>Deeps4Peeps</h1>

<div class="form">
  <label>
    Class
    <select bind:value={charClass}>
      {#each Object.keys(classes) as k}
        <option value={classes[k]}>{k}</option>
      {/each}
    </select>
  </label>

  <label>
    Level
    <input type="number" bind:value={level} />
  </label>

  <label>
    Main hand Weapon Type
    <select bind:value={weaponType}>
      {#each Object.keys(weaponTypes) as k}
        <option value={weaponTypes[k]}>{k}</option>
      {/each}
    </select>
  </label>

  <label>
    Main hand Base Damage
    <input type="number" bind:value={baseDamage} />
  </label>

  <label>
    Main hand Delay
    <input type="number" bind:value={delay} />
  </label>

  <label>
    Main hand Weapon Skill
    <input type="number" bind:value={weaponSkillLevel} />
  </label>

  <label>
    Offhand Weapon Type
    <select bind:value={offhandWeaponType}>
      {#each Object.keys(weaponTypes) as k}
        <option value={weaponTypes[k]}>{k}</option>
      {/each}
    </select>
  </label>

  <label>
    Offhand Base Damage
    <input type="number" bind:value={offhandBaseDamage} />
  </label>

  <label>
    Offhand Delay
    <input type="number" bind:value={offhandDelay} />
  </label>

  <label>
    Offhand Weapon Skill
    <input type="number" bind:value={offhandWeaponSkillLevel} />
  </label>

  <label>
    Offense Skill
    <input type="number" bind:value={offenseSkillLevel} />
  </label>

  <label>
    Dexterity
    <input type="number" bind:value={dexterity} />
  </label>

  <label>
    Strength
    <input type="number" bind:value={strength} />
  </label>

  <label>
    Target AC
    <input type="number" bind:value={targetAC} />
  </label>

  <label>
    Target Level
    <input type="number" bind:value={targetLevel} />
  </label>

  <label>
    Simulation Duration
    <input type="number" bind:value={simulation_duration} />
  </label>

  <button on:click={start_sim}>
    Simulate
  </button>


</div>



<h2>{result_string}</h2>
<p style="white-space: pre-line">{hits_string}</p>

<style>
.form {
  display: grid;
  grid-template-columns: 200px;
  gap: 10px;
}

label {
  display: flex;
  flex-direction: column;
}

button {
  margin-top: 10px;
}
</style>
