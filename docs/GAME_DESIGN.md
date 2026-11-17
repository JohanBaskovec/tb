# Game Design Document

> Statut : **brouillon**, mis à jour le 2026-11-17 avec les recettes du pad fournies par l'utilisateur.
> Légende : ✅ confirmé par l'utilisateur · 🟡 proposition à valider · ❓ question ouverte.

## Principes

- **Inspiré de, pas copié.** On reprend la *boucle de jeu* de *Team Buddies*
  (caisses → empilement → armes/compagnons/véhicules → combat d'escouades).
  Le nom, l'univers, les personnages, l'humour, les graphismes, les sons et les niveaux sont originaux.
- **Demake 2D vue de côté** : lisibilité d'un jeu 16-bit, contrôles nerveux de jeu de plateforme,
  parties courtes (5–10 min).
- **Jouable seul contre l'IA d'abord**, multijoueur local en écran partagé ensuite.

## 1. Référence : ce que je sais de *Team Buddies*

- Jeu PS1 de Psygnosis (2000), action/stratégie en 3D, caméra au-dessus du terrain.
- Des équipes de petits soldats (jusqu'à 4 équipes de couleurs différentes) s'affrontent.
- Des **caisses** tombent du ciel sur la carte. Il en existe de deux types : **rouges** (standard)
  et **bleues** (plus fortes). Les soldats les ramassent, les portent et les rapportent sur le
  **pad d'empilement** de leur base.
- Selon la **forme** des caisses empilées et la présence de caisses bleues, on obtient une **arme**,
  un **compagnon** (nouveau soldat) ou un **véhicule** (recettes confirmées, voir § 6).
- Le joueur contrôle **un soldat à la fois**, peut **passer d'un soldat à l'autre**,
  et donne des **ordres simples** aux autres.
- Les ennemis peuvent **voler les caisses** sur votre pad ou en chemin.
- Détruire la base d'une équipe l'empêche d'utiliser son pad.
- Mode campagne avec des missions à objectifs variés ; mode multijoueur jusqu'à 4 joueurs en écran partagé.
- Ton très humoristique et irrévérencieux.

### Suivi des questions sur le jeu d'origine

| # | Sujet | Statut |
|---|-------|--------|
| 1 | Combinaisons du pad | ✅ fournies (§ 6) |
| 2 | Classes de soldats | ✅ quatre variantes de compagnon (§ 4) ; PV 100 / 150 / 200 confirmés, la 4ᵉ est provisoire |
| 3 | Armes et véhicules | ✅ liste suffisante « pour l'instant » : pistolet, uzi, grenades, lance-roquettes, tank |
| 4 | Ordres de l'escouade | ✅ suivre, garder la base, ramasser, attaquer |
| 5 | Chutes de caisses | ✅ dépend de la carte (§ 5) |
| 6 | Bases / victoire | ◐ base de 1000 PV, destructible par toute arme ; détruite = pad inutilisable, l'équipe continue ; conditions de victoire multijoueur encore ❓ |
| 7 | Missions de campagne | ⏳ à voir mission par mission, plus tard |
| 8 | Autres mécaniques importantes pour le « feeling » | ❓ ouvert, à compléter au fil du jeu |

### Questions résolues ✅

1. **Règle « bleue »** : une seule caisse bleue dans la forme suffit pour la version puissante. Le 2×2 avec une bleue donne
   un compagnon **avec armure lourde** (encore plus de PV que le compagnon lourd).
2. **Menu du pad** : une construction est proposée dès que sa forme est présente parmi les caisses ; si elle n'en utilise qu'une
   partie, on consomme celles du haut d'abord ; « cliquer » = clic souris (sélection + validation au clavier / manette).
   Un pad non vide propose **toujours au moins une construction**, puisqu'une caisse seule est un 1×1.
3. **Forme en L** : 2 caisses côte à côte en bas + 1 sur l'une des deux, dans les deux orientations.
4. **Bases** : 1000 points de vie, destructibles avec n'importe quelle arme.
5. **PV** : 100 (compagnon), 150, 200, du plus faible au plus fort.
6. **Munitions** : dépendent de l'arme ; valeurs provisoires ci-dessous (§ 7), à ajuster en jouant.

### ❓ Questions encore ouvertes

1. **Quel PV pour quelle variante ?** Tu as donné 100, 150, 200 « du plus faible au plus fort » pour 4 variantes
   (compagnon, fort, lourd, lourd blindé). J'ai mis 100 / 150 / 200 / **300 (provisoire)** dans cet ordre. Correct ?
2. **Armement de départ du compagnon lourd blindé** : lance-roquettes comme le lourd ? (hypothèse retenue)
3. **Tank / tank lourd** : PV et armement (valeurs provisoires dans `tuning.h` le moment venu).

## 2. Pitch de la version 2D 🟡

Une petite île découpée en plateformes, vue de côté. Des caisses rouges et bleues descendent lentement du ciel.
Chaque équipe a un camp avec un **pad d'empilement**. Ramasser, porter, empiler, transformer,
se battre, voler les caisses des autres, détruire la base adverse. Gagner en éliminant toutes
les escouades adverses (ou selon l'objectif de la mission).

## 3. Caméra et écran 🟡

- Résolution logique **640×360**, mise à l'échelle entière (×2 = 720p, ×3 = 1080p), letterbox si besoin.
- Tuiles de **16×16 px**. Soldats ≈ 16×24 px. Caisses 16×16 px (1 tuile).
- Niveaux plus larges que l'écran (typiquement 120–300 tuiles de large, 30–60 de haut).
- Caméra qui suit le soldat contrôlé avec une zone morte et une légère anticipation dans la direction du regard.
- Écran partagé : 2 joueurs = deux bandes horizontales 640×180 ; 3–4 joueurs = quadrants 320×180.

## 4. Les soldats (« compagnons »)

### Déplacement 🟡
- Marcher, sauter (hauteur variable selon durée d'appui, *coyote time* et *jump buffer*),
  tomber à travers les plateformes fines (bas + saut), grimper aux échelles.
- **Porter une caisse** : la caisse est tenue au-dessus de la tête ; vitesse réduite, saut réduit,
  impossible de tirer. On peut la **poser** devant soi ou la **lancer** en cloche.
- Prendre un coup en portant une caisse la fait tomber.

### Santé et mort 🟡
- ✅ **100 points de vie** pour le compagnon de base ; 150 / 200 pour les variantes plus fortes (§ 4). 🟡 Recul quand on est touché.
- Un soldat mort lâche son arme et sa caisse. Pas de réapparition : l'équipe
  grossit uniquement via le pad.
- Une équipe sans soldat vivant est éliminée.

### Contrôle de l'escouade
- 🟡 Le joueur contrôle un soldat ; touche « changer » pour passer au suivant (cycle), avec un indicateur au-dessus du soldat actif.
- 🟡 Si le soldat contrôlé meurt, le contrôle passe automatiquement au plus proche.
- ✅ Ordres (liste confirmée ; portée — tous les soldats ou le soldat visé — encore à décider 🟡) :
  - **Suivre** : rester près du soldat contrôlé et tirer sur les ennemis à portée.
  - **Garder la base** : défendre le pad.
  - **Ramasser** : aller chercher les caisses et les rapporter.
  - **Attaquer** : aller vers la base adverse la plus proche.

### Variantes de compagnon ✅ (stats 🟡)
| Variante | Obtenu avec | Particularité |
|----------|-------------|---------------|
| Compagnon | 2×1 vertical, rouge | **100 PV** ✅. Soldat de base, arme de départ : poings 🟡 |
| Compagnon fort | 2×1 vertical, avec bleue | **150 PV** ✅ |
| Compagnon lourd | 2×2, rouge | **200 PV** ✅. Apparaît **avec un lance-roquettes** |
| Compagnon lourd blindé | 2×2, avec bleue | **Armure lourde, encore plus de PV** ✅ : 300 provisoire 🟡. Lance-roquettes 🟡 |

## 5. Les caisses ✅

- **Deux types** : **rouge** (standard) et **bleue** (plus forte). Visuellement très distinctes.
  Ajouter une seule bleue à une forme donne la version plus puissante du résultat (§ 6).
- Elles **atterrissent lentement du ciel** (descente visible de loin ; le parachute est un choix de rendu 🟡),
  puis deviennent des objets physiques au sol.
- **Le largage dépend de la carte.** Chaque zone de largage d'un niveau définit : sa position, la couleur
  des caisses (rouge / bleu / mixte), sa cadence et un plafond de caisses libres. Exemples voulus :
  - une seule zone de largage sur la carte ;
  - une zone de caisses **rouges** à côté de chaque base + une zone **centrale** qui ne largue que des caisses **bleues**
    (zone contestée, donc point stratégique).
- Une caisse libre est un corps soumis à la gravité ; elle repose sur le sol ou sur d'autres caisses.
- Plus tard 🟡 : caisses spéciales (piégée, dorée…).

## 6. Le pad d'empilement ✅ (recettes) / 🟡 (mécanique)

Transposition 2D du pad 3D : une grille **4 colonnes × 2 rangées** posée au sol du camp.

### Recettes ✅

| Forme (vue de côté) | Caisses | Rouge seulement | Avec ≥ 1 caisse bleue |
|---------------------|:-------:|-----------------|-----------------------|
| 1×1 | 1 | Pistolet | Uzi |
| 2×1 horizontal (côte à côte) | 2 | Grenades | Grenades plus puissantes (explosion plus grande) |
| 2×1 vertical (empilées) | 2 | Compagnon | Compagnon fort |
| Forme en L | 3 | Uzi | Lance-roquettes |
| 2×2 | 4 | Compagnon lourd (lance-roquettes, 200 PV) | Compagnon lourd blindé (armure lourde, encore plus de PV) |
| 4×2 (pad plein) | 8 | Tank | Tank lourd |

Forme en L en 2D : 2 caisses côte à côte en bas + 1 sur l'une des deux (2 orientations) ✅.
Les formes peuvent se situer n'importe où sur les 4 colonnes 🟡.

### Mécanique
- 🟡 Une caisse posée ou lâchée au-dessus du pad se **cale** dans la colonne la plus proche
  (gravité dans la colonne : rangée du bas d'abord).
- ✅ **Menu de construction** : quand un soldat s'approche du pad, les constructions possibles avec les caisses
  présentes s'affichent **à côté du pad** (icône + nom). Pour construire, le joueur **clique dessus**
  (sélection + validation au clavier / à la manette 🟡) ; cela **consomme les caisses, en commençant par celles du haut**.
  ✅ Une construction est proposée dès que sa forme est présente parmi les caisses du pad (même s'il y en a d'autres) ;
  la version bleue dépend des caisses réellement consommées. ✅ Un pad non vide propose toujours au moins un 1×1 ;
  un pad vide ne propose rien.
- 🟡 Il n'y a **aucune construction automatique** (pas de compte à rebours) : on empile librement, puis on choisit.
- 🟡 L'objet produit apparaît à côté du pad : une arme se ramasse, un compagnon rejoint l'escouade,
  un véhicule attend un pilote.
- 🟡 Les caisses d'un pad peuvent être **reprises par n'importe qui**, y compris un ennemi (vol).
- L'IA construit via la même fonction que le menu (aucune triche, mêmes recettes).

### Bases
- ✅ **Une base détruite ne peut plus utiliser son pad** ; l'équipe peut continuer à jouer avec les soldats qui lui restent
  (elle ne peut plus produire d'armes, de compagnons ni de véhicules).
- ✅ Une base a **1000 points de vie** et est **destructible avec n'importe quelle arme**.

## 7. Armes ✅

✅ **Chaque arme a un nombre de munitions limité** (valeurs 🟡 provisoires, dans `tuning.h` le moment venu).
🟡 Un soldat porte une arme à la fois ; une arme vide disparaît.

| Arme | Obtenue avec | Tir | Munitions 🟡 | Notes |
|------|--------------|-----|:------------:|-------|
| Pistolet | 1×1 rouge | Balle droite | 24 | |
| Uzi | 1×1 bleue, L rouge | Rafale | 60 | |
| Grenades | 2×1 horizontal rouge | Lancer en cloche, explosion | 6 | Dégâts de zone |
| Grenades puissantes | 2×1 horizontal avec bleue | Idem | 4 | Explosion plus grande |
| Lance-roquettes | L avec bleue ; équipement des compagnons lourds | Roquette droite, explosion | 5 | Dégâts de zone |

🟡 Corps à corps à mains nues (infini, faible) comme arme de repli.

Idées non confirmées, **non planifiées** : fusil à pompe, mitraillette, mine, lance-flammes, mortier, tourelle, frappe aérienne.

## 8. Véhicules ✅

| Véhicule | Obtenu avec |
|----------|-------------|
| Tank | 4×2 rouge |
| Tank lourd | 4×2 avec bleue |

- 🟡 Monter / descendre avec la touche d'action. Le véhicule a ses propres PV ; détruit, il éjecte le pilote.
- 🟡 Un véhicule ne peut pas porter de caisses.
- Armement et PV du tank / tank lourd : à définir 🟡.

Idées non confirmées, **non planifiées** : jeep, mini-hélico.

## 9. Niveaux 🟡

- Terrain en tuiles : sol plein, plateformes fines traversables par dessous, échelles, eau/vide mortel.
- Éléments placés : camps (pad + point d'apparition) par équipe, **zones de largage configurables
  (couleur, cadence, plafond)**, décor.
- Formes typiques : symétrique gauche/droite pour 2 équipes ; plusieurs étages pour 3–4 équipes.
- Décor destructible : hors périmètre initial, à envisager après le jalon « Contenu ».

## 10. Modes de jeu 🟡

1. **Escarmouche** (priorité) : 2 à 4 équipes, joueurs humains et/ou IA, victoire par élimination.
2. **Campagne** : suite de missions avec objectifs **propres à chaque mission** ✅ (à définir mission par mission, plus tard).
3. Variantes plus tard : « roi des caisses » (le plus de caisses en fin de chrono), capture de drapeau.

## 11. IA 🟡

- Chaque soldat IA a un état (`idle`, `suivre`, `chercher caisse`, `rapporter`, `défendre`, `attaquer`, `fuir`).
- Une équipe IA a un « commandant » qui répartit les rôles selon la situation (nombre de caisses libres, menace sur la base, effectifs).
- L'IA sait viser des formes du pad (ex. aller chercher les caisses bleues de la zone centrale pour des recettes puissantes).
- Plusieurs niveaux de difficulté : temps de réaction, précision, agressivité.

## 12. HUD 🟡

- Par joueur : PV et arme/munitions du soldat actif, nombre de soldats de l'équipe, ordre courant.
- Indicateurs hors écran : direction de la base et des caisses libres proches (couleur rouge / bleue distincte).
- Menu de construction affiché à côté du pad à l'approche d'un soldat (§ 6) ; constructions irréalisables absentes ou grisées 🟡.

## 13. Direction artistique et audio 🟡

- Pixel art original, palette limitée et vive ; une couleur forte par équipe (bleu, rouge, vert, jaune).
  Attention : les caisses rouges/bleues ne doivent pas être confondues avec les couleurs d'équipe
  (forme ou motif distinct pour les caisses).
- Personnages à inventer (forme, nom de l'espèce, ton humoristique).
- Placeholders au début : rectangles colorés + police bitmap.
- Sons courts et cartoon ; musique chiptune originale.

## 14. Hors périmètre (pour l'instant)

Multijoueur en ligne, éditeur de niveaux intégré, portage console, doublage.
