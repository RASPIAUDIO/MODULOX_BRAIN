# Modulox Sine

Prototype ESP-IDF pour le Modulox Brain (ESP32-S3 et WM8960). La sortie casque
joue à 44,1 kHz avec 1 à 8 voix. Les anciens patches s’ouvrent à une voix.
L’oscillateur interpole ses tables d’ondes en SRAM interne. La vélocité
détermine le niveau de chaque note.

La tâche audio et le pilote I²S tournent sur le cœur 1. La pile et la lecture
MIDI USB et MIDI physique, les commandes et l’affichage tournent sur le cœur 0.
Le MIDI transmet les notes par une file bornée. L'audio lit les paramètres modifiés à chaque bloc actif.

## Compiler dans VS Code

Ouvrir le dossier `firmware` comme dossier de travail, puis appuyer sur
`Ctrl+Maj+B` pour compiler. Les mises à jour normales passent ensuite par
l’éditeur USB ; la tâche ne flashe plus automatiquement le bootloader, les
presets d’usine ou les métadonnées de démarrage.
La tâche utilise ESP-IDF 5.4.1 et son environnement Python 3.11.
Le script prend aussi en charge les variables `IDF_PATH` et
`IDF_PYTHON_ENV_PATH` si elles sont déjà définies. Le premier build télécharge
les dépendances TinyUSB.

En PowerShell, depuis ce dossier, les mêmes commandes sont disponibles.

```powershell
powershell -ExecutionPolicy Bypass -File .\idf.ps1 build
python .\tools\release.py
```

Le fonctionnement A/B, la migration initiale, la fabrication et la procédure
de récupération sont décrits dans [UPDATES.md](UPDATES.md). La commande brute
`idf.py flash` reste réservée à la fabrication initiale : elle écrit aussi
le firmware factory, les exemples d’usine et les métadonnées OTA.

## Essayer sur la carte

L'écran affiche le module actif, la valeur sélectionnée et les noms de ses
paramètres en bas. **Osc** affiche la forme d’onde sur la page **Wave / PWM**.
Le PWM règle le rapport cyclique de Square de 1 à 99 %. En allant à droite
après PWM, la deuxième page présente **Pitch / Fine / Level**. Pitch
transpose de −24 à +24 demi-tons et Fine ajuste de −100 à +100 cents.
Les patches avec un réglage Octave sont convertis en demi-tons au chargement.
Un patch peut proposer 125 formes utilisateur en plus des trois formes
internes. La page Wave affiche leur nom et leur courbe. Le contenu PCM
est chargé en SRAM au changement de Wave, à raison d’un seul cycle actif
par oscillateur. Le moteur n’accède pas à la flash pendant son rendu.
**ADSR** propose Attack, Decay, Sustain et Release. Les modules déclarent les
libellés, les unités, les valeurs initiales, les limites et les pas de réglage.

**Filter** suit l'ADSR dans le menu et se place entre l'oscillateur et
l'enveloppe dans la chaîne audio. Il propose **Bypass**, **Low-pass**,
**High-pass** et **Band-pass**, avec **2 poles** ou **4 poles**, une fréquence
de 20 Hz à 18 kHz et un réglage de résonance. En passe-bande, Cutoff règle la
fréquence centrale et Reso resserre la bande. En passe-bas et passe-haut, Reso
accentue la réponse autour de la coupure.

En 1.2, une courbe montre cutoff, résonance et pente, avec le mode et l'ordre
indiqués en petit. Les captures de coefficients suivent les modulations à
10 Hz sur une voix active. La courbe revient aux réglages de base sans voix
active ou à partir de 80 % de charge audio. Les calculs graphiques restent sur
le cœur d'affichage ; [RELEASE-1.2.md](RELEASE-1.2.md) détaille le budget et les tests.

Le filtre utilise les biquads ESP-DSP optimisés pour le S3, sur un seul bloc
mono dans le graphe (utiliser Mono pour convertir une branche stéréo). Le cœur 0 calcule les coefficients des réglages
manuels. Avec un LFO relié à Cutoff, le cœur audio calcule les coefficients
par voix tous les quatre blocs et les interpole. Chaque voix garde son
propre état de filtrage. Le bypass évite le traitement du filtre.

