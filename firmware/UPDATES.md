# Mises à jour du firmware Modulox

## Usage côté utilisateur

Sur l’accueil de l’éditeur, connecter le Modulox en USB. Le panneau Firmware
affiche la version installée, la version disponible et la confirmation du
démarrage. **Mettre à jour** télécharge le fichier publié sur le même serveur,
vérifie son contenu puis demande une confirmation avant transfert. La même
version peut être réinstallée. Une version proposée par le serveur qui est plus
ancienne que celle installée est bloquée ; un fichier choisi explicitement
permet un retour volontaire à une ancienne version compatible.

**Choisir un fichier** ouvre une sauvegarde `.modulox-fw.json` contenant
manifest et image. Cette méthode fonctionne hors ligne, y compris dans le ZIP
Windows autonome. Aucun outil Python n’est nécessaire pour les mises à jour
ordinaires. L’éditeur envoie l’image par le CDC USB déjà utilisé pour les patches.

Le transfert peut être annulé avant sa validation finale. Une erreur, une
déconnexion ou un mauvais SHA abandonne l’emplacement en cours d’écriture et
ne sélectionne pas son contenu au démarrage. Après validation, le Modulox
redémarre et l’éditeur essaie de le reconnecter. Si le navigateur demande de
choisir de nouveau le port, cliquer sur Connecter en USB. Le succès n’est annoncé
qu’après relecture de la version, de l’identifiant de compilation complet et
de l’état confirmé. **Version précédente** demande un retour au firmware valide
précédent ; il peut s’agir du firmware factory si aucun autre OTA n’est valide.

## Organisation de la flash 16 Mio

Les adresses des anciennes partitions sont explicitement conservées.

| Partition | Adresse | Taille | Usage |
| --- | --- | --- | --- |
| nvs | 0x009000 | 24 Kio | Réglages existants |
| phy_init | 0x00f000 | 4 Kio | Calibration existante |
| factory | 0x010000 | 1 Mio | Firmware de secours existant |
| patches | 0x110000 | 128 Kio | Presets d’usine existants |
| userpatches | 0x130000 | 128 Kio | Patches personnels existants |
| waves | 0x150000 | 640 Kio | Formes d’onde existantes |
| otadata | 0x1f0000 | 8 Kio | Sélection et validation du démarrage |
| samples | 0x200000 | 10 Mio | Samples existants |
| ota_0 (A) | 0xc00000 | 1,5 Mio | Firmware A |
| ota_1 (B) | 0xd80000 | 1,5 Mio | Firmware B |

Le dernier Mio reste libre. Le firmware courant est toujours conservé pendant
l’écriture de l’autre emplacement. La limite de 1 Mio de factory n’empêche pas
les futurs firmwares OTA d’atteindre 1,5 Mio : ESP-IDF avertit si factory est trop
petit mais accepte une image qui tient dans les partitions OTA. Le firmware
factory de secours doit, lui, continuer à tenir dans sa partition.

## Confirmation du démarrage et rollback

Le bootloader ESP-IDF est compilé avec `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`.
Le firmware appelle `esp_ota_set_boot_partition()` uniquement après contrôle du
SHA-256, validation ESP-IDF de l’image et fin de l’écriture. Le bootloader fait
passer la nouvelle image de NEW à PENDING_VERIFY à son premier démarrage.

Une image en attente est confirmée après au moins cinq secondes et après :

- accès aux magasins de patches, waves et samples, PSRAM comprise ;
- configuration du codec sans erreur I²C ;
- création des tâches de contrôle ;
- installation de TinyUSB, sans exiger qu’un ordinateur soit connecté ;
- au moins 100 blocs I²S écrits, avec une écriture audio récente.

La confirmation utilise `esp_ota_mark_app_valid_cancel_rollback()`. Si elle
n’arrive pas dans les 30 secondes, un timer provoque un redémarrage. Un panic,
un reset watchdog ou un redémarrage avant confirmation conduit aussi le
bootloader à rejeter l’image en attente. Un crash après confirmation ne déclenche
pas automatiquement un retour arrière : cette protection porte sur le premier
démarrage. Ces vérifications ne prouvent pas la qualité sonore ou l’absence de
tout bug fonctionnel.

Pendant un transfert, le rendu audio est silencieux et les commandes de notes
sont ignorées. Les enregistrements, suppressions et sauvegardes de patches sont
bloqués. Le firmware actif n’est jamais écrasé par le protocole USB.

## Première migration d’un Modulox existant

Cette étape est une opération de préparation distincte. Les anciens firmwares
ne connaissent pas les commandes de mise à jour A/B. Le script nécessite
l’environnement Python ESP-IDF avec `esptool` et l’accès au port ROM USB.

1. Compiler normalement : `powershell -ExecutionPolicy Bypass -File firmware/idf.ps1 build`.
2. Préparer la distribution : `python firmware/tools/release.py`.
3. Fermer la connexion USB de l’éditeur. Débrancher le Modulox, maintenir
   l’encodeur argenté enfoncé, rebrancher l’USB, puis relâcher l’encodeur.
4. Avec le Python ESP-IDF, lancer `firmware/tools/migrate_ab.py --port COMx`.
5. Le script sauvegarde d’abord les 16 Mio dans `firmware/backups`, avec la MAC,
   le SHA-256 et les empreintes des partitions protégées.
