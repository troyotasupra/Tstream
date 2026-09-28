# Deadwater

Co-op naval survival shooter in Unreal Engine 5. The design plan is in `../GameDesign/PLAN.md`.

## Current state: milestone 1 (the boat)
- A placeholder skiff with a physics-driven outboard motor, floating on the Water plugin ocean.
- The ocean test map builds itself the first time the editor opens (`Content/Python/init_unreal.py`).
- Code has not been compiled yet. Expect a round of fixes on the first build.

## Helm controls
| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Throttle lever up / down (stays where you leave it) | W / S | Right / left trigger |
| Steer the motor | A / D | Left stick |
| Cut throttle to idle | X | B |
| Look around | Mouse | Right stick |

The top-left readout shows speed, throttle, fuel, engine health, and whether the prop is in the water.
