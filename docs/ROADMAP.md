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
- [x] Overlay debug : hitboxes, grille, vitesse, pastilles d'état (`--debug-overlay` / F1) ; pause / pas-à-pas / ralenti (F2/F3/F4, événements testés)
- [x] Fichier `tuning.h`
- [x] Outils de test : scripts d'entrées, bot aléatoire, bot « marcheur », `--start`, `--walk-to`, films de captures
- [x] Niveau de test `assets/levels/test_arena.lvl` (80×30) et scénarios de traversée automatisés

**Fin :** on parcourt agréablement un niveau test avec un soldat. ✅ techniquement (tout le niveau est traversable,
vérifié par bots et scénarios). **Le « agréablement » reste à valider à la manette/au clavier par l'utilisateur** :
les valeurs de `tuning.h` (vitesse, hauteur de saut, gravité…) sont des réglages de départ.
Pas encore de police de caractères : l'overlay de debug utilise des pastilles de couleur (la police arrive avec le HUD, M6).

## M2 — Caisses et pad (cœur du jeu)
- [x] Pool d'entités avec `EntityId` générationnels (seules les caisses y vivent pour l'instant ; le joueur y entre en M4)
- [x] Caisses rouges et bleues ; largage sur les zones `D` configurables par niveau (`drop.N=couleur,intervalle,plafond[,%bleu]`) : descente lente, puis physique
- [x] Empilement des caisses libres (alignées sur la grille, solides pour tout le monde, piles qui se tassent)
- [x] Prendre / porter / poser / lancer une caisse (C prend/pose, X lance ; vitesse et saut réduits en portant)
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
- [ ] Graphe de navigation (marcher, sauter, tomber, échelle) + affichage debug
- [ ] A* + suivi de chemin via `InputFrame` virtuelle
- [ ] FSM soldat IA (chercher caisse, rapporter, combattre, fuir)
- [ ] Commandant d'équipe IA
- [ ] Vol de caisses sur les pads adverses
- [ ] Test : partie IA vs IA simulée sans SDL pendant N ticks

**Fin :** une équipe IA joue une partie cohérente seule.

## M6 — Tranche verticale : escarmouche 1 vs IA
- [ ] Règles d'escarmouche, élimination, écran de résultats
- [ ] Bases destructibles : 1000 PV, dégâts de n'importe quelle arme, pad inutilisable une fois détruite (l'équipe continue à jouer)
- [ ] HUD (PV, arme, munitions, effectif, ordre, indicateurs hors écran)
- [ ] Écran titre, configuration de partie, pause
- [ ] Premier niveau soigné
- [ ] Sons placeholders (événements)

**Fin :** une partie complète du menu à l'écran de victoire. **Point d'étape avec l'utilisateur.**

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
- [ ] Direction artistique : personnages originaux, tilesets, animations
- [ ] Musique et effets sonores originaux
- [ ] 6–10 niveaux d'escarmouche
- [ ] Campagne : structure, objectifs de mission, progression sauvegardée
- [ ] Options (volume, plein écran, échelle, difficulté)

## M10 — Finition et distribution
- [ ] « Juice » : particules, tremblement d'écran, hit-stop, animations de squash
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
