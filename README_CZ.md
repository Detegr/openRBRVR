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

## Návod na instalaci

Plugin nainstalujte pomocí oficiálního [RSF](https://rallysimfans.hu)
instalátoru.

Chcete-li nainstalovat novější verzi, než která je k dispozici v instalátoru
RSF, stáhněte si nejnovější verzi a překopírujte soubory z adresáře
openRBRVR-x.x.x do hlavního adresáře hry RBR a přepište stávající soubory.
Ujistěte se, že v RSF Launcheru máte povolenou Virtuální Realitu a openRBRVR,
když děláte ruční instalaci.

## Nastavení

Nastavení pluginu můžete změnit ve hře v `Options -> Plugins -> openRBRVR`
respektivě `Nastavení -> Plugins -> openRBRVR` a jeho nastavení naleznete v
souboru `Plugins/openRBRVR.toml`

Při různých nastavení FoV (Field of View) mohou některé objekty zmizet z vašeho
periferního vidění dřív než se dostanou mimo obraz. V některých etapách je to
viditelnější než v jiných. Můžete to spravit přímo v RSF launcheru, NGPCarMenu
nebo přímo ve hře použitím PaceNote pluginu (Dvakrát klikněte pravým tlačítkem
myši pro zobrazení menu). U mého headsetu dobře fungují hodnoty 2.3 až 2.6

## Často kladené dotazy

- Naleznete v [FAQ](https://github.com/Detegr/openRBRVR/blob/master/FAQ_CZ.md).

## Známé chyby a omezení

- Polohu sedadla nelze změnit pomocí klávesnice, ale musíte použít PaceNote
  plugin dvojklikem pravým tlačítkem myši v okně hry
- BTB tratě mohou mít velký dopad na výkon pokud je zapnuto `cubic env maps`.
  Doporučuju tuto možnost v launcheru vypnout pro openRBRVR.

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