Le pourcentage en haut à droite indique la part moyenne du budget d'un bloc
utilisée par le traitement des notes et le rendu audio sur le cœur 1.
L'attente I²S est exclue. La valeur est affichée au dixième et vérifiée une
fois par seconde. Pour comparer deux réglages, maintenir une note le temps
que la mesure se stabilise.

- Les boutons extérieurs sous l'écran parcourent les paramètres, puis passent
  au module suivant.
- L'encodeur modifie le paramètre en surbrillance.
- Sur un paramètre, le bouton du milieu arme **MIDI Learn**. Bouger un
  contrôleur CC USB ou DIN pour l’affecter au paramètre. Un second appui
  annule l’écoute. Les CC suivent le canal MIDI sélectionné et utilisent
  la plage du paramètre. **Save patch** mémorise les affectations.
- Le bouton Menu à gauche ouvre ou ferme la liste des modules. L'encodeur
  ou les boutons extérieurs parcourent la liste. Le bouton du milieu valide.
  Les modules de même type sont regroupés (Osc, Osc 2, Osc 3, puis les autres
  familles). Les menus affichent cinq lignes ; les pages de paramètres restent
  inchangées.
- Dans **Menu → Settings**, **Voices** règle la polyphonie, **Volume** pilote
  la sortie casque du WM8960 et **MIDI Ch** sélectionne Omni ou un canal 1 à
  16 pour les deux entrées MIDI. À 0 %, la sortie casque est coupée. À 77 %,
  le gain est celui de l’ancien firmware.

## Patches

Appuyer sur l'encodeur pour ouvrir **Categories**, tourner pour choisir une
catégorie, puis valider avec le bouton central sous l'écran. Tourner pour
choisir le patch dans cette catégorie et valider avec le bouton central pour
le charger. Depuis les patches, un appui sur l'encodeur revient aux catégories ;
depuis les catégories, il referme le menu. Le chargement coupe les notes en cours.

**User** apparaît toujours en premier. Les autres catégories n'apparaissent que
si elles contiennent des patches : Bass, Lead, Pads, Keys, Pluck, Arp, FX, Drums.
Les anciens patches sans champ `category` vont dans User. L'éditeur permet de
choisir la catégorie dans **Réglages du patch**, puis de sauvegarder ou renvoyer
le patch. Le champ JSON optionnel accepte `user`, `bass`, `lead`, `pad`, `keys`,
`pluck`, `arp`, `fx`, `drums`. La sauvegarde depuis l'appareil conserve ce champ.

Dans **Menu → Settings**, aller à gauche depuis **Voices** (ou à droite
après **MIDI Ch**) pour afficher **Save patch**. Valider avec le bouton central
pour enregistrer les paramètres et réglages du patch actif. L’astérisque
indique une modification effectuée depuis le dernier chargement ou la
dernière sauvegarde. Si le même patch a été remplacé par USB entre-temps,
**Reload patch** invite à le recharger avant de sauvegarder. Une sauvegarde
d’un patch d’usine écrit sa version personnalisée dans la partition utilisateur.
Sans sauvegarde, recharger un patch rétablit les valeurs enregistrées.

- **Basic** reprend la chaîne Osc → Filter → ADSR.
- **Triple Osc** somme trois oscillateurs dans Mix, puis traverse le même
  filtre et la même enveloppe. Mix règle le niveau de chaque entrée et
  normalise la somme par trois pour conserver de la marge.

Le moteur prépare le plan audio au chargement, puis exécute les modules
dans l'ordre des dépendances sur le cœur 1. Il garde les chemins optimisés
pour les deux chaînes ci-dessus. Les graphes personnalisés acceptent trois
Osc, un Filter, un ADSR d’amplitude, un Mix, trois LFO, deux enveloppes Env,
un MIDI, une Disto et un Delay au maximum, avec une seule Sortie
et sans boucle audio. Chaque module doit contribuer à la Sortie. Les entrées inutilisées
du Mix sont silencieuses et ses gains restent normalisés par trois.

