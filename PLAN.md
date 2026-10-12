# PLAN.md: instructions for the coding agent

Game: **The Seventh Night**, a 2.5D narrative survival-horror game in C++17 and OpenGL 3.3 (no engine).
Libraries: GLFW, GLEW, glm, cgltf, stb_image (installed through vcpkg). Platform: Windows.
Story summary: Aarav, a quiet 20-year-old college student, is lost in the haunted Kaalvan forest for seven days.
On Day 4 he nearly gives up, a small boy with a diya (the Ember) appears, and he chooses to keep walking.
He escapes on Day 7 and rebuilds his life. Full story: see the "Seventh Night Game Story Bible" document.

## Rules
- Work on ONE milestone at a time. Build and run after every milestone and fix errors before moving on.
- Keep code simple and readable; no new libraries without asking.
- Game logic stays separate from rendering (src/game vs src/render) once Milestone 3 is done.
- Assets are CC0 Quaternius glTF files in /assets. Never edit the originals.

## Milestones
1. [x] Window, glTF loading, side camera, depth lanes, lantern light, fog (this starter)
2. [x] Animated characters & skeletal GPU skinning (loaded UAL1 & UAL2 clips, node-name keyframe samplers, GPU shader skinning, crossfade animator)
3. [x] Scene file format (simple text or JSON) listing models, positions, lanes; load a forest level from it
4. [x] Collision: axis-aligned boxes for ground, rocks, trees; platforms; one-way ledges; climbing
5. [x] Meters: Body, Will, Stamina, Hope; HUD drawn with a simple text and bar renderer
6. [x] Will rules: darkness drains it, lantern and memories restore it; heartbeat audio (miniaudio)
7. [ ] **Vertical slice: Day 4 "Grey Grove"**: desaturated world, whispers, forced slow walk,
       the Rest/Continue choice, the Ember appears, red thread glows, colour returns
8. [ ] Mirror Cave puzzle (light beams and mirrors), then The Burden boss
9. [ ] Dialogue and cutscene system; memory fragments; diary pages
10. [ ] Remaining days (1, 2, 3, 5, 6, 7), the three endings, Act 1, 2 and 4
11. [ ] Save/load, settings, audio mixing, polish

## Art direction
Cold greens and greys; warm orange only from the diya, the red thread and human memories.

## Animation plan

Packs (all CC0, glTF/GLB and FBX, same universal humanoid rig as the Universal Base Characters):
- Universal Animation Library 1 (UAL1): 120+ clips. Locomotion in 8 directions, jog, sprint, push, crawl, swim, sit, death, emotes.
- Universal Animation Library 2 (UAL2): 130+ clips. Parkour, climb, slide, knockback, lantern idle, carry walk, zombie walk, farming, combat combos.
- Use the **"Root Motion disabled"** versions. Our code moves the character (A/D, lanes, jumps), so root motion would
  fight the game's movement. Root motion is only useful later for cutscenes.
- The free "Standard" zips contain a subset (UAL1: 45 clips, UAL2: 42 clips). The Pro/Source versions contain all clips.
  Before coding, run the loader to print every clip name actually in your zips, and map the names below to them.

### Mapping: story moments to clips (verify exact names with the clip list)

| Game moment | Clip to look for | Library |
| --- | --- | --- |
| Idle, quiet, numb (Act 1) | Idle / walk idle | UAL1, UAL2 |
| Walking with the lantern (all forest days) | IDLE_LANTERN (hold), Walk/Jog | UAL2, UAL1 |
| Carrying tiffins, carrying Vikram's gear | WALK_CARRY | UAL2 |
| Run, sprint (Day 7 chase) | Sprint / Jog | UAL1 |
| Jump, fall, land | Jump start / loop / land (check names) | UAL1 |
| Climb ledges, Spine Cliffs | CLIMB_UP_1M | UAL2 |
| Slide down slopes, fleeing | SLIDE, SLIDE_LOOP | UAL2 |
| Day 4: sits and gives up | Sit / sitting idle | UAL1 |
| Hurt, hit by the Hollow's roots | KNOCKBACK | UAL2 |
| Day 4 collapse, bad ending | LAYFACEUP | UAL2 |
| Coughing when exhausted or in fog | COUGHING | UAL2 |
| Phone scenes (Act 1, the dead phone) | IDLE_TALKING_PHONE | UAL2 |
| Opening chests, boxes, outpost | CHEST_OPEN | UAL2 |
| Throwing a stone to distract a monster | OVERHAND_THROW | UAL2 |
| Wave, emotes with friends | WAVE and emotes | UAL2, UAL1 |
| Mimic Echoes, Drowned Children, Shadow Stalker | ZOMBIE_WALK_FWD, ZOMBIE_SCRATCH | UAL2 |
| The Ember (child with the diya) | Idle + walk, scaled down, glow shader | UAL1, UAL2 |

### Known gaps (plan around them)
- No clip of carrying a **person**: for rescuing Vikram, use an assisted-walk look made from WALK_CARRY plus a second
  character close beside Aarav, or handle it with a cutscene camera.
- No ladder or rope climb clip is confirmed: use CLIMB_UP_1M for ledges and design the Spine Cliffs as ledge-to-ledge climbs.
- Combat clips (sword, melee) are NOT needed: the story uses light and choices, not weapons. Skip them.
- Boss creatures (Antlered Hollow, the Hollow) are not humanoid: build them from roots, shaders and particles, or find a monster pack.

### Technical rules for the animation code
1. Match animation channels to the character's joints **by node name**, never by index.
2. Compare joint names between the character GLB and each animation GLB. If UAL2 differs from UAL1, write a small rename table.
3. Bake no animation into the vertex data; send joint matrices to the shader each frame.
4. Limit to 128 joints; the Universal rig has fewer than that.
5. Keep the animator separate from the renderer so NPCs, friends and the Ember all reuse it.
