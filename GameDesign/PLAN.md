# Working title: **Deadwater**: game design plan

A modern-day open-ocean survival and extraction game. It plays like Sea of Thieves, but the boats have engines, the pirates carry rifles, and the islands hide sealed military facilities.

Engine: Unreal Engine 5, chosen for its particle and fluid simulation.

---

## The pitch
A group of islands sits in a stretch of ocean that a military force has quietly locked down. They are searching for something (possibly several things) lost in underground facilities built into the islands decades ago. You arrive with a beat-up motorboat and almost nothing else. You scavenge old military gear and survival supplies. Armed mercenaries raid the same waters, and you have to fight them off with whatever you've found. The military is a third force that you either avoid or use against the mercs.

## Core loop
Sail out → scout an island or wreck → explore a facility on foot → loot gear and clues → escape the mercs or military → return to harbor → upgrade → repeat.

- **Short loop:** land, clear a small bunker, grab loot, get back to the boat.
- **Mid loop:** find keycards and documents that open deeper facility levels, and upgrade the boat.
- **Long loop:** piece together what the military lost and reach the final facility.

## Factions
| Faction | Role |
|---|---|
| **Mercenaries** | Rival crews of modern pirates. They chase, shoot and board your boat, fight over territory, and hunt you harder the more you hurt them. |
| **Military** | Grows stronger over time. Can be fought at any time, and has the best loot in the game. They also attack mercs. |
| **Survivors / traders** *(optional)* | A floating market and radio contacts that sell fuel and ammo and give leads. |

## Mercenaries

**Rival crews, not one army.** Several merc gangs work the islands, and they hate each other almost as much as they hate you. Each crew has its own:
- **Territory:** islands, camps, docks and fuel depots it controls.
- **Style:** how it fights, what boats it runs and what guns it carries. For example, one crew could be fast and reckless with speedboats and SMGs, one disciplined ex-soldiers with rifles and a gunboat, and one scavengers who set traps and ambushes.
- **Leader:** a boss who can be hunted down.

**Territory:**
- Clear a camp and that area is safer for a while, until the crew (or a rival) moves back in.
- When one crew weakens, rival crews expand into its territory, so the map shifts over time.
- You can play crews against each other: lead one gang's boats into another's waters, or steal from one and let the other take the blame.

**Escalation:** each crew tracks how much you've hurt it. The more damage you do, the harder it hunts you:
1. Ignored: you're just another boat.
2. Noticed: patrols attack on sight.
3. Hunted: ambushes at your usual spots, and raids on your anchored boat.
4. War: the crew sends its best boats and fighters after you, and the boss gets involved.

Escalation cools off over time, or drops sharply if you kill the crew's leader.

## Military

**Their presence grows over time.** The military is closing in on whatever it lost, and the islands change as it does:
1. **Early:** a few scout boats, a distant helicopter now and then, a supply drop offshore. Mostly rumors.
2. **Mid:** forward outposts on the islands, regular sea patrols, restricted zones around key bunkers, and searchlights at night.
3. **Late:** a heavy presence. Helicopters, armed patrol boats, drones, checkpoints, and soldiers clearing bunkers ahead of you.

**You can fight them any time, if you can survive it.**
- Soldiers are better trained, better armed and better coordinated than mercs.
- Military outposts and supply drops have the best loot in the game: modern rifles, armor, night vision, explosives and military boats.
- Attacking them raises an alert level. Push it too high and they send reinforcements, a helicopter or a gunboat.
- They fight the mercs as well. Luring a merc crew into a patrol is a real tactic.
- Their progress and yours are linked: the deeper they get into the bunkers, the more of the islands they lock down. That makes getting there first a race.

## Boats

**You start with nothing.** The crew washes up and has to build a raft from driftwood, barrels, rope and scrap. You paddle it at first, then bolt on a salvaged outboard motor once you find one.

**Getting better boats (there is no shop):**
- **Find:** wrecked and abandoned boats on beaches, reefs and in bunker docks. You have to repair them before they run: patch the hull, fix the engine, find fuel.
- **Take:** kill a merc crew and their boat is yours, if it's still afloat.
- **Repair and improve:** a boat is only as good as the parts you put into it: engines, hull plating, gun mounts, searchlights.