**Disto** peut être placée dans la chaîne audio. Elle propose Soft, Hard,
Drive et Mix. **Delay** traite la somme des voix, avec jusqu’à deux secondes
par canal. Il peut être suivi d’autres traitements stéréo. Ses tampons
sont réservés en RAM avant le chargement d’un patch qui l’utilise.

Les fichiers JSON d'usine dans `patches/` sont installés en SPIFFS lors
d'un flash standard. Les patches envoyés par USB sont enregistrés dans la
partition NVS `userpatches`, que le flash standard conserve. Un effacement
complet de la flash efface aussi les patches utilisateur. Le moteur applique
les réglages et le plan en RAM au chargement. Les tables d'ondes sont
partagées entre les oscillateurs.

Le port série USB de l'application utilise DTR actif. Envoyer une commande
ASCII terminée par LF et lire la réponse avant d'envoyer la suivante.

- `LIST` renvoie `OK <nombre>\n`, puis une ligne `<index> <fichier>\n`
  par patch, avec des index commençant à zéro.
- `GET <index>` et `CATALOG` renvoient `OK <octets>\n` suivi d'un JSON.
  Le catalogue fournit les ports, les limites d'instances et les paramètres.
- `PUT <clé> <octets>` attend une clé utilisateur formée de `u` suivi de
  13 chiffres hexadécimaux minuscules. Après `READY\n`, envoyer exactement
  le nombre d'octets UTF-8 annoncé (4096 maximum). La réponse `OK <index>\n`
  confirme l'écriture en flash. Une même clé remplace le patch utilisateur.
  La banque contient jusqu'à 64 patches, exemples d'usine compris, dans la
  limite des 128 Kio de NVS utilisateur. Le catalogue annonce `maxPatches: 64`
  et `patchCategories: true`. Les samples et tables d'onde occupent leurs propres
  partitions. La RAM contient seulement l'index de la banque et le patch actif.
- Un échec renvoie `ERR\n` ou `ERR <cause>\n`.

Les cycles utilisateur sont stockés dans la partition NVS `waves` de
640 Kio. Chaque identifiant est le préfixe de 14 caractères hexadécimaux
du SHA-256 des 2 048 octets PCM little-endian. Le tableau JSON `waves`
fixe l’ordre des cycles, avec `Wave` égal à 3 pour le premier cycle
utilisateur. L’USB expose `WHAS <id>`, `WGET <id>` et `WPUT <id>`.
Le transfert binaire utilise 32 octets pour le nom terminé par NUL, puis
2 048 octets PCM. Si la partition est pleine, l’éditeur demande `WGC`
pour récupérer la place des cycles sans référence dans les patches
enregistrés et dans le patch en cours.

Les formats JSON versions 2 et 3 décrivent chaque câble par `[source, cible, entrée]`.
L'entrée est numérotée à partir de zéro, et `out` désigne la sortie matérielle.
Les réglages absents utilisent la valeur initiale du module. Les fichiers
d'usine au format version 1 restent lisibles. La version 3 impose les
formats mono/stéréo des ports et une sortie stéréo explicite.

L’éditeur relie le **LFO** aux entrées de modulation bleues. Le LFO est
partagé entre les voix et repart de la phase zéro à chaque Note On accepté.
Il propose Sine, Square, Triangle, une fréquence de
0,05 à 20 Hz et une profondeur bipolaire de −100 à +100 %. **Level** décale
la modulation de −100 à +100 % autour de la valeur de base. Le moteur borne
les extrémités du balayage avant d’appliquer la forme d’onde. La page LFO
affiche la courbe sur une à huit périodes et la valeur du paramètre
sélectionné. Le format
`modulations` utilise `[source, cible, nom de l’entrée]` et `settings`
contient les réglages du patch. Le LFO peut cibler Pitch et Level sur la
Sortie, Pitch, Level et PWM par Osc, ainsi que Cutoff et Reso sur Filter.
Avec un LFO relié à Cutoff, la fréquence de base correspond au do central
(MIDI 60), puis suit la note de chaque voix à raison d’une octave par octave
jouée. À 25 % de Depth, le LFO balaie une octave de chaque côté du centre
tant qu’il reste entre 20 Hz et 18 kHz. Level décale le centre d’une octave
par 25 points. Les patches existants qui modulent Cutoff adoptent ce balayage.
Le Level de Sortie agit sur le signal audio. Le réglage Volume agit sur
le codec. Le Pitch global et individuel ajoutent chacun jusqu’à ±200 cents.

