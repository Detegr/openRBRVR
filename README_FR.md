# openRBRVR

[🇬🇧](README.md) - [🇨🇿](README_CZ.md) - [🇫🇷](README_FR.md)

![openRBRVR logo](img/openRBRVR.png)

Plugin open source VR pour Richard Burns Rally.

## Fonctionnalités

- De manière générale les performances semblent mieux comparées à RBRVR. Sur certaines spéciales la différence de performances est grande, sur d’autres les performances sont plutôt similaires.
- L'image est plus nette sur openRBRVR avec la même résolution comparé à RBRVR.
- Serveur Vulkan via dxvk ([fork](https://github.com/TheIronWolfModding/dxvk) par TheIronWolf qui a ajouté le support D3D9 VR).
- Le plugin PaceNote UI fonctionne correctement.
- Gaugerplugin fonctionne correctement (mais a des impacts sur la performance).

## Instructions d’installation

Ce plugin peut être installé avec le launcher officiel [RSF](https://rallysimfans.hu).

Pour installer une version plus récente que celle disponible sur le launcher RSF, il faut télécharger le dernier dossier dans "release" et copier coller tous les fichiers dans le dossier RBR, en remplacçant ceux déjà présent lorsque c'est demandé. Il faut s'assurer que "Réalité Virtuelle" et "openRBRVR" sont activés dans le launcher lorsqu'une nouvelle version du plugin est installée manuellement.

## Setup

Les paramètres du plugin peuvent être modifiés dans `Options -> Plugins -> openRBRVR` et sauvegardés dans `Plugins/openRBRVR.toml` via le menu.

## FAQ

- Voir [FAQ](https://github.com/Detegr/openRBRVR/blob/master/FAQ_FR.md).

## Instructions du build (pour les développeurs uniquement)

Le projet utilise CMake. Je l’ai développé avec Visual Studio 2022 community edition qui possède le support du module CMake.

Pour configurer d3d9.dll, le `dxvk` utilise meson. J’ai utilisé `meson setup
--backend=vs2022 --build_type=release` pour le configurer.

## Remerciements

- [Kegetys](https://www.kegetys.fi/) pour RBRVR, snous montrant ce qui est possible d’accomplir.
- [TheIronWolf](https://github.com/TheIronWolfModding) pour avoir patché le support VR pour D3D9 dans dxvk.
- Towerbrah pour l’idée d’implémenter le support VR utilisant le patch de TheIronWolf et pour l'aide corrigeant les problèmes de RBRHUD+RBRRX.
- [mika-n](https://github.com/mika-n) pour l’open source de [NGPCarMenu](https://github.com/mika-n/NGPCarMenu) et la collaboration pour faire fonctionner le plugin avec RSF et RBRControls.

## License

La licence est sous Mozilla Public License 2.0 (MPL-2.0). Le code source pour tous les travaux dérivés doivent être publiques.
