# Roadmap

Chaque jalon se termine par un état **jouable ou démontrable**. On ne passe au suivant
que lorsque les critères de fin sont remplis. Cases : `[ ]` à faire, `[x]` fait.

---

## M0 — Squelette technique
- [x] `CMakeLists.txt` racine, modules `cmake/` (warnings, sanitizers, SDL3 via FetchContent, option SDL système)
- [x] `.gitignore`, `.clang-format`, `.editorconfig` écrits pour ce projet
- [x] `core/` : types, log/assert, maths (Vec2, Rect), arène, RNG, PNG, `InputFrame`
- [x] Rendu headless : surface logicielle 640×360 + captures PNG
- [x] Bot aléatoire + parties simulées déterministes (test de hash)
- [x] Premiers tests CTest (core, png, game, fumée headless, déterminisme) — ASan/UBSan, GCC et Clang
- [x] `platform/` fenêtré : fenêtre redimensionnable, mise à l'échelle entière — boucle réelle exécutée sous le pilote vidéo SDL `dummy` (aucune fenêtre ne s'ouvre), capture 1280×720 vérifiée
- [x] Compteur FPS/TPS dans le titre (vérifié par `smoke_windowed_dummy`)
- [x] Entrées clavier → `InputFrame` : mapping testé (`test_platform`), événements F1–F4 testés via `SDL_PushEvent`

**Fin :** une fenêtre affiche un personnage déplaçable au clavier ; `ctest` passe ; zéro warning. ✅ côté technique.
**Reste à valider par l'utilisateur** (impossible sans écran) : affichage sur un vrai display, vsync, et lecture réelle du clavier.

## M1 — Monde et déplacement
- [x] Format de niveau ASCII + chargeur + tests (erreurs avec numéro de ligne)
- [x] Rendu des tuiles (placeholders colorés)
- [x] Physique AABB vs tuiles : sol, plafonds, murs (pas de tunneling à vitesse max)
- [x] Soldat : marcher, sauter (hauteur variable, coyote time, jump buffer)
- [x] Plateformes fines (traverser par dessous, descendre) et échelles (le haut d'une échelle sert de sol)
- [x] Caméra : suivi, zone morte, anticipation, limites du niveau
- [x] Overlay debug : hitboxes, grille, vitesse, pastilles d'état (`--debug-overlay` / F1) ; pause / pas-à-pas / ralenti (F6/F3/F4 depuis D067, événements testés)
- [x] Fichier `tuning.h`
- [x] Outils de test : scripts d'entrées, bot aléatoire, bot « marcheur », `--start`, `--walk-to`, films de captures
- [x] Niveau de test `assets/levels/test_arena.lvl` (80×30) et scénarios de traversée automatisés

**Fin :** on parcourt agréablement un niveau test avec un soldat. ✅ techniquement (tout le niveau est traversable,
vérifié par bots et scénarios). **Le « agréablement » reste à valider à la manette/au clavier par l'utilisateur** :
les valeurs de `tuning.h` (vitesse, hauteur de saut, gravité…) sont des réglages de départ.
La police de caractères est arrivée avec le HUD (M6) ; l'overlay de debug garde ses pastilles de couleur.

## M2 — Caisses et pad (cœur du jeu)
- [x] Pool d'entités avec `EntityId` générationnels (seules les caisses y vivent pour l'instant ; le joueur y entre en M4)
- [x] Caisses rouges et bleues ; largage sur les zones `D` configurables par niveau (`drop.N=couleur,intervalle,plafond[,%bleu]`) : descente lente, puis physique
- [x] Empilement des caisses libres (alignées sur la grille, solides pour tout le monde, piles qui se tassent)
- [x] Prendre / porter / poser / lancer une caisse (E prend/pose — C avant la révision après M6 —, X lance ; vitesse et saut réduits en portant)
- [x] Pad d'empilement 4×2 (marqueur `P` dans le niveau, une zone de 4×2 cases où les caisses se calent en colonne) ; `pad_options()` pur (1×1, 2×1 H/V, L, 2×2, 4×2 × rouge/bleu ; caisses du haut consommées d'abord), testé exhaustivement sur les 3⁸ grilles
- [x] Menu de construction à l'approche du pad (liste à côté du pad ; clic, ou Tab + Entrée) ; `pad_build()` consomme les caisses et produit un objet placeholder (l'IA appellera la même fonction)
- [x] File d'événements jeu → présentation (`GameState.events`, vidée à chaque tick ; aucun lecteur côté simulation)

