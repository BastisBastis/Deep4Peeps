const fs = require("fs");
const path = require("path");

const dumpPath = path.join(__dirname, "../src/db/quarm_2026-01-01-13_56.sql");
const outPath = path.join(__dirname, "../src/data/spells_en.json");

console.log("Reading dump...");
const dump = fs.readFileSync(dumpPath, "utf8");
const spells = [];

// Lista med kolumnnamn för spells_en
// OBS: justera med exakta kolumner i din spells_en-tabell
const columns = [
  "id","name","player_1","teleport_zone","you_cast","other_casts","cast_on_you",
  "cast_on_other","spell_fades","range","aoerange","pushback","pushup","cast_time",
  "recovery_time","recast_time","buffdurationformula","buffduration","AEDuration","mana",
  "effect_base_value1","effect_base_value2","effect_base_value3","effect_base_value4","effect_base_value5",
  "effect_base_value6","effect_base_value7","effect_base_value8","effect_base_value9","effect_base_value10",
  "effect_base_value11","effect_base_value12","effect_limit_value1","effect_limit_value2","effect_limit_value3",
  "effect_limit_value4","effect_limit_value5","effect_limit_value6","effect_limit_value7","effect_limit_value8",
  "effect_limit_value9","effect_limit_value10","effect_limit_value11","effect_limit_value12","max1","max2","max3",
  "max4","max5","max6","max7","max8","max9","max10","max11","max12","icon","memicon",
  "components1","components2","components3","components4","component_counts1","component_counts2",
  "component_counts3","component_counts4","NoexpendReagent1","NoexpendReagent2","NoexpendReagent3","NoexpendReagent4",
  "formula1","formula2","formula3","formula4","formula5","formula6","formula7","formula8","formula9","formula10",
  "formula11","formula12","LightType","goodEffect","Activated","resisttype","effectid1","effectid2","effectid3",
  "effectid4","effectid5","effectid6","effectid7","effectid8","effectid9","effectid10","effectid11","effectid12",
  "targettype","basediff","skill","zonetype","EnvironmentType","TimeOfDay","classes1","classes2","classes3",
  "classes4","classes5","classes6","classes7","classes8","classes9","classes10","classes11","classes12",
  "classes13","classes14","classes15","CastingAnim","TargetAnim","TravelType","SpellAffectIndex","disallow_sit",
  "deities0","deities1","deities2","deities3","deities4","deities5","deities6","deities7","deities8","deities9",
  "deities10","deities11","deities12","deities13","deities14","deities15","deities16","npc_no_cast","ai_pt_bonus",
  "new_icon","spellanim","uninterruptable","ResistDiff","dot_stacking_exempt","deleteable","RecourseLink",
  "no_partial_resist","small_targets_only","use_persistent_particles","short_buff_box","descnum","typedescnum",
  "effectdescnum","effectdescnum2","npc_no_los","reflectable","resist_per_level","resist_cap","EndurCost",
  "EndurTimerIndex","IsDiscipline","HateAdded","EndurUpkeep","pvpresistbase","pvpresistcalc","pvpresistcap",
  "spell_category","pvp_duration","pvp_duration_cap","cast_not_standing","can_mgb","nodispell","npc_category",
  "npc_usefulness","wear_off_message","suspendable","spellgroup","allow_spellscribe","allowrest","custom_icon",
  "not_player_spell","disabled","persist_through_death"
];

// Kolumner som vi kan ignorera i JSON
const skipColumns = ["created", "updated"];

// Robust regex som matchar INSERT INTO `spells_en` ... VALUES (...) med radbrytningar
const insertRegex = /INSERT INTO `spells_en` .*?VALUES\s*(.*?);/gs;

// Regex som matchar varje kolumn i en row
const valueRegex = /'((?:\\'|[^'])*)'|([^,()]+)/g;

let match;
while ((match = insertRegex.exec(dump)) !== null) {
  const valuesPart = match[1];

  // Separera rader på '),(' men ta bort eventuella parenteser i början/slutet
  const rows = valuesPart.split(/\),\s*\(/).map(r => r.replace(/^\(|\)$/g, "").trim());

  for (let row of rows) {
    if (!row) continue;

    const cols = [];
    let colMatch;
    while ((colMatch = valueRegex.exec(row)) !== null) {
      if (colMatch[1] !== undefined) {
        cols.push(colMatch[1].replace(/\\'/g, "'"));
      } else if (colMatch[2] !== undefined) {
        const num = Number(colMatch[2]);
        cols.push(isNaN(num) ? colMatch[2].trim() : num);
      }
    }

    if (cols.length !== columns.length) {
      console.warn("Skipped row due to column mismatch:", row.length, row);
      break;
      continue;
    }

    const spell = {};
    columns.forEach((colName, i) => {
      if (!skipColumns.includes(colName)) {
        spell[colName] = cols[i];
      }
    });

    spells.push(spell);
  }
}

//console.log("Found spells:", spells.length);
fs.writeFileSync(outPath, JSON.stringify(spells, null, 2));
console.log("Saved to", outPath);