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
pour un humain et pour l'IA : l'IA ne peut ni tricher ni diverger des règles. Aucun pathfinding en M4 : une cible hors du sol du soldat est ignorée (complété par D045–D046 : la navigation arrive en M5 sans changer ce principe).

### D042 — Un seul ordre par équipe, appliqué aux soldats non contrôlés
**Statut :** Provisoire
La portée des ordres était à décider (GDD) : l'ordre est celui de l'**équipe** et s'applique à tous ses soldats que le joueur ne contrôle pas ; la touche d'ordre le fait
défiler (suivre → garder → ramasser → attaquer, puis *voler* en M5 : D048). Choisir un ordre par soldat ou par soldat visé reste possible plus tard, sans changer l'IA.

### D043 — Mort définitive, eau = retour à la base
**Statut :** Provisoire (remplace D036)
Un soldat à 0 PV meurt pour de bon (arme et caisse lâchées, contrôle au plus proche allié, équipe éliminée avec le dernier). Tomber à l'eau n'est pas une mort :
retour au point de départ de l'équipe, en pleine santé, avec l'équipement de la variante (conserve la règle de M1 et les tests de franchissement du bassin).
Le joueur sans soldat voit l'écran s'assombrir ; F5 (débogage) recommence la partie.

### D044 — Tab : menu du pad d'abord, sinon changer de soldat
**Statut :** Acceptée
Une seule touche pour deux usages : tant que le menu du pad est ouvert (soldat contrôlé près de son pad, avec des options), Tab déplace la sélection du menu et le jeu ne le voit
pas (`Sim` le retire de l'entrée) ; ailleurs, Tab passe au soldat suivant. Si cela gêne en jeu, une touche dédiée est facile à ajouter (`platform_map_keyboard`).

### D045 — Le graphe de navigation est construit en rejouant la physique
**Statut :** Acceptée
Plutôt que de déduire les sauts d'une formule (qui divergerait dès qu'on touche à `tuning.h`), `nav_build` pose un soldat sur chaque place où l'on peut se tenir et rejoue
`companion_update` avec une dizaine d'entrées canned (marcher, sauter 3 directions × 5 durées d'appui, traverser une plateforme fine, monter / descendre) ; le nœud d'arrivée est la cible de l'arête.
Le graphe ne peut donc pas promettre un mouvement que la physique refuse, et suit le réglage tout seul. Deux garde-fous : un saut doit aboutir au même nœud depuis ±5 px du centre ;
chaque mouvement est simulé aussi **en portant une caisse** (flag `who` de l'arête : plus lent, saut plus bas, pas d'échelle), et A* ne prend que les arêtes valables pour son porteur.
Les caisses posées sont ignorées (le suiveur les saute comme obstacles). Coût : ≈ 14 ms par niveau de test en Release, à la demande.

### D046 — `Nav` dans `GameState`, tableaux de taille fixe, construit à la demande
**Statut :** Acceptée
Pas d'allocation dans la boucle de jeu et pas de fuite à libérer : `Nav` (≈ 146 Ko : 1024 nœuds, 10240 arêtes, mémoire de travail d'A*) est un champ de `GameState`, construit au premier besoin
(ou à `game_set_team_ai`) puis jamais invalidé, car il ne dépend que du terrain. Il n'est pas haché (dérivé du niveau) ; l'état de chaque soldat (`NavAgent` : but, chemin, avancement) l'est. Un niveau trop grand
met `ok = false` avec un avertissement et l'IA retombe sur la marche en ligne droite (`Walker`) : le jeu reste jouable, juste moins malin. `GameState` pèse ≈ 170 Ko.
Conséquence : les tests unitaires M0–M4 n'ont pas changé (rien ne construit le graphe tant qu'aucun soldat IA ne se déplace).

