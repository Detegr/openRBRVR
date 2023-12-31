# Často kladené dotazy

## Moje FPS jsou horší než v RBRVR pluginu

- Vypněte (Disable) `Cubic env maps` v RFS launcheru
- Ujistěte se, že nemáte nastavenou hodnotu anti-aliasingu moc vysoko. Obvykle je hodnota 4x SRS nejvyšší
  jakou dokáže počítač zvládnout (hodně záleží na PC sestavě a použitém VR). U většiny systémů
  použijte hodnotu 2x SRS, případně anti-aliasing úplně vypněte.
- Pokud nic z toho nefunguje, běžte ve hře do `Options (Nastavení) -> Plugins -> openRBRVR -> Debug
  settings -> Always call Present` a nastavte ho na `OFF` a restartujte hru.

## Jak resetovat (vycentrovat) obraz ve VR?

- Nastavte si tlačítko přes `Options (Nastavení) -> Plugins -> RBR Controls -> Page 2 -> Reset VR view`

## Jak si můžu posunout sedačku/pohled?

- V RSF launcheru klikněte na `Controls` respektivě `Ovládání` a  
  povolte `3-2-1 countdown pause` respektivě `Odpočítávání 3-2-1 pozastaveno` díky čemuž získáte čas na
  nastavení pohledu před startem RZety.
- Před startem RZety pak otevřete PaceNote plugin dvojklikem pravým tlačítkem myši v okně hry a pohled si nastavte.
  Nezapomeňte si polohu uložit kliknutím na tlačítko `Save` respektivě `Uložit` a zavřete okno s nastavením
  kliknutím na `X` v pravém horním rohu.

## Texty v menu ve hře nejsou vidět

- Zvyšte 2D rozlišení na 800x600 nebo i víc.

## Hru vidíte vždy jakoby na obrazovce a ne ve 3D v brýlích

- Tohle chování je standardně nastaveno pro menu. Pokud se to děje i při jízdě, tak nainstalujte SteamVR.

## Jaké nastavení grafiky mám použít?

- Hodně záleží na vaší PC sestavě. Používám SteamVR render resolution na 100% a nastavení v RSF launcheru:

![Příklad nastavení](img/example_settings.png)

## U některých textur vidím jejich problikávání

- K tomu dochází bez zapnutého Anti-Aliasingu a/nebo anizotropního filtrování. Zvyšte
  [anizotropní filtrování](https://github.com/Detegr/openRBRVR?tab=readme-ov-file#enabling-anisotropic-filtering)
  a Anti-Aliasing, pokud to vaše PC utáhne.
