# openRBRVR

![openRBRVR logo](img/openRBRVR.png)

Open source VR plugin for Richard Burns Rally.

## Fonctionnalités

- De manière général les performances semblent mieux comparées à RBRVR. Sur certaines spéciales la différence de performances est grande, sur d’autres les performances sont plutôt similaires. Je n’ai pas trouvé de spécial qui tournerai beaucoup moins bien sur openRBRVR.
- L'image est plus nette sur openRBRVR avec la même résolution comparé à RBRVR.
- Serveur Vulkan via dxvk ([fork](https://github.com/TheIronWolfModding/dxvk) par TheIronWolf qui a ajouté le support D3D9 VR).
- Le plugin PaceNote UI fonctionne correctement.
- Gaugerplugin fonctionne correctement (mais a des impacts sur la performance).

## Instructions d’installation

Les instructions d’installation supposent que openRBRVR est installé par-dessus le plugin [RallySimFans](https://rallysimfans.hu).

- Désactiver « Réalité Virtuelle » du launcher RSF
- Copier d3d9.dll à la racine du dossier RBR
- Copier openRBRVR.dll et openRBRVR.ini dans RBR/Plugins

Si RBRVR est installé:

- Renommer openvr_api.dll en openvr_api.dll.rbrvr
- Copier openvr_api.dll à la racine du dossier RBR

Pour re activer RBRVR:

- Effacer d3d9.dll (de openRBRVR)
- Effacer openvr_api.dll (de openRBRVR)
- Renommer openvr_api.dll.rbrvr en openvr_api.dll
- Activer « Réalité Virtuelle » du launcher RSF

Note : éviter de lancer le launcher RSF lorsque openRBRVR est installé, cela peut causer des interférences et écrire par-dessus les fichiers ou les paramètres. Toujours lancer depuis `RichardBurnsRally_SSE.exe`.

## Setup

Dans `Plugins/openRBRVR.ini` vous pouvez changer les valeurs suivantes :

- `superSampling` paramètres supersampling pour le HMD. Je le fais tourner à 2.0 sans problème (SteamVR SS 100%). Par défaut il est à 1.0
- `menuSize` la taille du menu en 2D lorsque le menu est visible. Par défaut il est à 1.0
- `overlaySize` la taille de l’overlay en 2D lorsque l’overlay est visible en course. Par défaut il est à 1.0
- `overlayTranslateX` la translation de l'overlay suivant X (en 2D)
- `overlayTranslateY` la translation de l'overlay suivant Y (en 2D)
- `overlayTranslateZ` la translation de l'overlay suivant Z (en 2D)
- `lockToHorizon` pour bloquer le mouvement du casque pour que le pilote ne bouge pas avec la voiture mais que la voiture tourne autour de la tête du pilote. Les valeurs supportées sont 1,2 et 3 avec 1 qui verrouille l'axe de roulis, 2 verrouille le tangage et 3 verrouille les deux.
- `debug` activer pour afficher les informationsde debug sur l'écran

Avec le FoV du bureau, les objets peuvent disparaitre de votre champ de vision avant qu’ils ne sortent du cadre. Sur certaines spéciales cela peut se produire plus fréquemment que sur d’autres. Cela peut être résolu depuis le launcher RSF, NGPCarMenu, ou en course en utilisant le plugin de PaceNote (double clic droit pour l’ouvrir). Les valeurs entre 2.3 et 2.6 fonctionnent bien pour le FoV large de mon casque.

### Remplacement du FoV avec le launcher RSF

Le plugin openRBRVR n’est pas supporté par RSF. Il va écrire par-dessus dd9.dll (et certains autres fichiers si vous activer accidentellement Réalité Virtuelle sur le launcher). Pour remplacer le FoV:

- Dans le launcher RSF, mettre une grande valeur dans "Remplacer la valeur du champ de vision".
- Fermer le launcher RSF.
- **Copier à nouveau openRBRVR d3d9.dll dans le dossier RBR**.
- Lancer `RichardBurnsRally_SSE.exe`.

## Les bugs connus et les limites

- La position du siège ne peut pas être modifié via le clavier. Le plugin PaceNote doit être utilisé à la place. L’UI fonctionne correctement.
- La vue en VR ne peut être recentrer que part Options -> Plugins -> openRBRVR -> Reset view. Le plugin RBRControls (inclu dans RSF install) aura une option pour recentrer la vue à partir d'une touche du volant dans une prochaine mise à jour.

## Instructions du build

Le projet utilise CMake. Je l’ai développé avec Visual Studio 2022 community edition qui possède le support du module CMake.

Pour configurer d3d9.dll, le `dxvk` utilise meson. J’ai utilisé `meson setup
--backend=vs2022 --build_type=release` pour le configurer.

## Remerciements

- [Kegetys](https://www.kegetys.fi/) pour RBRVR, snous montrant ce qui est possible d’accomplir.
- [TheIronWolf](https://github.com/TheIronWolfModding) pour avoir patché le support VR pour D3D9 dans dxvk.
- Towerbrah pour l’idée d’implémenter le support VR utilisant le patch de TheIronWolf et pour l'aide corrigeant les problèmes de RBRHUD+RBRRX.
- [mika-n](https://github.com/mika-n) pour l’open source de [NGPCarMenu](https://github.com/mika-n/NGPCarMenu).

## License

La licence est sous Mozilla Public License 2.0 (MPL-2.0). Le code source pour tous les travaux dérivés doivent être publiques.
