# Installation complète du firmware usine

Le panneau Firmware contient une action distincte **Installation firmware
usine…**, utilisable dans Chrome/Edge, avec le même serveur local ou le ZIP
Windows hors ligne. Python n'est pas nécessaire dans le navigateur. La copie
locale d'esptool-js et l'image complète sont incluses dans la distribution.

Cette action remplace **la flash complète (16 ou 32 Mio selon le modèle)** du Modulox : bootloader, table
des partitions, versions de firmware, patches, samples, formes d'onde et réglages.
Elle reproduit le contenu du modèle d'usine fourni. Les données présentes sur
la carte cible sont perdues ; le retour arrière A/B ne permet pas de les retrouver.
Les fichiers de patches sauvegardés sur l'ordinateur restent disponibles.

## Parcours

1. Sauvegarder sur l'ordinateur les patches à conserver.
2. Ouvrir **Installation firmware usine…** et lire les avertissements.
3. Mettre le Modulox en BOOT : maintenir BOOT pendant RESET, puis relâcher BOOT.
4. Cliquer **Identifier le Modulox en BOOT** et choisir son port de programmation.
5. L'éditeur identifie d'abord le modèle par ses mémoires et son type de bus,
   puis charge uniquement l'image correspondante et la contrôle (taille,
   SHA-256, partitionnement, sélection de démarrage et descripteur du firmware).
   Il accepte les configurations connues 16/8 Mio et 32/16 Mio, et refuse les
   configurations inconnues, le démarrage sécurisé, un compteur de chiffrement
   non nul ou une sécurité qu'il ne peut pas vérifier. Jusqu'ici, aucun octet
   de flash n'est écrit.
6. Vérifier la MAC affichée, cocher l'acceptation du remplacement complet, puis
   saisir **EFFACER**. Cliquer **Effacer et installer l'image usine**.
7. L'éditeur écrit à l'adresse zéro en conservant les paramètres d'en-tête de
   l'image. Comme le fichier couvre toute la flash, celle-ci est remplacée,
   sans commande d'effacement préalable distincte. Il vérifie ensuite le digest
   MD5 de l'ensemble sur la carte, attendu dans le manifeste validé avec le binaire.
8. Après vérification, appuyer sur RESET sans BOOT si nécessaire, puis utiliser
   **Connecter en USB** dans l'éditeur pour contrôler le démarrage et la version.

La programmation ne peut pas être annulée après son lancement. La fermeture
du dialogue est bloquée pendant l'opération ; le navigateur demande confirmation
avant de quitter l'onglet. Une interruption après le début de l'écriture peut
laisser une carte non démarrable : la remettre en BOOT et relancer cette
installation complète. La récupération A/B ordinaire reste une autre action.

Pour programmer plusieurs cartes, refaire l'identification et la confirmation
pour chaque Modulox. Le texte EFFACER et la case ne sont jamais conservés d'une
carte à la suivante. Aucune eFuse ni adresse MAC matérielle n'est écrite.

## Image fournie et publication

L'image 16/8 conservée est la copie complète vérifiée du modèle, firmware 1.2.0
confirmé sur A et 1.1.0 confirmé sur B, avec ses données. Ce n'est pas un binaire
d'application destiné au bouton normal **Mettre à jour**.

L'image 32/16 est reconstruite depuis sa compilation et les ressources des
40 presets d'usine. Elle inclut son bootloader octal, ses deux slots de mise
à jour et sa partition samples de 26 Mio. Son premier démarrage doit être
validé sur matériel avant de l'employer en série. Voir `MODELES-MATERIELS.md`
dans le ZIP ou `firmware/BOARDS.md` dans le dépôt.

`python firmware/tools/publish_factory.py <dossier-export-complet>` publie
`editor/dist/factory/latest.json` et `modulox-usine-<version>.bin`. Le script
vérifie le SHA-256 de l'export et son partitionnement et exige une source
confirmée, non chiffrée et vérifiée contre sa flash. Les métadonnées publiques
ne contiennent ni MAC source, ni port, ni chemin de machine. Republier ces deux
fichiers ensemble et reconstruire le ZIP avec `editor/build-offline.ps1`.

Les tests vérifient validation des images, contrôles matériels, consentement,
absence de toute écriture pendant l'identification, effacement logique de
l'ensemble des 16 ou 32 Mio, vérification et nettoyage USB. Les parcours destructifs
sont testés avec un chargeur simulé ; le modèle n'est pas reflashé automatiquement.
