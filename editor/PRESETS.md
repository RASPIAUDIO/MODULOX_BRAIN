# Banque de 40 presets Modulox

Les 40 presets ont un volume général de 85 %. Dans **Presets d’usine**, le bouton
**Charger tous les presets sur le Modulox** prépare les ressources puis envoie
la banque avec une progression. Il vérifie les emplacements disponibles sans
supprimer de patches. Les copies anciennes non modifiées sont mises à jour ;
les copies personnelles modifiées sont conservées. Les emplacements créés par
ce bouton sont réutilisés lors des prochains chargements de la banque.
**Arrêter après ce patch** termine le patch en cours puis arrête l’envoi. Fermer
la fenêtre demande aussi l’arrêt. Après une interruption, relancer reprend
les presets manquants ou différents. Si la flash est pleine malgré les places
libres, l’envoi s’arrête et indique les écritures déjà confirmées.

Ouvrir **Presets d’usine**, choisir un nom, puis **Envoyer au Modulox**. Pour une copie personnelle sur disque : **Enregistrer dans mon dossier**.
Les formes AKWF et les textures granulaires necessaires sont importees automatiquement dans les bibliotheques du navigateur au chargement du preset, puis transferees par USB avec lui.
La banque est incluse dans l'éditeur et le ZIP hors ligne. Le firmware 1.1 accepte jusqu'à 64 patches (contre 16 en 1.0), selon l'espace flash disponible. Les 40 presets possèdent une catégorie pour les menus de l'appareil ; les copies personnelles restent dans le dossier choisi sur l'ordinateur.

Basses et leads : trois oscillateurs VA chacun. Les autres familles utilisent librement VA, tables AKWF, FM4, bruit, ring modulation et Granulizer.
Les basses et les solos mono ont un glide legato ; les nouvelles basses gardent leurs enveloppes en legato (Retrigger Off). Jouer les basses autour de MIDI 36–48, les leads/claviers/nappes autour de 60–72.
Les wobble et Radio Steps utilisent l'horloge interne a 120 BPM ; Clock/BPM restent modifiables dans les reglages du patch.

Les intitulés de timbre decrivent l'intention : l'ecoute finale et les ajustements restent a faire sur le Modulox.

