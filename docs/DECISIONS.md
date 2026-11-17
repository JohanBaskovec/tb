# Journal des décisions

Format : une entrée par décision, jamais supprimée. Si elle est remise en cause,
on ajoute une nouvelle entrée qui la remplace et on change le statut de l'ancienne.

Statuts : **Proposée** (en attente de validation par l'utilisateur), **Acceptée**, **Remplacée par DXXX**.

---

### D001 — Langage : C17
**Statut :** Proposée
C17 strict, sans extensions GNU. Assez moderne (`_Static_assert`, initialiseurs désignés, `stdbool`)
et portable entre GCC, Clang et MSVC.

### D002 — Bibliothèque : SDL3, récupérée par CMake FetchContent
**Statut :** Proposée
SDL3 est la version stable actuelle de SDL (gamepads, audio streams, rendu 2D, présentation logique).
Récupérée à une version **épinglée** (tag de release) via `FetchContent` pour un build reproductible
qui ne dépend pas des paquets de la distribution. Option `CS_USE_SYSTEM_SDL` pour utiliser une installation système.
*Alternative écartée :* SDL2 — plus ancienne, API moins pratique pour les manettes et l'audio.

### D003 — Rendu : SDL_Renderer 2D, pas d'OpenGL/Vulkan
**Statut :** Proposée
Largement suffisant pour du pixel art 640×360 avec quelques centaines de sprites. Si besoin plus tard
(shaders, effets), on pourra passer à `SDL_GPU` sans toucher à `game/`.

### D004 — Résolution logique 640×360, tuiles 16 px
**Statut :** Proposée
16:9, mise à l'échelle entière vers 720p (×2), 1080p (×3), 1440p (×4). En écran partagé 4 joueurs,
chaque vue fait 320×180, ce qui reste lisible.

### D005 — Simulation à pas fixe 60 Hz, rendu interpolé
**Statut :** Proposée
Physique stable et reproductible, indépendante du framerate.

### D006 — Simulation indépendante de SDL
**Statut :** Proposée
`core/` et `game/` n'incluent pas SDL. Permet les tests headless, les parties IA simulées et le déterminisme.

### D007 — Entités : tableau fixe + union + handles générationnels
**Statut :** Proposée
Plus simple qu'un ECS en C, sans allocation dynamique, références sûres via `EntityId`.
*Alternative écartée :* ECS complet — trop de plomberie pour la taille du jeu.

### D008 — Niveaux au format texte ASCII
**Statut :** Proposée
Éditable à la main, lisible dans un diff, aucun parseur tiers nécessaire.
On pourra ajouter un import depuis Tiled plus tard si l'édition à la main devient pénible.

### D009 — Dépendances tierces limitées aux single-headers vendorisés
**Statut :** Proposée
`third_party/stb_image.h` pour le PNG. Audio : WAV via `SDL_LoadWAV` au départ ;
`stb_vorbis.c` si la musique en OGG devient nécessaire. Toute autre dépendance demande une validation.

### D010 — Assets et univers 100 % originaux
**Statut :** Acceptée
Aucun asset, nom, personnage, texte, musique ou niveau du jeu d'origine. Seules les mécaniques inspirent le projet.

### D011 — Langues
**Statut :** Proposée
Documentation et commits en français ; code et commentaires en anglais.

### D012 — Pad d'empilement 2D : grille 4×2
**Statut :** Remplacée par D013 (la grille 4×2 est conservée, les recettes changent)
Transposition du pad 2×2×2 en vue de côté : 2 caisses → arme, 2×2 → soldat, 4×2 → véhicule/arme lourde,
avec compte à rebours relancé à chaque ajout. Voir GAME_DESIGN.md § 6. En attente des précisions sur le jeu d'origine.

### D013 — Caisses rouges/bleues et recettes du pad
**Statut :** Acceptée (fournie par l'utilisateur, 2026-11-17)
Deux types de caisses : rouge (standard) et bleue (plus forte). Grille 4×2. Une forme contenant au moins
une caisse bleue donne la version puissante (confirmé le 2026-11-17). 2×2 rouge : compagnon lourd ; 2×2 avec bleue : compagnon lourd blindé.
Recettes : 1×1 pistolet/uzi ; 2×1 horizontal grenades/grenades puissantes ; 2×1 vertical compagnon/compagnon fort ;
L uzi/lance-roquettes ; 2×2 compagnon lourd ; 4×2 tank/tank lourd. Les recettes vivent dans une table de données.

### D014 — Vocabulaire : « compagnon » (companion)
**Statut :** Acceptée
Les soldats s'appellent *companion* dans le code (`companion.h`, `ENT_COMPANION`, `companion_*`), pas *buddy*,
pour rester éloigné du nom du jeu d'origine.

### D015 — Les zones de largage sont des données de niveau
**Statut :** Acceptée (fournie par l'utilisateur)
Chaque niveau décrit ses zones de largage (position, couleur de caisse, cadence, plafond). Cas voulus : une zone unique ;
ou une zone rouge près de chaque base + une zone centrale 100 % bleue.

### D016 — Une base détruite ne peut plus utiliser son pad
**Statut :** Acceptée (fournie par l'utilisateur)
Les bases ont 1000 PV et sont destructibles avec n'importe quelle arme. L'équipe peut continuer à jouer avec ses
soldats restants. Les conditions de victoire multijoueur restent à définir.

### D017 — Construction manuelle via un menu à côté du pad (pas de compte à rebours)
**Statut :** Acceptée (fournie par l'utilisateur, détails confirmés le 2026-11-17)
À l'approche du pad, les constructions possibles s'affichent à côté. Le joueur en choisit une (clic ; sélection +
validation au clavier / manette), ce qui consomme les caisses en commençant par celles du haut. Remplace le compte à
rebours automatique que j'avais proposé. L'IA passe par la même fonction.

### D018 — 100 PV par personnage, munitions limitées par arme
**Statut :** Acceptée (fournie par l'utilisateur)

### D019 — Le haut d'une échelle est une plateforme traversable
**Statut :** Acceptée
Une cellule `H` dont la cellule au-dessus n'est pas une échelle se comporte comme une plateforme fine sur sa face
supérieure : on peut s'y tenir debout et la traverser vers le bas en saisissant l'échelle. Aucun type de tuile
supplémentaire. Convention de niveau : une échelle monte jusqu'à la rangée dont le dessus est le sol à atteindre.

### D020 — Réglages de départ du mouvement
**Statut :** Proposée (à valider en jouant)
Marche 100 px/s, gravité 900 px/s², saut 300 px/s (≈ 50 px, soit 3 tuiles), chute max 420 px/s, échelle 70 px/s,
coyote time et jump buffer 0,1 s, hitbox 12×22 px. Tout est dans `game/tuning.h`. Les murs et plateformes du niveau de
test sont dimensionnés avec ces valeurs : les scénarios de `test_scenarios.c` casseront si on les change trop.

### D021 — La boucle fenêtrée est testée sous le pilote vidéo SDL `dummy`
**Statut :** Acceptée
Permet d'exécuter la vraie boucle (fenêtre, renderer, FPS dans le titre, captures) sans display ni fenêtre visible.
L'option `--expect-video-driver` sert de garde : le programme refuse de créer une fenêtre si SDL n'a pas choisi le pilote attendu.

### D022 — Outils de test intégrés au jeu
**Statut :** Acceptée
Scripts d'entrées, bot aléatoire, bot « marcheur », `--start`, `--trace`, films de captures : disponibles en CLI
et réutilisés par les tests. Le « marcheur » n'est pas un pathfinder ; l'IA vient avec M5.

### D023 — PV des compagnons et munitions provisoires
**Statut :** Acceptée pour 100 / 150 / 200 PV (fournis par l'utilisateur) ; **Proposée** pour le reste
Compagnon 100 PV, fort 150, lourd 200, lourd blindé 300 (provisoire : « encore plus de PV »). Munitions provisoires :
pistolet 24, uzi 60, grenades 6, grenades puissantes 4, lance-roquettes 5. Valeurs dans `game/tuning.h` à l'implémentation,
à ajuster en jouant.
