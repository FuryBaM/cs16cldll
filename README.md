# CS 1.6 client.dll for Steam GoldSrc

Экспериментальная полная замена `cstrike/cl_dlls/client.dll`, адаптированная
для обычного 32-битного Steam GoldSrc. Каркас DLL и lifecycle взяты из
стандартного Half-Life `cl_dll`, а CS 1.6 HUD, события и shared-weapon код — из
Velaron/cs16-client. Это не сборка для Xash3D: активные Xash/mobile/render API
не используются.

## Почему этот проект уникален

> **Это единственный клиент, который компилируется под оригинальный Steam GoldSrc
> и работает на серверах без вылетов.**

Проект не просто собирает старый HLSDK-код современным компилятором. Он сохраняет
контракт оригинального 32-битного Steam GoldSrc и одновременно возвращает
Counter-Strike-специфичную клиентскую логику:

- **Нативный Steam GoldSrc ABI.** Результат — PE32/x86 `client.dll` с полной
  таблицей из 44 экспортов, ожидаемых движком, без зависимости от `xash.dll`,
  `mainui.dll` или мобильных API.
- **Оригинальный VGUI.** Клиент использует поставляемые Steam библиотеки
  `vgui.dll` и `vgui2.dll`, оригинальные ресурсы CS 1.6 и совместимый VGUI1
  fallback. Интерфейс не заменён Xash-реализацией.
- **Рабочий prediction.** Shared-weapon код выполняется через штатный
  `HUD_PostRunCmd`; клиентский адаптер отделяет prediction от серверных
  `PRECACHE_*`/`SET_MODEL` callback и не вызывает отсутствующие функции движка.
- **Стабильный мультиплеер.** Исправлены lifecycle, таблицы callback, границы
  индексов игроков, prediction, scoreboard, spectator HUD и voice HUD. Клиент
  проходит реальные игровые smoke-тесты: подключение, spawn, покупка, стрельба,
  смена оружия, смерть, наблюдение и голосовая связь.

Многие старые проекты ориентированы на Xash3D, mobile/render API либо содержат
только базовый Half-Life `cl_dll`. Здесь целевой движок — именно оригинальный
Steam GoldSrc, сохранён оригинальный CS VGUI, восстановлен weapon prediction и
проверена полноценная сетевая сессия, а не только запуск локальной карты.

## Что изменено

- цель сборки ограничена Windows x86, как требует Steam GoldSrc;
- восстановлен стандартный GoldSrc ABI и полная таблица из 44 экспортов;
- используется обычный GoldSrc input (`input.cpp` + `inputw32.cpp`/SDL2);
- добавлен настоящий VGUI2 viewport через штатные GoldSrc-интерфейсы
  `IVGui`, `IPanel`, `ISurface` и `IInput`; стандартные CS menu ID `2`,
  `26`–`34` обрабатывают выбор команды, моделей T/CT и полное дерево покупки
  без Xash API;
- клиент публикует ожидаемый движком интерфейс `VClientVGUI001`; если VGUI2
  отсутствует или не инициализировался, меню автоматически обслуживает
  существующий VGUI1 viewport;
- `_vgui_menus` включается только после успешного создания viewport; пока не
  реализованный VGUI menu ID безопасно переключает текущую сессию обратно на
  штатный текстовый `ShowMenu`;
- сохранены CS HUD, scoreboard, radar, spectator HUD, client-side weapon
  prediction, события оружия, voice mask/mute и иконка говорящего;
- scoreboard и голосовой HUD показывают Steam-аватары игроков, полученные через
  уже загруженный игрой `steam_api.dll`; аватары кэшируются и не добавляют DLL
  новых обязательных зависимостей;
- kill feed вмещает до шести событий, подсвечивает убийства с участием локального
  игрока, различает team kill и плавно затухает поверх компактной тёмной панели;
- радар получил кольца дальности, центральный маркер, цвета
  команд, зелёную индикацию говорящих игроков и подпись текущей локации;
- добавлены Steam Rich Presence и опциональный Discord RPC через локальный IPC,
  совместимый с 32-битным GoldSrc без сторонней Discord DLL;
- защищена опциональная загрузка `particleman.dll`, исправлены границы индексов
  игроков, shutdown и заполнение GoldSrc function table.
- восстановлен клиентский адаптер shared-weapon кода: серверные
  `PRECACHE_MODEL`, `PRECACHE_SOUND` и `SET_MODEL` становятся no-op внутри
  `client.dll`, поэтому первый `HUD_PostRunCmd` больше не вызывает нулевой
  server callback при создании prediction-объектов оружия.
- `cl_charset` и `con_charset`, которые Xash создаёт сам, теперь имеют
  GoldSrc-совместимый fallback и null-check; первый `CHud::Redraw` больше не
  разыменовывает отсутствующий Xash cvar.
- подключены штатные сообщения CS `AllowSpec` и `BuyClose`, поэтому GoldSrc не
  сообщает об отсутствующем обработчике и корректно закрывает активное меню.
