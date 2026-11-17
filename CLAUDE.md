# CLAUDE.md

Consignes pour Claude (ou tout assistant IA) travaillant sur ce dépôt.

## Le projet en une phrase

Jeu 2D vue de côté en C17 + SDL3 + CMake, inspiré des mécaniques de *Team Buddies* (PS1),
avec des assets et un univers **entièrement originaux**.
Lire [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md) et [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) avant toute tâche non triviale.

## Règles impératives

1. **Rien ne vient de l'extérieur du dépôt.** Ne jamais lire, copier ou s'inspirer de fichiers
   de configuration (`.clang-format`, `.editorconfig`, `CMakeLists.txt`, presets, `.gitignore`,
   settings d'IDE, etc.) situés ailleurs sur la machine de l'utilisateur. Tout fichier de config
   est écrit de zéro pour ce projet. Ne pas explorer en dehors de `/home/johan/IdeaProjects/tb`.
2. **Aucun asset du jeu d'origine** : pas de sprites, sons, musiques, noms de personnages,
   textes ou niveaux repris de *Team Buddies*. Les placeholders sont des formes colorées
   générées ou dessinées pour le projet.
3. **La logique de jeu ne dépend pas de SDL.** Les dossiers `src/core/` et `src/game/`
   n'incluent jamais `<SDL3/...>`. Seuls `src/platform/`, `src/render/`, `src/audio/`
   et `src/main.c` y ont droit. (Voir ARCHITECTURE.md § Couches.)
4. **Pas de nouvelle dépendance** (bibliothèque tierce, outil de build) sans accord explicite.
   Dépendances actuelles : SDL3 (FetchContent), et en `third_party/` uniquement des
   bibliothèques single-header validées (voir DECISIONS.md).
5. **Ne pas committer ni pousser** sans demande explicite.
6. Ne pas coder en avance sur la roadmap : implémenter le jalon en cours, petit à petit.

## Langues

- Documentation, messages de commit, échanges avec l'utilisateur : **français**.
- Code, identifiants, commentaires dans le code : **anglais**.

## Commandes

```sh
# Configurer (Debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCS_SANITIZERS=ON
# Compiler
cmake --build build -j
# Tests
ctest --test-dir build --output-on-failure
# Lancer (toujours en headless ici)
./build/cratesquad --headless --bot --frames 600 --screenshot build/screenshots/x.png --debug-overlay
```

## Tests : toujours en headless

- **Ne jamais ouvrir de vraie fenêtre.** Deux façons sûres de lancer `cratesquad` :
  `--headless` (aucun sous-système vidéo), ou la boucle fenêtrée sous le pilote SDL `dummy` :
  `SDL_VIDEODRIVER=dummy cratesquad --expect-video-driver dummy --max-ticks N …`
  (la garde `--expect-video-driver` refuse de créer une fenêtre si SDL choisit un autre pilote).
  Jamais de lancement nu de `./build/cratesquad`.
- Écrire et lancer les tests au fur et à mesure (unitaires sans SDL, parties simulées avec bots,
  captures PNG via `--screenshot`, à regarder avec l'outil Read quand utile).
- Captures et fichiers temporaires dans `build/screenshots/`, jamais à la racine du dépôt.
- Pour explorer un comportement : `--trace N`, `--start X,Y`, `--script`, `--walk-to`, `--debug-overlay`,
  `--shot-cam X,Y` (voir `docs/ARCHITECTURE.md` § Exécution headless). Un bug de physique ou de niveau
  se reproduit d'abord par un script ou un scénario dans `tests/test_scenarios.c`, puis se corrige.

Avant de déclarer une tâche terminée : le projet compile **sans warning** avec
`-Wall -Wextra -Wpedantic`, et `ctest` passe.

## Style

Voir [docs/CONVENTIONS.md](docs/CONVENTIONS.md). Résumé :
- C17, pas d'extensions GNU, `snake_case`, préfixe de module (`crate_`, `pad_`, `companion_`…).
- Pas de `malloc` dans la boucle de jeu : arènes et pools alloués au chargement.
- Pas d'état global caché hors de `GameState` / `Platform` (sauf logger).
- RNG maison (`rng_*`), jamais `rand()`.
- Fonctions courtes, `static` par défaut, un header par module.

## Méthode de travail

- Mettre à jour la case correspondante dans [docs/ROADMAP.md](docs/ROADMAP.md) quand une tâche est faite.
- Toute décision technique structurante → nouvelle entrée dans [docs/DECISIONS.md](docs/DECISIONS.md).
- Toute règle de gameplay modifiée → mettre à jour [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md).
- Les valeurs de gameplay (vitesses, dégâts, délais) vont dans `src/game/tuning.h`,
  jamais en nombres magiques dispersés.
- La logique pure (collisions, pad, IA, règles) doit être couverte par des tests dans `tests/`.
- En cas de doute sur une règle du jeu d'origine : **demander**, ne pas inventer en silence.
  Les points encore incertains sont listés dans GAME_DESIGN.md § « Points à valider ».
