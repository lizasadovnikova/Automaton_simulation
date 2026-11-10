import subprocess
import csv
import sys
import os
import re
from pathlib import Path

# Шлях до вашого скомпільованого V0_sequential.exe
# УВАГА: змініть цей шлях, якщо ваш exe лежить в іншій папці
v0_exe = r".\V0_sequential.exe"

# Перевіряємо, чи існує файл
if not os.path.exists(v0_exe):
    print(f"ПОМИЛКА: Файл {v0_exe} не знайдено.")
    print("Будь ласка, покладіть цей скрипт у папку з V0_sequential.exe або вкажіть повний шлях.")
    sys.exit(1)

# Параметри для експерименту
N = 200  # Розмір сітки (200 достатньо для перколяції, щоб було швидко)
d_values = [i / 100 for i in range(50, 71)]  # від 0.50 до 0.70 з кроком 0.01
num_realizations = 50  # Кількість запусків для кожного d

print(f"Починаємо експеримент: розмір сітки N={N}, запусків на кожне d={num_realizations}")
print("-" * 50)

results = []

for d in d_values:
    reached_edge_count = 0
    print(f"Тестуємо d={d:.2f}... ", end="", flush=True)
    
    for i in range(num_realizations):
        # Генеруємо унікальний seed для кожного запуску
        seed = 42 + int(d * 100) * 1000 + i
        
        # Запускаємо процес V0_sequential.exe N d seed
        # Важливо: V0_sequential.exe має приймати аргументи: <N> <d> <seed>
        cmd = [v0_exe, str(N), str(d), str(seed)]
        
        try:
            # Викликаємо програму і перехоплюємо її вивід
            process = subprocess.run(cmd, capture_output=True, text=True, check=True)
            output = process.stdout
            
            # Шукаємо в тексті, чи дійшов вогонь до правого краю
            # Очікується рядок на зразок "Reached the right edge: Yes" або "CSV_RESULT:... ,1,..."
            
            # Найнадійніший спосіб: шукати в CSV-рядку (як ви показали раніше: CSV_RESULT:1500,7732,857766,1,40.2251)
            # Передостаннє число - це флаг досягнення краю (1 - дійшов, 0 - не дійшов)
            csv_match = re.search(r'CSV_RESULT:.*?,(\d),[\d\.]+$', output, re.MULTILINE)
            
            if csv_match:
                reached = int(csv_match.group(1))
                if reached == 1:
                    reached_edge_count += 1
            else:
                # Резервний спосіб: шукати текстом
                if "Reached the right edge: Yes" in output or "Reached right edge: Yes" in output:
                    reached_edge_count += 1
                    
        except subprocess.CalledProcessError as e:
            print(f"\nПомилка запуску для d={d}, seed={seed}: {e}")
            sys.exit(1)
            
    # Обчислюємо ймовірність
    probability = reached_edge_count / num_realizations
    results.append({'d': round(d, 2), 'Probability': probability})
    
    print(f"Ймовірність = {probability:.2f} ({reached_edge_count}/{num_realizations})")

# Зберігаємо результати у CSV
csv_filename = "percolation_results.csv"
with open(csv_filename, mode='w', newline='') as file:
    writer = csv.DictWriter(file, fieldnames=['d', 'Probability'])
    writer.writeheader()
    for row in results:
        writer.writerow(row)

print("-" * 50)
print(f"Експеримент завершено! Дані збережено у {csv_filename}")
print("Тепер ви можете побудувати графік залежності Probability від d.")
