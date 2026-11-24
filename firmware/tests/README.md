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

`cmd /c firmware\tests\run-reverb-test.cmd` vérifie le DSP Reverb et son
intégration au moteur (huit voix, chaîne stéréo et queues après Note Off).
`node editor/tests/reverb.test.mjs` valide le graphe, les limites mono/stéréo,
les paramètres/CV/MIDI Learn et les exports/imports. Les tests DSP sur PC
ne mesurent pas la charge audio ni la bande passante PSRAM de la carte.

`node editor/tests/resources.test.mjs` vérifie les budgets RAM 8/16 Mio,
les buffers partagés et conservés, les marges et pics de chargement, la
fragmentation, les coûts CPU calibrés, la polyphonie/unisson, les refus
d’ajout et la sérialisation des échanges USB avec la télémétrie.

`cmd /c firmware\tests\run-insert-effects-test.cmd` vérifie Decimator,
Bitcrusher, Compressor et TB-303 : cadence entre blocs, niveaux signés,
bypass exact, compression liée L/R et release, comparaison au noyau MIT
Open303 original, stabilité/atténuation, huit voix, effets communs et bit
CV 63. Le script compile les deux fichiers amont de référence uniquement
sur PC. `node editor/tests/insert-effects.test.mjs` vérifie les paramètres,
les formats de ports, les modulations et le transport des patches.

`cmd /c firmware\tests\run-catalog-writer-test.cmd` vérifie la sérialisation
progressive du catalogue avec cJSON réel, la fusion des métadonnées, le
catalogue vide et le refus de dépassement du buffer.
