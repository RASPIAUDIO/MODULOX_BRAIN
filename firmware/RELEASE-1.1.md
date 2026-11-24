# Firmware 1.1.0

Cette version est préparée pour une installation par USB depuis l'éditeur.
La carte de test n'est pas flashée automatiquement : elle peut rester en 1.0.0
jusqu'au test utilisateur. Le partitionnement A/B et le protocole restent les mêmes.

## Navigation

- Les modules d'un même type se suivent dans le menu, sans changer leurs
  identifiants DSP, les connexions ou le MIDI Learn.
  Les blocs sans paramètres (MIDI, Stereo, Mono) n'encombrent plus cette liste.
- Les listes Modules, Categories et Patches affichent cinq lignes de 38 pixels.
  Les pages de paramètres ne changent pas.
- Appui sur l'encodeur : catégories. Tourner : parcourir. Bouton central :
  entrer dans la catégorie, puis charger le patch choisi.
- Appui sur l'encodeur depuis les patches : retour aux catégories. Depuis les
  catégories : retour aux paramètres.
- User est toujours en premier, même vide. Les autres catégories vides sont
  masquées. Chaque catégorie indique son nombre de patches.

Dans l'éditeur, **Réglages du patch → Catégorie** choisit User, Bass, Lead,
Pads, Keys, Pluck, Arp, FX ou Drums. Sauvegarder le fichier local et renvoyer le
patch pour appliquer le classement sur le Modulox. Les 40 presets de l'éditeur
sont classés. Les patches déjà sur la carte sans catégorie restent dans User :
aucune réécriture automatique des données n'est faite par le firmware.

## Capacité

La limite passe de 16 à **64 patches au total**, exemples d'usine compris.
L'index statique passe d'environ 1,25 Kio à 5,25 Kio ; les JSON ne sont pas tous
chargés en RAM. La partition `userpatches` reste à 128 Kio et ses adresses sont
inchangées. Samples et cycles PCM restent dans leurs partitions séparées.

Le générateur NVS officiel ESP-IDF valide une banque de 64 patches représentatifs
de 90 199 octets de JSON, ainsi qu'une banque de 64 copies du plus gros preset
de 114 048 octets de JSON. Ces chiffres incluent le champ catégorie, et les
images générées tiennent dans 128 Kio avec les métadonnées NVS. Il ne s'agit pas
d'une garantie pour 64 patches de taille maximale : 64 × 4096 octets ne tiennent
pas dans cette partition. Le firmware refuse un enregistrement si les places
ou la flash sont épuisées. Une modification ultérieure grossissant les patches
peut nécessiter d'en supprimer un pour garder de la place.

Les données restent lisibles par la version 1.0 en cas de retour arrière ;
celle-ci n'affichera toutefois que sa limite de 16 patches et ignorera les
catégories. Le classement est inclus dans le JSON, sans format de compression
ou nouvelle migration de partition.

## Test de mise à jour

1. Recharger l'éditeur, puis connecter le Modulox encore en 1.0.0.
2. Le panneau Firmware doit proposer 1.1.0. Cliquer sur **Mettre à jour**.
   Hors ligne, choisir `modulox-1.1.0.modulox-fw.json`.
3. Après redémarrage, vérifier **Installé : 1.1.0 · démarrage confirmé**.
4. Vérifier les cinq lignes des menus et les oscillateurs regroupés.
5. Ouvrir les catégories : les anciens patches se trouvent dans User. Envoyer
   un preset Bass et un preset Lead depuis l'éditeur pour faire apparaître
   ces catégories, puis les charger depuis les boutons de l'appareil.

La compilation et les tests sur PC précèdent la distribution. La validation
visuelle sur l'écran du Modulox et le chargement USB de cette nouvelle version
sont laissés à l'essai utilisateur.
