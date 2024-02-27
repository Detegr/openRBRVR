# Často kladené dotazy

[🇬🇧](FAQ.md) - [🇨🇿](FAQ_CZ.md) - [🇫🇷](FAQ_FR.md)

## Plugin se nespustí

### Direct3DCreateVR is missing

![Direct3DCreateVR missing](img/d3dcreatevr.png)

Soubor `d3d9.dll` není z openRBRVR pluginu. RSF launcher někdy špatně prohodí soubory.
Pro vyřešení problému zkuste následující postup:

- Ukončete RBR
- Změňte nastavení VR pluginu na RBRVR a pak zpět na openRBRVR (pokud máte oba nainstalovaný).

Pokud to nepomůže, zkuste:

- Ukončete RBR
- Vypněte VR mód v RSF launcheru a ujistěte se, že 2D fullscreen mód je nastaven na "Normal" a NE na "Vulkan".
- Zavřete RSF launcher a spusťte RSF instalátor a vyberte "Update Existing Installation" (nezapomeňte zaškrtnout RBRVR i openRBRVR plugin v seznamu).

## OpenComposite error: unsupported apptype

openRBRVR nefunguje s OpenComposite, protože má reálnou implementaci OpenXR.
Vypněte prosím OpenComposite pokud chcete používat openRBRVR.

### Hra hned spadne na plochu (desktop)

- Pokud máte Light plugin, tak se ujistěte, že ho máte zakázaný. openRBRVR není s tímto pluginem kompatibilní.
- Jestli používáte openXR, tak se ujistětem že je váš headset podporován.

## Můžu použít OpenXR místo OpenVR/SteamVR?

OpenXR je podporován u headsetů, které mají dostupný 32-bit OpenXR runtime.
V následující tabulce se můžete podívat jestli ho váš headset podporuje:

| Výrobce      | Runtime                                                                    | Podpora     | Komentář                                                                    |
| ------------ | -------------------------------------------------------------------------- | ----------- | --------------------------------------------------------------------------- |
| Pimax        | [Pimax-OpenXR](https://github.com/mbucchia/Pimax-OpenXR)                   | ✅          |                                                                             |
| Oculus       | Oculus OpenXR / [VDXR](https://github.com/mbucchia/VirtualDesktop-OpenXR)  | ✅          |                                                                             |
| Pico         | [VDXR](https://github.com/mbucchia/VirtualDesktop-OpenXR)                  | ✅          |                                                                             |
| Reverb       | WMR with [OpenXR-Vk-D3D12](https://github.com/mbucchia/OpenXR-Vk-D3D12)    | ✅*         | Potřebuje řešení synchronizace, které ale může mít dopad na výkon           |
| Valve        | SteamVR OpenXR                                                             | ⛔          | Nemá 32-bit runtime                                                         |
| Varjo        | Varjo OpenXR                                                               | ⛔          | Nemá 32-bit runtime                                                         |

OpenXR runtime zapnete v `Options (Nastavení) -> Plugins -> openRBRVR -> VR runtime` nebo editací `openRBRVR.toml`
a upravením řádku `runtime = 'openxr'`.
Pokud máte Reverb headset, vyberte `OpenXR (Reverb compatibility mode)` ve hře a nebo upravte řádek v toml na
`runtime = 'openxr-wmr'`.

## Moje FPS jsou horší než v RBRVR pluginu

- Vypněte (Disable) `Cubic env maps` v RFS launcheru
- Ujistěte se, že nemáte nastavenou hodnotu anti-aliasingu moc vysoko. Obvykle je hodnota 4x SRS nejvyšší
  jakou dokáže počítač zvládnout (hodně záleží na PC sestavě a použitém VR). U většiny systémů
  použijte hodnotu 2x SRS, případně anti-aliasing úplně vypněte.
- Pokud nepotřebujete co-driver mod (bonnet camera desktop window mode), tak se podívejte do
  `Options (Nastavení) -> Plugins -> openRBRVR -> Desktop window settings -> Desktop window mode` 
  a použijte `VR view` nebo `Off` pro dosažení více fps.

## Jak resetovat (vycentrovat) obraz ve VR?

- Nastavte si tlačítko přes `Options (Nastavení) -> Plugins -> RBR Controls -> Page 2 -> Reset VR view`

## Jak si můžu posunout sedačku/pohled?

- V RSF launcheru klikněte na `Controls` respektivě `Ovládání` a  
  povolte `3-2-1 countdown pause` respektivě `Odpočítávání 3-2-1 pozastaveno` díky čemuž získáte čas na
  nastavení pohledu před startem RZety.
- Před startem RZety pak otevřete PaceNote plugin dvojklikem pravým tlačítkem myši v okně hry a pohled si nastavte.
  Nezapomeňte si polohu uložit kliknutím na tlačítko `Save` respektivě `Uložit` a zavřete okno s nastavením
  kliknutím na `X` v pravém horním rohu.
  
## Jak si zapnu anizotropního filtrování?

RSF Launcher nezobrazuje nastavení pro jeho změnu, proto musíte ručně změnit
soubor `dxvk.conf` přidáním/přepsáním řádku:
`d3d9.samplerAnisotropy = `

Použitelné hodnoty: 0, 2, 4, 8 nebo 16

## Texty v menu ve hře nejsou vidět

- Zvyšte 2D rozlišení na 800x600 nebo větší.

## Hru vidíte vždy jakoby na obrazovce a ne ve 3D v brýlích

- Tohle chování je standardně nastaveno pro menu. Pokud se to děje i při jízdě, tak nainstalujte SteamVR.

## Jaké nastavení grafiky mám použít?

- Hodně záleží na vaší PC sestavě. Používám SteamVR render resolution na 100% a anizotropní filtrování na 16x
s tímto nastavením v RSF launcheru:

![Příklad nastavení](img/example_settings.png)

## U některých textur vidím jejich problikávání

- K tomu dochází bez zapnutého Anti-Aliasingu a/nebo anizotropního filtrování. Zvyšte
  anizotropní filtrování a potom i anti-aliasing, pokud to vaše PC utáhne.
