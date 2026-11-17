# Crate Squad *(titre de travail)*

Jeu d'action / stratégie en 2D, vue de côté, écrit en **C** avec **SDL3** et **CMake**.

Le jeu s'inspire des mécaniques de *Team Buddies* (Psygnosis, PlayStation, 2000) :
des escouades de petits soldats se disputent des caisses qui tombent du ciel,
les ramènent à leur base et les empilent pour obtenir des armes, des recrues et des véhicules.

> **Projet de fan non affilié.** Le nom, les personnages, les graphismes, les sons,
> les musiques et les niveaux sont **originaux**. Aucun asset, nom ou personnage
> du jeu d'origine n'est utilisé. Voir [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md#principes).

## État du projet

Jalons M0 à M4 faits : socle technique, niveau et déplacement, caisses et pad de construction, combat (armes, projectiles,
explosions, santé, mannequins), escouade (compagnons, équipes, ordres). La navigation et l'IA d'équipe arrivent ensuite. Avancement détaillé : [docs/ROADMAP.md](docs/ROADMAP.md).

## Prérequis

- Un compilateur C17 : GCC ≥ 11 ou Clang ≥ 14
- CMake ≥ 3.24
- Git (CMake télécharge SDL3 au premier build via `FetchContent`)
- Sous Linux, les bibliothèques de développement nécessaires à la compilation de SDL3
  (X11 et/ou Wayland, ALSA/PulseAudio/PipeWire, libudev). Exemple pour Debian/Ubuntu :

  ```sh
  sudo apt install build-essential cmake git pkg-config \
      libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev \
      libwayland-dev libxkbcommon-dev wayland-protocols \
      libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev
  ```

## Compiler et lancer

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/cratesquad                      # jeu interactif (fenêtre) : niveau de test, caisses, pad, armes, mannequins
```

Sans écran (CI, captures, bots) :

```sh
./build/cratesquad --headless --bot --frames 3000 --screenshot build/screenshots/bot.png --debug-overlay
./build/cratesquad --headless --start 984,432 --script "U:75" --trace 25      # monter l'échelle
./build/cratesquad --headless --walk-to 250 --frames 400                      # bot « marcheur »
```

Toutes les options : `docs/ARCHITECTURE.md` § Exécution headless.

Options CMake prévues :

| Option                | Défaut | Rôle                                                       |
|-----------------------|--------|------------------------------------------------------------|
| `CS_USE_SYSTEM_SDL`   | `OFF`  | Utiliser le SDL3 installé sur le système au lieu de le télécharger |
| `CS_WARNINGS_AS_ERRORS` | `OFF` | Traiter les warnings comme des erreurs                    |
| `CS_SANITIZERS`       | `OFF`  | Activer ASan + UBSan (Debug)                               |
| `CS_BUILD_TESTS`      | `ON`   | Compiler les tests unitaires                               |

## Tests

```sh
cmake --build build -j && ctest --test-dir build --output-on-failure
```

## Commandes (proposition, détails dans le GDD)

| Action              | Clavier       | Manette          |
|---------------------|---------------|------------------|
| Se déplacer         | ← → ou **ZQSD** (Q / D) | Stick G / croix |
| Monter / descendre (échelles) | ↑ ↓ ou **ZQSD** (Z / S) | Stick G / croix |
| Sauter              | Espace        | A / Croix        |
| Viser | Souris (dès qu'elle bouge) | — |
| Tirer / frapper     | X, ou **bouton gauche** (maintenu pour l'uzi) | X / Carré |
| Prendre / poser une caisse ; échanger d'arme | C | B / Rond |
| Lancer la caisse portée | X       | X / Carré        |
| Changer de soldat (sans menu de pad ouvert) | Tab | RB / R1 |
| Menu du pad : entrée suivante / valider | Tab / Entrée (ou clic gauche sur l'entrée) | — |
| Ordre de l'équipe suivant (suivre → garder → ramasser → attaquer) | touche à la position de Q (étiquetée A en AZERTY) | LB / L1 |
| Pause               | Échap         | Start            |
| Debug overlay       | F1            | —                |
| Pause / pas-à-pas / ralenti ×0,25 (debug) | F2 / F3 / F4 | — |
| Recommencer la partie (debug) | F5 | — |

**Les touches sont repérées par leur position physique** (scancodes SDL), pas par la lettre imprimée dessus :
les touches de déplacement sont celles de la position WASD, qui se lisent **ZQSD sur un clavier AZERTY** et WASD sur un
QWERTY. Ce sont les mêmes touches physiques sur tous les claviers, sans réglage. Les flèches fonctionnent aussi.

## Documentation

| Document | Contenu |
|----------|---------|
| [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md) | Le jeu : règles, mécaniques, adaptation 2D, points à valider |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Organisation du code, modules, boucle de jeu, données |
| [docs/CONVENTIONS.md](docs/CONVENTIONS.md) | Style de code C, nommage, mémoire, erreurs |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Jalons et tâches |
| [docs/DECISIONS.md](docs/DECISIONS.md) | Journal des décisions techniques |
| [CLAUDE.md](CLAUDE.md) | Consignes pour l'assistant IA travaillant sur le projet |