- снова включён Counter-Strike studio renderer с 9-way blend, pitch/yaw blend
  и CS gait-анимацией: временный Half-Life renderer давал игрокам неправильные
  позы. Опасные указатели, sequence/gaitsequence и число model attachments
  ограничены штатными пределами GoldSrc;
- `cl_recoil_crosshair_scale` теперь создаётся самим клиентом: обычный GoldSrc
  не предоставляет эту переменную, а первый кадр прицела после спавна раньше
  разыменовывал нулевой указатель.
- `+showscores` теперь одновременно выставляет GoldSrc `IN_SCORE` и открывает
  встроенный CS scoreboard; прежний input-handler поглощал команду, не включая
  отрисовку таблицы.
- центральные `TextMsg` (`Terrorists Win`, `Bomb has been planted` и другие)
  имеют собственный устойчивый HUD-слой. Клиент читает штатные UTF-8/UTF-16LE
  `resource/valve_*.txt` и `resource/cstrike_*.txt`, а форматирование серверных
  строк не передаётся небезопасно в `printf`.
- зарегистрированы обе штатные команды `+commandmenu`/`-commandmenu`. Клавиша
  `H` открывает нативное VGUI1-меню из игрового `commandmenu.txt`; поддержаны
  вложенные секции, фильтры `TEAMn`/`MAP`, `TOGGLE`, мышь и горячие клавиши.
  UTF-8 BOM пропускается, а UTF-8-подписи переводятся в однобайтовую кодировку,
  ожидаемую старым `vgui.dll`; это предотвращает вылет на кириллических меню.
  При отсутствии файла используется небольшое встроенное запасное меню.

Для MinGW-сборки внешние C++-интерфейсы `particleman.dll` и
`GameClientExports001` намеренно отключены: Steam-модули собраны с MSVC, и их
vtable ABI несовместим с MinGW. VGUI1 является отдельным исключением: его
маленький viewport собирается Microsoft-ABI-совместимым объектом и общается с
остальным клиентом только через C-функции. Основной GoldSrc API также остаётся
чистым C ABI.

Сейчас VGUI2 покрывает выбор команды, модели и покупку. VGUI1 остаётся для
`commandmenu.txt` и как запасной viewport. Радио и остальные ещё не перенесённые
menu ID автоматически остаются в классическом текстовом виде.
Голосовой HUD показывает имя говорящего игрока и использует штатный voice status.
Используется штатный `vgui.dll`, уже поставляемый Steam GoldSrc; копировать
версию этой библиотеки из Xash3D в папку игры нельзя.

Spectator HUD показывает адаптивную верхнюю ленту команд со Steam-аватарами,
статусом жив/мёртв, индикатором голоса и выделением наблюдаемого игрока. Снизу
рисуется карточка текущей цели с аватаром, ником, HP и доступными данными оружия.
Классический вид можно вернуть командой `cl_spectator_hud_modern 0`.

## Steam и Discord Presence

Steam Rich Presence включён по умолчанию и показывает текущую карту. Управление:

```cfg
cl_steam_rich_presence 1 // 0 — отключить
```