**Fin :** on remplit son pad et on obtient des objets selon la forme construite.

## M3 — Combat
- [x] Armes data-driven : pistolet, uzi, grenades (+ puissantes), lance-roquettes ; ramassage, munitions limitées par arme
- [x] Projectiles : droits, gerbe (dispersion de l'uzi), en cloche, explosions (dégâts de zone)
- [x] Santé (100 PV pour le soldat de base ; 150 / 200 / 300 arrivent avec les variantes de compagnon en M4), recul, mort (lâcher arme et caisse)
- [x] Poings
- [x] Mannequins ennemis immobiles pour tester (marqueur `T`)

**Fin :** le pad donne de vraies armes et on peut détruire des cibles. ✅

## M4 — Escouade
- [x] Équipes (couleur, pad, membres)
- [x] Le pad crée des compagnons : 2×1 vertical (fort si bleue), 2×2 (lourd, avec lance-roquettes)
- [x] Changer de soldat contrôlé, transfert auto à la mort
- [x] Ordres : suivre, garder la base, ramasser, attaquer (comportements simples sans pathfinding)
- [x] Variantes de compagnon (normal 100 PV, fort 150, lourd 200 avec lance-roquettes, lourd blindé 300 provisoire) : PV et équipement dans `tuning.h`

**Fin :** on gère une petite escouade qui grossit grâce au pad. ✅

## M5 — IA et navigation
- [x] Graphe de navigation (marcher, sauter, tomber, échelle) + affichage debug — construit en **simulant la vraie physique** (`game/nav.c`, D045) ; `--debug-nav`
- [x] A* + suivi de chemin via `InputFrame` virtuelle — arêtes « libre » / « avec une caisse » (pas d'échelle, saut plus bas), recalage et replanification
- [x] FSM soldat IA (chercher caisse, rapporter, combattre, fuir) — modes `AiMode` (aussi : s'armer, voler, attaquer, garder, suivre)
- [x] Commandant d'équipe IA — construit sur le pad (mêmes `pad_build` et recettes que le menu) et distribue des rôles (`game/commander.c`, D047)
- [x] Vol de caisses sur les pads adverses — ordre `steal` (nouvel ordre, aussi pour le joueur)
- [x] Test : partie IA vs IA simulée sans SDL pendant N ticks — `test_ai` (7000 ticks, invariants, déterminisme, affrontement de lourds) + fumée `smoke_ai_match`

**Fin :** une équipe IA joue une partie cohérente seule. ✅ (`cratesquad --headless --level assets/levels/ai_arena.lvl --ai-team 1,2 …`)

## M6 — Tranche verticale : escarmouche 1 vs IA
- [x] Règles d'escarmouche, élimination, écran de résultats — `game/rules.c` : une équipe est éliminée avec son dernier soldat, la dernière debout gagne (match nul possible) ; résultats 2,5 s après (fabriqués, éliminés, pertes, soldats, base) ; une escarmouche lancée du menu commence à 3 soldats par équipe (D052, D056)
- [x] Bases destructibles : 1000 PV, dégâts de n'importe quelle arme, pad inutilisable une fois détruite (l'équipe continue à jouer) — `game/base.c`, bâtiment sur le point de départ ; l'IA attaque la base adverse quand personne ne la défend (D051)
- [x] HUD (PV, arme, munitions, effectif, ordre, indicateurs hors écran) — police bitmap maison (`render/font.c`, D054), PV des bases, chrono, flèches vers les bases et les caisses proches, nom de la construction choisie dans le menu du pad
- [x] Écran titre, configuration de partie, pause — `app/front.c` sans SDL (D053) : titre, niveau + mode (contre l'IA / IA contre IA) avec aperçu, pause (reprendre, recommencer, abandonner) ; clavier et souris
- [x] Premier niveau soigné — « La Crique » (`beach.lvl`, D058) : symétrique, falaises, plages, chenal et son pont, plateau central ; habillage du terrain (herbe, terre, planches, eau animée, collines en parallaxe)
- [x] Sons placeholders (événements) — synthétisés au démarrage (`app/sfx.c`), joués par `audio/audio.c` (SDL_AudioStream), atténués avec la distance (D055)

**Fin :** une partie complète du menu à l'écran de victoire. ✅ **Point d'étape avec l'utilisateur.**

Après le point d'étape (demandé par l'utilisateur) :
- [x] Musique placeholder — séquenceur chiptune sans fichier (`app/music.c`, D059) : 4 morceaux originaux (titre, La Crique, L'Arène, résultats), fondus entre écrans, baissée en pause, coupée à la fin
  du match pour la fanfare ; `--no-music`, `--export-audio DIR` pour les écouter en WAV
- [x] Effectif de départ réglable dans le menu (1 à 5, **1 par défaut**, D061)
- [x] Chutes de caisses plus fréquentes avec plafond par zone, caisses présentes au départ (`start.N`), caisses qui s'étalent au lieu de s'empiler (D062)
- [x] Porteur de caisse plus rapide (×0,9 au lieu de ×0,75)
- [x] Sprites pixel art des soldats (4 variantes × 10 poses, assemblés à partir de parties), bras qui visent, armes, roquettes, caisses, parachute (`render/sprites.c`, D063)
- [x] Particules, animations (course, saut, chute, échelle, portage, coup de poing, écrasement à l'atterrissage), tremblement d'écran (`app/fx.c`, D064)
- [x] Tir et coups **maintenus** pour toutes les armes, délais raccourcis (D066)
- [x] Écrasement des caisses à l'atterrissage et à la pose (`fx_crate_squash`)
- [x] Cadres blancs : caisse qui serait prise / détruite, case où la caisse portée serait posée (D066)
- [x] Prendre / poser sur **E** (au lieu de C)
- [x] Touche **M** : musique allumée / coupée, **coupée au départ** (`--music` pour la démarrer allumée)
- [x] **Objectif de construction** : clic du milieu → menu circulaire des 12 constructions (forme en caisses) ; les alliés en *ramasser* apportent les bonnes caisses (couleur préférée, bleu déjà là accepté) et construisent (D068)
- [x] Menu de debug **F2** : faire apparaître une caisse rouge / bleue, un allié, une arme ou un tank près du soldat (D067) ; la pause passe sur F6
- [x] Détruire une caisse au **clic droit** : cœurs ou munitions au hasard (D066)
- [x] Tileset (herbe, terre en profondeur, planches, échelles, décor de touffes, fleurs et cailloux), eau animée, base en 3 états + ruine, drapeau, mannequin, tanks en maquette,
  mallette d'arme, pad en acier, icônes du HUD et du menu du pad, camps sur l'écran titre (D065)
- [x] Sons manquants — grosse explosion, ricochet, saut et atterrissage (du soldat suivi seulement, D060), caisse qui se pose, mannequin brisé, arme lâchée, changement de soldat, équipe éliminée,
  alarme de base attaquée, ouverture du menu du pad

## M7 — Véhicules et armes lourdes
- [ ] Pad plein 4×2 → tank (rouge) / tank lourd (avec bleue)
- [ ] Monter / descendre, PV, destruction
- [ ] IA : utilisation des véhicules

## M8 — Multijoueur local
- [ ] Manettes `SDL_Gamepad`, hotplug, attribution aux joueurs
- [ ] Remappage des touches
- [ ] Écran partagé 2 / 3 / 4 joueurs (viewports + caméras)
- [ ] Équipes mixtes humains + IA, jusqu'à 4 équipes

## M9 — Contenu
- [ ] Direction artistique : personnages originaux, tilesets, animations — premiers sprites, animations et tileset faits après M6 (D063, D064, D065) ; nom de l'espèce, autres décors (par niveau) à faire
- [ ] Musique et effets sonores originaux — une première version synthétisée existe (D055, D059) ; à enrichir ou remplacer
- [ ] 6–10 niveaux d'escarmouche
- [ ] Campagne : structure, objectifs de mission, progression sauvegardée
- [ ] Options (volume, plein écran, échelle, difficulté)

## M10 — Finition et distribution
- [ ] « Juice » : particules, tremblement d'écran, hit-stop, animations de squash — particules, tremblement et écrasement faits (D064) ; reste le hit-stop
- [ ] Équilibrage
- [ ] Profilage, perfs (cible : 60 FPS en 4 joueurs sur une machine modeste)
- [ ] Packaging Linux (et Windows via MinGW / MSVC)
- [ ] Choix du titre définitif

---

## Idées en attente (non planifiées)
- Décor destructible
- Caisses spéciales (piégées, dorées)
- Replays (grâce au déterminisme)
- Modes alternatifs : roi des caisses, capture de drapeau
- Rechargement à chaud des niveaux et du tuning
