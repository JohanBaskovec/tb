# Architecture

## Vue d'ensemble

```
            ┌─────────────────────────────────────────────┐
 main.c ──► │ platform  (SDL3 : fenêtre, événements,      │
            │            manettes, temps, fichiers)       │
            └──────┬──────────────────────────┬───────────┘
                   │ InputFrame               │ SDL_Renderer / audio
            ┌──────▼───────────┐       ┌──────▼──────────┐
            │ scenes           │       │ render / audio  │
            │ (titre, menu,    │──────►│ (lisent l'état, │
            │  partie, pause)  │       │  ne le modifient│
            └──────┬───────────┘       │  jamais)        │
                   │                   └──────▲──────────┘
            ┌──────▼──────────────────────────┴───────────┐
            │ game  (règles, entités, physique, IA, pad)  │  ← sans SDL
            └──────┬──────────────────────────────────────┘
            ┌──────▼──────────────────────────────────────┐
            │ core  (maths, arènes, RNG, log, conteneurs) │  ← sans SDL
            └─────────────────────────────────────────────┘
```

## Couches et règles de dépendance

| Couche | Peut inclure | SDL ? | Rôle |
|--------|--------------|-------|------|
| `core/` | libc uniquement | ❌ | Outils génériques |
| `game/` | `core/` | ❌ | Toute la simulation : déterministe, testable sans fenêtre |
| `render/`, `audio/` | `core/`, `game/` (lecture seule) | ✅ | Affichage et son |
| `ui/` | `core/`, `game/`, `render/` | ✅ (via render) | HUD, menus |
| `scenes/` | tout sauf `platform/` interne | ✅ | Enchaînement des écrans |
| `platform/` | `core/` | ✅ | Seul point de contact avec l'OS |

Conséquence : `game/` reçoit une structure `InputFrame` neutre (actions booléennes + axes)
et produit un `GameState` ; les tests peuvent simuler des parties entières sans SDL.

## Arborescence prévue

