# Éditeur de patches Modulox

## Configurateur Web

Ouvrir [le configurateur Modulox](https://raspiaudio.com/modulox/configurateur/)
dans Chrome ou Edge, brancher le Modulox en USB et cliquer sur **Connecter en
USB**. Le navigateur demande de choisir le port. Les patches, les samples et
les mises à jour transitent directement entre le navigateur et le Modulox.

Le drapeau en haut ouvre le menu des langues. Le configurateur utilise par
défaut la langue du navigateur si elle est disponible, ou le français. Une
langue choisie dans le menu est conservée dans le navigateur. Le ZIP hors ligne
inclut aussi les traductions et les drapeaux.

Ouvrir **Clavier** en bas de la page et cliquer sur **Activer le MIDI** pour
jouer le patch chargé sur le Modulox. Autoriser le MIDI dans Chrome ou Edge,
puis jouer avec les touches affichées, à la souris, au doigt ou avec le clavier
de l’ordinateur. Le son sort du Modulox. Le canal 1 convient aux patches en
mode Omni. Pour un patch réglé sur un canal précis, choisir ce canal dans le
tiroir.

Pour publier l’éditeur, lancer `node editor/build-web.mjs` depuis la racine,
puis servir le contenu de `editor/dist/` en HTTPS. Ce script prépare l’index
des presets et les deux exemples avec les fichiers déjà présents dans le
dépôt. Le serveur doit servir les modules `.mjs` comme JavaScript et rediriger
l’adresse du dossier vers sa version avec une barre oblique finale.

Pour reprendre un patch personnel du lanceur Windows, exporter le fichier
`.modulox.json` depuis l’éditeur local, puis l’ouvrir dans le configurateur
Web. La sélection du dossier et les brouillons du navigateur sont propres
à chaque adresse.

## Lanceur Windows hors ligne

Le panneau Firmware propose aussi **Installation firmware usine…** pour
programmer une carte neuve ou réinitialiser complètement un Modulox en BOOT.
Cette action remplace ses 16 Mio, après identification et confirmation explicite.
L'image est incluse hors ligne. Voir [INSTALLATION-USINE.md](INSTALLATION-USINE.md).

Sous Windows, double-cliquer sur `lancer.cmd` ou taper `.\lancer.cmd` dans
PowerShell depuis le dossier `editor`. Le lanceur utilise Windows PowerShell,
démarre un petit serveur local masqué puis ouvre automatiquement Chrome (ou
Edge) dans une fenêtre d’application. Python et Node.js ne sont pas nécessaires.
Internet n’est pas nécessaire : tout est servi depuis `http://127.0.0.1:8765/`.
`arreter.cmd` arrête le serveur ; fermer uniquement le navigateur le laisse
disponible en arrière-plan. Un nouveau lancement réutilise la même instance.

Pour distribuer la version Windows autonome, exécuter
`powershell -NoProfile -ExecutionPolicy Bypass -File editor/build-offline.ps1`
depuis la racine. Le ZIP `editor/releases/Modulox-Windows-offline.zip` contient
l’application, les 40 presets, les ressources, les lanceurs et une notice.
L’utilisateur extrait le ZIP et double-clique sur `lancer.cmd`, sans droits
administrateur ni moteur de serveur supplémentaire à installer. Le navigateur
Chrome ou Edge doit déjà être installé. Le lanceur ne modifie pas la stratégie
d’exécution PowerShell du système ; son option s’applique au processus lancé.

Le serveur autonome utilise les sockets .NET, liés uniquement à l’adresse
de boucle locale. Il sert une liste limitée de fichiers et n’accepte pas
d’écriture de patches. Le bouton d’arrêt passe par une session locale dédiée.
Ne pas lancer en même temps un autre serveur sur le port 8765.
Pour les développeurs, `python editor/serve.py` et `node editor/serve.mjs`
restent disponibles ; ils n’ouvrent pas automatiquement le navigateur.

## Créer et transférer un patch

1. Dans **Patches**, cliquer sur **Nouveau patch** ou sélectionner un patch
   dans la banque et cliquer sur **Configurer**. Un seul patch reste en cours
   d’édition. L’onglet **Éditeur** permet de le retrouver.
2. Dans **Câblage**, déposer les modules dans le graphe ou cliquer sur leur
   nom dans la palette.
   Déplacer un module par son titre. Pour câbler, faire glisser un port de
   sortie vers un port d’entrée, ou cliquer sur les deux ports l’un après
   l’autre. Chaque entrée accepte un seul câble. L’inspecteur permet de
   retirer les câbles et de modifier les paramètres du module sélectionné.
3. Relier le graphe à la **Sortie**, puis connecter le port USB de
   l’application Modulox, par exemple COM7. **Envoyer au Modulox** enregistre
   le patch en flash. Appuyer sur l’encodeur du synthé pour ouvrir **Patches**,
   puis choisir le patch et valider avec le bouton central.
4. **Relire le Modulox** actualise la banque tout en conservant l’édition
   en cours. **Remplacer sur le Modulox** met à jour le patch d’origine,
   même après un changement de nom. Un envoi réussi ferme l’édition.
5. **Enregistrer dans mon dossier** écrit un vrai fichier `.modulox.json`
   sur l’ordinateur, avec le patch, ses formes d’onde et samples.
   **Mes patches → Choisir mon dossier de patches** permet de choisir ce dossier.
   Les anciens patches du navigateur restent accessibles dans **Mes patches**.
6. **Supprimer** retire le patch sélectionné du Modulox après confirmation.
   La suppression et le remplacement des patches d’usine demandent le
   firmware avec la capacité USB `patchManagement`. Après une suppression,
   recharger un patch sur le synthé avant d’utiliser sa sauvegarde physique.

**Modules** ouvre ou replie la palette, fermée par défaut pour garder de la place
au graphe. Six familles permettent de choisir les modules : **Generators**
(Osc, Sampler, FM4, Granulizer, Noise, Sub), **Effects** (Filter, TB-303, Disto, Decimator, Bitcrusher, Compressor,
Ring Mod, Wavefolder), **Time effects** (Delay, Reverb, Chorus, Rotary),
**Modulation** (ADSR, Env, LFO et utilitaires CV), **MIDI & Clock** (MIDI,
Clock, Arpeggiator) et **Routing** (Mix, VCA, Pan et conversions stéréo).
Seule la famille sélectionnée est affichée ; le clic, le glisser-déposer et
les compteurs d'instances restent disponibles. La vue
**Paramètres** rassemble les réglages dans des panneaux par module.
Le panneau **Firmware** de l’accueil lit la version du Modulox, compare la
version disponible et permet la mise à jour USB A/B, avec confirmation et
vérification du redémarrage. **Choisir un fichier** permet une installation
hors ligne. Les anciens Modulox doivent d’abord être migrés une fois vers le
partitionnement A/B. Voir [la procédure firmware](../firmware/UPDATES.md).

La surface de câblage mesure au moins 1 800 pixels en hauteur et se parcourt
avec le défilement vertical. Déplacer des modules vers le bas agrandit la
surface automatiquement, avec une réserve de 300 pixels sous les modules.

L’éditeur lit les paramètres et leurs valeurs par défaut via le catalogue
USB. Les anciens firmwares restent compatibles en lecture avec le
catalogue intégré. Le firmware de l’éditeur est requis pour l’écriture.

La banque accepte 16 patches au total, exemples d’usine compris. Un patch
peut contenir jusqu’à trois Osc, trois LFO et trois Pan, ainsi que les autres
modules selon les limites de la palette. Les câbles orange sont mono ; les
câbles turquoise transportent L/R. Les boucles de rétroaction sont refusées.

## Écran du Modulox

Connecter le Modulox en USB et ouvrir **Écran du Modulox**. L’onglet affiche
les mêmes menus et réglages que l’écran physique. Utiliser **Menu**, les
flèches et **Valider** comme sur la façade. Cliquer sur l’encodeur pour
l’enfoncer, ou le tourner avec la molette, les flèches du clavier quand il
a le focus, ou les boutons **−** et **+**. Les gestes effectués sur la façade
se reflètent aussi dans le navigateur.

La fonction est disponible à partir du firmware 1.3.3 sur les modèles
16/8 Mio et 32/16 Mio.

## Banque de patches sur le serveur

La banque comprend **40 presets** : 9 basses, 8 leads, 7 nappes, 8 claviers,
6 FX et 2 percussions. Le détail des sons figure dans [PRESETS.md](PRESETS.md).
Les basses et leads emploient trois oscillateurs VA. Les autres familles
utilisent aussi FM4, tables AKWF, bruit, Ring Mod, Wavefolder et Granulizer.
Les formes d’onde et textures nécessaires sont chargées automatiquement
depuis le serveur dans les bibliothèques du navigateur, puis envoyées avec
le patch par USB. Aucune importation manuelle de WAV n’est nécessaire.

**Presets d’usine** liste les fichiers JSON du
dossier `editor/dist/presets`. Les lanceurs Python et Node découvrent ces
fichiers à chaque ouverture de la liste. Après une mise à jour du serveur,
relancer le lanceur et actualiser la page. Cliquer sur un nom ouvre une copie
dans l’éditeur ; **Envoyer au Modulox** permet ensuite de la transférer.
Les fichiers du serveur sont conservés indépendamment du navigateur.

Premier essai : **VA Iron Bass**, basse VA mono à trois oscillateurs
(deux carrés désaccordés et un triangle à l’octave inférieure), filtre
passe-bas quatre pôles avec enveloppe dédiée, ADSR d’amplitude et distorsion
douce dosée à 60 %. Sortie centrée, glide legato de 65 ms.
Essayer autour de MIDI 36–48. Sans module MIDI, le clavier et les enveloppes
sont routés implicitement par le moteur. Aucun WAV ni sample externe requis.
Ce patch utilise les fonctions VA du firmware récent. La banque serveur
peut dépasser les 16 emplacements du Modulox ; le transfert en flash reste
limité à 16 patches simultanés.

## Patches personnels : fichiers sur l’ordinateur

Dans **Mes patches**, choisir **Choisir mon dossier de patches**. Chrome ou
Edge affiche le sélecteur de dossiers et demande l’accès en lecture/écriture.
Ce choix appartient à chaque utilisateur. Le navigateur mémorise la référence
du dossier ; **Actualiser** redemande l’accès si l’autorisation a expiré.
Un hébergement public doit utiliser HTTPS ; l’adresse locale `127.0.0.1`
convient également.

**Enregistrer dans mon dossier** crée une nouvelle copie, sans remplacer les
fichiers existants. Le fichier `.modulox.json` contient le graphe, les réglages,
le MIDI Learn, la disposition et le PCM de chaque ressource nécessaire.
Il reste utilisable si les données du navigateur sont effacées. Le dossier
choisi n’est jamais envoyé au serveur : les serveurs ne fournissent que
l’application et les presets d’usine. Aucun compte ni service de stockage
distant n’est nécessaire.

**Mes patches** liste les fichiers `.modulox.json` du dossier. Cliquer sur un
fichier ouvre une copie dans l’éditeur. **Importer un fichier** ouvre aussi
un fichier provenant d’un autre ordinateur ; il n’est pas modifié. Pour
enregistrer ses changements, utiliser **Enregistrer dans mon dossier**.
**Exporter un fichier** télécharge la même sauvegarde complète, même si le
navigateur ne propose pas de sélecteur de dossiers.

Les ressources sont vérifiées par SHA-256 avant importation. Les fichiers
incomplets ou corrompus sont refusés et ne remplacent pas le patch en cours.
Un graphe en cours de construction peut être sauvegardé sur disque, mais
l’envoi USB exige toujours un graphe complet et valide. Les anciens JSON
simples sont acceptés ; leurs ressources externes doivent déjà être présentes
dans les bibliothèques du navigateur.

Le navigateur conserve encore le brouillon de travail et les bibliothèques
utilisées pendant l’édition. Cela ne remplace pas l’enregistrement sur disque.
Les anciennes sauvegardes restent sous **Anciennes sauvegardes du navigateur** :
ouvrir chaque patch, puis l’enregistrer dans le dossier pour le convertir.
Si une ressource existe uniquement sur le Modulox, connecter celui-ci permet
de la lire par USB avant l’enregistrement. Si elle manque, la sauvegarde
échoue explicitement plutôt que produire un fichier incomplet.

Vérification : `node editor/tests/patch-files.test.mjs` teste l’aller-retour
des 40 presets et de leur PCM, les fichiers corrompus, les brouillons et
les erreurs d’écriture disque sans écrasement de fichiers existants.
`node editor/tests/personal-folder.test.mjs` vérifie les actions de l’interface,
les autorisations du dossier et l’absence de requêtes HTTP personnelles.

## Réglages et modulation

**Réglages du patch** ouvre les réglages Voices, Volume et MIDI Ch. Fine et
PWM se trouvent dans l’inspecteur de chaque Osc. Pour utiliser un LFO, déposer
le module, puis relier sa sortie bleue aux entrées bleues des Osc, du Filter
ou de la Sortie. Sur un oscillateur, PWM agit avec la forme Square.

Un patch peut employer jusqu’à trois LFO et deux modules **Env**. Les
sorties de modulation bleues se relient aux entrées bleues. Le module
**MIDI** expose Pitch, Velocity, Gate et Trigger sur les ports violets.
Relier les ports de même nom. Avec ce module, câbler Pitch aux Osc qui
doivent suivre le clavier et Trigger aux LFO qui doivent redémarrer à
chaque note. **Delay**, **Chorus** et **Rotary** se câblent dans la chaîne
stéréo ; ils peuvent se suivre et se mélanger à un chemin sec.

## Formes d’onde Wavekids

L’onglet **Waveforms** ouvre la bibliothèque de cycles. Depuis l’éditeur,
**Waveforms du patch** permet aussi d’affecter des formes au patch.
Cliquer sur **Importer des WAV** et sélectionner
un ou plusieurs fichiers WAV mono PCM 16 bits contenant chacun un cycle.
L’importateur rééchantillonne les cycles à 1 024 points par interpolation
linéaire avec raccord de boucle et conserve le gain du WAV. Cliquer sur le
nom pour afficher la courbe, puis cocher les formes à mettre dans le patch.
**Ajouter les résultats au patch** ajoute les WAV filtrés jusqu’à la limite
de 125. La bibliothèque reste dans ce navigateur.

Choisir ensuite une des formes ajoutées dans la liste **Wave** d’un
oscillateur. **Envoyer au Modulox** transfère les cycles manquants puis
le patch. Pour une forme déjà présente sur le Modulox, son aperçu peut se
télécharger à la demande dans l’éditeur.

## Sampler

1. Dans **Samples**, importe tes WAV ou clique sur **Importer les démos
   Sampbrain2**. Le navigateur les convertit en PCM mono 16 bits à 44,1 kHz
   et les conserve dans sa bibliothèque locale. Chaque sample peut occuper
   jusqu’à 1 Mio après conversion, soit environ 11,9 secondes.
2. Dans **Câblage**, ajoute un **Sampler**, relie sa sortie via **Pan** ou **Stereo** à **Sortie**, ou
   au reste du graphe, puis choisis un fichier dans **Sample**. Un patch
   peut contenir jusqu’à trois Sampler.
3. **Root** fixe la note racine MIDI, **Pitch** et **Fine** transposent la
   lecture, et **Start** et **End** définissent la tranche en pourcentage.
   **One-shot** joue la tranche jusqu’au bout, **Gate** suit la note et
   **Loop** boucle tant que la note est tenue. **Drum** joue la tranche à
   hauteur fixe sur la seule note Root. **MIDI Ch** vaut 0 pour Omni.
4. Envoie le patch au Modulox. Les fichiers absents sont envoyés
   automatiquement. **Stocker sur le Modulox** permet aussi de transférer
   un sample individuellement depuis la bibliothèque.

Sur le Modulox, le premier écran du Sampler regroupe **Sample, Play, Start
et End**. L’encodeur modifie la commande sélectionnée ; le bouton central
déclenche Play ou le MIDI Learn sur Start/End. Deux traits sur la courbe
indiquent la tranche : vert au début, orange à la fin. La courbe reste en
cache pendant ces réglages. Les pages suivantes regroupent Root/Pitch/Fine/
Level, puis Mode/MIDI Ch. La sauvegarde de la tranche reste explicite avec
Save patch, comme les autres paramètres.
5. Choisis le patch sur le Modulox. Ses samples sont chargés en PSRAM avant
   la lecture et les buffers sont partagés si plusieurs Sampler du patch
   pointent vers le même fichier.

## INPUT : entrée micro en direct (1.7.0-dev, validation locale)

Dans **Modules → Generators**, **INPUT** expose le canal mono gauche de
l’entrée audio/micro du Modulox, comme l’enregistreur. Il fonctionne sans
note MIDI et une seule fois pour tout le patch, même à huit voix. **Level**
règle son niveau de 0 à 100 %, avec une transition sur un bloc.

Exemples : **INPUT → TB-303 → Stereo → Sortie**, ou
**INPUT → Stereo → Reverb → Sortie**. Stereo ou Pan convertit le signal
mono avant un effet stéréo. Pour moduler son amplitude, utiliser un VCA
avec un LFO ou un contrôle global. Un ADSR audio ou une enveloppe par voix
ne se place pas après cette source commune. Level accepte MIDI Learn.

Le patch `examples/INPUT-Reverb.json` est prêt à ouvrir via **Importer un
fichier**. Cette version reste locale : installer le firmware `1.7.0-dev`
du bon modèle via le panneau Firmware de l’éditeur local avant l’envoi.
Un appareil équipé d’un ancien firmware laisse préparer le patch, mais
l’envoi d’INPUT reste bloqué jusqu’à la mise à jour.

RX fonctionne à 44,1 kHz avec les mêmes horloges I2S que la sortie. Le
traitement utilise des blocs de 32 frames (environ 0,73 ms), sans nouvelle
file audio ni accès flash. La lecture live ne bloque pas ; après un retard,
les blocs anciens sont abandonnés pour reprendre le plus récent. Une absence
de données produit du silence, sans répéter le bloc précédent. La latence
totale inclut les DMA, le codec et les effets ; elle reste à mesurer sur carte.

L’enregistrement suspend la restitution live pendant la prise et la
sauvegarde. Changer de patch, suspendre l’audio ou lancer une mise à jour
arrête RX ; la reprise repart d’une file fraîche. Aucun buffer PSRAM
supplémentaire n’est alloué pour INPUT. L’écoute et les transitions matérielles
restent à valider avant publication.

## Granulizer

Ajoute un **Granulizer** dans **Câblage**, choisis un fichier dans **Sample**
et relie sa sortie au graphe. Il partage la bibliothèque et les buffers PSRAM
du Sampler. Sur le Modulox, l’écran **Sample / Play** affiche la forme
d’onde, permet de changer de fichier et de l’écouter. **Save patch**
mémorise le choix.

**Start** et **End** délimitent la zone de lecture. **Size** règle la durée
d’un grain, **Density** le nombre de départs par seconde, **Speed** le
balayage de la zone et **Spray** la dispersion des départs. À **Speed = 0**,
le balayage est fixe. Une vitesse négative inverse le balayage.
**Window** propose Hann, Triangle et Rect. **Pitch** transpose les grains
et **Level** règle leur niveau. Chaque réglage a une entrée CV utilisable
avec un LFO ou une enveloppe. Le moteur limite le recouvrement à huit grains
simultanés par voix.

## Horloge, arpégiateur et contrôles

Dans **Réglages du patch**, choisir **Clock** parmi Internal, USB, DIN ou
Sync jack. **BPM** règle le tempo interne. **Run** autorise la lecture.
**Jack PPQN** fixe le nombre d’impulsions par noire pour le jack physique
(1, 2, 4 ou 24, valeur initiale 2). **Clock loss** choisit l’arrêt ou la
poursuite au dernier tempo quand une source externe disparaît. Les horloges
USB et DIN sont indépendantes du canal de notes et suivent Start, Stop,
Continue et Song Position Pointer. En absence de transport MIDI, les
impulsions reçues lancent la synchronisation.

Le **LFO** et le **Delay** proposent **Sync** et **Division**. Le LFO conserve
sa fréquence libre quand Sync est désactivé. **Phase** décale son cycle.
Le temps du Delay est limité à deux secondes, y compris en synchro.

Le module **Clock** expose des sorties vertes Trigger, Reset et Run.
Trigger suit sa Division, Reset marque le redémarrage et Run indique que
le transport tourne. Les entrées Trigger et Reset acceptent des impulsions,
et Run se branche sur une entrée Run. Le Trigger du module MIDI peut aussi
déclencher les utilitaires ou réinitialiser un LFO.

**Arpeggiator** mémorise les notes tenues et produit un arpège Up, Down,
Up-down, Played ou Random. Régler Division, Gate et Octaves, puis câbler ses
sorties Pitch et Velocity sur les oscillateurs. Sans entrée Trigger câblée,
il utilise l’horloge commune. Le routage des notes s’applique au patch entier.
Le réglage On permet de retrouver le jeu direct.
Quand Trigger est câblé, Division est grisée dans l’éditeur et masquée sur
le Modulox : chaque impulsion reçue fait avancer l’arpège. Débrancher Trigger
rétablit la division mémorisée. Les câbles Reset et Run ne la désactivent pas.

**S&H** mémorise son entrée CV **Value** à chaque Trigger et conserve cette
valeur. **Random** produit une valeur aléatoire à chaque Trigger, avec Depth
et Offset. **Gain/Offset** transforme l’amplitude et le centre d’une CV.
**Slew** limite sa vitesse de montée et de descente, avec Rise et Fall.
Les sorties CV peuvent se câbler à Value ou aux entrées de paramètres
bleues. Les boucles sont refusées. Une chaîne alimentée par une enveloppe
est évaluée séparément pour chaque voix et se destine aux paramètres
des voix plutôt qu’aux effets communs.

## Synthèse VA : unisson, jeu et nouvelles briques

Chaque **Osc** dispose de **Unison** (1 à 4 oscillations), **Detune**
(écart maximal en cents, réparti symétriquement) et **Mix**. À 1, le son
simple est conservé. À partir de 2, Mix dose le son simple et la couche
d’oscillations désaccordées ; Level règle l’ensemble. La couche est moyennée
pour garder une marge de niveau. Cela ne change pas le gain des voix MIDI.
Sur le Modulox, les trois pages sont Wave/PWM, Pitch/Fine/Level, puis
Unison/Detune/Mix.

La carte **Jeu** de l’onglet Paramètres correspond à la page **Play** dans
Settings sur le Modulox. **Poly** utilise Voices ; **Mono** utilise une voix
et donne priorité à la dernière note tenue. Relâcher cette note revient à
la précédente encore tenue, y compris entre USB et DIN. **Glide** propose
Off, Always et Legato ; **Glide time** va de 0 à 2000 ms. Le temps est celui
du trajet complet, quelle que soit la distance entre les notes. Legato
nécessite un chevauchement des notes. **Retrigger** relance les enveloppes
et les sources lors des transitions liées en mono ; désactivé, il conserve
leur progression. L’horloge et le retrigger explicite des LFO restent
indépendants. Le glide agit aussi sur les notes générées par l’arpégiateur.
En polyphonie, chaque voix part de sa dernière hauteur connue ; la première
note d’une voix part directement à la bonne hauteur.

| Module | Utilisation |
| --- | --- |
| Noise | Bruit blanc par voix, Level modulable et entrée MIDI Velocity. |
| Sub | Sinus, carré ou triangle, à -1 ou -2 octaves ; Pitch/Level modulables et entrées MIDI. Suit le glide. |
| VCA | Entrée audio, entrée CV Gain et entrée MIDI Velocity. Level fixe le maximum et Depth dose l’action de Gain. |
| Ring Mod | Deux entrées audio obligatoires, multipliées ; Mix modulable revient à la première entrée à 0 %. |
| Wavefolder | Drive, Symmetry et Mix modulables ; suppression du continu après le repliement. |

Pour une amplitude ADSR modulaire, relier **Osc → VCA → Pan → Sortie**, **Env →
Gain du VCA**, puis **MIDI Gate → Env Gate** si le patch contient un module
MIDI/Arpeggiator. Mettre Env Depth à 100 % pour exploiter toute l’amplitude.
Le relâchement de cette enveloppe continue après Note Off, même sans module
ADSR audio. Un LFO bipolaire peut fermer le VCA sur sa partie négative :
ajuster son Level/Depth ou ajouter Gain/Offset pour obtenir une CV positive.

Le traitement reste à 44,1 kHz, sans suréchantillonnage des non-linéarités.
Les réglages élevés de Wavefolder ou les multiplications de fréquences
élevées peuvent produire de l’aliasing. L’éditeur permet la préparation
hors connexion, et bloque l’envoi de ces fonctions vers un ancien firmware.

## Stéréo

La Sortie reçoit un signal **stéréo L/R**. Les canaux restent indépendants
jusqu’au codec. Les ports indiquent leur format, les câbles stéréo sont
turquoise, et seuls les ports de format compatible sont proposés pendant
le câblage. Les nouveaux patches utilisent le format version 3.

| Module | Entrées → sortie | Fonction |
| --- | --- | --- |
| Pan | Mono → stéréo | Pan de -100 % (gauche) à +100 % (droite), plus Level. Les deux sont modulables. Loi à puissance constante, centre à -3 dB par canal. Jusqu’à trois Pan. |
| Stereo | L/Mono et R mono → stéréo | L/Mono seul est dupliqué sans atténuation ; avec R, assemble deux sources distinctes. |
| Mono | Stéréo → mono | Moyenne de L et R pour rejoindre un filtre ou un autre traitement mono. |
| Stereo Mix | Trois stéréo → stéréo | Somme les deux canaux avec les gains In 1–3 et la même réserve de niveau que Mix. |
| Chorus | Stéréo → stéréo | Rate 0,05–8 Hz, Depth et Mix modulables ; les deux canaux utilisent des phases différentes. |
| Delay | Stéréo → stéréo | Modes Mono, Stereo et Ping-pong, Time L/R et divisions indépendantes. |
| Rotary | Stéréo → stéréo | Deux rotors avec modulation de délai, d’amplitude et de position ; Slow/Fast, Transition, Depth et Mix. |

Un premier patch peut suivre **Osc → Filter → ADSR → Pan → Chorus → Delay
→ Rotary → Sortie**. Pour garder un son centré identique sur les deux
canaux, utiliser **Stereo** plutôt que Pan. Pour revenir à un effet mono,
passer par Mono, puis refaire une conversion explicite vers la stéréo.

Dans Delay, **Time L** correspond à l’ancien paramètre Time. **Link On**
lie les deux temps et divisions ; **Link Off** permet de régler **Time R**
et **Division R** séparément. Sync utilise les divisions et l’horloge
commune, avec une limite de deux secondes par canal. Mono renvoie les mêmes
répétitions des deux côtés en conservant le signal sec stéréo. Stereo
traite les deux canaux séparément. Ping-pong injecte le signal dans la ligne
gauche, puis les répétitions passent alternativement à droite et à gauche.

Rotary conserve son mouvement entre les notes. **Transition** est la
constante d’inertie du moteur : le passage Slow/Fast est progressif, sans
réinitialiser la phase. Slow et Fast sont affichés en Hz sur le Modulox ;
dans l’éditeur, une valeur de 100 correspond à 1 Hz. Speed, Depth et Mix
sont modulables. Le signal traité simule une cabine commune à partir de la
somme L/R ; le signal sec conserve sa stéréo.

Chorus, Delay et Rotary sont communs à toutes les voix. Les modules placés
après eux sont aussi communs. Les enveloppes et commandes par voix doivent
donc agir avant ces effets ; utiliser un LFO ou une modulation globale
après. Une branche sèche peut être mélangée aux effets avec Stereo Mix.

Les anciens patches restent lisibles par le firmware. L’éditeur insère un
convertisseur Stereo visible lors de leur ouverture et préserve leurs
réglages et leurs identifiants de remplacement. Les imports de samples
restent en mono ; Pan permet de les positionner dans le nouveau graphe.

## FM4 et couplage des oscillateurs

**FM4** est une source mono à quatre opérateurs sinusoïdaux, avec huit
architectures inspirées du YM2612. Il possède ses propres enveloppes : un
premier patch peut simplement relier **FM4 → Pan → Sortie**. Avec un module
MIDI dans le patch, relier Pitch et Velocity à FM4, et Trigger si l’on veut
réinitialiser les phases à chaque déclenchement.

Algorithm choisit le routage, illustré dans l’onglet Paramètres. Depth règle
la profondeur de modulation, Feedback la réinjection de l’opérateur 1, puis
Level, Pitch et Fine règlent la sortie et l’accordage. Chaque opérateur a
son Ratio (0,5 puis 1 à 16), son Level et son ADSR. Modifier l’enveloppe d’un
modulateur fait évoluer le timbre ; celle d’un porteur règle son amplitude.
Pitch, Depth, Feedback et Level acceptent une modulation CV. Les trente
paramètres sont sauvegardés dans le patch et accessibles au MIDI Learn.
Sur le Modulox, ils sont répartis entre les pages générales et deux pages
par opérateur.

Le premier **Osc** gagne aussi un paramètre **FM**, de 0 (désactivé) à
100 %. Il utilise le signal du deuxième Osc comme modulateur de phase,
avant son Level : mettre le deuxième Osc à Level 0 permet de l’utiliser
uniquement comme modulateur. Il peut rester sans câble audio ; le moteur
le calcule automatiquement avant le premier. Ses réglages de forme d’onde,
Pitch, Fine, PWM et Unison déterminent la modulation. FM accepte aussi
une entrée CV. Ce réglage n’est disponible que si le patch contient les
deux oscillateurs ; un nouveau firmware est nécessaire pour le transfert.

La synthèse utilise la modulation de phase à la cadence audio. FM4 reprend
les architectures à quatre opérateurs, avec des ADSR classiques : il ne
reproduit pas exactement le YM2612 et n’importe pas les SysEx DX7. Comme
les autres sources, il fonctionne à 44,1 kHz sans suréchantillonnage ; les
ratios et profondeurs élevés peuvent produire de l’aliasing.

## Enregistrer et récupérer un sample

Sur le Modulox, ouvrir **Menu → Samples**. Cette bibliothèque est disponible
pour tous les patches, même sans Sampler. Les boutons extérieurs choisissent
Sample, Play, Record ou Delete. Sur Sample, tourner l’encodeur parcourt la flash ;
la courbe et le nom apparaissent. Sur Play, le bouton central écoute le
sample brut. Sur Record, il lance une prise depuis l’entrée physique gauche
du codec ; un second appui l’arrête, sinon elle se termine à cinq secondes.

La prise est en mono 16 bits à 44,1 kHz. Avant la sauvegarde, 30 ms sont
coupées au début et 170 ms à la fin pour retirer les bruits de boutons,
puis un fondu de 2 ms est appliqué à chaque bord et le pic est normalisé
au maximum du PCM 16 bits. Une prise très courte conserve au moins sa
moitié ; le silence reste silencieux. Une capture complète de cinq
secondes donne donc environ 4,80 secondes sauvegardées.

Le moteur est suspendu pendant la capture et la sauvegarde. Le message
**Traitement en cours** apparaît sur l’écran pendant cette étape, même
si l’on a quitté le menu Samples. Attendre sa disparition avant de couper
l’alimentation. La prise devient automatiquement
le sample sélectionné et reçoit un nom `enregistrement1`, `enregistrement2`,
etc. Elle est ensuite disponible dans les sélecteurs du Sampler et du
Granulizer. Les données identiques sont dédupliquées par leur hash.
Si l’écriture échoue, **Retry** réessaie avec la prise conservée en PSRAM ;
un redémarrage perd cette prise non sauvegardée. Retry conserve la coupe
et la normalisation déjà effectuées, sans les appliquer une deuxième fois.

Dans l’onglet **Samples** de l’éditeur, la connexion USB et le bouton
**Actualiser les samples du Modulox** lisent toute la bibliothèque flash,
y compris les prises qui ne sont utilisées dans aucun patch. La courbe se
charge automatiquement à la sélection et reste en cache pour la session.
Avec le nouveau firmware, seul un aperçu léger est transmis ; un ancien
firmware utilise le PCM en mémoire temporaire. Cela ne crée pas de copie
dans la bibliothèque locale. Cliquer **Lire depuis le Modulox** pour l’ajouter à la
bibliothèque locale, l’écouter ou l’exporter en WAV. **Renommer** enregistre
le nouveau nom sur le Modulox si le sample y est présent et connecté, et
dans la copie locale si elle existe. Le nom ne change pas l’identifiant
ni les connexions des patches. Les noms matériels sont limités à 31
caractères ASCII ; les accents sont translittérés.

Pour supprimer un sample de la flash : sur le Modulox, choisir **Delete**,
puis appuyer deux fois sur le bouton central. Les boutons extérieurs
annulent la confirmation. Dans l’éditeur, sélectionner le sample puis
cliquer **Supprimer du Modulox** et confirmer. Le firmware refuse la
suppression si un patch sauvegardé ou le patch actuellement chargé utilise
ce sample. Dans ce cas, le remplacer et sauvegarder le patch d’abord.

La suppression du Modulox conserve une éventuelle copie locale. La petite
croix à droite de la ligne supprime uniquement cette copie du navigateur.
Si un brouillon ou un patch local utilise le sample, l’éditeur en conserve
une copie locale avant de l’enlever du Modulox, pour permettre un futur
transfert. Le bouton de suppression en flash demande le nouveau firmware.

Ces commandes demandent le nouveau firmware. Un transfert USB déjà en
cours empêche le démarrage d’une prise ; pendant la capture/sauvegarde,
les nouvelles commandes USB demandent d’attendre. Actualiser les samples
ne remplace pas le patch en cours d’édition.

## Reverb

Le firmware 1.4.0 ajoute Reverb dans la palette : une entrée et une sortie
stéréo, avec Decay, Damping, Pre-delay, Width et Mix. Pour une source mono,
insérer Stereo ou Pan avant Reverb. Le module est commun aux voix ; les
enveloppes Env par voix ne peuvent pas le moduler. Les cinq paramètres
acceptent une modulation globale et MIDI Learn. Sur un ancien firmware,
le patch reste éditable mais son envoi demande une mise à jour.

## Budget RAM et CPU (firmware 1.5.0)

La barre **RAM / CPU** suit le Modulox connecté. **Ressources…** détaille les
coûts du projet. Hors ligne, choisir 8 ou 16 Mio permet une simulation ;
la RAM libre simulée n'est pas une mesure de l'appareil. Avec le firmware
1.5.0, les mesures sont actualisées toutes les deux secondes pendant l'édition.
La SRAM interne est indiquée séparément de la PSRAM.

Le calcul soustrait au heap PSRAM libre les nouveaux samples uniques et
les buffers d'effets qui ne sont pas déjà réservés. Les samples identiques
entre Sampler/Granulizer ne sont comptés qu'une fois dans le nouveau patch.
Les buffers de l'ancien patch restent déjà inclus dans la mémoire occupée :
on ne les déduit pas avant le chargement des nouveaux. La marge ajoute
1 Kio par module et 10 % des nouvelles allocations, avec une réserve globale
de 256 Kio. Les samples choisis dont la taille est inconnue réservent le maximum
par sample annoncé par le catalogue. Un Sampler vide ne réserve que sa marge ;
le choix du sample est contrôlé avant affectation et un patch sans sample ne
peut pas être envoyé. La taille du plus grand bloc libre
permet aussi de détecter une fragmentation évidente. Les états DSP statiques
et les tables d'oscillateur sont déjà alloués en SRAM au démarrage.

Un dépassement du budget RAM bloque le clic ou le dépôt d'un module et
l'envoi du patch. Le changement de sample ou d'un paramètre est refusé s'il
aggrave un dépassement ; la réduction et la suppression restent possibles.
Une mesure RAM fraîche est exigée juste avant l'envoi sur un firmware qui
annonce `resourceTelemetry`. Cette estimation avec marge ne garantit pas
contre toute fragmentation ou modification de l'appareil après la mesure.

**CPU joué** affiche la charge réelle du patch actuellement chargé sur le
synthé, qui peut différer du brouillon. Le détail donne la moyenne, le pic
par bloc et les blocs dépassant le budget audio sur la dernière seconde.
Une période d'enregistrement, de chargement ou de mise à jour invalide la
mesure CPU. Le pilote I2S n'est pas inclus dans le temps de rendu.

**Calibrer l'estimation CPU** accepte des mesures manuelles : moteur seul,
puis surcoût par module et par voix (Unison=1), ou coût total pour un effet
commun. Les mesures sont locales à ce navigateur et séparées par modèle et
version de firmware. Le calcul suit la polyphonie et prend le pire nombre de
copies pour l'unisson ; les effets communs ne sont comptés qu'une fois. Il
ajoute 10 % sur les coûts et interdit les augmentations au-delà de 85 %.
Sans mesures, le coût reste « À mesurer » : aucun pourcentage arbitraire n'est
utilisé. Un coût connu dépassant déjà la limite suffit toutefois à bloquer,
même si d'autres coûts restent inconnus. Ce modèle additif reste une estimation,
notamment pour les chemins de graphe, la bande passante PSRAM et les modulations.

Pour transmettre des mesures : utiliser un même patch de référence et une
note tenue, attendre cinq secondes, noter moyenne et pic, puis ajouter un
module à la fois. Préciser modèle, firmware, Voices, Unison et paramètres.
Pour le Filter, mesurer quatre pôles avec modulation ; pour le Granulizer,
viser huit grains simultanés par voix. Mesurer les effets communs sur la même
source, à niveau constant, avec/sans effet. La différence donne un premier
surcoût ; garder le plus grand résultat sur plusieurs essais et vérifier
ensuite les combinaisons avant de considérer la calibration comme fiable.

### Première référence CPU : 16 Mio / firmware 1.5.0

L'éditeur utilise automatiquement les mesures utilisateur suivantes, uniquement
pour le modèle `modulox-brain-s3-32m16m` en version `1.5.0` :

| Notes | Osc seul | Chaîne avec reverb |
|---:|---:|---:|
| 0 | 1,7 % | 37 % |
| 1 | 10 % | 40 % |
| 8 | 40 % | 65 % |

Ces observations sont des charges moyennes rapportées ; les formes d'onde,
l'unisson, les paramètres de reverb et les pics ne sont pas entièrement décrits.
La calibration reste provisoire. Osc est estimé par interpolation entre
les observations à une et huit voix, après retrait des 1,7 points à vide.
Cette courbe inclut le chemin de rendu des voix : ce n'est pas une mesure
isolée de toutes les opérations de l'oscillateur. Une reverb est provisionnée
à 36 points, soit l'écart maximal observé arrondi vers le haut ; le filtre
reçoit une provision globale de 1 point, conformément au retour utilisateur.
La marge de 10 % s'applique ensuite aux coûts des modules. Osc + Filter +
Reverb à huit voix est ainsi estimé à 84,53 %, pour une limite de 85 %.
Les voix intermédiaires, plusieurs Osc, l'unisson et les combinaisons restent
des extrapolations à vérifier, et les autres modules restent à mesurer.

Les mesures locales saisies dans le navigateur sont prioritaires. Un nombre
pour Osc remplace sa courbe par un coût par voix ; un champ vide conserve la
courbe intégrée. Le filtre de référence est libellé « % total ». Les anciennes
calibrations manuelles par voix conservent leur interprétation. Aucune de ces
références n'est appliquée à la carte 8 Mio, à une autre version de firmware
ou hors ligne sans version connue. Aucune mise à jour du firmware n'est
nécessaire pour ce changement de calibration de l'éditeur.

## Nouveaux effets (firmware 1.6.0)

La famille **Effects** propose Decimator (Rate/Mix), Bitcrusher (Bits/Mix),
Compressor (Threshold/Ratio/Attack/Release/Makeup/Mix) et TB-303
(Cutoff/Reso/Drive/Mix). Compressor est stéréo et commun aux voix ; les trois
autres sont mono. Utiliser Stereo ou Pan avant Compressor, et Mono avant
un insert mono placé après une chaîne stéréo.

Rate, Bits, Threshold, Cutoff et Reso exposent chacun une entrée CV ; les
réglages restants acceptent MIDI Learn. L'envoi à un ancien firmware est
bloqué avec un message de mise à jour, mais la préparation hors ligne reste
possible. Ces modules ne réservent aucun buffer PSRAM et gardent leur marge
RAM par module. Les coûts CPU restent à mesurer, sans pourcentage inventé.
La référence utilisateur 16 Mio/1.5.0 est réutilisée provisoirement en 1.6.0
pour les seuls anciens modules, avec un avertissement explicite.

Le filtre TB-303 adapte uniquement le noyau MIT Open303 TeeBee, en float
et avec suréchantillonnage x4. Le signal sec est conservé à Mix=0. Il ne
reproduit pas la voix complète de JC303 et ses réglages de résonance/drive
sont bornés pour le moteur embarqué. Voir les précisions du firmware et
`firmware/third_party/open303/README.md` pour la source et les adaptations.