**Stealing works both ways:**
- Mercs can board and take your boat. If you leave it anchored unguarded while you raid a bunker, it might be gone when you come back.
- Leaving someone on watch becomes a real crew decision.
- A stolen boat can be tracked down and taken back.

**How boats handle:**
- Realistic handling: weight, momentum, waves, fuel and throttle all matter.
- Damage matters: a shot-up engine smokes and sputters, and hull holes let water in until you patch and bail.
- Rough progression: raft → patched-up small boats → merc speedboats and RHIBs → ex-military patrol boat.
- Mercs pull alongside and board, which is the signature fight.

## Weapons

**Crafted first, looted later.**
- **Early game (crafted):** spears, a machete from scrap, a bow, fishing-spear guns, pipe shotguns, Molotovs, and scrap-metal armor.
- **Later (looted):** old military rifles, pistols, shotguns, grenades and flares from bunkers, plus modern guns taken off dead mercs.
- Crafted weapons stay useful. A quiet bow still matters when the crew wants to avoid attention.

**Realistic gunplay:**
- A few hits kill, for players and enemies alike.
- Real recoil and bullet drop, with no crosshair when firing from the hip.
- Old military guns can jam and wear down, and need cleaning and parts.
- Ammo is scarce, and each gun uses its own caliber.
- Shooting from a moving boat is hard: the boat rocks and bounces on the waves.
- Gunshots carry, so mercs and the military can hear you.

## Survival (medium)
- **Hunger and thirst** matter but don't nag. Fish, gather coconuts and fruit, hunt, collect rainwater, and boil or filter water.
- **Injuries:** bleeding needs bandages and deeper wounds need a med kit. Being hurt slows you down and makes your aim shaky.
- **Sun and heat:** days are hot, and night is dark and dangerous.
- **Crafting** uses what the islands and wrecks provide: driftwood, rope, scrap metal, cloth, plastic and parts.

## Loot
- **Military:** old guns, ammo, grenades, flares, night vision, radios, gas masks, keycards, maps, documents.
- **Survival:** canned food, water, med kits, fuel, tools, batteries, scuba gear.
- **Boat parts:** outboard motors, engines, props, hull patches, fuel.
- **Crafting materials:** scrap, rope, cloth, plastic and electronics.

## Islands and underground facilities
- A flooded submarine pen (swim through air pockets)
- A radar station with a collapsed lower level
- A weapons-testing lab
- A Cold War comms bunker (lore and documents)
- A hidden dry dock that unlocks the military patrol boat

Facilities are gated by keycards, generators you have to fuel, and explosives.

## Locked-in decisions
- **Co-op crew of up to 4.** Friends share one boat: driver, gunner, and two more to repair, bail, navigate or go ashore. It should still be playable with fewer players.
- **Realistic boat handling.** Weight, momentum, wave behavior and engine limits all matter. Skill at driving the boat counts.
- **Setting:** a made-up island chain with a tropical paradise look: clear turquoise water, white sand, palms, jungle and reefs. The paradise surface hides abandoned military bunkers underneath.
- **First-person** everywhere, including at the helm.
- **Death:** you lose everything you were carrying, but it stays in the world where you fell. You start over, then go back and get it when you're strong enough. If your boat sinks, it stays on the seabed.

## The lost thing (undecided)
The military is searching the island facilities for something it lost. What that is hasn't been decided yet.

## Death and recovery
- When you die, your gear drops in a marked stash where you fell. You respawn at harbor with nothing.
- The stash stays until you recover it. Mercs roaming nearby can find and take it, so the longer you wait, the riskier it gets.
- Crewmates can recover each other's stashes, or revive you if they reach you fast enough.
- A sunk boat stays on the seabed with its cargo. Dive for it, or salvage parts from it.

## Milestones
1. The boat feels great on the ocean
2. Walk on a moving boat; swim; drive from the helm
3. One island and one bunker to explore
4. Inventory and three weapons
5. Merc soldiers guarding a bunker
6. Merc boats that chase and board
7. Boat damage and repair
8. The military faction and a heat system
9. Raft building, boat salvage and repair, and boat theft
10. Playable slice: 3 islands, 3 facilities, first chapter of the story

## Open design decisions
- When were the facilities built, and by whom?
- What is the island chain called?
- What did the military lose?
