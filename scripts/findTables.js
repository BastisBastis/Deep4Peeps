const fs = require("fs");
const path = require("path");

const dumpPath = path.join(__dirname, "../src/db/quarm_2026-01-01-13_56.sql");
const outPath = path.join(__dirname, "../src/data/spells.json");

console.log("Reading dump...");
const dump = fs.readFileSync(dumpPath, "utf8");

// Hitta alla INSERT INTO-satser
const insertTables = [...dump.matchAll(/INSERT INTO `(.*?)`/g)].map(m => m[1]);
console.log("INSERT INTO tables found:", [...new Set(insertTables)]);

// Hitta alla CREATE TABLE-satser
const createTables = [...dump.matchAll(/CREATE TABLE `(.*?)`/g)].map(m => m[1]);
console.log("CREATE TABLE tables found:", [...new Set(createTables)]);