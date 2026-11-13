# Stunt Bikes — стант для мотоциклов DayZ

Мод для DayZ 1.30, который учит ванильные мотоциклы (`Motorbike_01`, `Motorbike_02`) вилли, стоппи и управлению в воздухе. Только скрипты, без новых моделей. Работает в одиночной игре и на серверах, без рывков в мультиплеере.

*English below.*

## Три уровня

Газ сам по себе перёд не поднимает ни на одном уровне: трюк начинается только по стрелке.

| Уровень | Как играется |
|---|---|
| **Новичок** | Держи ↑ — вилли примерно на 35°, держи ↓ — стоппи примерно на 20°. Отпустил — колесо мягко опускается. Перевернуться нельзя |
| **Райдер** | ↑ подбрасывает перёд, дальше угол держишь сам: ↑ выше, ↓ ниже, без клавиш колесо оседает, и байк немного «гуляет». Держи метку в зелёной зоне. В красной зоне система ловит байк и возвращает его в зелёную |
| **Профи** | Физика из MX Bikes: двигатель, сцепление, вес гонщика, пампинг. Перёд поднимается при зажатой ↑, высоту держишь газом. Перегазуешь — перевернёшься |

Пока колесо в воздухе, справа на экране видна полоска угла: зелёная зона — хорошо, красная — опасно.

## Управление

| Клавиша | Действие |
|---|---|
| ↑ | Вилли (в воздухе — нос вверх) |
| ↓ | Стоппи (в воздухе — нос вниз) |
| End | Сменить уровень: Новичок → Райдер → Профи |
| Правый Shift | Сцепление (Профи): выжми, раскрути, брось |
| Правый Ctrl | Пампинг (Профи): присядь перед трамплином, отпусти на кромке |

Клавиши меняются в настройках управления игры, раздел Stunt Bikes. В воздухе без клавиш байк сам выравнивается к приземлению (Новичок и Райдер). Жёсткая посадка на колёса не сбрасывает гонщика, если байк стоит ровно.

## Установка

1. Скопируй `@StuntBikes` на сервер и в клиент (или подпишись в Steam Workshop).
2. Положи `@StuntBikes/keys/StuntBikes.bikey` в папку `keys` сервера.
3. Запускай с `-mod=@StuntBikes` и на сервере, и у игроков: скрипты нужны с обеих сторон.
4. При первом запуске сервер создаст `profiles/StuntBikes/config.json` и разошлёт настройки игрокам при входе.

## Настройки сервера (`config.json`)

Все параметры с комментариями — в [`SB_Config.c`](StuntBikes/scripts/3_Game/StuntBikes/SB_Config.c), пример — [`config.example.json`](config.example.json). Главные:

| Параметр | По умолчанию | Смысл |
|---|---|---|
| `StuntMode` | 1 | Уровень по умолчанию: 0 Новичок, 1 Райдер, 2 Профи |
| `AllowPlayerModeSwitch` | 1 | Игроки могут менять уровень клавишей End (выбор запоминается) |
| `ShowAngleHud` | 1 | Полоска угла на экране |
| `AssistWheelieAngle` / `AssistStoppieAngle` | 35 / 20 | Углы, которые держит Новичок, ° |
| `AssistMinSpeed` | 8 | Скорость, ниже которой трюк не начинается, км/ч |
| `RiderGreenMin` / `RiderGreenMax` / `RiderRedAngle` | 25 / 45 / 55 | Зоны Райдера для вилли, ° |
| `RiderDrift` | 0.15 | Насколько байк «гуляет» на Райдере (0 — не гуляет) |
| `SafeLandings` | 1 | Мягкие посадки на колёса |
| `ParkObjects` | `[]` | Рампы и другие объекты, которые сервер ставит при старте |
| `DebugLog` | 0 | Подробный лог в `script_*.log` |

Для уровня «Профи» есть полный набор параметров MX Bikes: кривая момента и торможения двигателем, сцепление, передачи, массы, тормоза.

## Как устроено

Ванильный стабилизатор мотоцикла гасит вращение по тангажу, поэтому во время трюка мод сам ведёт тангаж: каждый шаг физики (`EOnSimulate`) он считает нужную угловую скорость и записывает её в тело байка. Вращение идёт вокруг пятна контакта колеса, а не вокруг центра масс, поэтому байк не «закапывается» задом.