Dans l’éditeur, les ports MIDI violets relient des sorties et des entrées
de même nom. En présence du module **MIDI**, connecter Pitch à chaque
oscillateur qui doit suivre le clavier et Velocity à chaque oscillateur
dont le volume doit suivre la vélocité. Sans connexion Pitch, l’oscillateur
part du do central (MIDI 60). L’entrée Trigger force la remise à zéro de
sa phase. Sur un LFO, Pitch règle la vitesse autour du do central,
Velocity multiplie Depth et Trigger relance le cycle. Une enveloppe **Env**
produit un signal de modulation par voix. Elle se déclenche sur les notes
en câblage implicite, ou par Gate/Trigger si le module MIDI est présent.
Les patches sans module MIDI conservent leur comportement de clavier et
de redéclenchement des LFO.

Les câbles MIDI sont stockés dans `modulations` avec un quatrième champ
qui donne la sortie MIDI. Les affectations apprises prennent la forme
`midiLearn: [[id, paramètre, numéroCC]]`. L’identifiant `$settings`
désigne les réglages du patch.

Le port série de l'application est distinct du port de programmation
affiché par le bootloader. L'interface USB MIDI reste disponible en parallèle.

1. Brancher le Modulox et choisir son port de programmation à la place de
   `COMx`. Si nécessaire, mettre la carte en mode bootloader pour faire
   apparaître le port. Le flash remplace le firmware en cours.
2. Après le redémarrage, brancher un casque sur le Modulox. Dans un logiciel
   MIDI sur l'ordinateur, envoyer des notes à la sortie **Modulox Sine**.
3. Envoyer A4 (note MIDI 69) et la maintenir. On doit entendre un sinus de
   440 Hz avec les réglages initiaux, puis son extinction au Note Off.
   Un Note On de vélocité zéro déclenche aussi le relâchement.

Le port USB natif présente les interfaces MIDI et série après démarrage. Si le port COM de
programmation disparaît, repasser par le bootloader pour flasher à nouveau.
## Sampler et partition de samples

Le Sampler utilise la partition SPIFFS `samples` de 10 Mio. Son premier
déploiement demande un flash du bootloader et de la table de partitions
avec la commande `flash` habituelle. Les adresses de `userpatches`
et `waves` restent identiques. Au premier démarrage, le firmware initialise
la partition samples si son espace est vierge.

Le stockage utilise du PCM mono 16 bits à 44,1 kHz. Chaque fichier fait au
plus 1 Mio. Le transfert USB vérifie le hash avant de rendre le fichier
visible. Seuls les fichiers référencés dans le patch choisi sont chargés
en PSRAM et le moteur audio lit ces buffers. Le catalogue USB expose le
Sampler quand la partition et la PSRAM sont accessibles.

## Horloge et Sync In

Le jack Sync In reprend GPIO48 et le pull-down de `libraries/groovuino/mb.h`.
L’interruption capture les fronts montants et leur horodatage, puis transmet
les événements à la tâche audio par une file fixe. Les réceptions de notes
et d’horloge utilisent des files distinctes. La tâche audio applique les
événements au début d’un bloc de 32 échantillons (environ 0,73 ms à 44,1 kHz).
Le filtrage des fronts accepte un intervalle minimum de 1 ms.