### D047 — Commandant d'équipe : mêmes actions qu'un joueur, rôles par soldat
**Statut :** Provisoire (valeurs)
Le commandant n'a que les actions d'un joueur : `pad_build` (le menu appelle la même fonction) et l'ordre donné aux soldats. Une équipe IA n'a pas d'ordre d'équipe ; chaque soldat reçoit un **rôle**
(`SoldierAi.role`, un `Order`) : *ramasser* 1 à 3 selon l'effectif, *voler* 1 s'il y a des caisses sur un pad adverse, *attaquer* la moitié du reste dès 4 soldats (les lourds toujours), *garder* le reste ;
tout le monde défend quand un ennemi est à moins de 220 px de la base ; qui porte une caisse finit sa livraison. Construction : moins de 3 soldats ⇒ seulement des soldats (un pistolet n'aiderait pas) ;
au-delà, on attend un produit de valeur ≥ 20 (lourd, lance-roquettes) sauf si le pad n'a pas reçu de caisse depuis 15 s ; jamais de véhicule avant M7 ; 8 soldats par équipe au plus.
Les valeurs de produit sont un tableau dans `commander.c` ; les seuils sont dans `tuning.h`. Séparer `Team.ai` + `SoldierAi.role` de `Team.order` évite de mélanger les ordres du joueur et les rôles de l'IA.

### D048 — Nouvel ordre « voler » ; l'IA ne prend pas les caisses des pads (le sien comme les autres) en ramassant
**Statut :** Provisoire
Le vol de caisses sur les pads adverses (jalon M5) est un ordre à part entière, `ORDER_STEAL`, donc aussi à la disposition du joueur (la touche d'ordre en compte maintenant cinq). *Ramasser* n'y touche pas ;
*voler* préfère les caisses d'un pad adverse (dont rien n'est posé dessus et qu'on peut approcher : la case où se tenir ne doit pas être occupée par une autre caisse) puis retombe sur les caisses libres.
Pas de défense dédiée : les gardiens tirent sur ce qu'ils voient, comme pour tout intrus.

### D049 — Une caisse n'est visée que si on sait la prendre ET la rapporter
**Statut :** Acceptée
Les premières parties IA contre IA ont montré deux pièges : un soldat qui pique une caisse qu'il ne peut pas ramener en la portant (échelle, saut trop haut) reste coincé en haut et la repose en boucle ; et un
tas de 3 caisses ou plus dont la dernière est hors de portée depuis le sol bloque le couloir et l'IA s'acharne dessus. Avant de choisir une caisse, l'IA exige donc (1) une place où se tenir à portée de corps
(`crate_stand`, rangée de la caisse ou celle sous elle, case libre) et (2) un chemin **« avec une caisse »** de là jusqu'au pad. Une caisse refusée est exclue 20 s (quatre exclusions mémorisées par soldat).
Les travailleurs (ramasser / voler) ne quittent pas leur tâche pour poursuivre un ennemi à plus de 40 px.

### D050 — Niveau `ai_arena.lvl` pour les parties IA
**Statut :** Acceptée
`test_arena.lvl` n'est pas praticable dans les deux sens (un mur de quatre tuiles bloque le retour vers la base de gauche) : il convient à la mécanique, pas à une partie. `ai_arena.lvl` est symétrique : une colline à marches d'une tuile au centre,
un étage par côté atteint par une échelle, un pad par côté. Les zones de largage sont plafonnées (2 près des bases, 1 sur la colline) car les piles de 3 caisses ou plus sont hors de portée depuis le sol et finissent en mur.
Les parties par défaut de l'exécutable (`ai_opponents`) mettent l'IA sur chaque équipe adverse qui a un point de départ et un pad ; les tests de logique (`Sim` sans cette option) n'ont aucune équipe IA sauf demande.


### D051 — Bases : un bâtiment sur le point de départ, non solide, épargné par les explosions de son équipe
**Statut :** Provisoire (forme, taille, règles de dégâts)
Une équipe qui a un point de départ **et** un pad a une base : un bâtiment de 40 × 40 px posé sur son point de départ, derrière les soldats (il n'est pas solide : on passe devant).
Elle a 1000 PV (`TUNING_BASE_HP`) et c'est une cible comme les autres dans `combat.c` : balles et coups des autres équipes s'arrêtent dessus ; une explosion la touche si elle n'est pas
déclenchée par sa propre équipe (`combat_explode_by(…, équipe)` ; sans équipe, elle touche toutes les bases). Une base ne recule pas. Détruite, elle reste en ruine, n'est plus une cible,
et `pad_usable` devient faux : plus de menu, plus de construction, mais les caisses du pad restent prenables. L'équipe continue avec les soldats qui lui restent. Un pad sans base (niveaux
de test) reste utilisable. Une base touchée garde son équipe à la maison 3 s (le commandant se sent menacé). L'IA ne vise une base que si aucun soldat ni mannequin ennemi n'est en vue,
et l'ordre *attaquer* mène devant la base adverse (3 tuiles avant, côté d'où l'on vient), puis vers son pad une fois détruite.
Statistiques par équipe (`TeamStats` : éliminés, pertes, constructions, dégâts aux bases) pour l'écran de résultats ; un ennemi achevé est crédité à l'équipe qui a porté le coup.

### D052 — Escarmouche : élimination au dernier soldat, résultat verrouillé
**Statut :** Provisoire (en attente des règles multijoueur du jeu d'origine)
Une équipe *joue* dès qu'elle a eu un soldat (`Team.playing`) ; elle est éliminée quand son dernier soldat meurt (l'eau ne tue pas). La partie est finie quand il reste au plus une équipe parmi
au moins deux qui ont joué (seul contre des mannequins, elle ne finit jamais) : la dernière debout gagne, si les dernières tombent ensemble c'est un match nul. Le résultat (`GameState.match` :
fini, gagnant, tick de fin) est verrouillé et haché ; `EV_MATCH_OVER` est émis une fois. La simulation continue derrière : l'écran de résultats arrive 2,5 s plus tard (`TUNING_RESULTS_DELAY_S`)
et la fige. Détruire une base n'élimine pas : c'est le moyen de couper la production adverse.

### D053 — Les écrans (titre, configuration, pause, résultats) dans `app/front.c`, sans SDL
**Statut :** Acceptée
Comme le menu du pad, l'enchaînement des écrans est une machine à états pure qui lit des `InputFrame` (haut / bas : choisir, gauche / droite : régler, Entrée ou Espace : valider, Échap : retour /
pause) et des positions de souris en pixels logiques (`front_menu_rect` donne les rectangles au dessin et au clic). `Front` possède le `Sim` de la partie en cours (gros : il vit en statique dans
`main.c`). Testé sans fenêtre (`test_front`) et pilotable par script en headless (`--front`). Mode fenêtré : écran titre par défaut ; `--play` va droit à une partie avec les options de la
ligne de commande (c'est ce que font les tests et le débogage). Le dessin est dans `render/draw_front.c`. Le plan prévoyait `ui/` et `scenes/` : inutile tant que la logique tient dans `app/`.

### D054 — Police bitmap écrite dans le code
**Statut :** Acceptée
Pas de fichier de police ni de bibliothèque (pas de stb_truetype) : une police 5 × 7 en capitales, chiffres et ponctuation, dessinée pour le projet dans `render/font.c` (une chaîne de `#` par glyphe),
avec les accents du français posés au-dessus (é è ê ë à â ä î ï ô ö ù û ü, cédille dessous). Le texte est en UTF-8 ; les minuscules s'affichent en capitales ; un caractère inconnu donne « ? ».
La partie données / mesure est sans SDL (testable, liée à `cs_app`), le dessin (`ui_text`, un rectangle par pixel allumé, mise à l'échelle entière) est dans `render/draw_ui.c`.
Le rendu active le mélange alpha (`SDL_BLENDMODE_BLEND`) : panneaux et voiles sont translucides.

### D055 — Sons placeholders synthétisés, joués par des `SDL_AudioStream`
**Statut :** Acceptée
Aucun fichier son : chaque son (tir, explosion, caisse, construction, mort, plouf, victoire, menus…) est décrit par 1 à 4 voix (carré, triangle, dent de scie, bruit ; glissement de hauteur,
filtre passe-bas, décroissance) et synthétisé une fois au démarrage (`app/sfx.c`, sans SDL, déterministe). `sfx_consume` transforme les événements d'un tick en « cues » (son, volume, panoramique)
entendus depuis le centre de la caméra : pleine puissance jusqu'à 220 px, silence au-delà de 760 px ; un même son a un écart minimal entre deux lectures (une rafale de coups fait un son).
`audio/audio.c` ouvre le périphérique par défaut et lit sur 12 voix (`SDL_AudioStream` liés au même périphérique, mixés par SDL). Jamais de son en headless ; sous les tests (`--expect-video-driver`),
seulement avec `SDL_AUDIODRIVER=dummy` ; `--mute` coupe tout.

### D056 — Escarmouche à trois soldats ; garde devant la base, pas sur le pad
**Statut :** Provisoire (à valider en jouant)
Avec un seul soldat par équipe, la partie se joue au premier accrochage. Une escarmouche lancée du menu donne **3 soldats** à chaque équipe (`TUNING_SKIRMISH_START_SQUAD` ; `SimConfig.start_squad`,
`--squad N` en ligne de commande, 1 par défaut pour que les tests et `--play` gardent leur départ). Cela a montré un défaut : les gardiens se postaient au centre du pad, or une caisse ne se pose pas
sur une case occupée par un soldat ; le poste de garde (et l'attente d'un ramasseur sans travail, et la fuite) est donc **le point de départ**, devant la base.

### D057 — L'IA vise avec une petite erreur, tirée du RNG du jeu
**Statut :** Provisoire (valeur ; base des futures difficultés)
Une IA parfaite et sans hasard rejouait la même partie quelle que soit la graine (le RNG ne servait qu'à la dispersion de l'uzi et aux zones mixtes), et un petit avantage de côté décidait toujours
du vainqueur. Chaque tick où il vise, un soldat IA décale son point de visée de ±7 px (`TUNING_AI_AIM_ERROR`) avec `gs->rng` : toujours déterministe pour une graine, mais chaque graine donne une partie
différente, et l'IA n'est plus un tireur d'élite.

### D058 — « La Crique », premier niveau soigné, symétrique et vérifié par la navigation
**Statut :** Acceptée
112 × 32 tuiles : une base sur une falaise de chaque côté (avec une vigie atteinte par une échelle), une plage, un chenal d'eau de 2 tuiles enjambé par un petit pont (un porteur de caisse ne saute pas
2 tuiles), et un plateau central à marches d'une tuile où tombent les caisses bleues. Cinq zones de largage plafonnées (rouges aux bases et sur les plages, bleue au centre). `test_nav` vérifie que
toute place est atteignable depuis les deux départs et qu'une caisse peut être portée de chaque zone jusqu'aux deux pads, et y fait des trajets aléatoires. C'est le niveau proposé en premier par le menu.
L'habillage du terrain (herbe, terre qui fonce en profondeur, planches, eau animée, ciel du soir, collines en parallaxe) est dessiné pour tous les niveaux par `draw_game.c`.

### D059 — Musique : un petit séquenceur chiptune écrit dans le code
**Statut :** Acceptée (les morceaux sont des placeholders)
Comme les sons (D055), la musique ne vient d'aucun fichier et d'aucune bibliothèque : `app/music.c` (sans SDL) contient quatre morceaux originaux écrits en texte (accords par mesure, mélodie à la
double-croche, plan de batterie) et un synthétiseur à quatre canaux façon console 8 bits (mélodie en onde pulsée, accompagnement tiré des accords — arpège ou accords à contretemps —, basse en
triangle, batterie en bruit et sinus). Chaque morceau a une intro jouée une fois puis une boucle. `music_render` est déterministe et n'alloue rien : SDL l'appelle sur son thread audio par un
callback de flux (`SDL_SetAudioStreamGetCallback`) ; le thread principal ne fait que demander un morceau et un volume sous le verrou du flux. Changer de morceau fait un fondu de 0,5 s. Le choix du
morceau vient des écrans (`front_music`, testé) : titre et configuration → thème du titre ; match → morceau du niveau (baissé en pause) ; fin du match → silence pour la fanfare ; résultats →
morceau calme. `--export-audio DIR` écrit tous les sons et morceaux en WAV pour les écouter sans lancer le jeu.

### D060 — Les pas (sauts, atterrissages) : seulement ceux du soldat suivi
**Statut :** Acceptée
Les événements `EV_JUMP` et `EV_LANDED` (au-dessus de 160 px/s de chute) sont émis pour tous les soldats, mais les entendre tous noyait le combat : sur une minute d'IA contre IA, ils faisaient
60 % des sons. Seuls ceux du soldat que suit la caméra jouent (`SfxState.focus_soldier`) ; `test_sfx` vérifie qu'une partie reste lisible (pas plus de 30 sons dans la seconde la plus chargée).

### D061 — Une escarmouche commence à 1 soldat par équipe, réglable de 1 à 5
**Statut :** Acceptée (demande de l'utilisateur ; remplace l'effectif imposé de D056)
L'écran de configuration a une entrée « ÉQUIPES » (1 à 5 soldats, `TUNING_SKIRMISH_START_SQUAD` = 1 par défaut, `TUNING_SKIRMISH_MAX_SQUAD` = 5). Le reste de D056 tient : le poste de garde
est le point de départ.

### D062 — Plus de caisses : chutes fréquentes plafonnées, caisses de départ, étalement
**Statut :** Acceptée (valeurs provisoires)
Demande de l'utilisateur : des caisses plus souvent, avec une limite par zone, et des caisses déjà là au début. Les intervalles de La Crique et de L'Arène passent à 3-4 s (rouges) et 7 s
(bleues), les plafonds à 2-3. Nouvelle clé de niveau `start.N=COUNT` (0..plafond) : `crates_place_start` pose ces caisses au repos sous la zone (puis une colonne à droite, une à gauche…)
en évitant l'eau, les échelles, les pads et les bases. Plus de caisses, c'était aussi des tours de 3 sur la vigie, que personne n'atteint : une caisse qui tomberait sur une autre (posée
ou encore en l'air dans la colonne) descend dans une colonne voisine au sol libre (tirage dans le RNG du jeu, donc déterministe). Le porteur va aussi plus vite (×0,9).

### D063 — Sprites : du pixel art écrit en texte dans le code, un atlas par équipe
**Statut :** Acceptée
Comme la police (D054), les sprites ne viennent d'aucun fichier ni d'aucune bibliothèque (pas besoin de `stb_image`) : `render/sprites.c` (sans SDL) les décrit en texte, un caractère
par pixel et par couleur d'une petite palette (dont la couleur d'équipe et ses nuances). Les soldats sont assemblés à partir de parties (tête, torse, jambes par pose) : 4 variantes × 10
poses sans écrire 40 images. Les bras sont des sprites à part, tournés pour viser ou porter. `sprites_build_atlas` remplit un atlas RGBA par palette (4 équipes + une silhouette blanche
pour le clignotement des coups) ; `render.c` en fait des textures au démarrage et `render_sprite` dessine un sprite par son point de pivot, avec rotation, miroir, échelle et
transparence (y compris en headless, par le rendu logiciel de SDL). `--sprite-sheet PATH` exporte les atlas agrandis pour les regarder.

