# Форк AmneziaVPN (intpoln/amnezia-client)

Личная улучшенная версия клиента [amnezia-vpn/amnezia-client](https://github.com/amnezia-vpn/amnezia-client) для **Windows**.

## Цели
1. **Исключать из VPN целые папки**, а не отдельные `.exe`.
2. **Стабильность и отзывчивость**: убрать зависания, бесконечное «Подключение…» и потерянные ошибки.

Чего форк **не** делает (решения в [DECISIONS.md](DECISIONS.md)):
- Android: отложен, в нём ничего не меняем.
- Режим «только выбранные приложения через VPN»: не нужен, хватает режима исключений.
- Свой драйвер раздельного туннелирования: используем готовый подписанный драйвер Mullvad как есть.

## Навигация

| Файл | Зачем |
|---|---|
| [PLAN.md](PLAN.md) | Этапы и задачи с чекбоксами |
| [STATUS.md](STATUS.md) | Что сделано, что в работе, что ждёт ручной проверки |
| [DECISIONS.md](DECISIONS.md) | Принятые решения и их причины |
| [TESTING.md](TESTING.md) | Как тестировать и чек-листы ручной проверки |
| [NOTES.md](NOTES.md) | Память агентов: правила работы, карта кода, ловушки, команды |
| [research/REPORT.md](research/REPORT.md) | Исходное исследование: архитектура, проблемы, сравнение с другими клиентами |
| [research/codex_report.md](research/codex_report.md) | Независимое исследование от Codex |

## Ветки и remotes
- `upstream` → `amnezia-vpn/amnezia-client`. Только fetch, push туда отключён.
- `origin` → `intpoln/amnezia-client`.
- `dev`: чистое зеркало `upstream/dev`, в нём не коммитим.
- `custom`: основная ветка форка (по умолчанию на GitHub).
- `feat/<что-то>`, `fix/<что-то>`: задачи. Ответвляются от `custom` и вливаются в него же.

Обновление из upstream, примерно раз в месяц:
```bash
git fetch upstream
git checkout dev && git merge --ff-only upstream/dev && git push origin dev
git checkout custom && git merge dev
```

## Сборка
- **CI:** workflow [`.github/workflows/fork-windows.yml`](../.github/workflows/fork-windows.yml) запускается на push в `custom`, `feat/**`, `fix/**` и вручную. Результат: установщики `.exe`/`.msi` и unit-тесты. Скачать можно во вкладке Actions → нужный запуск → Artifacts.
- **Локально:** инструкция в [NOTES.md](NOTES.md#локальная-сборка).

## Подписка Amnezia Premium
Подписка работает в форке напрямую, как в официальном клиенте: при сборке встраивается публичный ключ шлюза Amnezia из официальной сборки. В репозитории его нет, как он попадает в сборку — в [NOTES.md](NOTES.md#ключ-шлюза-premium).
