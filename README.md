# CS 1.6 client.dll for Steam GoldSrc

Экспериментальная полная замена `cstrike/cl_dlls/client.dll`, адаптированная
для обычного 32-битного Steam GoldSrc. Каркас DLL и lifecycle взяты из
стандартного Half-Life `cl_dll`, а CS 1.6 HUD, события и shared-weapon код — из
Velaron/cs16-client. Это не сборка для Xash3D: активные Xash/mobile/render API
не используются.

## Что изменено

- цель сборки ограничена Windows x86, как требует Steam GoldSrc;
- восстановлен стандартный GoldSrc ABI и полная таблица из 44 экспортов;
- используется обычный GoldSrc input (`input.cpp` + `inputw32.cpp`/SDL2);
- добавлен небольшой нативный VGUI1 viewport через штатный GoldSrc
  `VGui_GetPanel`; стандартные CS menu ID `2`, `26`–`34` обрабатывают выбор
  команды, моделей T/CT и полное дерево покупки без Xash API;
- VGUI-классы отделены C ABI-мостом, чтобы код, наследующийся от `vgui::Panel`,
  всегда использовал совместимый с `vgui.dll` Microsoft C++ ABI;
- `_vgui_menus` включается только после успешного создания viewport; пока не
  реализованный VGUI menu ID безопасно переключает текущую сессию обратно на
  штатный текстовый `ShowMenu`;
- сохранены CS HUD, scoreboard, radar, spectator HUD, client-side weapon
  prediction, события оружия, voice mask/mute и иконка говорящего;
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
  При отсутствии файла используется небольшое встроенное запасное меню.

Для MinGW-сборки внешние C++-интерфейсы `particleman.dll` и
`GameClientExports001` намеренно отключены: Steam-модули собраны с MSVC, и их
vtable ABI несовместим с MinGW. VGUI1 является отдельным исключением: его
маленький viewport собирается Microsoft-ABI-совместимым объектом и общается с
остальным клиентом только через C-функции. Основной GoldSrc API также остаётся
чистым C ABI.

Сейчас VGUI1 покрывает выбор команды, модели и покупку. Радио и остальные ещё
не перенесённые menu ID автоматически остаются в классическом текстовом виде.
Голос работает, но отдельные VGUI-плашки с именем говорящего пока отсутствуют.
Используется штатный `vgui.dll`, уже поставляемый Steam GoldSrc; копировать
версию этой библиотеки из Xash3D в папку игры нельзя.

## Сборка

Нужны Visual Studio 2022 и workload **Desktop development with C++**.

```powershell
msbuild .\cs16cldll.sln /m /p:Configuration=Release /p:Platform=x86
.\scripts\verify-client.ps1 -Path .\build\Release\client.dll
```

Готовый файл появится в `build/Release/client.dll`. Release использует
статический MSVC runtime; `SDL2.dll` берётся из установленного Steam GoldSrc.

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
