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

### D024 — Les caisses au repos sont alignées sur la grille et écrites dans une couche solide du niveau
**Statut :** Proposée
En atterrissant, une caisse se cale dans la colonne qu'elle recouvre le plus (et qui est soutenue). Les caisses au repos
sont dans `Level.dyn_solid`, que `level_is_solid()` consulte : aucune règle spéciale dans la physique. Rend l'empilement
robuste et prépare le pad 4×2. *Alternative écartée :* caisses à position libre avec collisions AABB entre elles, plus fragile
pour les piles et sans lien avec la grille du pad.

### D025 — Prendre / poser / lancer
**Statut :** Proposée (à valider en jouant)
Touche d'action (C) : prend la caisse du dessus la plus proche devant soi (portée 16 px), ou pose la caisse portée devant soi
(jusqu'à 2 tuiles plus haut si la case est prise). Touche de tir (X) en portant : lance en cloche (le tir est de toute façon
impossible en portant). En portant : vitesse ×0,75, saut ×0,8, pas d'échelle. Valeurs dans `game/tuning.h`.

### D026 — Configuration des zones de largage dans l'en-tête du niveau
**Statut :** Acceptée
`drop.N=COULEUR,INTERVALLE_S,PLAFOND[,POURCENT_BLEU]`, N comptant les marqueurs `D` dans l'ordre de lecture. Le plafond compte
les caisses issues de la zone encore existantes ; le compte à rebours ne tourne que sous le plafond. Zone non configurée :
mixte, 8 s, plafond 3, 25 % de bleues.

### D027 — Les caisses ne blessent pas et attendent si un soldat occupe leur case
**Statut :** Proposée
Une caisse qui atterrit sur la case d'un soldat attend qu'il la libère avant de se verrouiller. Les dégâts éventuels
viennent avec le combat (M3). Une caisse tombée à l'eau ou dans le vide est détruite ; le soldat qui tombe à l'eau perd sa caisse.

