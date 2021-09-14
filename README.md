# Traumas

---------------------------------------
### Contents
1. [Traumas](#traumas)
    - [Concept](#concept)
    - [Technical Side](#technical-side)
    - [Installation](#installation)

---------------------------------------
## Traumas
Just a stupid custom spell trigger system

### Concept
Damage dealt may cause negative effects, which will only get worse...

### Technical Side
- A unit script (spells)
- Currently 5 base and 5 chained effects (10 spells total)
- Using entries **455000-455200** in **Spell.dbc**, `spell_linked_spell`, `spell_proc`
- Expected client locale: **enGB**

### Installation
- [Download Files](https://github.com/trickerer/Traumas/releases)
- For client: move `patch-enGB-4.MPQ` into `/Data/enGB` folder
- For dbc: move provided `Spell.dbc` to your `/dbc` folder or apply provided patch (created using **MyDbc Editor ver. 1.2.2**)
- For script: move `traumas.cpp` file into `/src/server/scripts/Custom` folder and use diff to update other files
- For DB: apply `traumas_world.sql` to your `world` DB

[Demonstration](https://www.youtube.com/watch?v=aKRh-uHMMoc)