Discord RPC не требует дополнительной DLL, но нуждается в Application ID вашего
проекта в [Discord Developer Portal](https://discord.com/developers/applications):

1. Создайте приложение и скопируйте его **Application ID**.
2. В консоли игры задайте ID и включите интеграцию:

```cfg
cl_discord_appid "123456789012345678"
cl_discord_rpc 1
```

Discord должен быть запущен на том же компьютере. Пока ID не задан,
`cl_discord_rpc` ничего не подключает и не отправляет. Для отключения используйте
`cl_discord_rpc 0`.

Настройки обновлённого радара:

```cfg
cl_radar_style 1         // 0 — классические маркеры без новой сетки
cl_radar_alpha 180       // яркость сетки и маркеров, 40–255
cl_radar_show_location 1 // подпись зоны под радаром
```

## Сборка

### 1. Что установить

- Windows 10 или Windows 11;
- **Visual Studio Community 2026** (18.x);
- workload **Desktop development with C++**;
- компоненты **MSVC v145 C++ x64/x86 build tools** и **Windows 10/11 SDK**;
- Git for Windows.

Отдельно скачивать HLSDK, SDL2 или VGUI SDK не нужно: используемые заголовки,
`SDL2.lib` и импортная библиотека VGUI уже находятся в репозитории. Установленная
через Steam Counter-Strike 1.6 нужна только для запуска и проверки DLL.

### 2. Получить исходники

```powershell
git clone https://github.com/FuryBaM/cs16-goldsrc-client.git
cd cs16-goldsrc-client
```

### 3. Собрать Release DLL

Откройте **Developer PowerShell for VS 2026** и выполните:

```powershell
msbuild .\cs16cldll.sln /m /p:Configuration=Release /p:Platform=x86
powershell -ExecutionPolicy Bypass -File .\scripts\verify-client.ps1 `
  -Path .\build\Release\client.dll
```

Либо откройте `cs16cldll.sln` в Visual Studio, выберите **Release** и **x86**, затем
выполните **Build → Build Solution**. Готовый файл появится в
`build/Release/client.dll`.

Скрипт проверки подтверждает, что файл имеет формат PE32/x86, содержит все 44
GoldSrc-экспорта и не импортирует библиотеки Xash3D. Release использует
статический MSVC runtime; `SDL2.dll` и `vgui.dll` берутся из установленного
Steam GoldSrc.

### 4. Запустить собранную DLL

1. Закройте Counter-Strike 1.6.
2. Найдите папку игры: Steam → Counter-Strike → **Properties** →
   **Installed Files** → **Browse**.
3. Сделайте резервную копию `cstrike/cl_dlls/client.dll`.
4. Скопируйте `build/Release/client.dll` в `cstrike/cl_dlls/client.dll` с заменой.
5. Добавьте параметры запуска `-insecure -dev -console`.
6. Запустите игру и сначала проверьте DLL локально командой `map de_dust2`.

`-insecure` обязателен при разработке и тестировании изменённого клиентского
модуля. Не подключайтесь с этой DLL к VAC-secured серверам.

### Частые ошибки сборки

- **MSB8020 / не найден v145:** установите Visual Studio 2026 и компонент
  **MSVC v145 C++ x64/x86 build tools**.
- **Не найден Windows SDK:** добавьте Windows 10 или Windows 11 SDK через
  Visual Studio Installer → **Individual components**.
- **Собирается не та архитектура:** используйте только `Platform=x86`; Steam
  GoldSrc не загрузит 64-битную клиентскую DLL.
- **DLL не удаётся заменить:** полностью закройте игру перед копированием.
- **При запуске отсутствует SDL2.dll или vgui.dll:** проверьте файлы игры через
  Steam; не копируйте эти библиотеки из Xash3D.

Заголовки VGUI2 размещены в `external/hl1_source_sdk`; рядом сохранены
лицензия Source 1 SDK и `thirdpartylegalnotices.txt`.

При загрузке клиент динамически открывает штатный Steam `vgui2.dll`, проверяет
точные версии GoldSrc-интерфейсов и публикует `VClientVGUI001`. Жёсткого импорта
`vgui2.dll` у `client.dll` намеренно нет, поэтому при неудаче остаётся рабочий
VGUI1 fallback. VGUI2-панель регистрируется напрямую как `IClientPanel`, без
несовместимого статического `vgui_controls.lib`. Команда `cs_vgui2_status`
показывает состояние модуля, фабрики, viewport и каждого интерфейса.

## Безопасная установка и проверка

1. Закройте игру и сделайте резервную копию
   `Half-Life/cstrike/cl_dlls/client.dll`.
2. Скопируйте новую DLL в `Half-Life/cstrike/cl_dlls/client.dll`.
3. Для теста добавьте параметры запуска `-insecure -dev -console`.
4. Сначала проверьте локально: `map de_dust2`.
5. В консоли можно отдельно открыть `cs_vgui_team`, `cs_vgui_class_t`,
   `cs_vgui_class_ct` или `cs_vgui_buy`, а закрыть панель командой
   `cs_vgui_hide`. `cs_vgui_reload_commandmenu` перечитывает
   `commandmenu.txt`, а `cs_test_centertext` проверяет центральное объявление.
6. Проверьте движение и мышь, выбор команды, покупку, стрельбу, HUD,
   scoreboard, spectator mode и голос.

Переменная `cs_vgui_enable 0` полностью отключает новый viewport и возвращает
текстовые меню; значение `1` включает его обратно при следующем обновлении
userinfo.

Диагностическая MinGW-сборка пишет этапы запуска и обработку серверных
сообщений в `%TEMP%\cs16_goldsrc_startup.log`. Записи вида `enter` и `complete`
образуют пары: если игра закрылась после непарной строки `enter`, эта строка
показывает callback, внутри которого произошёл сбой. Диагностическая сборка
также включает подробную покадровую трассировку после `joinclass` и записывает
код исключения, модуль и RVA аварийной инструкции. Она защищает sign-on от
отсутствующего локального игрока и нестандартного значения имени карты.

`-insecure` обязателен для разработки и тестирования изменённого клиентского
модуля. Не подключайтесь с ним к VAC-secured серверам.

Если GoldSrc получил пока не реализованный `VGUIMenu` (например, радио), клиент
переключит userinfo обратно на текстовые меню и попросит открыть меню ещё раз.

## Откат

Верните резервную копию `client.dll`. Если копии нет: Steam → Counter-Strike →
Properties → Installed Files → Verify integrity of game files.

## Статус

R11 прошёл реальный игровой smoke test: подключение, spawn, HUD, radar,
стрельба, VGUI buy menu, scoreboard, радио и death notice работают. R12
добавляет полноценное меню `H`, исправляет дублирование центральных сообщений
и проходит проверку PE32/x86, 44 экспортов и импортов. Коррекция поз удалённых
игроков остаётся отдельной задачей studio renderer.
