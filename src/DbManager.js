import sqlite3 from "sqlite3";
import path from "path";

const dbPath = path.resolve("src/db/pq_items.db");

const db = new sqlite3.Database(dbPath);

function get(sql, params = []) {
  return new Promise((resolve, reject) => {
    db.get(sql, params, (err, row) => {
      if (err) reject(err);
      else resolve(row);
    });
  });
}

function all(sql, params = []) {
  return new Promise((resolve, reject) => {
    db.all(sql, params, (err, rows) => {
      if (err) reject(err);
      else resolve(rows);
    });
  });
}

export async function test () {
    const allWeapons = await all("SELECT id, name, damage, delay FROM items WHERE damage > 0");
    console.log(allWeapons)
}

test()

/*
SELECT id, Name, damage, delay, itemtype
FROM items
WHERE itemclass = 0
AND damage > 0
AND delay > 0
AND itemtype IN (0,1,2,3,4,5,35);
*/