Уровень «Профи» использует данные MSM 250XF из MX Bikes: кривые момента и торможения двигателем, инерцию коленвала, сцепление, передаточные числа и звёзды, массы байка и гонщика, сцепление шины. Вилли получается из реальной физики: момент от тяги заднего колеса против момента от веса.

Трюк считает клиент водителя. 20 раз в секунду он отправляет серверу ориентацию и вращение байка, а сервер ведёт свою копию к ним с поправкой на задержку. Поэтому сетевая сверка DayZ не дёргает байк.

| Файл | Назначение |
|---|---|
| `3_Game/SB_Config.c` | Настройки, загрузка и проверка `config.json`, уровень игрока |
| `4_World/SB_Control.c` | Трюки: Новичок, Райдер, Профи (физика MX Bikes), воздух, следование сервера |
| `4_World/SB_MotorbikeScript.c` | Подключение к `MotorbikeScript`, сеть, мягкие посадки, звук оборотов |
| `4_World/SB_PlayerBase.c` | Отправка настроек игрокам |
| `5_Mission/SB_Hud.c` | Полоска угла |
| `5_Mission/SB_Missions.c` | Загрузка настроек, объекты парка, клавиша смены уровня |
| `data/inputs.xml`, `data/sb_hud.layout` | Клавиши и разметка полоски |

## Сборка

Нужен Python 3. Мод — только скрипты, бинаризация не нужна.

```bat
python tools\build.py
python tools\build.py --sign keys --dsutils "C:\...\DayZ Tools\Bin\DsUtils"
```

Готовый мод появится в `dist\@StuntBikes`. Ключ для подписи создаётся один раз через `DSCreateKey.exe` из DayZ Tools и кладётся в `keys\`. В репозиторий попадает только публичный `.bikey`, приватный `.biprivatekey` закрыт в `.gitignore`.

## Тест

`test\run_local_test.bat` собирает и подписывает мод, поднимает отдельный сервер DayZDiag со своим хранилищем и профилем и подключает к нему клиент. Серверный мод `test\StuntBikesTest` ставит игрока в поле с двумя байками и рампами. Через 100 секунд после старта он сам прогоняет автотест всех трёх уровней на отдельном байке и пишет результаты в `script_*.log` строками `[SBT-H]`. Пути в начале батника поправь под себя.

## Ограничения

- Поза гонщика не меняется: в ванили нет анимаций для вилли, видно только движение байка.
- На «Профи» сцепление и коробка MX Bikes влияют на тангаж и звук, но разгон байка по-прежнему считает DayZ.
- Положение байка во время трюка задаёт клиент (с ограничениями на скорость вращения). Для стант- и RP-серверов это нормально, на PvP-сервере учитывай.
- Проверено на DayZ 1.30 (DayZDiag, локальный сервер).

---

## English

Stunts for the vanilla DayZ 1.30 motorbikes: wheelies, stoppies and air control. Scripts only, works offline and on servers, no rubber-banding in multiplayer.

**Three levels.** Gas alone never lifts the front: a stunt starts only on the arrow key.

- **Novice** — hold UP for a wheelie (about 35°), hold DOWN for a stoppie (about 20°). Release and the wheel comes down softly. You cannot loop out.
- **Rider** — UP pops the front, then you hold the angle: UP raises, DOWN lowers, no key and it sinks, with a little drift to correct. Keep the marker in the green zone of the HUD bar. In the red zone the system catches the bike.
- **Pro** — MX Bikes physics (MSM 250XF data): engine, clutch, rider weight, pumping. Hold UP and control the height with the gas. Too much gas and you loop out.

**Keys:** UP / DOWN — wheelie / stoppie (rotate in the air), End — change level, Right Shift — clutch (Pro), Right Ctrl — pump (Pro). Rebind in the game's controls, section Stunt Bikes.

**Install:** put `@StuntBikes` on the server and the clients, copy `keys/StuntBikes.bikey` to the server `keys` folder, start both with `-mod=@StuntBikes`. The server creates `profiles/StuntBikes/config.json` on first start and sends the settings to players. `StuntMode` sets the default level (0 Novice, 1 Rider, 2 Pro), `AllowPlayerModeSwitch` lets players change it with End.

**Limits:** the rider pose does not change (no vanilla wheelie animations). The driver's client sets the bike orientation during a stunt, which is fine for stunt and RP servers; keep it in mind on PvP servers.