Le champ JSON `routings` contient des tuples
`[source, destination, entrée, sortie]`. Value utilise une sortie CV.
Trigger et Reset acceptent Trigger ou Reset d’un Clock, ou Trigger d’un
MIDI/Arpeggiator. Run accepte uniquement Run d’un Clock.
Les paramètres ordinaires restent dans `modulations` et l’audio dans
`connections`. Le compilateur prépare l’ordre des utilitaires et détecte
les boucles de CV avant d’installer le patch.

L’horloge générale, les modes Sync et les nouveaux modules sont facultatifs.
Les anciens patches conservent les valeurs initiales Free pour LFO et Delay.
Les deux buffers du Delay sont alloués à la préparation du patch. La ligne
gauche préfère la RAM interne avec une réserve de 48 Kio ; la seconde
préfère la PSRAM. Chorus et Rotary gardent aussi une réserve de 48 Kio
lors de leurs allocations. Aucun de ces buffers n’est alloué pendant le rendu.

## Extension VA

Unisson et glide n’utilisent aucune allocation pendant le rendu. Les
oscillations d’un Osc partagent sa table en SRAM. Detune utilise les tables
de rapports pré-calculées ; à Unison=1 le mélange des copies est sauté.
Mix=100 lit uniquement la couche d’unisson, Mix=0 uniquement le son simple
(les phases des copies continuent d’avancer). Les états d’oscillation sont
propres à chaque voix ; les copies partagent filtre et enveloppes.

Le glide interpole la hauteur en demi-tons, puis les rapports de fréquence
pré-calculés. Les incréments de phase sont interpolés à chaque échantillon
entre les bornes du bloc, sans exponentielle dans la boucle des oscillateurs.
Il suit aussi la transposition du Sampler (hors Drum) et du Granulizer à la
cadence de bloc. Le mode mono conserve jusqu’à 64 identités source/canal/note
dans une pile fixe. Les nouvelles options sont facultatives : Poly, Glide
Off et Unison=1 préservent le comportement des anciens patches.

Noise, Sub, VCA, Ring Mod et Wavefolder gardent leurs slots historiques
27 à 31. Les masques de modules et de modulations utilisent
64 bits ; les anciens indices sont conservés. Le catalogue annonce
`vaExpansion: true`. Les indices des anciens paramètres et affectations CC
restent stables, et la limite des paramètres/settings passe à 12.

Le VCA lisse le gain sur un bloc et laisse terminer les enveloppes CV après
Note Off. Les deux entrées du Ring Mod sont validées au chargement. Le
Wavefolder opère à la fréquence audio nominale, avec un filtre de suppression
du continu à 20 Hz ; il ne comporte pas de suréchantillonnage.

## Graphe stéréo

Le catalogue expose `stereoGraph: true`, `inputChannels`, `outputChannels`
et `sharedEffect`. Les slots 32–39 ajoutent trois Pan, Chorus, Rotary,
Stereo, Mono et Stereo Mix. Les masques de sélection des menus sont aussi
sur 64 bits ; les convertisseurs sans paramètres sont omis de ces menus.
Les paramètres des modules et le format des câbles MIDI/CV sont conservés.

Le compilateur distingue les nœuds par voix des effets communs et de leurs
successeurs. Il prépare jusqu’à douze bus de sommation, utilisés uniquement
aux frontières entre ces parties. Les buffers mono ne sont pas doublés :
huit buffers droits sont réservés aux seuls slots pouvant produire L/R.
Les chemins Basic/Triple mono conservent leur rendu direct. Les tables de
sinus existantes servent au Pan et aux modulations des effets.

Chorus, Delay et Rotary sont traités une seule fois par bloc, après la somme
des voix. Une branche sèche peut rejoindre un effet commun via Stereo Mix.
Mono permet aussi de retourner à Filter, VCA, Disto ou Wavefolder communs.
Le compilateur refuse un ADSR audio ou une commande par voix dans cette
partie commune. La validation des modules contrôle leurs formats de ports.