6. Il refuse un partitionnement inconnu, une réserve OTA non vide, un autre
   modèle de puce ou un kit corrompu. Le mode `--check-only` prépare et contrôle
   le plan sans effectuer d’écriture en flash.
7. Il écrit seulement le bootloader, la table et l’application A. Il relit
   ensuite factory et les partitions de données pour les comparer au backup.
8. Il sélectionne A seulement après cette comparaison, en inscrivant NEW dans
   les métadonnées. Redémarrer avec RESET sans maintenir BOOT si nécessaire.

La migration ne lance jamais `erase_flash` et ne flashe jamais `patches.bin`.
Elle laisse l’ancien firmware factory intact comme premier secours. Elle ne
doit être exécutée qu’une fois. Un backup réutilisé avec `--backup` doit provenir
de la même carte et avoir son fichier `.device.json`. Les backups restent privés
et sont exclus de Git et des ZIP distribués.

Pour une carte neuve en fabrication, le flash ESP-IDF complet peut initialiser
factory et les presets. Les mises à jour clients suivantes passent par USB A/B.

## Récupération depuis Chrome

Si le firmware ne répond plus, choisir d’abord une version publiée ou un fichier
de mise à jour, puis **Récupération USB (BOOT)**. Le dialogue montre comment
maintenir l’encodeur argenté pendant le branchement USB-C. Après avoir relâché
l’encodeur, sélectionner le port ROM `303a:1001`.

Le module utilise une copie locale d’esptool-js 0.7.1 et de pako 2.1.0, avec leurs
licences dans `editor/dist/vendor/esptool`. Aucun CDN n’est utilisé. Il contrôle
le modèle ESP32-S3, la taille de flash, les adresses du partitionnement A/B et
les métadonnées. Il conserve l’emplacement confirmé le plus récent, écrit
l’autre emplacement puis relit l’image pour vérifier le SHA. Il n’active la
nouvelle image qu’ensuite, en écrivant le secteur OTA alternatif. Factory et les
données ne sont pas écrits. Si le reset logiciel laisse la carte en mode ROM,
appuyer sur RESET sans BOOT, puis reconnecter l’éditeur.

La récupération depuis Chrome ne transforme pas un ancien partitionnement :
la migration initiale reste nécessaire. En atelier, un backup complet et
esptool permettent également une restauration par USB ROM.

## Versions et publication

`firmware/version.txt` est la version SemVer inscrite dans le descripteur ESP-IDF.
Après compilation, `python firmware/tools/release.py` génère :

- `firmware/releases/<version>` : image, manifeste, fichier autonome et kit de migration ;
- `editor/dist/firmware/latest.json` : version, modèle, layout, protocole, taille,
  SHA-256, identifiant ELF complet, nom du binaire et notes de version ;
- le binaire et le fichier `.modulox-fw.json` dans le même dossier public.

Publier `editor/dist` sur le serveur HTTPS de l’éditeur suffit à proposer cette
version aux utilisateurs connectés. Le manifeste est consulté sans envoyer la
version du synthé ni les patches au serveur : la comparaison se fait dans le
navigateur. Le ZIP autonome inclut la version publiée au moment de sa création.
Pour découvrir une version plus récente depuis une ancienne copie hors ligne,
il faut obtenir un nouveau fichier ou une nouvelle copie de l’éditeur.

Le SHA-256 contrôle l’intégrité. Il ne remplace pas une signature de publication.
Le système actuel s’appuie sur une distribution de confiance (HTTPS ou fichier
fourni par le fabricant) ; Secure Boot et signature cryptographique ne sont pas
activés par cette modification.

## Tests et état de validation

Tests automatiques : `node editor/tests/firmware.test.mjs`,
`node editor/tests/firmware-recovery.test.mjs`,
`python firmware/tests/migration_test.py` et `node editor/tests/offline.test.mjs`.

Essais réalisés sur le Modulox ESP32-S3 16 Mio / PSRAM 8 Mio : backup complet,
comparaison de toutes les partitions protégées pendant migration, premier
démarrage confirmé, abandon volontaire, SHA incorrect rejeté, transfert USB
réussi vers l’autre emplacement, retour explicite au firmware précédent,
retour automatique après une image volontairement non confirmée, puis
réinstallation et confirmation du firmware de production. Les 16 patches
présents ont été relus après les essais. Chrome a lu la version et affiché
« À jour » ; le dernier clic de transfert dans Chrome n’a pas été automatisé,
car le contrôle de navigateur a interrompu la session faute de certitude sur
l’URL. Le même protocole de transfert a été éprouvé sur USB par le client CLI.
La récupération ROM JavaScript est testée sur ses plans d’écriture et imports
hors ligne, mais son parcours matériel complet reste à valider dans Chrome.
Aucune coupure physique pendant l’écriture n’a été provoquée durant ces essais.

L’option CMake `MODULOX_ROLLBACK_TEST=ON` crée uniquement une image de test dont
la confirmation est volontairement inhibée et le délai ramené à sept secondes.
Le générateur de release refuse de publier cette configuration. Revenir à OFF
avant de compiler la production. `tools/usb_update.py --hardware-test --image
<production.bin> --rollback-test-image <test.bin>` reproduit les essais USB,
mais réécrit les emplacements OTA : usage atelier uniquement.