### D064 — Particules, animation des soldats et tremblement d'écran dans `app/fx`
**Statut :** Acceptée
Tout ce qui est visuel et que le jeu ne suit pas reste dans `FxList` (sans SDL, hors du hachage) : effets courts, jusqu'à 640 particules (points, fumée, étincelles, débris qui rebondissent
sur le terrain, étoiles, ondes de choc, soldat éliminé qui tournoie), l'état d'animation de chaque soldat (distance parcourue pour le cycle de course, distance grimpée, écrasement après
un atterrissage) et l'amplitude du tremblement (atténuée loin de la caméra). Les particules tirent leur hasard d'un RNG à elles, jamais de celui du jeu. Le jeu émet `EV_JUMP` /
`EV_LANDED` et `EV_PROJECTILE_HIT` dit s'il a touché un mur ; le reste vient des événements existants et d'un coup d'œil à l'état (roquettes en vol, soldat qui vient de mourir).

### D065 — Tileset et décor dans l'atlas des sprites
**Statut :** Acceptée
Le terrain, le décor (touffes, fleurs, cailloux), la base, le drapeau, le mannequin, les tanks et la mallette rejoignent les sprites en texte de `render/sprites.c` plutôt qu'un
fichier image : un seul format, validé par les tests, aucune dépendance de chargement. Les tuiles n'utilisent que des couleurs fixes (identiques dans chaque atlas), la base et le
drapeau prennent celles de l'équipe. Les sprites de plus de 32 px (la base, 40 × 40) occupent 2 × 2 cases, rangées après les autres. Une tuile peut en superposer une autre
(herbe sur terre) et changer des couleurs (pâquerette = fleur en blanc). La profondeur assombrit la terre par `SpriteDraw.darken` (modulation de couleur de la texture) plus un demi-pas
sur la moitié basse, pour des dégradés sans bandes nettes ; arêtes, débord de l'herbe, eau et écume restent tracés au dessin car ils dépendent des voisins.

