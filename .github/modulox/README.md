# Publier Modulox 2.0

L’action [Publier Modulox](https://github.com/RASPIAUDIO/MODULOX_BRAIN/actions/workflows/publish-modulox.yml)
publie le dernier commit de `Modulox-Brain-2.0` sur le
[configurateur](https://raspiaudio.com/modulox/configurateur/).

1. Pousse les modifications sur `Modulox-Brain-2.0`. Pour l’éditeur, lance
   `node editor/build-web.mjs` si les presets ou les exemples ont changé.
   Pour le firmware, suis la procédure de compilation du
   [profil matériel](https://github.com/RASPIAUDIO/MODULOX_BRAIN/blob/Modulox-Brain-2.0/firmware/BOARDS.md)
   et commit les fichiers générés dans `editor/dist/firmware/`, manifeste inclus.
2. Dans GitHub Actions, ouvre **Publier Modulox → Run workflow** et lance
   l’action depuis `main`. Un accès en écriture au dépôt suffit.
3. Attends l’étape de vérification HTTPS. Elle compare l’éditeur, les
   manifestes et les binaires servis. Le fichier
   [deployment.json](https://raspiaudio.com/modulox/configurateur/deployment.json)
   indique le commit publié.

L’action déploie le dossier statique avec les mises à jour des deux modèles et
leurs images usine. Le premier job prépare et vérifie le paquet. Le second
utilise le secret `MODULOX_DEPLOY_KEY` de l’environnement `modulox-production`
pour l’envoyer au compte SSH dédié. Le programme serveur vérifie le paquet,
bascule la publication d’un seul coup et conserve deux déploiements précédents.
