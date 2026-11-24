# Deux modèles Modulox

Le code du moteur reste commun. Les profils de `boards/profiles.json` définissent les identités et les capacités ; les fichiers `.defaults` règlent le bus mémoire. Aucun eFuse n'est modifié.

| Profil | Flash | PSRAM | Flash / PSRAM | Partition samples |
|---|---:|---:|---|---:|
| `s3-16m8m` (ancien, défaut) | 16 Mio | 8 Mio | Quad / Octal 3,3 V | 10 Mio |
| `s3-32m16m` | 32 Mio | 16 Mio | Octal / Octal 1,8 V | 26 Mio |

La PSRAM est à 40 MHz et la flash à 80 MHz. Le CPU reste à 240 MHz. Les limites par sample (1 Mio), le moteur audio et les 64 emplacements de patches restent communs pour cette première étape. La mémoire supplémentaire permettra des fonctions adaptées plus tard ; elle n'augmente pas à elle seule la puissance de calcul.

## Compiler depuis la racine du dépôt

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File firmware/idf.ps1 build
powershell -NoProfile -ExecutionPolicy Bypass -File firmware/idf.ps1 -Model s3-32m16m build
```

L'ancien utilise `firmware/build` et `firmware/sdkconfig`. Le nouveau utilise `firmware/build-s3-32m16m` et `firmware/sdkconfig.s3-32m16m`. Ne pas réutiliser la configuration d'un modèle pour l'autre. Une vérification CMake refuse les principales incohérences. Sur une installation neuve, chaque configuration est produite à partir des valeurs communes et du profil.

Avec le Python de l'environnement ESP-IDF :

```powershell
python firmware/tools/sync_boards.py
python firmware/tools/release.py
python firmware/tools/release.py --model s3-32m16m
python firmware/tools/build_factory.py --model s3-32m16m
```

Les mises à jour 16/8 conservent leurs adresses publiques historiques. Les images 32/16 sont dans des sous-dossiers `s3-32m16m`, à la fois sous `firmware/releases/<version>` et dans `editor/dist/firmware` / `editor/dist/factory`.

## Compatibilité et sélection

En fonctionnement normal, `FWINFO` et le catalogue exposent le modèle, les capacités détectées et la disposition des partitions. L'éditeur utilise le modèle connecté pour choisir sa mise à jour. Les projets ESP-IDF ont des noms distincts : `modulox_sine` et `modulox_sine_32m16m`. Le firmware refuse lui-même une application dont le nom de projet ne correspond pas, en complément des validations de l'éditeur.

En installation usine, l'éditeur lit d'abord les informations ROM : taille JEDEC de la flash, capacité et variante de PSRAM embarquée, type de bus flash. Il télécharge ensuite uniquement l'image de ce modèle. Un modèle inconnu ou une image absente provoque un refus, jamais un repli vers l'autre image. Les protections existantes (contrôle sécurité, SHA-256, consentement EFFACER, vérification MD5 complète) restent actives. L'identification seule n'écrit rien.

La carte 32/16 branchée le 23 novembre 2026 contenait initialement une disposition Arduino (`app0`, `app1`, `ffat`) différente. Une carte encore dans cette disposition nécessite une **installation usine complète avec effacement des données de la carte**. Une mise à jour A/B ou une récupération sans effacement ne constitue pas une migration de cette ancienne disposition.

Les deux dispositions gardent deux slots OTA de 1,5 Mio et une application de secours de 1 Mio. Sur le 32/16, les slots sont à `0x1c00000` et `0x1d80000`. SPIFFS utilise des pages de 512 octets pour adresser les 26 Mio de samples sans déborder son index de pages de 16 bits. Les partitions SPIFFS sont donc reconstruites, pas agrandies par simple remplissage.

## Images usine

L'image 16/8 déjà extraite et vérifiée sur matériel est conservée. `build_factory.py` produit une nouvelle image propre à partir de la compilation du modèle et des 40 presets, avec leurs quatre ondes et trois samples. Il ne copie pas les calibrations ou les données personnelles d'une autre carte. L'image démarre sur OTA A à confirmer par les autotests, avec le même firmware dans le secours et le slot B.

Le champ `hardwareValidated: false` signale une image reconstruite dont le premier démarrage doit encore être testé sur la carte correspondante. Une compilation réussie et les tests de fichiers ne remplacent pas cet essai matériel. Ne pas utiliser une image 32/16 en production série avant cette validation.

Sources : [configuration flash/PSRAM ESP-IDF 5.4.1](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/api-guides/flash_psram_config.html), [détection des capacités dans esptool](https://github.com/espressif/esptool/blob/master/esptool/targets/esp32s3.py).

## Passage du port de démarrage à MIDI + CDC (1.3.2)

Sur le 32/16 de test, Windows conservait le périphérique Serial/JTAG de la ROM (`303a:1001`) après le passage au contrôleur OTG. Le démarrage et TinyUSB étaient pourtant initialisés et confirmés. Les états enregistrés montraient qu'aucune requête du PC n'arrivait au périphérique TinyUSB. Une pause après la seule désactivation des pads Serial/JTAG ne résolvait pas ce cas.

L'initialisation effectue maintenant une vraie déconnexion du contrôleur avec `tud_disconnect()`, attend 500 ms, puis appelle `tud_connect()` et annonce explicitement le périphérique Full Speed sur D+. Windows peut alors énumérer le nouveau périphérique `303a:4009` avec MIDI et CDC. Cette séquence s'exécute une seule fois au démarrage et ne change pas la boucle audio.

Les profils Arduino mémorisés ont été comparés : les projets 16 et 32 Mio utilisent tous deux USB-OTG (TinyUSB) et CDC activé ; les différences principales portent bien sur les mémoires QIO/OPI et les capacités. La préparation Full Speed explicite reprend celle du HAL Arduino.

`MODULOX_USB_DIAGNOSTICS=ON` est une option de développement : elle enregistre trois états USB dans l'espace NVS `usb_diag`. Elle est désactivée dans les versions normales, et `release.py` refuse de publier une compilation de diagnostic.