Les patches versions 1/2 gardent leur rendu mono historique. Le nouveau
Mode du Delay reste utilisable depuis ces patches sur le synthé, car leur
Delay était terminal. À l’ouverture, l’éditeur migre vers la version 3 en
insérant des convertisseurs visibles. Le limiteur final applique un gain
commun aux deux canaux, déterminé par la plus grande crête, pour préserver
la position stéréo. L’aperçu des samples est ajouté aux deux canaux en sortie.

## FM4

Le slot 40 ajoute une source mono FM4, à quatre opérateurs sinusoïdaux et
huit architectures avec feedback sur le premier opérateur. Le catalogue
annonce `fm4: true`. Les ports MIDI sont Pitch, Velocity et Trigger ; les
entrées CV sont Pitch, Depth, Feedback et Level. Les ADSR sont propres aux
opérateurs et leurs relâchements maintiennent la voix active sans ADSR audio
supplémentaire. Le moteur ne calcule pas les voix éteintes.

Les phases utilisent des accumulateurs 32 bits et les tables sinus en SRAM
déjà présentes. La modulation de phase est calculée à chaque échantillon,
avec multiplication entière et débordement non signé ; les fréquences et
coefficients sont préparés à la cadence de bloc. Aucun buffer n’est alloué
pendant le rendu. Les réglages de niveau et de profondeur sont lissés.
Ce moteur n’est pas une émulation exacte des registres, enveloppes ou
convertisseurs du YM2612, et ne comporte pas d’import DX7.

FM4 expose trente paramètres. Les lignes des autres modules restent à
douze éléments : seuls FM4 et sa ligne MIDI Learn disposent du stockage
étendu. Les pages de paramètres sont décrites par le module, et les
masques de présence des paramètres passent sur 64 bits. Le buffer de
réponse du catalogue USB passe à 20 Kio, avec la même limite dans l’éditeur.

FM est ajouté en fin de liste des paramètres du premier Osc uniquement.
Osc 2 est une dépendance audio implicite d’Osc 1 lorsqu’ils sont tous deux
présents, même sans câble audio en sortie d’Osc 2. Le modulateur est lu
avant son gain de sortie et calculé une seule fois par voix ; FM=0 conserve
le chemin sans modulation. L’ordre de rendu inclut cette dépendance dans
les chemins génériques et optimisés. Les anciens indices de paramètres
et les patches sans FM restent compatibles.

## Entrée audio et enregistrement

I2S0 fonctionne maintenant en duplex : TX conserve ses réglages, RX reçoit
sur GPIO4, avec les mêmes horloges, format MSB/left-justified, 16 bits et
44,1 kHz. RX possède quatre buffers DMA de 32 frames stéréo. Son canal
reste désactivé hors enregistrement, sans lecture ni copie d’entrée dans
la boucle normale du moteur. Le callback de débordement RX est en IRAM ;
une perte de données ou une erreur de lecture interrompt la prise et
affiche Input error.

Le WM8960 reprend les connexions INPUT1 → PGA → boost → ADC du Granulizer
Arduino : PGA à 0 dB, boost à +29 dB, ADC numérique à 0 dB. Aucun retour
direct de l’entrée vers le casque n’est activé. La capture garde le canal
gauche, comme l’ancien projet ; les fichiers restent compatibles avec les
sources mono existantes. Le niveau analogique et l’entrée effective restent
à vérifier sur le matériel.

Le buffer de prise fait au maximum 220 500 échantillons, soit 441 000 octets
en PSRAM. La tâche audio copie les blocs RX en mono et publie la prise
complète après désactivation de RX. Le graphe et les notes MIDI sont
suspendus pendant la capture et la sauvegarde. Une tâche dédiée sur le
cœur de contrôle calcule le hash puis écrit la prise via un tampon SRAM
de 4 Kio. Aucun accès flash ni allocation n’a lieu dans la capture audio.
En cas d’échec de sauvegarde, le buffer reste disponible pour Retry.