```
tb/
├── CMakeLists.txt
├── cmake/                  # Modules CMake (warnings, sanitizers, fetch SDL3)
├── src/
│   ├── main.c              # Options CLI, boucles headless et fenêtrée, shutdown
│   ├── app/
│   │   └── sim.h/.c        # Sim : niveau + GameState + caméra + source d'entrées (sans SDL)
│   ├── core/
│   │   ├── types.h         # u8..u64, i8..i64, f32, f64, usize, macros (ARRAY_LEN, MIN, MAX)
│   │   ├── config.h        # Constantes moteur : LOGICAL_W/H, TILE_SIZE, TICK_HZ/TICK_DT
│   │   ├── geom.h          # Vec2, Rect (AABB), clamp, lerp, approach (header-only)
│   │   ├── input_frame.h   # Action, InputFrame (neutre, produit par platform ou par un bot)
│   │   ├── arena.h/.c      # Allocateur linéaire
│   │   ├── pool.h          # Pool d'objets avec handles générationnels (à venir)
│   │   ├── rng.h/.c        # PCG32
│   │   ├── png.h/.c        # Écriture PNG minimale (captures headless)
│   │   └── log.h/.c        # Log niveaux + assert
│   ├── platform/
│   │   ├── platform.h/.c   # Fenêtre, renderer SDL, boucle d'événements
│   │   ├── input.h/.c      # Clavier + manettes → InputFrame par joueur, hotplug
│   │   └── fs.h/.c         # Chemins assets, lecture fichiers
│   ├── render/
│   │   ├── render.h/.c     # Cible logique 640×360, mise à l'échelle, primitives
│   │   ├── texture.h/.c    # Chargement (stb_image), atlas
│   │   ├── sprite.h/.c     # Sprites et animations
│   │   ├── camera.h/.c     # Suivi, zone morte, anticipation, bornes (sans SDL) ; viewports écran partagé à venir
│   │   ├── font.h/.c       # Police bitmap
│   │   └── debug_draw.h/.c # Hitboxes, graphe de navigation, états IA
│   ├── audio/
│   │   └── audio.h/.c      # SDL_AudioStream, mixage simple d'effets + musique
│   ├── game/
│   │   ├── game.h/.c       # GameState, game_init / game_update
│   │   ├── tuning.h        # Toutes les constantes de gameplay
│   │   ├── script.h/.c     # Scripts d'entrées ("R:60 RJ:12") pour les tests et démos
│   │   ├── bot.h/.c        # Bot aléatoire (parties simulées, tests d'invariants)
│   │   ├── walker.h/.c     # Bot « marcheur » : va vers un x et saute les obstacles (embryon d'IA)
│   │   ├── level.h/.c      # Chargement niveau ASCII, tilemap, requêtes de tuiles
│   │   ├── physics.h/.c    # AABB vs tuiles, gravité, plateformes fines, échelles
│   │   ├── entity.h/.c     # Pool d'entités, création/destruction différée
│   │   ├── companion.h/.c  # Soldats : déplacement, portage, tir, santé, variantes
│   │   ├── crate.h/.c      # Caisses : largage, parachute, physique, prise
│   │   ├── pad.h/.c        # Grille d'empilement, détection de formes, transformation
│   │   ├── weapon.h/.c     # Table des armes (data-driven)
│   │   ├── loot.h/.c       # Armes au sol et produits du pad : physique, ramassables
│   │   ├── combat.h/.c     # Tir, projectiles, explosions, dégâts, mannequins, mort du soldat
│   │   ├── vehicle.h/.c    # Véhicules
│   │   ├── team.h/.c       # Équipes, ordres, élimination
│   │   ├── ai.h/.c         # FSM soldat + commandant d'équipe
│   │   ├── nav.h/.c        # Graphe de navigation plateformes + A*
│   │   ├── rules.h/.c      # Modes de jeu, conditions de victoire
│   │   └── events.h        # File d'événements (son, particules, HUD)
│   ├── ui/
│   │   ├── hud.h/.c
│   │   ├── pad_menu.h/.c   # Menu de construction (M2 : dans app/pad_menu.h/.c, sans SDL ; dessin dans render/draw_game.c)
│   │   └── menu.h/.c
│   └── scenes/
│       ├── scene.h         # Interface commune
│       ├── scene_title.c
│       ├── scene_setup.c   # Choix des équipes / joueurs / niveau
│       ├── scene_play.c
│       └── scene_results.c
├── assets/
│   ├── sprites/  sounds/  music/  fonts/
│   └── levels/             # Niveaux au format texte (.lvl)
├── tests/                  # Tests unitaires (CTest), sans SDL
├── third_party/            # Single-headers vendorisés (stb_image…)
└── docs/
```

## Boucle principale

Pas de temps **fixe à 60 Hz** pour la simulation, rendu aussi souvent que possible (vsync).

```
accumulator += frame_time (plafonné à 250 ms)
while accumulator >= DT:
    platform_poll_input(&inputs)        // une InputFrame par joueur local
    scene_update(scene, &inputs, DT)    // → game_update() en partie
    accumulator -= DT
alpha = accumulator / DT
scene_render(scene, alpha)              // interpolation positions précédentes/actuelles
```

- `game_update` est **déterministe** pour une graine RNG et une séquence d'`InputFrame` donnée :
  utile pour les tests, le débogage et d'éventuels replays.
- Mode debug : F1 overlay, F2 pause, F3 avance d'une frame, F4 ralenti ×0.25.

## Modèle de données

### Entités

Un tableau de taille fixe d'`Entity` (`EntityPool`, 256 emplacements) avec un en-tête commun et une union par type.
Simple, lisible en C, sans allocation dynamique. **État actuel (M3) : le pool contient les caisses, les produits du pad, les armes au sol,
les projectiles et les mannequins** ; le joueur est encore un champ de `GameState` ; les compagnons y entrent avec le jalon « escouade » (M4).
Le schéma ci-dessous est la cible.

```c
typedef struct { u16 index; u16 generation; } EntityId;   // 0 = invalide

typedef enum { ENT_NONE, ENT_COMPANION, ENT_CRATE, ENT_PROJECTILE,
               ENT_PICKUP, ENT_VEHICLE, ENT_EFFECT } EntityType;

typedef struct Entity {
    EntityType type;
    u16        generation;
    u32        flags;          // ALIVE, ON_GROUND, PENDING_DESTROY…
    i8         team;           // -1 = neutre
    Vec2       pos, prev_pos, vel;
    Rect       hitbox;         // relatif à pos
    union {
        Companion      companion;
        Crate      crate;
        Projectile projectile;
        Pickup     pickup;
        Vehicle    vehicle;
    } as;
} Entity;
```

