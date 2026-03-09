<script>
  import {get_damage} from "./DamageSimulator.js"

  let count = 0
  
  

  var result = "Resultat"
</script>

<h1>Deeps4Peeps</h1>

<button on:click={() => count++}>
  Klick {count}
</button>

<h2>{result}: {get_damage()}</h2>
<h2>{result}: {get_damage()}</h2>
<h2>{result}: {get_damage()}</h2>
<h2>{result}: {get_damage()}</h2>