### D028 — Contrôles clavier par position physique : ZQSD en AZERTY = WASD en QWERTY
**Statut :** Acceptée (demandée par l'utilisateur)
Les entrées clavier utilisent les scancodes SDL (position physique de la touche) et non les keycodes (lettre imprimée). Les quatre
touches de déplacement sont à la position WASD : ZQSD sur un clavier AZERTY, WASD sur un QWERTY, donc les mêmes touches physiques sur
tous les claviers, sans détection de disposition. Les flèches restent actives en parallèle. Conséquence : la touche d'ordres (scancode Q)
est étiquetée A sur un AZERTY. Si les contrôles deviennent configurables, ils le seront par scancode.

### D029 — Le pad est une zone du niveau marquée `P`
**Statut :** Acceptée
Un pad est une zone de 4×2 cases dont le marqueur `P` désigne la case en bas à gauche (la case reste vide). Le N-ᵉ marqueur dans l'ordre de
lecture appartient à l'équipe N, sauf `pad.N=ÉQUIPE` dans l'en-tête. Les caisses n'ont pas de logique de « calage » propre : elles se verrouillent
déjà dans la grille (D024), donc une caisse lâchée au-dessus du pad finit dans la colonne qu'elle recouvre le plus. Le contenu du pad est lu
dans le monde (caisses *au repos* dans les 8 cases) au lieu d'être dupliqué dans une grille tenue à jour.

### D030 — Les constructions sont des commandes appliquées entre deux ticks, via `pad_build()`
**Statut :** Acceptée
Le menu (humain) et l'IA appellent `pad_build(gs, équipe, forme, bleu?)` ; la fonction recalcule les options au moment de l'appel et refuse
ce qui n'est plus valide, ce qui rend les menus périmés inoffensifs. Ce n'est pas une entrée de `game_update` : si un rejeu / du réseau devient
nécessaire, les commandes devront être enregistrées avec leur numéro de tick (à décider alors). Pour une même forme, la version rouge et
la version bleue sont deux entrées distinctes quand les deux existent ; les caisses consommées sont les plus hautes, puis les plus à gauche.

### D031 — File d'événements vidée à chaque tick, hors du hash
**Statut :** Acceptée
`GameState.events` contient les événements du dernier tick et est vidée au début de `game_update`. Elle est de taille fixe (les débordements
sont comptés) et exclue de `game_hash` : la présentation lit, la simulation n'écrit que dedans, jamais l'inverse.

### D032 — Produit du pad : placeholder jusqu'aux armes et aux compagnons
**Statut :** Provisoire
`ENT_PRODUCT` (type + équipe) tombe et repose à côté du pad (à gauche ; le menu est à droite). Il sert à vérifier la chaîne caisses → recette →
objet avant M3 (armes ramassables) et M4 (compagnons). Un produit enterré par une caisse qui se pose est remonté au sommet de la pile.

### D033 — Les armes sont une table de données ; ses chiffres restent dans la table
**Statut :** Acceptée
`weapon.c` contient un tableau `WeaponDef[WEAPON_COUNT]` (type de tir, munitions, cadence, vitesse, dispersion, dégâts, rayon, recul, portée,
taille). Ajouter une arme = ajouter une ligne, sans toucher à la logique de combat. Ces chiffres sont des valeurs de gameplay mais ne sont pas
éclatés en macros de `tuning.h` : la table *est* le réglage (les autres constantes de combat — PV, clignotement, rebond des grenades — sont,
elles, dans `tuning.h`). Les valeurs sont provisoires (GDD § 7).

### D034 — Tir ami : balles et coups épargnent l'équipe du tireur, pas les explosions
**Statut :** Provisoire
Les balles et les coups ne blessent que les autres équipes ; les explosions blessent tout le monde, tireur compris, mais pas à travers les murs
(segment échantillonné contre le terrain et les caisses empilées). Cela rend les grenades risquées à bout portant sans pénaliser le tir normal.
À revoir si les tests de jeu montrent que c'est frustrant.

### D035 — Ramassage : automatique à mains nues, échange sur la touche d'action
**Statut :** Provisoire
Un soldat sans arme ramasse automatiquement celle sur laquelle il se trouve ; armé, il l'échange avec la touche d'action (l'ancienne est lâchée
avec ses munitions). Une action de caisse est prioritaire sur la même touche (portage, prise, pose, lancer) : le tir et l'échange sont
ignorés ce tick. Une arme vide disparaît (retour aux poings) au lieu de rester au sol.

### D036 — Mort du soldat : réapparition provisoire jusqu'à l'escouade
**Statut :** Remplacée par D043
Le soldat tombé à 0 PV lâche son arme (avec ses munitions) et sa caisse sur place, puis réapparaît au point de départ en pleine santé : il n'y a qu'un
soldat tant que M4 n'existe pas. Tomber à l'eau reste un respawn sans dégâts (l'arme est perdue, pas lâchée). Le comportement final
(« une équipe sans soldat est éliminée ») viendra avec l'escouade.

### D037 — Mannequins posés par le niveau (`T`), sans réapparition
**Statut :** Acceptée
Les cibles de test sont des entités `ENT_DUMMY` créées à `game_init` depuis les marqueurs `T` du niveau (équipe 1, 100 PV, immobiles, sans riposte).
Un mannequin détruit ne revient pas : les tests comptent les éliminations.

### D038 — Effets visuels côté application, nourris par les événements
**Statut :** Acceptée
`app/fx` convertit les événements d'un tick en effets de quelques ticks (explosion, étincelle, flash). La liste vit dans `Sim`, hors de `GameState` et
du hash : le jeu ne la lit jamais. `sim_tick` la nourrit à chaque tick, donc plusieurs ticks par image ne perdent aucun événement.

### D039 — Visée à la souris : le jeu ne connaît que `aim`, pas la souris
**Statut :** Acceptée (demandée par l'utilisateur)
La souris est traduite hors du jeu : `sim_tick` convertit la position (pixels logiques → monde, avec la caméra courante) en `InputFrame.aim` et ajoute
`ACT_FIRE` tant que le bouton gauche est tenu. Le jeu ne dépend donc ni de SDL ni de la caméra, et un script ou une IA vise de la même façon
(`KEYS@X/Y:TICKS`). Le bouton gauche sert aussi au menu du pad : un clic sur une entrée construit et ne tire pas, et le bouton reste muet jusqu'à son
relâchement. La visée s'active au premier mouvement de souris (un joueur au clavier seul n'est pas gêné) ; le soldat se tourne alors vers le curseur.
La direction visée fait partie de l'état (`Companion.aim_dir`, hachée). Grenade : lancer balistique vers le curseur plutôt qu'un jet fixe.

### D040 — Les soldats vivent dans un tableau à part, pas dans le pool d'entités
**Statut :** Acceptée
`GameState.soldiers[32]` (indice = identité) plutôt que des `ENT_SOLDIER` : un soldat est gros (corps, arme, portage, mémoire d'IA), a un cycle de vie
propre (contrôle, élimination d'équipe) et sert de cible, d'équipier et de porteur ; les références croisées se font par indice, pas par `EntityId`.
`game_player()` renvoie toujours un `Companion` (un fantôme sans PV quand l'équipe est éliminée) pour que la caméra, le HUD et les tests n'aient jamais de `NULL`.

### D041 — L'IA ne produit qu'une `InputFrame`
**Statut :** Acceptée
Un soldat non contrôlé est piloté par `ai_think()` qui remplit une `InputFrame` (déplacement via `Walker`, visée, tir, touche d'action). La mise à jour d'un soldat est la même
pour un humain et pour l'IA : l'IA ne peut ni tricher ni diverger des règles. Aucun pathfinding en M4 : une cible hors du sol du soldat est ignorée (M5).

### D042 — Un seul ordre par équipe, appliqué aux soldats non contrôlés
**Statut :** Provisoire
La portée des ordres était à décider (GDD) : l'ordre est celui de l'**équipe** et s'applique à tous ses soldats que le joueur ne contrôle pas ; la touche d'ordre le fait
défiler (suivre → garder → ramasser → attaquer). Choisir un ordre par soldat ou par soldat visé reste possible plus tard, sans changer l'IA.

### D043 — Mort définitive, eau = retour à la base
**Statut :** Provisoire (remplace D036)
Un soldat à 0 PV meurt pour de bon (arme et caisse lâchées, contrôle au plus proche allié, équipe éliminée avec le dernier). Tomber à l'eau n'est pas une mort :
retour au point de départ de l'équipe, en pleine santé, avec l'équipement de la variante (conserve la règle de M1 et les tests de franchissement du bassin).
Le joueur sans soldat voit l'écran s'assombrir ; F5 (débogage) recommence la partie.

### D044 — Tab : menu du pad d'abord, sinon changer de soldat
**Statut :** Acceptée
Une seule touche pour deux usages : tant que le menu du pad est ouvert (soldat contrôlé près de son pad, avec des options), Tab déplace la sélection du menu et le jeu ne le voit
pas (`Sim` le retire de l'entrée) ; ailleurs, Tab passe au soldat suivant. Si cela gêne en jeu, une touche dédiée est facile à ajouter (`platform_map_keyboard`).