- Références entre entités via `EntityId` (jamais de pointeur conservé d'une frame à l'autre).
- Destruction **différée** en fin de frame (`pool_mark_destroy` puis `pool_flush`) ; création possible pendant la frame.
- Les identifiants sont **générationnels** : un `EntityId` périmé (entité détruite, emplacement réutilisé) se résout en `NULL`.
  Le compagnon garde l'id de la caisse portée (`Companion.carrying`), jamais un pointeur.

### Caisses et grille

- États d'une caisse : `DESCENDING` (descente lente à vitesse constante) → `FALLING` (corps libre : posée, lancée ou
  privée de support) → `RESTING` (verrouillée dans une case) ; `CARRIED` (au-dessus d'un compagnon).
- Les caisses au repos sont écrites dans une **couche solide dynamique** du `Level` (`dyn_solid`, mêmes dimensions que
  `tiles`). `level_is_solid()` = terrain solide **ou** caisse : la physique des compagnons et des caisses n'a donc rien
  de spécial à savoir. `level_tile()` ne voit que la carte statique (le rendu dessine les caisses à part).
- Une caisse qui atterrit **se cale sur la grille** (colonne la plus recouverte et soutenue). Ce choix rend l'empilement
  robuste et prépare le pad 4×2. Les caisses sont traitées **de bas en haut** pour qu'une pile se pose en un seul tick.
- `GameState.level` est donc un `Level *` mutable ; `Sim` possède le niveau.
- Les zones de largage (`drop.N=couleur,intervalle_s,plafond[,%bleu]` dans l'en-tête du niveau) vivent dans les données
  de niveau ; `GameState.drop_timers` compte à rebours par zone, et ne tourne que sous le plafond.
- Ordre de mise à jour fixe par type : entrées → IA → soldats → véhicules → projectiles → caisses → pads → règles.
  Actuellement dans `game_update` : soldat → prendre/poser/lancer → caisses (largage, physique, calage) → caisse portée
  suit la tête → respawn (eau) → `pool_flush`.

### GameState

```c
typedef struct GameState {
    Arena    level_arena;          // libérée en bloc à la fin de la partie
    Level    level;                // tilemap + métadonnées
    Entity   entities[MAX_ENTITIES];
    Team     teams[MAX_TEAMS];     // couleur, pad, contrôleur (humain n°X / IA), ordres
    Pad      pads[MAX_TEAMS];
    NavGraph nav;
    Rng      rng;
    u64      tick;
    EventQueue events;             // consommée par audio / render / ui
    MatchRules rules;
} GameState;
```

### Communication jeu → présentation

La simulation ne joue pas de son et ne crée pas de particules. Elle pousse des
événements dans `GameState.events` (`game/events.h` : `EV_CRATE_SPAWNED`, `EV_CRATE_SETTLED`, `EV_CRATE_PICKED_UP`,
`EV_CRATE_PLACED`, `EV_CRATE_THROWN`, `EV_CRATE_DESTROYED`, `EV_CRATE_CONSUMED`, `EV_PAD_BUILT`, `EV_PLAYER_RESPAWNED` ; combat : `EV_SHOT`, `EV_PUNCH`, `EV_PROJECTILE_HIT`, `EV_EXPLOSION`, `EV_DAMAGE`, `EV_DUMMY_DESTROYED`,
`EV_PLAYER_DIED`, `EV_WEAPON_PICKED_UP`, `EV_WEAPON_DROPPED`, `EV_WEAPON_EMPTY`), que `audio/`, `render/` (effets) et `ui/` lisent après l'update.
Chaque événement porte un type, une position et deux entiers (`a`, `b`) dont le sens dépend du type.

- La file est **vidée au début de chaque `game_update`** : elle contient les événements du dernier tick seulement.
  Une boucle qui exécute plusieurs ticks par image doit donc la vider après chaque tick.
- Elle est de capacité fixe (`MAX_EVENTS`) ; en cas de débordement les événements en trop sont comptés (`dropped`), jamais fatals.
- Elle n'est **pas dans le hash d'état** et la simulation ne la relit jamais : la présentation ne peut pas influencer le jeu.
- Premier consommateur : `app/fx.h/.c` (sans SDL), qui en tire de courts effets visuels (explosions, étincelles, flash de bouche) tenus par
  `Sim.fx` et dessinés par `draw_game`. `sim_tick` la vide à chaque tick.

## Physique

- Collisions **AABB contre la grille de tuiles**, résolution axe par axe (X puis Y).
- Tuiles : `EMPTY`, `SOLID`, `PLATFORM` (traversable par dessous et en descendant), `LADDER`, `HAZARD`.
- Les caisses libres au repos sont solides pour les autres caisses et pour les soldats (on peut monter dessus).
  Implémentation : grille d'occupation dynamique mise à jour à chaque frame pour les caisses au repos.
- Collisions entité–entité (projectiles, ramassage) : test AABB en force brute au début ;
  grille spatiale uniforme si le nombre d'entités l'exige.

## Pad d'empilement

- Grille 4 colonnes × 2 rangées : `u8 cells[PAD_ROWS][PAD_COLS]` avec `0` = vide, `1` = caisse rouge,
  `2` = caisse bleue, liée à des `EntityId` de caisses.
- `pad_options()` est une fonction **pure** : grille → liste des constructions possibles, chacune avec
  les cellules à consommer (**celles du haut d'abord**) et sa version rouge/bleue.
  Formes : `1x1`, `2x1_horizontal`, `2x1_vertical`, `L`, `2x2`, `4x2`. `pad_build()` applique un choix
  (consomme les caisses, émet l'événement de production). Testées unitairement : toutes les formes,
  rouge/bleu, orientations du L, ordre de consommation, pad vide, et « un pad non vide propose toujours un 1×1 ».
- **Dans le monde** : le pad d'une équipe est une zone de 4×2 cases marquée `P` dans le niveau (case en bas à gauche ; `Level.pads[équipe]`).
  `pad_read_grid()` lit les caisses *au repos* de ces cases ; `pad_world_options()` en déduit le menu.
  `pad_build(gs, équipe, forme, bleu?)` **recalcule** les options au moment de l'appel, refuse (sans rien changer) si la forme n'est plus là,
  si le pad est inutilisable ou s'il n'y a plus de place pour le produit, puis consomme les caisses (`crate_consume`, du haut vers le bas) et crée un `ENT_PRODUCT`.
  Les menus périmés sont donc sans danger.
- **Menu** (`app/pad_menu.h/.c`, sans SDL) : visible tant que le soldat contrôlé est près du pad et que le pad propose quelque chose ;
  entrées à droite du pad (à gauche en bord de carte), rangées de bas en haut, deuxième colonne au-delà de 6. La sélection suit
  un couple (forme, couleur), pas un indice. Clavier : `ACT_SWITCH` (Tab) puis `ACT_CONFIRM` (Entrée) ; souris : `sim_click()` (position
  logique → monde). Le menu appelle `pad_build()` **entre deux ticks**, comme le fera l'IA ; ce n'est pas une entrée de `game_update`.
- **Pas de construction automatique** : le menu de construction est l'unique déclencheur, pour les humains comme pour l'IA.
- Une table de recettes `(forme, bleue?) → résultat` est **data-driven** (voir GAME_DESIGN.md § 6) :
  ajuster une recette ne touche pas au code.
- Les types de caisses sont `CRATE_RED` et `CRATE_BLUE`, communs au pad, aux zones de largage et au rendu.

## Combat

- **Armes** : table `WeaponDef` dans `game/weapon.c` (type de tir, munitions, cadence, vitesse, dispersion, dégâts, rayon, recul, portée / mèche, taille du projectile).
  Quatre types de tir : `SHOT_MELEE` (poings), `SHOT_BULLET` (droite, sans gravité), `SHOT_GRENADE` (gravité, rebonds, mèche), `SHOT_ROCKET`
  (droite, explose). Ajouter une arme = ajouter une ligne. Les chiffres vivent dans cette table plutôt que dans `tuning.h` (D033).
- **Soldat** : `Companion` porte `hp / max_hp`, `weapon`, `ammo`, `fire_cooldown`, `hurt_ticks`, `punch_ticks` (poings = « sans arme »).
- **Cibles** : le soldat du joueur (équipe 0) et les mannequins (`ENT_DUMMY`, équipe 1, `T` dans le niveau). `combat.c` rassemble les cibles vivantes
  puis les teste contre les projectiles, les coups et les explosions. Les PV ne baissent que dans `combat.c` ; un soldat à 0 PV meurt en fin de tick
  (`combat_player_died` : lâche l'arme et la caisse, puis `game_update` le fait réapparaître).
- **Projectiles** (`ENT_PROJECTILE`) : une balle avance de `vitesse × dt` par tick (moins d'une case, donc sans traversée) et s'arrête sur le terrain ou
  sur une caisse empilée ; une roquette explose au dernier point libre ; une grenade utilise `body_move` (rebonds, frottement) et explose à la fin de sa mèche
  ou au contact d'un ennemi. L'équipe du tireur évite le tir ami pour les balles et les coups, pas pour les explosions.
- **Explosions** : `combat_explode(gs, centre, arme)` blesse toute cible dont le point le plus proche est dans le rayon **et visible** depuis le centre
  (segment échantillonné tous les 2 px contre `level_is_solid`) ; dégâts de 100 % à 40 % du centre au bord ; impulsion d'éloignement avec un biais vers le haut.
- **Visée** : `Companion.aim_dir` est le vecteur unitaire de l'arme (vers le curseur si on vise, sinon la direction du regard) ; en visant,
  le soldat se tourne vers le curseur. Balles et roquettes partent dans cette direction (la dispersion de l'uzi est perpendiculaire) ; une grenade
  est lancée en cloche pour **retomber sur le curseur** (temps de vol borné, vitesse bornée). Le tir part de l'épaule, un peu le long de la visée.
- **Touches** : `combat_player_update` reçoit `crate_busy` (vrai si une action de caisse a utilisé la touche ce tick) : portage, prise, pose et lancer
  sont prioritaires sur le tir, les coups et l'échange d'arme.
- **Ramassage** (`loot.c`) : armes et produits non-armes tombent et reposent comme le produit du pad en M2 ; une caisse qui se pose dessus les remonte.
- Ordre dans `game_update` : soldat → caisses du joueur → combat du joueur → projectiles / mannequins → caisses → caisse portée → objets au sol → mort / eau → `pool_flush`.

## IA et navigation

- **Graphe de navigation** pré-calculé au chargement du niveau : nœuds = segments de sol marchables,
  arêtes = marcher, sauter (testé avec la trajectoire de saut réelle), tomber, échelle.
- A* sur ce graphe ; les soldats suivent le chemin avec des commandes virtuelles
  (l'IA produit une `InputFrame`, exactement comme un joueur → même code de déplacement).
- FSM par soldat + commandant par équipe réévalué ~2 fois par seconde.

## Rendu

- Tout est dessiné dans une texture cible 640×360 (`SDL_TEXTUREACCESS_TARGET`), puis copiée à l'écran
  avec une mise à l'échelle entière (`SDL_SetRenderLogicalPresentation` en mode intégral).
- Écran partagé : un viewport et une caméra par joueur local ; le monde est rendu une fois par viewport.
- Couches : fond parallaxe → tuiles → caisses/pickups → soldats/véhicules → projectiles/effets → HUD.
- Positions interpolées avec `alpha` entre `prev_pos` et `pos`, arrondies au pixel.

## Entrées

- `platform/input` mappe clavier et `SDL_Gamepad` vers des **actions** (`ACT_LEFT`, `ACT_JUMP`,
  `ACT_FIRE`, `ACT_GRAB`, `ACT_SWITCH`, `ACT_ORDER`, `ACT_PAUSE`…) avec états `down / pressed / released`.
- Le clavier est lu par **scancodes SDL** (position physique des touches), jamais par keycodes : les touches de
  déplacement sont celles de la position WASD (ZQSD sur AZERTY), identiques sur tous les claviers ; les flèches restent actives.
  `platform_map_keyboard()` est pure et testée.
- Jusqu'à 4 joueurs locaux ; attribution des manettes dans l'écran de configuration ; hotplug géré.
- Souris : clic pour choisir une construction du pad ; au clavier / à la manette, sélection + validation.
- **Visée à la souris** : `InputFrame` porte `aim_active` et `aim` (point visé, en pixels monde). La fenêtre lit la position et le bouton gauche
  (`platform_read_mouse`) et les donne à `sim_set_mouse` ; `sim_tick` complète alors l'entrée humaine avec la visée et `ACT_FIRE` tant que le
  bouton est tenu. Un clic plus court qu'une image tire quand même (`mouse_click_pending`). Un clic sur une entrée du menu du pad ne tire
  pas, et le bouton ne tire plus tant qu'il n'a pas été relâché. La visée n'existe qu'après le premier mouvement de souris : au clavier seul,
  le soldat tire dans la direction où il regarde. Les scripts visent avec `KEYS@X/Y:TICKS` ; le jeu ne connaît pas la souris, seulement `aim`.

## Données et formats

- **Niveaux** : format texte ASCII (`.lvl`), un caractère par tuile + section d'en-tête clé=valeur
  (nom, taille, fréquence de largage…). Lisible, versionnable, éditable à la main.
  Légende : `#` sol, `=` plateforme fine, `H` échelle, `~` eau, `1`..`4` camps, `D` zone de largage,
  `T` mannequin (cible immobile, debout sur la case),
  `P` pad d'empilement (case en bas à gauche de la zone 4×2 ; le N-ᵉ `P` dans l'ordre de lecture appartient à l'équipe N,
  sauf `pad.N=ÉQUIPE` dans l'en-tête).
  En-tête : `name=…` et `drop.N=rouge|bleu|mixte…` : voir `game/level.h` (couleur `red|blue|mixed`, intervalle en secondes,
  plafond de caisses simultanées pour cette zone, % de bleues pour `mixed`, 25 par défaut).
- **Constantes de gameplay** : `tuning.h` au départ ; éventuellement un fichier texte rechargé à chaud plus tard.
- **Sauvegarde / options** : fichier texte clé=valeur dans `SDL_GetPrefPath()`.

## Mémoire et erreurs

- Allocation au démarrage (arène permanente) et au chargement de niveau (arène de niveau).
- Aucune allocation dans `game_update` / `scene_render`.
- Erreurs récupérables : retour `bool` + log. Erreurs de programmation : `ASSERT` (actif en Debug).

## Exécution headless (sans fenêtre)

`cratesquad --headless` n'initialise ni vidéo ni audio : le rendu se fait dans une `SDL_Surface`
RGBA 640×360 via le renderer logiciel de SDL. Aucun display n'est requis.

| Option | Rôle |
|--------|------|
| `--headless` | Simule puis quitte, sans fenêtre |
| `--frames N` | Nombre de ticks à simuler (défaut 600, ou la longueur du script) |
| `--bot` | Bot aléatoire (marche, saute, échelles, traverse les plateformes) |
| `--script "R:60 RJ:12"` | Script d'entrées (syntaxe dans `game/script.h`) ; `F@488/420:1` tire en visant le point (488, 420) du monde |
| `--walk-to X` | Bot « marcheur » vers l'abscisse X (pixels) |
| `--start X,Y` | Position de départ (pieds, pixels) à la place du spawn |
| `--seed N` | Graine RNG |
| `--level PATH` / `--assets DIR` | Niveau et dossier d'assets (défaut : `assets/levels/test_arena.lvl`) |
| `--screenshot PATH` | PNG écrit après le dernier tick |
| `--shot-every N` | Avec `--screenshot motif_%u.png` : un PNG tous les N ticks (films) |
| `--shot-cam X,Y` | Coin haut-gauche de la caméra pour la capture (voir une autre partie du niveau) |
| `--give ARME` | Équipe le soldat d'une arme neuve : `pistol`, `uzi`, `grenades`, `strong-grenades`, `rocket` |
| `--fill-pad HAUT,BAS` | Pose des caisses dans le pad de l'équipe 1 (ex. `r...,rr.b` : r rouge, b bleue, `.` vide) |
| `--debug-overlay` | Grille, hitboxes, vitesse, pastilles d'état, spawns et zones de largage |
| `--trace N` | Affiche l'état du joueur tous les N ticks |

La dernière ligne affichée (`ticks=… respawns=… products=… hp=… weapon=… ammo=… shots=… hits=… kills=… deaths=… hash=…`) sert aux tests de déterminisme.

### Boucle fenêtrée testée sans fenêtre

`run_windowed` (la vraie boucle interactive : fenêtre, vsync, titre FPS/TPS, touches F1–F4) est exécutée
par les tests sous le pilote vidéo SDL `dummy` : `SDL_VIDEODRIVER=dummy cratesquad --expect-video-driver dummy
--max-ticks N …`. `--expect-video-driver` est une garde : si SDL choisit un autre pilote, le programme refuse
de créer une fenêtre (code 3). **Ne jamais lancer le mode fenêtré sans cette garde et le pilote `dummy`.**
Les touches F1–F4 sont testées en injectant des événements (`SDL_PushEvent`) ; la lecture réelle du clavier
(`SDL_GetKeyboardState`) n'est pas exerçable sans clavier : seul le mapping touches → actions est testé.

## Tests

- Exécutables CTest dans `tests/`. Les tests de logique sont liés à `cs_game` / `cs_app` (sans SDL) ;
  `test_platform` lie SDL mais n'initialise que le sous-système d'événements.
- `core` (maths, arène, RNG, entrées), `png`, `level` (parseur, erreurs, config des zones de largage, couche solide, vrai fichier),
  `entity` (pool générationnel), `crate` (descente, empilement, alignement, solidité, prendre/porter/poser/lancer, effondrement,
  eau, zones de largage), `physics`, `companion`
  (accélération, saut variable, coyote, buffer, plateformes, échelles), `script`, `camera`,
  `game` (invariants sous bots sur 24 graines × 8000 ticks, déterminisme, eau/respawn),
  `scenarios` (traversée du vrai niveau : sauts plateforme à plateforme, échelle, fosse ; bot marcheur),
  `platform` (mapping clavier, événements dont le clic gauche), `pad` (recettes : toutes les formes, rouge/bleu, orientations du L,
  ordre de consommation, pad vide, « non vide ⇒ au moins un 1×1 », **toutes les 3⁸ grilles** comparées à un calcul indépendant ;
  monde : lecture des caisses, construction, produits, menu périmé, pool plein), `pad_menu` (visibilité, disposition, clavier, souris,
  bot qui martèle Tab/Entrée), `events` (file, débordement, événements émis par les caisses, hash inchangé, effets visuels),
  `combat` (table d'armes, chaque arme tire / s'épuise, portée, murs, dispersion de l'uzi, poings, décroissance et blocage des explosions, recul,
  grenade : cloche / mèche / contact / rebond, roquette, mannequins, ramassage et échange, priorité des caisses, mort et réapparition,
  produit du pad → arme utilisable, visée (direction, dispersion, grenade sur le curseur, souris, clic court, menu), et un bot armé sur 6000 ticks avec invariants et déterminisme).
- Tests de fumée sur l'exécutable : headless (bot, script + overlay, film), déterminisme, arguments/niveau/script
  invalides, boucle fenêtrée sous `dummy`, garde du pilote vidéo, tirs (uzi tue un mannequin, explosion de roquette), bot armé.
- Les invariants vérifiés à chaque tick sous bots : jamais dans une tuile solide, dans les bornes du niveau,
  vitesses bornées, `on_ground` ⇒ appui sous les pieds, caméra dans le niveau.
  Pour les caisses : grille et pool cohérents (une case solide par caisse au repos, alignement exact, appui sous chaque caisse),
  au plus une caisse portée et référencée par le joueur, plafond de chaque zone respecté.
- À venir : A* (M5), partie IA vs IA simulée.

## Cibles CMake

| Cible | Type | Contenu |
|-------|------|---------|
| `cs_core` | static lib | `src/core` |
| `cs_game` | static lib | `src/game` (dépend de `cs_core`) |
| `cs_app` | static lib | `src/render/camera.c`, `src/app/sim.c` (sans SDL ; dépend de `cs_game`) |
| `cs_platform` | static lib | `src/platform` + SDL3 |
| `cratesquad` | exécutable | `main.c`, `src/render` (SDL) ; lie `cs_app`, `cs_platform` |
| `test_*` | exécutables | `tests/` |
