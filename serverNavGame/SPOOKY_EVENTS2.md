“Something is wrong with the relationship between me, the house, and the game.”

The Forest has 4 phases, 
1. Something followed me -> The weirdness wasn't confined to the house
2. The area knows how I play -> The game is reacting to player behavior
3. The rules are changing -> Doors, rooms, inventory and geometry become unreliable
4. I am not supposed to be here -> The game begins behaving as though the player is an intruder

Additional Change Required
- Put Gate_Clearing.props.mapsrc into forest.mapsrc and forest.map
- Gate should not rotate UNTIL the player has collected the key in Tent_Camp

PHASE 1

**F01 · Path leads to the foyer in the previous area.**
One door in the Forest may teleport you to a room like the Foyer in the previous area.
It says Foyer, but it clearly doesn't look right, the tree is moved, there is little light,
and the lever opens the exit.

**F02 · A red man can be seen when entering a room.  He walks to the exit and fades out.**
Chance for the title to happen.  Ideally the man will be obscurred in darkness.

**F03 · In the forest, the camera follows you, and the only source of light is you.**
Title.  Additionally, the backdrop is gone, just pitch black in the background.

PHASE 2

**F04 · A sign appears, it says how long it took you to find it.**
What the title says, perhaps something like "You left me here # seconds ago."

**F05 · A tree is interactable, it begins to follow the player.**
The tree has an E prompt, pressing it doesn't do anything, but the tree begins to
follow the player slowly, even if they didn't press it.  It may even appear in another room
moving towards them.

**F06 · Items disappear from your inventory and appear in the previous room.**
Title.  This also requires an inventory system to be implemented.  I would do this by pressing
"ESC" opens up an overlay with Resume, Options, Exit buttons on left.  And inventory on right.

**F07 · A path is blocked, when you try to go the other way, it responds.**
A path is blocked, it says "You're afraid to go this way".  When you get close to the other door,
a prompt appears saying "You would rather go the other way".

PHASE 3

**F08 · A door becomes a wall after you walk through it**
Title.

**F08 · One door becomes altered, walking into it turns your character grey and you lose collision + gravity**
Title, this effect happens until they open the settings and return.  After doing that, they will find themselves
in the room with mushrooms.

**F09 · The player hears a noise in a neighboring room, when they enter, the room is inverted.**
Title, props are upside down and/or have their X values inverted.

**F10 · Relaunching the game may load the player in a different room titled "NOT HERE NOT ANYWHERE"**
Title.  This room should invert the player's controls.

**F11 · A path is blocked by a tree.  It may be unblocked if you stay in the room for 8 seconds.**

PHASE 4

**F12 · A man appears on the other side of the room and mirrors your movements.**
If you touch it, the game will fake a crash.  Relaunch will begin with
a brief shot of the man staring into the camera, censored face.
In this room the camera is zoomed out, so they can see the mirrored.

**F13 · A room is renamed to "YOU"**

**F14 · The camera may drift away from the player slowly and focus on the door they came from.**

**F15 · A doll will be in the top center of the room, if you interact with it, it will laugh and rotate slightly.**
Title, it will laugh and rotate slightly when you walk away.  It will be gone when you re-enter the room.

**F16 · A mannequin appears in a room, but it is facing the north wall, and you cannot interact with it.**

**F17 · Relaunching the game may trigger a prompt saying "Welcome back home." and a note will spawn.**
Somewhere on the map, a note will spawn saying "I'm sorry for what happened here.  I understand if you
don't want to see me again."

**F18 · Relaunching the game 3 times will make the bridge in the north repair itself.**

**F19 · Opening the menu, only has the Quit button.  But it is renamed to "Leave"**

**F20 · A door may randomly transport the player to a caged room.**
To escape, they must pull several levers moving objects out of their way.
Upon escaping, they appear in a room with several mannequins looking at them.
Going back puts them in the starting room.