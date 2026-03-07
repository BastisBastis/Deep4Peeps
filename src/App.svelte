<script>
  let count = 0

  const get_damage = (primary = true) => {
    var damage = 1
    var level = 60
    var baseDamage = 10
    var elementalDamage = 0
    baseDamage += elementalDamage

    //skip damage caps - not worth it
    var damageBonus = 0
    if (primary) {
      damageBonus = 10
    }

    return baseDamage + damageBonus
  }

  var result = "Resultat: "
</script>

<h1>Hello Svelte</h1>

<button on:click={() => count++}>
  Klick {count}
</button>

<h2>{result}: {get_damage()}</h2>