# Tests de rendu sur PC

`cmd /c firmware\tests\run-display-test.cmd` valide la courbe du filtre,
les captures LFO/enveloppe sans modification du signal, les échanges inter-cœurs
et le rasteriseur Noto lissé. Il génère quatre PPM dans `build`, assemblables par
`python firmware/tools/render_display_preview.py`. `run-display-bench.cmd
/DPREVIEW_BENCH` mesure le moteur à huit voix avec/sans capture à 10 Hz ; un
benchmark PC ne remplace pas la mesure du budget temps réel sur la carte.

`cmd /c firmware\tests\run-navigation-test.cmd` vérifie l'ordre des modules
sur des masques complets et partiels, les catégories vides, User en premier et
les pages de cinq lignes jusqu'au 64e patch. `node editor/tests/categories.test.mjs`
vérifie les catégories des presets, la compatibilité des anciens fichiers et
la conservation du classement après export/import.

Avec le Python ESP-IDF, `firmware/tests/patch_capacity_test.py` produit des
partitions NVS de test dans `build/capacity-test` via le générateur officiel :
64 patches représentatifs et 64 copies du plus gros preset tiennent dans
128 Kio. 64 blobs de 4096 octets dépassent cette capacité. Ce test ne mesure
pas l'usure, la fragmentation ou les écritures répétées sur une carte réelle.

Depuis PowerShell, exécuter `cmd /c firmware\tests\run-glide-test.cmd` à la
racine du dépôt. Requiert les outils C++ de Visual Studio 2022 Build Tools
(ou un terminal développeur Visual Studio) et les composants ESP-DSP déjà
téléchargés par un premier build ESP-IDF.

`glide_test.cpp` vérifie la transition progressive des coefficients du filtre,
le suivi des hauteurs fractionnaires et la continuité du premier échantillon
lors d'un glide mono avec changement de vélocité. Le moteur réel est rendu
dans les chemins audio simple et graphe stéréo, et le niveau final est vérifié.
Les fichiers `host` remplacent les déclarations matérielles ; le filtre utilise
les générateurs de coefficients et le biquad ANSI réels d'ESP-DSP, à la place
du biquad assembleur Xtensa. Ces tests ne remplacent pas l'écoute sur la carte.

`cmd /c firmware\tests\run-preset-bank-test.cmd` valide les 40 JSON et leurs
ressources avec Node, puis rend chacun cinq secondes dans le moteur réel :
note seule, accord/jeu mono, relâchement et queues d’effets. Les empreintes
PCM, le cache, les erreurs de chargement, les limites de taille et les trois
oscillateurs des basses/leads sont vérifiés. Le rendu doit être non silencieux
et ne contenir aucun échantillon à pleine échelle. Les fixtures C++ sont
générées depuis les fichiers JSON exacts de la banque dans `build`.
