# TODO: Замеры и иллюстрации для диплома

Этот файл содержит все замеры и описания иллюстраций, которые необходимо
провести и добавить в LaTeX-текст перед финальной сдачей.
Ссылки на разделы плана — по `plan_part1.md`.

---

## Реферат

- [ ] **Подсчитать и вставить**: число страниц, рисунков, таблиц, источников, приложений
  - Заменить `\ldots{}` в `chapters/abstract.tex` на реальные числа

---

## Глава 1

- [ ] Нет активных TODO в главе 1

---

## Глава 2

### Рисунок 2.1 — Стек компиляции CUDA
- [ ] **Скриншот/диаграмма**: Нарисовать схему стека компиляции
  - CUDA C++ → NVCC/Clang → PTX → PTXAS → SASS → fatbinary
  - Показать место PTX в иерархии
  - Инструмент: draw.io / PlantUML / TikZ
  - Файл: `report/images/cuda_stack.pdf`
  - Код в тексте: убрать `\fbox` в `ch2_theory.tex`, вставить `\includegraphics`

### Таблица 2.3 — Сравнение инструментов
- [ ] Заполнить реальными данными (версии, даты последнего коммита Ocelot/GPGPU-Sim)

---

## Глава 3

### Рисунок 3.1 — Схема перехвата LD_PRELOAD
- [ ] **Диаграмма компонентов**:
  - CUDA App → PLT → libemuruntime.so → {PTX Parser, Executor, Profiler}
  - LD_PRELOAD заменяет libcudart.so
  - Инструмент: PlantUML component diagram или draw.io
  - Команда для демонстрации:
    ```bash
    LD_DEBUG=bindings LD_PRELOAD=./libemuruntime.so ./test_vadd 2>&1 | grep cudaMalloc
    ```
  - Файл: `report/images/ldpreload_scheme.pdf`

### Таблица 3.1 — Точки перехвата
- [ ] Проверить полный список перехватываемых функций в `app/runtime/`

### Таблица 3.2 — Переменные окружения
- [ ] Верифицировать со списком в `app/runtime/rt_stream.cpp` или `runtime_def.cpp`

### Таблица покрытия ABI (раздел 3.2.1)
- [ ] **Замер**: `nm -D libemuruntime.so | grep " T " > emu_symbols.txt`
- [ ] Сравнить с символами libcudart (если доступна)
- [ ] Составить таблицу: реализовано / stub / не поддерживается
- [ ] Вставить таблицу в `ch3_ldpreload.tex` (убрать `% TODO: вставить таблицу`)

---

## Глава 4 (нет активных TODO)

---

## Глава 5 (нет активных TODO)

---

## Глава 6 (нет активных TODO)

---

## Глава 7

### Рисунок 7.1 — HTML-отчёт (панель пилюль)
- [ ] **Скриншот**:
  ```bash
  cmake --build build --target fft_custom
  cd build && cuemu --collect-profiling ./test_fft
  python -m tools.ptx_report --profiling profiling.txt \
      --ptx fft_custom.ptx --output report_fft.html
  # Открыть в браузере, сделать скриншот панели пилюль
  ```
  - Файл: `report/images/html_pills.png`
  - Заменить `\fbox` в `ch7_profiling.tex` на `\includegraphics`

---

## Глава 8

### 8.1 Модульные тесты — покрытие кода
- [ ] **Замер lcov**:
  ```bash
  cmake -B build_cov -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
  cmake --build build_cov --target unit_tests
  cd build_cov && ctest
  lcov --capture --directory . --output-file coverage.info
  genhtml coverage.info --output-directory coverage_html
  ```
  - Записать: строк покрыто X%, ветвей покрыто Y%
  - Заменить **X%** и **Y%** в `ch8_testing.tex` (раздел 8.1)

### 8.2.1 vadd_custom — таблица производительности (Таблица 8.1)
- [ ] **Замер**:
  ```bash
  for N in 256 1024 4096 16384; do
      cmake --build build --target vadd_custom
      # без профилировщика:
      time cuemu ./test_vadd $N
      # со сбором:
      CUEMU_COLLECT_PROFILING=1 time cuemu ./test_vadd $N
  done
  ```
  - Записать медианы (10 запусков) в `tab:vadd_perf`

### 8.2.2 gelu_custom
- [ ] **Замер**: запуск с `CUEMU_COLLECT_PROFILING=1`, проверить N={1024,4096,16384}
- [ ] Зафиксировать максимальную относительную ошибку и число инструкций PTX