Avant le hash et l’écriture, `Recording::prepare` traite le PCM en place :
coupe de 30 ms en tête et 170 ms en queue, fondu de 2 ms aux deux bords,
puis normalisation de crête à 32767. Le budget de coupe est limité à la
moitié d’une prise courte. Le calcul traite correctement -32768 et le
silence, sans allocation. `recording_prepared` empêche un nouveau traitement
lors d’un Retry ; le hash et la durée portent sur le PCM traité. La tâche
de stockage reste sur le cœur de contrôle, sous la priorité de l’affichage.
L’état Processing/Saving affiche un message global sans charger de sample
ni lire ses métadonnées en flash.

SampleStore sérialise ses accès et réserve l’écriture à la prise ; une
transaction USB existante n’est pas interrompue pour commencer une prise.
Le nom est séparé du PCM et peut être remplacé sans changer le hash. Les
fichiers de noms utilisent un remplacement avec copie de secours `.old`,
lisible après une interruption. Le commit PCM vérifie le hash et utilise
le fichier temporaire existant. Deux prises identiques partagent le même
fichier. Le sample sélectionné dans la bibliothèque est chargé en PSRAM et
peut être écouté indépendamment du patch.

L’affichage Samples garde le nom et les métadonnées en cache. Un changement
de sample fournit déjà son nom ; une nouvelle lecture n’a lieu que si la
révision du stockage change, notamment après un renommage USB. Le suivi
périodique du temps d’enregistrement ne relit pas ces fichiers, et aucune
actualisation de métadonnées n’est effectuée pendant une prise/sauvegarde.

Le premier écran du Sampler montre Sample/Play/Start/End et les deux
repères de tranche. Le module fournit `menu_order` et ses pages dans
`ModuleParameters` : seuls les indices de présentation sont réordonnés.
Les paramètres JSON et MIDI Learn gardent leurs indices historiques.
Les CC utilisent `parameter_position` pour retrouver la position visible.
La courbe est mise en cache sur cet écran ; déplacer les bornes redessine
les repères sans accès aux fichiers. Le Granulizer garde ses pages actuelles.

Le catalogue annonce `sampleLibrary`, `audioRecording` et
`maxRecordingSeconds: 5`. **SLIST offset** renvoie un objet JSON contenant
au plus 32 `items` (`id`, `name`, `bytes`) et `next` (offset suivant ou null).
SGET récupère le PCM et SNAME renomme un sample existant. Les nouvelles
commandes reçues pendant une prise renvoient `ERR recording-busy`.
Les partitions ne changent pas : `app-flash` suffit pour cette mise à jour.

`samplePreview` et `sampleDeletion` annoncent deux commandes supplémentaires.
**SPREVIEW hash** renvoie 1028 octets : le nombre de frames en uint32 LE,
puis 256 couples min/max int16 LE. Une seule ouverture du fichier et un
tampon temporaire de 4 Kio suffisent, sans charger un autre sample en PSRAM.
L’éditeur met cette courbe en cache et sérialise les requêtes lors d’une
sélection rapide. L’aperçu ne modifie pas la bibliothèque locale.

**SDELETE hash** passe par le même coordinateur que le Delete matériel.
Il vérifie les références du patch chargé, y compris non sauvegardées, puis
de tous les patches sauvegardés. Un échec de lecture ou d’analyse d’un patch
bloque la suppression. Les transactions et prises en cours la bloquent aussi.
L’audio est suspendu pendant l’effacement ; une audition de bibliothèque
est détachée avant de libérer son buffer. Le PCM, son nom et le secours
`.old` sont retirés ensemble. Les autres samples ne sont pas touchés.
La confirmation matérielle mémorise la révision de la sélection et refuse
d’effacer un autre sample si celle-ci a changé entre les deux appuis.

Le test portable `tests/recording_test.cpp` vérifie la durée exacte, le
canal gauche, la borne du dernier bloc, l’arrêt anticipé et les états de
sauvegarde. Il ne simule pas le codec ni les interruptions I2S. La compilation
Windows du composant demande au ramasse-miettes de GCC de travailler plus
souvent pour réduire sa mémoire de pointe, sans changer l’optimisation du
code produit ; sur un PC chargé, construire avec un seul job (`ninja -j1`).

