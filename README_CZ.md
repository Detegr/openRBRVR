# openRBRVR

[🇬🇧](README.md) - [🇨🇿](README_CZ.md) - [🇫🇷](README_FR.md)

![openRBRVR logo](img/openRBRVR.png)

Open source VR plugin pro Richard Burns Rally.

## Vlastnosti

- Celkový výkon se zdá být proti RBRVR v některých etapách o dost lepší a v
  jiných podobný.
- Obraz je viditelně "čistější" s openRBRVR při stejném rozlišení v porovnání s
  RBRVR.
- Vulkan backend přes dxvk ([fork](https://github.com/TheIronWolfModding/dxvk)
  od TheIronWolf, který přidává podporu D3D9 VR.
- PaceNote plugin UI funguje správně.
- Gaugerplugin taky funguje (má ale dopad na výkon).
- Podporuje jak OpenXR, tak OpenVR.

## Návod na instalaci

Plugin nainstalujte pomocí oficiálního [RSF](https://rallysimfans.hu)
instalátoru.

Chcete-li nainstalovat novější verzi, než která je k dispozici v instalátoru
RSF, stáhněte si nejnovější verzi a překopírujte soubory do hlavního adresáře 
hry RBR a přepište stávající soubory. Ujistěte se, že v RSF Launcheru máte 
povolenou Virtuální Realitu a openRBRVR, když děláte ruční instalaci nové
verze pluginu.

## Nastavení

Nastavení pluginu můžete změnit ve hře v `Options -> Plugins -> openRBRVR`
respektivě `Nastavení -> Plugins -> openRBRVR` a jeho nastavení naleznete v
souboru `Plugins/openRBRVR.toml`

## Často kladené dotazy

- Naleznete v [FAQ](https://github.com/Detegr/openRBRVR/blob/master/FAQ_CZ.md).

## Pokyny pro sestavení

Projekt používá CMake a je vyvíjen ve Visual Studiu 2022 community edition,
který má podporu CMake zabudovanou v sobě.

Pro vytvoření d3d9.dll, zkompilujte `dxvk` použitím mesonu. Používám parametry:
`meson setup --backend=vs2022 --build_type=release`

## Poděkování

- [Kegetys](https://www.kegetys.fi/) za RBRVR (ukázání, že je to možné udělat)
- [TheIronWolf](https://github.com/TheIronWolfModding) za zahrnutí VR podpory
  pro D3D9 do dxvk.
- Towerbrah za nápad implementovat VR podporu použitím forku od TheIronWolfa a
  za pomoc s debuggingem problémů s RBRHUD+RBRRX.
- [mika-n](https://github.com/mika-n) za open sourcing
  [NGPCarMenu](https://github.com/mika-n/NGPCarMenu) a spolupráci na začlenění
  pluginu do RSF a RBRControls.

## Licence

Licensed under Mozilla Public License 2.0 (MPL-2.0). Source code for all
derived work must be disclosed.
