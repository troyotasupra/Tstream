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
| **Mercenaries** | Modern pirates in speedboats and RHIBs. They chase, shoot and board your boat, and camp loot spots. This is the main enemy. |
| **Military** | Patrols, restricted zones, searchlights and helicopters. Dangerous if they spot you, but they also attack mercs. |
| **Survivors / traders** *(optional)* | A floating market and radio contacts that sell fuel and ammo and give leads. |

## Boats
- Engine boats with fuel, throttle and real wave handling: you catch air off swells and lose thrust when the prop leaves the water.
- Damage that matters: a shot-up engine smokes and sputters, and hull holes let water in until you patch and bail.
- Progression: jon boat → center-console → RHIB → ex-military patrol boat.
- Mounted weapons and searchlights on later boats.
- Mercs can pull alongside and board you, which is the signature fight.

## Loot
- **Military:** old rifles, pistols, grenades, flares, night vision, radios, gas masks, keycards, maps, documents.
- **Survival:** food, water, med kits, fuel, tools, batteries, scuba gear.
- **Boat parts:** engine upgrades, props, hull patches.
- Old military gear is worn, so weapons can jam.

## Islands and underground facilities
- A flooded submarine pen (swim through air pockets)
- A radar station with a collapsed lower level
- A weapons-testing lab
- A Cold War comms bunker (lore and documents)
- A hidden dry dock that unlocks the military patrol boat

Facilities are gated by keycards, generators you have to fuel, and explosives.

## The lost thing (story hook, undecided)
- A prototype weapon
- An experimental power source
- A missing warhead
- A missing scientist who went down and never came back

The story is told through documents, radio chatter and environments rather than cutscenes.

## Milestones
1. The boat feels great on the ocean
2. Walk on a moving boat; swim; drive from the helm
3. One island and one bunker to explore
4. Inventory and three weapons
5. Merc soldiers guarding a bunker
6. Merc boats that chase and board
7. Boat damage and repair
8. The military faction and a heat system
9. Harbor, upgrades and boat tiers
10. Playable slice: 3 islands, 3 facilities, first chapter of the story

## Open design decisions
- Solo or co-op?
- First-person, third-person, or both?
- Lose your gear on death (extraction-style) or a persistent world?
- Arcade or realistic boat handling?
- Setting: a real region or a fictional archipelago? What era were the facilities built in?
- What is the lost thing?
