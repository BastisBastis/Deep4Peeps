const fs = require("fs");
const path = require("path");

const dumpPath = path.join(__dirname, "../src/db/quarm_2026-01-01-13_56.sql");
const outPath = path.join(__dirname, "../src/data/weapons_filtered.json");

console.log("Reading dump...");
const dump = fs.readFileSync(dumpPath, "utf8");
const weapons = [];

// Alla kolumnnamn som finns i dumpen (som tidigare)
const columns = [
  "id", "minstatus", "Name", "aagi", "ac", "acha", "adex", "aint", "asta", "astr",
  "awis", "bagsize", "bagslots", "bagtype", "bagwr", "banedmgamt", "banedmgbody", "banedmgrace",
  "bardtype", "bardvalue", "book", "casttime", "casttime_", "classes", "color", "price",
  "cr", "damage", "deity", "delay", "dr", "clicktype", "clicklevel2", "elemdmgtype",
  "elemdmgamt", "factionamt1", "factionamt2", "factionamt3", "factionamt4",
  "factionmod1", "factionmod2", "factionmod3", "factionmod4", "filename", "focuseffect",
  "fr", "fvnodrop", "clicklevel", "hp", "icon", "idfile", "itemclass", "itemtype",
  "light", "lore", "magic", "mana", "material", "maxcharges", "mr", "nodrop", "norent",
  "pr", "procrate", "races", "range", "reclevel", "recskill", "reqlevel", "sellrate",
  "size", "skillmodtype", "skillmodvalue", "slots", "clickeffect", "tradeskills", "weight",
  "booktype", "recastdelay", "recasttype", "updated", "comment", "stacksize", "stackable",
  "proceffect", "proctype", "proclevel2", "proclevel", "worneffect", "worntype",
  "wornlevel2", "wornlevel", "focustype", "focuslevel2", "focuslevel", "scrolleffect",
  "scrolltype", "scrolllevel2", "scrolllevel", "serialized", "verified", "serialization",
  "source", "lorefile", "questitemflag", "clickunk5", "clickunk6", "clickunk7",
  "procunk1", "procunk2", "procunk3", "procunk4", "procunk6", "procunk7", "wornunk1",
  "wornunk2", "wornunk3", "wornunk4", "wornunk5", "wornunk6", "wornunk7", "focusunk1",
  "focusunk2", "focusunk3", "focusunk4", "focusunk5", "focusunk6", "focusunk7",
  "scrollunk1", "scrollunk2", "scrollunk3", "scrollunk4", "scrollunk5", "scrollunk6",
  "scrollunk7", "clickname", "procname", "wornname", "focusname", "scrollname", "created",
  "bardeffect", "bardeffecttype", "bardlevel2", "bardlevel", "bardunk1", "bardunk2",
  "bardunk3", "bardunk4", "bardunk5", "bardname", "bardunk7", "gmflag", "soulbound",
  "min_expansion", "max_expansion", "legacy_item", "content_flags", "content_flags_disabled"
];

// Lista med kolumner som ska **tas bort**
const skipColumns = [
  "minstatus","bagsize","bagslots","bagtype","bagwr","banedmgamt","banedmgbody","banedmgrace",
  "bardtype","bardvalue","book","casttime","casttime_","factionamt1","factionamt2","factionamt3",
  "factionamt4","factionmod1","factionmod2","factionmod3","factionmod4","filename","clicklevel",
  "booktype","recastdelay","recasttype","updated","comment","stacksize","stackable","scrolleffect",
  "scrolltype","scrolllevel2","scrolllevel","serialized","verified","serialization","source",
  "lorefile","questitemflag","created","bardeffect","bardeffecttype","bardlevel2","bardlevel",
  "bardunk1","bardunk2","bardunk3","bardunk4","bardunk5","bardname","bardunk7","gmflag","soulbound",
  "min_expansion","max_expansion","legacy_item","content_flags","content_flags_disabled"
];

// Regex som matchar alla INSERT INTO `items` ... VALUES (...);
const insertRegex = /INSERT INTO `items` .*?VALUES\s*(.*?);/gs;

// Regex som matchar varje enskild kolumn i en row, tar hänsyn till strängar med kommatecken
const valueRegex = /'((?:\\'|[^'])*)'|([^,()]+)/g;

let match;
while ((match = insertRegex.exec(dump)) !== null) {
  const valuesPart = match[1];
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

    if (cols.length !== columns.length) continue;

    const item = {};
    columns.forEach((colName, i) => {
      if (!skipColumns.includes(colName)) {
        item[colName] = cols[i];
      }
    });

    // Filtrera vapen
    if (
      item.itemclass === 0 &&
      item.damage > 0 &&
      item.delay > 0 &&
      [0,1,2,3,4,5,35].includes(item.itemtype)
    ) {
      weapons.push(item);
    }
  }
}

console.log("Found weapons:", weapons.length);
fs.writeFileSync(outPath, JSON.stringify(weapons, null, 2));
console.log("Saved to", outPath);