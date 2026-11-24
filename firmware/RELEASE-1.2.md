# Firmware 1.2.0

## Affichage

Les titres ont un bandeau gris avec bord clair et ombre en bas. Les boutons
des paramètres et la ligne sélectionnée des menus ont le même relief léger.
La jauge de charge audio reste visible dans le bandeau.

Les textes moyens et grands utilisent les polices Noto lissées déjà présentes
dans `libraries/groovuino` : Sans Bold 15/36 et Sans Mono 20. Un petit corps de
secours évite de couper les légendes longues des boutons. Les glyphes ASCII
sont préparés en flash par `tools/display_fonts.py`. Aucun chargement de fichier,
allocation de police ou redimensionnement de glyphes ne se fait pendant le jeu.
Les minuscules indicateurs gardent la police 5×7 existante.

## Courbe du filtre

La courbe suit cutoff, résonance et ordre sur une échelle logarithmique de
20 Hz à 18 kHz : passe-bas, passe-haut, passe-bande ou bypass. Le mode et l'ordre
restent indiqués en petit au-dessus des quatre boutons. La pente est indiquée
en dB/octave pour passe-bas/passe-haut, en pôles pour passe-bande. La valeur du
paramètre sélectionné reste affichée au-dessus de la courbe, sans popup.

Avec une voix active, l'aperçu utilise les coefficients réellement appliqués,
donc le LFO ou l'enveloppe sur cutoff/résonance et le lissage sont inclus.
En polyphonie, une seule courbe représente la dernière voix encore active.
Un filtre partagé après les effets utilise les coefficients du filtre partagé.
Sans voix active, l'écran affiche les réglages de base. `live` signale une
capture active dans la légende.

## Performance

- Aucune nouvelle vérification, modulation ou courbe n'est ajoutée dans la
  boucle de rendu audio. La tâche d'affichage, sur le cœur 0 à priorité basse,
  demande au plus une capture toutes les 100 ms, uniquement sur la page Filter.
- Une commande dans la file existante copie une cinquantaine d'octets sur
  le cœur audio. Aucun coefficient supplémentaire n'y est calculé. Une place
  est ajoutée à la file pour conserver les 32 places MIDI/contrôle existantes.
  L'envoi ne bloque jamais l'écran.
- L'échange des captures utilise un protocole producteur/consommateur avec
  lectures/écritures atomiques alignées, sans verrou ni allocation. L'assemblage
  ESP32-S3 du producteur montre des copies mémoire et barrières, sans section
  critique ni auxiliaire atomique.
- La tâche d'affichage calcule 96 points, interpolés vers les 280 pixels du LCD.
  La trigonométrie est précalculée une fois. Les courbes et légendes inchangées
  ne sont pas retransmises au LCD.
- À partir de 80 % de charge audio mesurée, l'animation revient aux réglages
  de base et les captures sont suspendues. Elles reprennent sous ce seuil.
  La jauge existante se met à jour chaque seconde.

Le tampon DMA de huit lignes est conservé, sans framebuffer de 134 Ko.
La pile d'affichage passe de 4 à 6 Kio ; la table précalculée occupe environ
1,5 Kio. Les glyphes et métriques occupent environ 71 Ko de flash, dans les
emplacements firmware existants de 1,5 Mio. Le stockage des patches ne change pas.

## Validation et installation

`tests/run-display-test.cmd` vérifie les formes des courbes, les modulations
LFO/enveloppe sur quatre voix et l'identité de chaque échantillon audio avec
et sans capture. Il teste aussi 20 000 échanges concurrents, les limites du
rendu des textes et le lissage alpha. Les images de test utilisent le même
rasteriseur que le firmware ; `tools/render_display_preview.py` produit un PNG.

Un benchmark PC à huit voix compare les exécutables 1.1 et 1.2 en alternance :
médianes d'environ 127 ms pour 25 000 blocs en 1.1 et avec capture en 1.2,
avec un signal de contrôle identique. Cela ne remplace pas une mesure de temps
réel sur l'ESP32-S3. Lisibilité sur le panneau physique et charge avec des
patches extrêmes restent à vérifier sur le Modulox après installation.

Les partitions et le protocole de mise à jour A/B restent identiques.
Cette distribution ne flashe pas automatiquement l'appareil. Utiliser le
panneau Firmware ou le fichier `modulox-1.2.0.modulox-fw.json` pour le test.