| # | Famille | Preset | Intention |
| --- | --- | --- | --- |
| 1 | Basses | [VA Iron Bass](dist/presets/01-va-iron-bass.json) | Premier patch valide : basse agressive, deux pulses et triangle grave, filtre et distorsion. |
| 2 | Basses | [VA Analo Bass](dist/presets/02-va-analo-bass.json) | Basse ronde : deux saws, triangle grave, enveloppe courte. |
| 3 | Basses | [VA Wobble Bass](dist/presets/03-va-wobble-bass.json) | Balayage de filtre synchronise a la croche, distorsion douce. |
| 4 | Basses | [VA Acid Pulse](dist/presets/04-va-acid-pulse.json) | Carres et sinus grave, resonance et morsure percussive. |
| 5 | Basses | [VA Reese Motion](dist/presets/05-va-reese-motion.json) | Saws desaccordees en unisson, mouvement lent et chorus leger. |
| 6 | Basses | [VA Rubber Bass](dist/presets/06-va-rubber-bass.json) | Pulse et triangles : filtre elastique, attaque courte et glide appuye. |
| 7 | Basses | [VA Dub Sub](dist/presets/07-va-dub-sub.json) | Sinus grave et triangles, filtre ferme, echo discret. |
| 8 | Basses | [VA Hard Bite](dist/presets/08-va-hard-bite.json) | Saws et pulse, wavefolder modere puis saturation franche. |
| 9 | Basses | [VA Pulse Driver](dist/presets/09-va-pulse-driver.json) | Basse pulse avec PWM lente, tenue stable et attaque de filtre. |
| 10 | Leads | [VA Velvet Solo](dist/presets/10-va-velvet-solo.json) | Lead mono doux, vibrato discret, glide legato et echo stereo. |
| 11 | Leads | [VA Wide Saw](dist/presets/11-va-wide-saw.json) | Trois saws, unisson dose, chorus et ping-pong. |
| 12 | Leads | [VA PWM Runner](dist/presets/12-va-pwm-runner.json) | Lead pulse articule, PWM lente et delay synchronise. |
| 13 | Leads | [VA Acid Scream](dist/presets/13-va-acid-scream.json) | Lead mono resonant, distorsion et echo court. |
| 14 | Leads | [VA Octave Stack](dist/presets/14-va-octave-stack.json) | Saws sur trois octaves pour un solo present et large. |
| 15 | Leads | [VA Glass Glide](dist/presets/15-va-glass-glide.json) | Sinus et triangles, attaque douce et long glide de 180 ms. |
| 16 | Leads | [VA Retro Brass](dist/presets/16-va-retro-brass.json) | Attaque de cuivre, saws legerement detunees et chorus vintage. |
| 17 | Leads | [VA Fifth Flight](dist/presets/17-va-fifth-flight.json) | Lead a la quinte avec pulse et saw, ping-pong ample. |
| 18 | Nappes | [PAD Analog Dawn](dist/presets/18-pad-analog-dawn.json) | Trois saws, ouverture lente, unisson et chorus ample. |
| 19 | Nappes | [PAD Glass Choir](dist/presets/19-pad-glass-choir.json) | FM douce a enveloppes longues, timbre anime et echo stereo. |
| 20 | Nappes | [PAD Orbit Strings](dist/presets/20-pad-orbit-strings.json) | Tables AKWF Stringbox avec vibrato lent, filtre doux et chorus. |
| 21 | Nappes | [PAD Grain Aurora](dist/presets/21-pad-grain-aurora.json) | Texture vocale C4 fournie, grains longs, balayage lent et stereo. |
| 22 | Nappes | [PAD Frozen Voices](dist/presets/22-pad-frozen-voices.json) | Tables vocales AKWF sur trois octaves, filtre lent et echo long. |
| 23 | Nappes | [PAD Midnight Drone](dist/presets/23-pad-midnight-drone.json) | Triangles et table granulaire, wavefolder leger et mouvement lent. |
| 24 | Nappes | [PAD Shimmer Tides](dist/presets/24-pad-shimmer-tides.json) | FM additive aux harmoniques douces, montee lente et ping-pong. |
| 25 | Claviers | [KEY FM Clav Dry](dist/presets/25-key-fm-clav-dry.json) | Clavinet FM sec et court, attaques brillantes et filtre doux. |
| 26 | Claviers | [KEY FM Clav Funk](dist/presets/26-key-fm-clav-funk.json) | Clavinet FM claquant avec saturation legere, rotary rapide et slap. |
| 27 | Claviers | [KEY EP Velvet](dist/presets/27-key-ep-velvet.json) | Piano electrique doux, tine courte puis corps long avec chorus. |
| 28 | Claviers | [KEY EP Neon](dist/presets/28-key-ep-neon.json) | Piano electrique plus brillant, tine metallique et chorus stereo. |
| 29 | Claviers | [KEY Tine Bell](dist/presets/29-key-tine-bell.json) | Cloche FM a deux paires, harmoniques longues et echo ping-pong. |
| 30 | Claviers | [KEY Music Box](dist/presets/30-key-music-box.json) | Boite a musique FM, attaque fine et decroissance sans sustain. |
| 31 | Claviers | [KEY Pluck Glass](dist/presets/31-key-pluck-glass.json) | Pluck FM percussif, profondeur qui chute vite et echo a la croche. |
| 32 | Claviers | [KEY Organ Rotary](dist/presets/32-key-organ-rotary.json) | FM additive utilisee en tirettes : fondamentale, octave, quinte, octave haute. |
| 33 | FX | [FX Noise Sweep](dist/presets/33-fx-noise-sweep.json) | Souffle filtre : ouverture de 1,8 s, amplitude douce et delay. |
| 34 | FX | [FX Ring Visitor](dist/presets/34-fx-ring-visitor.json) | Deux triangles multiplies, quinte metallique, animation et ping-pong. |
| 35 | FX | [FX Grain Storm](dist/presets/35-fx-grain-storm.json) | Texture de metal fournie, grains denses et dispersion animee. |
| 36 | FX | [FX Reverse Dust](dist/presets/36-fx-reverse-dust.json) | Poussiere rythmee fournie, lecture granulaire inverse, filtre passe-haut et grain sature. |
| 37 | FX | [FX Radio Steps](dist/presets/37-fx-radio-steps.json) | Random declenche a la croche, Slew et filtre passe-bande : brouillage musical. |
| 38 | FX | [FX Cosmic Rise](dist/presets/38-fx-cosmic-rise.json) | Tables granulaires et vocales, montee de hauteur et wavefolder leger. |
| 39 | Percussions | [PERC FM Kick](dist/presets/39-perc-fm-kick.json) | Kick FM accorde : jouer autour de C2/MIDI 36, chute de pitch et corps court. |
| 40 | Percussions | [PERC FM Metal Hit](dist/presets/40-perc-fm-metal-hit.json) | Percussion FM metallique, ratios eleves et echo court. |

## Ressources et regeneration

Quatre cycles AKWF de Kristoffer Ekstrand / Adventure Kid, pris dans la bibliotheque deja presente du projet, sont reechantillonnes a 1024 points. Attribution originale : `dist/presets/assets/AKWF-CREDITS.txt`. Le manifeste conserve les chemins sources exacts.
Trois textures C4 originales de trois secondes (choeur synthetique, metal et poussiere pulsee), PCM mono 16 bits/44,1 kHz, sont generees de maniere deterministe. Aucun telechargement ni sample externe n'est requis.
Les identifiants sont les sept premiers octets du SHA-256 du PCM. Le chargeur verifie tailles et empreintes avant de sauvegarder les ressources.
Regeneration : `node editor/build-presets.mjs` depuis la racine. Le premier preset valide n'est pas reecrit.
