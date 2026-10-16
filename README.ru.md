<div align="center">

<picture>
  <source media="(prefers-reduced-motion: reduce)" srcset="docs/images/ru/hero.png">
  <source srcset="docs/images/ru/hero.avif" type="image/avif">
  <source srcset="docs/images/ru/hero.webp" type="image/webp">
  <img src="docs/images/ru/hero.gif" alt="Ctrl+Alt+C, выбор аккаунта в Claude Profile Switcher, и аккаунт открывается уже с выполненным входом" width="100%">
</picture>

# Claude Profile Switcher

**Несколько аккаунтов Claude Desktop на одном компьютере с Windows: каждый в своём окне, и во всех выполнен вход.**

[![Последний релиз](https://img.shields.io/github/v/release/Piwqust/claude-switcher?style=flat-square&color=d9704c&label=%D1%80%D0%B5%D0%BB%D0%B8%D0%B7)](https://github.com/Piwqust/claude-switcher/releases/latest)
[![Скачивания](https://img.shields.io/github/downloads/Piwqust/claude-switcher/total?style=flat-square&color=d9704c&label=%D1%81%D0%BA%D0%B0%D1%87%D0%B8%D0%B2%D0%B0%D0%BD%D0%B8%D1%8F)](https://github.com/Piwqust/claude-switcher/releases)
[![Лицензия MIT](https://img.shields.io/github/license/Piwqust/claude-switcher?style=flat-square)](LICENSE)

<a href="https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-x64.exe"><img src="docs/images/ru/download-x64.svg" alt="Скачать для Windows x64" height="52"></a>
&nbsp;
<a href="https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-arm64.exe"><img src="docs/images/ru/download-arm64.svg" alt="Скачать для Windows ARM64" height="52"></a>

<sub>Один exe, ничего ставить не нужно · <a href="https://claude-switcher.angleford.co">сайт</a> · <a href="https://github.com/Piwqust/claude-switcher/releases">все версии</a> · <a href="docs/guide.ru.md">руководство</a> · <a href="README.md">English</a></sub>

</div>

Claude Desktop держит только один вход: чтобы перейти в рабочий аккаунт, приходится выходить из личного. Claude Profile Switcher даёт каждому аккаунту своё окно со своим входом, перепиской и настройками, и открыть их можно все сразу.

- **Одно сочетание клавиш.** <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>C</kbd> открывает окно выбора из любой программы, <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd>…<kbd>9</kbd> — сразу нужный аккаунт.
- **Всегда под рукой.** Ждёт в трее, стартует вместе с Windows и может сам открывать привычные аккаунты.
- **Лимиты на виду.** На каждой карточке видно, сколько израсходовано за 5 часов и за 7 дней.
- **Основной аккаунт остаётся как есть**, с автообновлением и ссылками `claude://`.
- **Маленький и нативный.** Один exe на 550 КБ на C++, без .NET и Electron, английский и русский, тёмная и светлая темы.

## Установка

1. Скачайте exe: [x64](https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-x64.exe) для Intel и AMD (почти все ПК) или [ARM64](https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-arm64.exe) для Snapdragon.
2. Запустите его. Программа установится сама, без прав администратора, и откроет окно выбора.
3. Нажмите **Добавить** и войдите в другой аккаунт в новом окне Claude. Готово: вход сохранится в обоих.

Нужны Windows 10 или 11 и [Claude Desktop](https://claude.ai/download). Exe не подписан сертификатом, поэтому при первом запуске Windows может показать «Windows защитил ваш компьютер»: нажмите **Подробнее → Выполнить в любом случае**. Чтобы обновиться, запустите новую версию — аккаунты и настройки сохранятся.

## Как пользоваться

| Что нужно | Как сделать |
| --- | --- |
| Открыть аккаунт | клик по карточке или <kbd>1</kbd>…<kbd>9</kbd> |
| Открыть несколько сразу | <kbd>Ctrl</kbd>+клик по карточкам, затем **Открыть 2 аккаунта** |
| Переименовать или удалить | <kbd>F2</kbd> или <kbd>Del</kbd>, либо меню **⋯** на карточке |
| Вызвать из любой программы | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>C</kbd> или клик по значку в трее |

Зелёная точка на карточке значит, что аккаунт уже открыт: клик выведет его окно вперёд. Отсчёт при входе в Windows, отдельные кнопки на панели задач, перенос чатов Claude Code между аккаунтами, командная строка и решение проблем — в **[руководстве](docs/guide.ru.md)**.

## Как это работает

Claude Desktop — приложение на Electron, а Electron хранит всё в одной папке с данными. Если запустить его с `--user-data-dir` и другой папкой, получится отдельный вход, который работает рядом с первым. Переключатель держит эти папки в `%LOCALAPPDATA%\ClaudeProfiles`, запускает Claude с нужной и следит, какие окна открыты. Основной аккаунт запускается как обычно и не изменяется.

Программа не ходит в сеть и не читает токены, cookie и пароли. Что у аккаунтов общее, откуда берутся цифры лимитов и остальные подробности — в разделе **[Как это работает](docs/how-it-works.ru.md)**.

## Сравнение с оригиналом

Это переписанная на C++ версия [Claude Code Desktop Switcher](https://github.com/PriyanshuGeTRekT/Claude-Code-Desktop-Switcher) от [@PriyanshuGeTRekT](https://github.com/PriyanshuGeTRekT) — скрипта на PowerShell, в котором и придумали эту идею. Папки профилей те же, поэтому существующие аккаунты переносятся сами.

| | Оригинал 2.0 | Эта версия 1.0 |
| --- | --- | --- |
| Что запускается | скрипт PowerShell | один exe на 550 КБ |
| Окно появляется за | около 10 с | около 75 мс |
| Память | около 126 МБ | около 16 МБ |
| Что нового | | горячие клавиши, панель в трее, лимиты, несколько аккаунтов разом, отсчёт при входе, русский язык, тёмная тема, ARM64 |

Полное сравнение и почему так быстрее — в разделе [Сравнение с оригиналом](docs/how-it-works.ru.md#сравнение-с-оригиналом).

## Сборка из исходников

Нужна Visual Studio 2022 или Build Tools с компонентом «Разработка классических приложений на C++», затем `build.cmd` (или `build.cmd arm64`). Демо с вымышленными аккаунтами и что проверить перед пулл-реквестом — в [CONTRIBUTING.md](CONTRIBUTING.md).

---

<sub>Неофициальный проект: не связан с Anthropic, не одобрен и не поддерживается ею. Claude — товарный знак Anthropic. Использование нескольких аккаунтов регулируется [условиями Anthropic](https://www.anthropic.com/legal/consumer-terms). Спасибо @PriyanshuGeTRekT за оригинал. [Лицензия MIT](LICENSE).</sub>
