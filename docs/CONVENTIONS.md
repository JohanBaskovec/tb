# Conventions de code

## Langage et compilation

- **C17** strict (`-std=c17`, pas de `gnu17`). Pas de VLA, pas d'extensions GNU.
- Warnings : `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
  -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wdouble-promotion`.
- Le code doit compiler proprement avec GCC **et** Clang.
- Debug : `-O0 -g` + ASan/UBSan via `CS_SANITIZERS=ON`. Release : `-O2`.

## Nommage

| Élément | Style | Exemple |
|---------|-------|---------|
| Fichiers | `snake_case` | `projectile.c` |
| Fonctions publiques | `module_verbe_objet` | `pad_find_best_shape()` |
| Fonctions privées | `static`, même style | `static void resolve_x(...)` |
| Types (struct, enum, typedef) | `PascalCase` | `GameState`, `EntityId` |
| Valeurs d'enum | `PREFIXE_MAJUSCULES` | `ENT_CRATE`, `ACT_JUMP` |
| Macros, constantes | `MAJUSCULES` | `MAX_ENTITIES`, `TILE_SIZE` |
| Variables, champs | `snake_case` | `jump_buffer_timer` |
| Booléens | préfixe `is_`, `has_`, `can_` | `is_carrying` |

- Types de base : `u8 u16 u32 u64 i8 i16 i32 i64 f32 f64` (définis dans `core/types.h`), `bool` de `<stdbool.h>`.
- Unités dans les noms quand c'est ambigu : `_px`, `_tiles`, `_s` (secondes), `_ticks`.

## Organisation

- Un module = `module.h` (interface publique minimale) + `module.c`.
- Headers protégés par `#pragma once`.
- Ordre des includes : header du module, en-têtes du projet, bibliothèques tierces, libc.
- Tout ce qui n'est pas dans le header est `static`.
- Pas de variables globales modifiables, sauf l'état du logger. L'état passe par paramètres (`GameState *gs`).
- Les fonctions de mise à jour prennent `(GameState *gs, ...)` ; les fonctions de rendu prennent `const GameState *gs`.

## Mémoire

- Pas de `malloc`/`free` dans la boucle de jeu. Arènes (`arena_push`) et pools de taille fixe.
- Les tailles maximales sont des constantes (`MAX_ENTITIES`, `MAX_TEAMS`…) avec `ASSERT` en cas de dépassement
  et un comportement défini en Release (refus de création + log).
- Jamais de pointeur vers une entité conservé d'une frame à l'autre : utiliser `EntityId`.

## Erreurs

- Fonctions faillibles : retournent `bool` (ou un enum de résultat) ; la sortie passe par pointeur.
- `ASSERT(cond)` pour les invariants (désactivé en Release). `LOG_ERROR / LOG_WARN / LOG_INFO / LOG_DEBUG`.
- Toujours vérifier les retours de SDL et logger `SDL_GetError()`.

## Nombres et déterminisme

- Simulation en `f32`, pas de temps fixe `DT = 1/60`.
- Pas de `rand()`, pas de lecture d'horloge dans `game/` : RNG `rng_*` stockée dans `GameState`.
- Les constantes de gameplay vont dans `game/tuning.h`, exprimées en unités lisibles
  (px/s, secondes) et converties si besoin.

## Formatage

- Indentation 4 espaces, pas de tabulations. Lignes ≤ 100 colonnes.
- Accolade ouvrante sur la même ligne (y compris fonctions).
- Toujours des accolades, même pour un `if` d'une ligne.
- Le `.clang-format` à la racine fait foi (écrit pour ce projet).

```c
static void companion_apply_gravity(Entity *e, f32 dt) {
    if (e->flags & EF_ON_LADDER) {
        return;
    }
    e->vel.y = approach(e->vel.y, TUNING_MAX_FALL_SPEED, TUNING_GRAVITY * dt);
}
```

## Commentaires

- En anglais. Expliquer le *pourquoi*, pas le *quoi*.
- Chaque header commence par 1–3 lignes décrivant la responsabilité du module.

## Commits

- En français, à l'impératif, préfixés par le module : `pad: détecte les formes 2×2`.
- Un commit = un changement cohérent qui compile et passe les tests.