### 8.2.3 softmax_custom
- [ ] **Замер**: число вызовов `bar.sync` в profiling.txt (grep `bar.sync`)
- [ ] `avg_branch_efficiency` для инструкций редукции из HTML-отчёта

### 8.2.4 mmul_custom — таблица 8.2
- [ ] **Замер** для M=N=K в {16, 32, 64, 128}
- [ ] `avg_bank_conflicts` для `ld.shared` из profiling

### 8.2.5 attention_custom
- [ ] **Замер**: time для seq_len={16,32,64}, d_head={16,32}
- [ ] Глубина стека дивергенции (max stack depth — можно вывести в debug лог)

### 8.2.6 fft_custom
- [ ] **Замер**: time для batches={1,8,32,128}
- [ ] Размер PTX-модуля в строках: `wc -l fft_custom.ptx`

### 8.2.7 conjugate_gradient
- [ ] **Замер**: time и число итераций для N={16,64,256}
- [ ] Минимум/медиана/максимум по 10 запускам с разными матрицами

### 8.2.8 Сводная таблица (Таблица 8.3)
- [ ] Заполнить после всех замеров выше — заменить все `\textit{TBD}`

### 8.3.1 branch_efficiency — таблица
- [ ] **Замер**:
  ```bash
  CUEMU_COLLECT_PROFILING=1 cuemu ./test_branch_divergence
  grep "@%p" profiling.txt | awk '{print $4, $6, ...}'
  ```
  - Заполнить таблицу: PC | маска | popcount | η_emu | η_analytic | Δ

### 8.3.2 bank_conflicts — Таблица 8.4
- [ ] **Замер**: запустить 3 паттерна из `bank_conflicts.cu`
- [ ] Заполнить `tab:bank_conflicts_test`

### 8.3.3 global_coalescing — Таблица 8.5
- [ ] **Замер**: запустить `test_coalescing` (unit) и `global_coalescing.cu`
- [ ] Заполнить `tab:coalescing_test` (5 строк)

### 8.3.4 write_ops
- [ ] Из HTML-отчёта vadd: считать write_ops для %f, %r, %rd
- [ ] Сравнить с аналитическим: write_ops[%f] = 32

### 8.3.5 .loc аннотация
- [ ] Компилировать fft с `-lineinfo`: `nvcc -lineinfo fft_custom.cu`
- [ ] Проверить 5 инструкций: их .loc в PTX vs. source:line в HTML

### 8.3.6 Агрегация — Таблица 8.6
- [ ] Запустить vadd, получить HTML, считать exec_count, avg_be, bc_total, txns
- [ ] Заполнить `tab:aggregation_test`

---

## Приложение Д (appendix_e.tex) — скриншоты HTML-отчёта

- [ ] **Рисунок Д.1** — Панель пилюль: `report/images/html_pills.png`
  - Команда: см. TODO Главы 7 выше

- [ ] **Рисунок Д.2** — Таблица инструкций с тепловой подсветкой
  - `report/images/html_table.png`
  - Секция «Instructions» в HTML для fft_custom

- [ ] **Рисунок Д.3** — Аннотированный исходный код
  - `report/images/html_source.png`
  - Секция «Source» в HTML для fft_custom, строки butterfly-loop

---

## Оформление

- [ ] Добавить `\label{lastpage}` перед `\end{document}` в `main.tex`
- [ ] Заполнить реальные ФИО студента и научного руководителя в `title.tex`
- [ ] Заполнить реальное название кафедры и университета в `title.tex`
- [ ] Заменить `X% / Y%` покрытия на реальные значения (lcov)
- [ ] Проверить стиль библиографии: `ugost2008.bst` должен быть установлен или скопирован в `report/`
- [ ] Проверить компиляцию: `cd report && pdflatex main && bibtex main && pdflatex main && pdflatex main`

---

## Контрольный список перед сдачей

| Пункт | Статус |
|-------|--------|
| Все `\textit{TBD}` заменены | ☐ |
| Все `\fbox{...}` (placeholder-рисунки) заменены на реальные | ☐ |
| lcov: X% и Y% заменены | ☐ |
| ФИО, кафедра, науч. руководитель заполнены | ☐ |
| Реферат: числа страниц/рисунков заполнены | ☐ |
| Библиография компилируется без ошибок | ☐ |
| Финальный PDF проверен на корректность разбивки | ☐ |