### D066 — Tir maintenu, touche E, cadres blancs, détruire une caisse
**Statut :** Acceptée
- **Tir maintenu** : `combat_soldier_update` lit `input_down(ACT_FIRE)` pour toutes les armes ; le champ `WeaponDef.automatic` disparaît (il n'y a plus d'arme « un tir par appui ») et l'IA
  tient la touche au lieu d'alterner. Les délais passent à poings 0,25 s, pistolet 0,2 s, grenades 0,5 s, roquettes 0,55 s. Lancer une caisse impose `TUNING_THROW_RECOVERY_S` (0,3 s)
  sans tir, sinon la touche encore tenue tirerait aussitôt. Conséquence : les IA, qui utilisent la même table, tirent plus vite elles aussi.
- **Touche d'action sur E** (clavier physique, identique en QWERTY et en AZERTY) ; C ne fait plus rien. Remplace la touche de D025.
- **Cadres blancs** : `crate_pickup_target` et `crate_place_target` (dans `game/crate.c`, qui partage `find_pickup` / `find_place` avec les vraies actions) disent ce qu'un appui ferait ;
  `draw_game` les dessine pour le joueur humain seulement. Le rendu ne refait donc aucun calcul de portée.
- **Détruire** : action `ACT_DESTROY` (bouton droit de la souris, lue par `sim_set_mouse(…, right_down)` ; lettre `B` des scripts), traitée avec les autres actions de caisse
  dans `crate_carry_update`. `crate_remove` distingue maintenant détruite / consommée / récupérée ; la récupération émet `EV_CRATE_SALVAGED` (`a` = `SalvageReward`, `b` = quantité) et
  tire dans le RNG du jeu, donc reste déterministe et rejouable. Pas de touche clavier : à la manette (M8) un bouton lui sera donné.
- **Écrasement des caisses** : `fx_observe` repère les passages d'un état en vol (ou porté) à posé, par case du pool, sans nouvel événement du jeu ; le pivot du sprite de caisse passe
  en bas pour que l'écrasement reste au sol.

