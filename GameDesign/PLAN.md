# Working title: **Deadwater** — Unreal Engine 5 game plan

A modern-day open-ocean survival and extraction game. It plays like Sea of Thieves, but the boats have engines, the pirates carry rifles, and the islands hide sealed military facilities.

---

## 1. The pitch in one paragraph

A group of islands sits in a stretch of ocean that a military force has quietly locked down. They are searching for *something* (possibly several things) lost in underground facilities built into the islands decades ago. You arrive with a beat-up motorboat and almost nothing else. You scavenge old military gear and survival supplies from wrecks, bunkers and abandoned outposts. Armed mercenaries raid the same waters, and you have to fight them off with whatever you've found. The military patrols are a third force: a hazard you avoid, or use against the mercs.

## 2. Core loop

```
Sail out ─► Scout island / wreck ─► Explore facility (on foot) ─► Loot gear & clues
   ▲                                                                  │
   │                                                                  ▼
Upgrade boat / base ◄── Return to safe harbor ◄── Escape mercs / military ◄┘
```

- **Short loop (5–15 min):** land on an island, clear a small bunker, grab loot, get back to the boat.
- **Mid loop (1–2 hrs):** find keycards and documents that unlock deeper facility levels, and upgrade the engine, hull and weapon mounts.
- **Long loop (campaign):** piece together what the military lost, reach the final facility and decide what to do with it.

## 3. Three factions (what makes combat interesting)

| Faction | Behavior | Gear | Player relationship |
|---|---|---|---|
| **Mercenaries** ("modern pirates") | Roam in RHIBs/speedboats, board other boats, camp loot spots | AKs, shotguns, flares, maybe RPGs late-game | Always hostile: the main combat enemy |
| **Military** | Patrol boats, helicopters, sealed perimeters, searchlights | Better arms, drones | Hostile if spotted in restricted zones; also attacks mercs, so you can lure mercs into them |
| **Survivors / traders** (optional) | Floating market, radio contacts | — | Sell fuel/ammo, give leads |

## 4. Why Unreal (and which features to lean on)

Unreal 5.x covers the things Godot made hard:

| Need | Unreal feature |
|---|---|
| Ocean, waves, shorelines | **Water plugin** (Water Body Ocean, Gerstner waves, buoyancy component) |
| Wakes, spray, splashes, smoke, fire, explosions | **Niagara** particle system |
| Real fluid sim (engine wash, water pouring into a hull, smoke in bunkers) | **Niagara Fluids** plugin (2D/3D grid fluids: use sparingly, it's expensive) |
| Destructible doors, crates, boat damage | **Chaos Destruction** (Geometry Collections) |
| Boat physics | Chaos physics + Water plugin **Buoyancy** component, custom thrust forces |
| Big open ocean with many islands | **World Partition** + **Level Instances** |
| Procedural island foliage/rocks | **PCG framework** |
| Enemy AI | **StateTree** or Behavior Trees + **EQS** + **AI Perception** |
| Weapons, health, abilities, status effects | **Gameplay Ability System (GAS)** (worth learning early) |
| Input | **Enhanced Input** |
| Dark, moody facility lighting | **Lumen** + volumetric fog |
| Co-op | Built-in replication (see §9) |

**Language:** use Blueprints for prototyping and C++ for the boat physics, inventory and anything performance-sensitive. The *Lyra* sample and the *Game Animation Sample* are good references for character setup.

## 5. Key systems (build in this order)

### 5.1 Boat with an engine (the heart of the game, so build it first)
- A pawn with a `BuoyancyComponent` and 4–8 pontoon points along the hull.
- **Engine thrust** is applied at the propeller location *only when the prop is underwater*, so the boat loses thrust when it catches air off a wave. It feels great and costs almost nothing.
- **Rudder/outboard steering** is torque proportional to speed, so the boat turns badly when slow.
- **Throttle states:** reverse, idle, 1/4, 1/2, full. Fuel drains per second × throttle.
- **Engine damage:** takes bullet hits → smokes (Niagara) → sputters (random thrust dropouts) → dies. Repair with a toolkit.
- **Hull damage:** hit points on hull zones. Holes let water in, which raises boat mass and lowers the buoyancy offset. Bail it out or patch it (the Sea of Thieves feel).
- **Boat tiers:** jon boat → center-console → RHIB → ex-military patrol boat.
- **Mounts:** hardpoints for a salvaged M2 / M60 or a searchlight.

### 5.2 First-person character (on foot and on deck)
- Walking on a moving boat needs a character that inherits the platform's velocity. `CharacterMovementComponent` supports **based movement**, so test this early because it gets finicky with rocking boats.
- Swimming and diving (with an oxygen meter; scuba gear is a loot upgrade).
- Stamina, health, bleeding, hunger/thirst (keep survival light).

### 5.3 Inventory and loot
- Grid inventory (Tarkov-lite) or slot-based (simpler, so start here).
- Item data as `PrimaryDataAsset`s: weight, stack size, rarity, category.
- **Loot categories:**
  - *Military:* old rifles, pistols, ammo crates, grenades, flares, NVGs, radios, gas masks, keycards, maps, documents.
  - *Survival:* canned food, water, med kits, fuel cans, tools, rope, batteries.
  - *Boat parts:* spark plugs, props, hull patches, engine upgrades.
- Loot tables per location type (bunker, wreck, merc camp, military outpost).

### 5.4 Combat
- Hitscan for rifles at close/mid range, projectile + drop for long range. Shooting at moving boats should feel skill-based.
- Recoil, spread and weapon condition (old gear jams).
- Explosives damage Chaos destructibles.
- Mercs can **board your boat**. This is the signature fight moment.

### 5.5 Enemy AI
- **Merc boat AI:** spline-following patrols; when the player is spotted, chase, circle-strafe and attempt boarding. (Boat AI steers with the same thrust/rudder inputs as the player.)
- **Merc infantry:** StateTree with Patrol → Investigate → Engage → Take Cover → Flee, and AI Perception for sight and hearing (gunshots, engine noise).
- **Military:** stronger, coordinated, with searchlights. Getting spotted raises a "heat" level that brings a helicopter.

### 5.6 Islands and underground facilities
- Islands are hand-built outdoors, with PCG for foliage and rocks.
- Facilities are hand-built interiors streamed as **Level Instances**, so each bunker is its own sublevel.
- **Facility ideas:**
  - Flooded submarine pen (swim through, air pockets)
  - Radar station with a collapsed lower level
  - Weapons testing lab (the lost "thing" is here?)
  - Cold War comms bunker (documents/lore)
  - Hidden dry dock (unlocks the military patrol boat)
- Doors gated by keycards, generators (find fuel to power them) and explosives.

### 5.7 The "lost thing" (story hook, pick one or mix)
- A prototype weapon, an experimental power source, or a missing nuclear payload.
- Or a *person*: a scientist who went into the facilities and never came out.
- Documents and radio chatter found in facilities tell the story gradually, with no cutscenes needed.

## 6. Milestones

Each milestone ends with something **playable**. Don't move on until it's fun.

| # | Milestone | Done when… |
|---|---|---|
| **M0** | Project setup | UE 5.x project, Water plugin on, Git + LFS set up, test ocean map |
| **M1** | **Boat feels good** | Can drive a motorboat around an ocean with waves, catch air, wake/spray particles, fuel |
| **M2** | Character + boat | Walk around on a moving boat, get on/off, swim, drive from the helm |
| **M3** | One island + one bunker | Land, explore a small bunker, open doors, pick up items |
| **M4** | Inventory + 3 weapons | Pistol, rifle, shotgun; ammo; loot tables |
| **M5** | Merc infantry AI | Mercs guard a bunker, fight back, take cover |
| **M6** | Merc boats + boarding | Mercs chase you on water and board your boat |
| **M7** | Boat damage + repair | Engine/hull damage, flooding, repair kits |
| **M8** | Military faction | Restricted zones, patrols, heat system |
| **M9** | Progression | Safe harbor, upgrades, boat tiers, keycard chain |
| **M10** | Vertical slice | 3 islands, 3 facilities, first chunk of the story, polished |

M1 alone may take a few weeks and that's fine: it's what makes or breaks the game.

## 7. Suggested folder structure (inside the Unreal project)

```
Content/
  Deadwater/
    Core/          (GameMode, PlayerController, GameInstance, save system)
    Boats/         (BP_BoatBase, engines, damage, mounts)
    Characters/    (Player, Mercs, Military)
    AI/            (StateTrees, EQS, perception configs)
    Weapons/
    Items/         (DataAssets, loot tables)
    UI/
    VFX/           (Niagara: wake, spray, muzzle, smoke, fire, fluids)
    Audio/
    Maps/
      Ocean_Main   (World Partition)
      Facilities/  (Level Instances)
Source/Deadwater/  (C++: BoatMovement, Inventory, Buoyancy tweaks, GAS setup)
```

## 8. Tools and assets to grab
- **Fab (Unreal Marketplace):** free monthly assets, military props, bunker interior kits.
- **Quixel Megascans** (via Fab): rocks, cliffs, concrete surfaces for bunkers.
- **Mixamo:** placeholder character animations.
- **Git LFS** (or Perforce if the project grows): `.uasset` and `.umap` files are binary. Set LFS up *before* the first commit.
- Recommended plugins: Water, Niagara Fluids, Gameplay Abilities, StateTree, PCG, Enhanced Input.

## 9. Open decisions (make these before M2)
1. **Solo or co-op?** Sea of Thieves is fundamentally co-op. Co-op is much easier if the boat and character are built with replication from day one, and very painful to retrofit. *Recommendation: build the boat and character replicated from the start even if you only play solo at first.*
2. **First-person or third-person?** First-person is more immersive for bunkers and shooting; third-person is better for seeing the boat. (You can switch cameras: third-person on the boat, first-person on foot.)
3. **Persistence:** is it a run-based extraction game (lose gear on death) or a persistent open world with saves?
4. **Realism level:** arcade handling like Sea of Thieves, or simulation-leaning? (Recommendation: arcade with a physics feel.)
5. **Setting era/location:** present day in the Pacific? A fictional archipelago with Cold War-era facilities is a good fit for "old military equipment".

## 10. Lessons to carry over from the Godot attempt
Worth writing down when you get home: what specifically felt wrong in Godot (water visuals? boat physics? particles? performance?). That list is your acceptance criteria for M1 in Unreal. If the boat doesn't beat the Godot version on those points, keep iterating before building anything else.

## 11. First session checklist (tonight)
- [ ] Install UE 5.x via the Epic Games Launcher (plus Visual Studio 2022 with the "Game development with C++" workload if on Windows)
- [ ] Create a **C++ First Person** project named `Deadwater`
- [ ] Enable plugins: Water, Water Extras, Niagara Fluids, Gameplay Abilities, StateTree, PCG
- [ ] Set up a Git repo with LFS (`.gitattributes` for `*.uasset`, `*.umap`, textures, audio) and an Unreal `.gitignore`
- [ ] Make a test map: Water Body Ocean + one Water Body Island
- [ ] Drop a cube with a `BuoyancyComponent` and 4 pontoons, and watch it float
- [ ] Add thrust at the stern + steering torque, then drive it around. You've started M1.
